#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

root=$(cd -- "${BASH_SOURCE[0]%/*}" && pwd -P)
metadata=$root/public-source/native
source_lock=$metadata/source-lock.sha256
artifact_lock=$metadata/artifacts.tsv
mode=
work_dir=
write_lock=
owns_write_lock=0

usage() {
    printf 'usage: %s --check|--write\n' "$0" >&2
    exit 2
}

die() {
    printf 'refresh-public-native-locks: %s\n' "$*" >&2
    exit 1
}

cleanup() {
    if [[ -n $work_dir && -d $work_dir ]]; then
        rm -rf -- "$work_dir"
    fi
    if ((owns_write_lock)) && [[ -n $write_lock && -d $write_lock ]]; then
        rmdir -- "$write_lock" 2>/dev/null || true
    fi
}
trap cleanup EXIT
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM

[[ $# -eq 1 ]] || usage
case $1 in
    --check|--write)
        mode=$1
        ;;
    *)
        usage
        ;;
esac

export LC_ALL=C
for tool in sha256sum find sort cmp diff mktemp cut mkdir rmdir rm mv chmod; do
    command -v -- "$tool" >/dev/null 2>&1 || die "missing tool: $tool"
done
[[ -d $metadata ]] || die "missing metadata directory: $metadata"
[[ -f $source_lock && ! -L $source_lock ]] ||
    die "source lock is missing or is a symlink: $source_lock"
[[ -f $artifact_lock && ! -L $artifact_lock ]] ||
    die "artifact inventory is missing or is a symlink: $artifact_lock"

safe_relative_path() {
    local path=$1
    case $path in
        ''|/*|.|..|../*|*/../*|*/..|*//*|*$'\n'*|*$'\r'*)
            return 1
            ;;
    esac
    return 0
}

source_paths=(
    steam-cert-fopen64-shim.c
    steam-cert-open-shim.c
    steam-cert-shim.c
    steam-cert-stat-shim.c
    steam-dlmopen-shim-v2.c
    steam-helper-shim.c
    steam-module-shim.c
    steam-open-shim.c
    steam-path-shim.c
    steam-proc-self-shim.c
    steam-sysv-shim.c
    steam-unix-socket-shim.c
    steam-x11-main-shim.c
    reference-steamclienttermux/diagnostics/native-lsof.c
    steam-bridge/client.c
    steam-bridge/protocol.h
    steam-bridge/server.cpp
)
while IFS= read -r include_name; do
    [[ -n $include_name ]] || continue
    source_paths+=("steam-bridge/$include_name")
done < <(find "$root/steam-bridge" -maxdepth 1 -type f \
    -name '*_native.inc' -printf '%f\n' | sort)

generate_source_lock() {
    local path
    for path in "${source_paths[@]}"; do
        safe_relative_path "$path" || die "unsafe source path: $path"
        [[ -f $root/$path && ! -L $root/$path ]] ||
            die "source is missing or is a symlink: $path"
        (cd -- "$root" && sha256sum -- "$path")
    done
}

generate_artifact_lock() {
    local line artifact state abi recipe sources expected extra actual
    while IFS= read -r line || [[ -n $line ]]; do
        if [[ -z $line || ${line:0:1} == '#' ]]; then
            printf '%s\n' "$line"
            continue
        fi
        IFS=$'\t' read -r artifact state abi recipe sources expected extra <<< "$line"
        [[ -n $artifact && -n $state && -n $abi && -n $recipe && \
           -n $sources && -n $expected && -z ${extra:-} ]] ||
            die "invalid artifact inventory row: $line"
        safe_relative_path "$artifact" || die "unsafe artifact path: $artifact"
        case $state in
            build|delegated|excluded|external)
                ;;
            *)
                die "invalid artifact state '$state' for $artifact"
                ;;
        esac
        if [[ $state == build ]]; then
            [[ -f $root/$artifact && ! -L $root/$artifact ]] ||
                die "direct-build artifact is missing or is a symlink: $artifact"
            actual=$(sha256sum "$root/$artifact" | cut -d' ' -f1)
            expected=$actual
        fi
        printf '%s\t%s\t%s\t%s\t%s\t%s\n' \
            "$artifact" "$state" "$abi" "$recipe" "$sources" "$expected"
    done < "$artifact_lock"
}

if [[ $mode == --write ]]; then
    write_lock=$metadata/.refresh-public-native-locks.lock
    mkdir -- "$write_lock" 2>/dev/null ||
        die "another lock refresh may be active: $write_lock"
    owns_write_lock=1
    work_dir=$(mktemp -d "$metadata/.refresh-public-native-locks.XXXXXX") ||
        die 'cannot create same-filesystem staging directory'
else
    temporary_root=${TMPDIR:-/tmp}
    [[ $temporary_root == /* && $temporary_root != / ]] ||
        die "unsafe temporary directory: $temporary_root"
    [[ -d $temporary_root ]] || die "temporary directory does not exist: $temporary_root"
    work_dir=$(mktemp -d "$temporary_root/public-native-locks.XXXXXX") ||
        die 'cannot create temporary check directory'
fi

source_candidate=$work_dir/source-lock.sha256
artifact_candidate=$work_dir/artifacts.tsv
generate_source_lock > "$source_candidate"
generate_artifact_lock > "$artifact_candidate"

# Refuse to publish a mixed snapshot if another session is still rebuilding or
# editing the inputs while this helper is running.
source_verify=$work_dir/source-lock.verify.sha256
artifact_verify=$work_dir/artifacts.verify.tsv
generate_source_lock > "$source_verify"
generate_artifact_lock > "$artifact_verify"
cmp -s -- "$source_candidate" "$source_verify" ||
    die 'source inputs changed during refresh; wait for the implementation to stabilize'
cmp -s -- "$artifact_candidate" "$artifact_verify" ||
    die 'native artifacts changed during refresh; wait for all rebuilds to finish'

source_changed=0
artifact_changed=0
cmp -s -- "$source_lock" "$source_candidate" || source_changed=1
cmp -s -- "$artifact_lock" "$artifact_candidate" || artifact_changed=1

if [[ $mode == --check ]]; then
    if ((source_changed)); then
        printf '%s\n' 'source-lock.sha256 is stale:' >&2
        diff -u -- "$source_lock" "$source_candidate" >&2 || true
    fi
    if ((artifact_changed)); then
        printf '%s\n' 'artifacts.tsv contains stale direct-build hashes:' >&2
        diff -u -- "$artifact_lock" "$artifact_candidate" >&2 || true
    fi
    if ((source_changed || artifact_changed)); then
        die 'native locks need an explicit --write after the implementation and binaries are stable'
    fi
    printf 'Public native locks: OK (%d source inputs)\n' "${#source_paths[@]}"
    exit 0
fi

if ((source_changed)); then
    chmod --reference="$source_lock" "$source_candidate"
    mv -- "$source_candidate" "$source_lock"
fi
if ((artifact_changed)); then
    chmod --reference="$artifact_lock" "$artifact_candidate"
    mv -- "$artifact_candidate" "$artifact_lock"
fi
if ((source_changed || artifact_changed)); then
    printf 'Updated public native locks: source=%d artifacts=%d\n' \
        "$source_changed" "$artifact_changed"
else
    printf '%s\n' 'Public native locks already current; no files changed.'
fi
