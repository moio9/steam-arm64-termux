#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail
umask 022

root=$(cd -- "${BASH_SOURCE[0]%/*}" && pwd -P)
repo=$root/proton-lsteamclient-src
metadata=$root/public-source/lsteamclient
lock=$metadata/source.lock
patch=$metadata/patches/0001-termux-arm64-bridge.patch
mode=
work_dir=
write_lock=
owns_write_lock=0
tracked_paths=()
bridge_paths=()

usage()
{
    printf 'usage: %s --check|--write\n' "$0" >&2
    exit 2
}

die()
{
    printf 'refresh-public-lsteamclient-patch: %s\n' "$*" >&2
    exit 1
}

cleanup()
{
    if [[ -n $work_dir && -d $work_dir && ! -L $work_dir ]]; then
        case ${work_dir##*/} in
            public-lsteamclient-patch.*|.refresh-public-lsteamclient.*)
                rm -rf -- "$work_dir"
                ;;
        esac
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
    --check|--write) mode=$1 ;;
    *) usage ;;
esac

export LC_ALL=C
for tool in git awk grep sha256sum cmp sort stat mktemp cut mkdir rmdir rm mv chmod; do
    command -v -- "$tool" >/dev/null 2>&1 || die "missing tool: $tool"
done

[[ -f $lock && ! -L $lock ]] || die "source lock is missing or is a symlink: $lock"
[[ -f $patch && ! -L $patch ]] || die "public patch is missing or is a symlink: $patch"
[[ -d $repo && ! -L $repo ]] || die "maintainer Proton checkout is missing or is a symlink: $repo"
repo=$(cd -- "$repo" && pwd -P)
[[ $(git -C "$repo" rev-parse --show-toplevel) == "$repo" ]] || die "unexpected Git worktree root: $repo"

[[ $(grep -c '^PROTON_COMMIT=' "$lock") -eq 1 ]] || die 'source.lock must contain exactly one PROTON_COMMIT line'
[[ $(grep -c '^PATCH_SHA256=' "$lock") -eq 1 ]] || die 'source.lock must contain exactly one PATCH_SHA256 line'
proton_commit=$(awk -F= '$1 == "PROTON_COMMIT" {print $2}' "$lock")
locked_patch_sha=$(awk -F= '$1 == "PATCH_SHA256" {print $2}' "$lock")
[[ $proton_commit =~ ^[0-9a-f]{40}$ ]] || die "invalid PROTON_COMMIT in source.lock: $proton_commit"
[[ $locked_patch_sha =~ ^[0-9a-f]{64}$ ]] || die "invalid PATCH_SHA256 in source.lock: $locked_patch_sha"
[[ $(git -C "$repo" rev-parse HEAD) == "$proton_commit" ]] || die "maintainer checkout is not at locked commit $proton_commit"

git_config=(-c color.ui=false -c core.quotePath=true)
diff_options=(
    --no-ext-diff
    --no-textconv
    --no-renames
    --full-index
    --unified=3
    --inter-hunk-context=0
    --src-prefix=a/
    --dst-prefix=b/
)

validate_changed_path()
{
    local path=$1
    case $path in
        ''|/*|.|..|../*|*/../*|*/..|./*|*/./*|*/.|*//*|*$'\n'*|*$'\r'*|*$'\t'*)
            die "unsafe changed path: $path"
            ;;
        lsteamclient/*) ;;
        *) die "change outside lsteamclient is not publishable: $path" ;;
    esac
    case /$path/ in
        */steamworks_sdk_*/*)
            die "Steamworks SDK paths must never enter the patch: $path"
            ;;
    esac
    case ${path##*/} in
        *.orig|*.rej|*.log|*.LOG)
            die "debris/log path must not enter the patch: $path"
            ;;
        *.o|*.a|*.so|*.so.*|*.dll|*.exe|*.bin)
            die "binary-looking path must not enter the patch: $path"
            ;;
    esac
}

numstat_is_binary()
{
    local text=$1 added removed rest
    [[ -n $text ]] || return 1
    while IFS=$'\t' read -r added removed rest; do
        if [[ $added == - || $removed == - ]]; then return 0; fi
    done <<<"$text"
    return 1
}

untracked_numstat()
{
    local path=$1 output rc
    if output=$(git -C "$repo" "${git_config[@]}" diff --no-index --no-ext-diff --no-textconv --numstat -- /dev/null "$path"); then
        die "untracked bridge produced no diff: $path"
    else
        rc=$?
        [[ $rc -eq 1 ]] || die "git diff --no-index failed for $path (status $rc)"
    fi
    printf '%s' "$output"
}

collect_paths()
{
    local path status numstat relative
    tracked_paths=()
    bridge_paths=()

    while IFS= read -r -d '' path; do
        tracked_paths+=("$path")
    done < <(git -C "$repo" "${git_config[@]}" diff --no-ext-diff --no-textconv --name-only -z HEAD -- | sort -z)

    for path in "${tracked_paths[@]}"; do
        validate_changed_path "$path"
        status=$(git -C "$repo" "${git_config[@]}" diff --no-ext-diff --no-textconv --no-renames --name-status HEAD -- "$path")
        [[ $status == "M"$'\t'"$path" ]] || die "only tracked modifications are allowed (got '${status:-none}' for $path)"
        numstat=$(git -C "$repo" "${git_config[@]}" diff --no-ext-diff --no-textconv --numstat HEAD -- "$path")
        numstat_is_binary "$numstat" && die "binary tracked diff is not publishable: $path"
    done

    while IFS= read -r -d '' path; do
        validate_changed_path "$path"
        relative=${path#lsteamclient/}
        case $relative in
            bridge_*.inc)
                [[ $relative != */* ]] || die "nested untracked bridge path is not allowed: $path"
                ;;
            *)
                die "only untracked lsteamclient/bridge_*.inc files are allowed: $path"
                ;;
        esac
        [[ -f $repo/$path && ! -L $repo/$path ]] || die "untracked bridge is missing, non-regular, or a symlink: $path"
        numstat=$(untracked_numstat "$path")
        numstat_is_binary "$numstat" && die "binary untracked bridge is not publishable: $path"
        bridge_paths+=("$path")
    done < <(git -C "$repo" ls-files --others --exclude-standard -z | sort -z)
}

tracked_diff()
{
    git -C "$repo" "${git_config[@]}" diff "${diff_options[@]}" HEAD -- "${tracked_paths[@]}"
}

write_snapshot()
{
    local output=$1 path mode_bits content_sha
    {
        printf 'HEAD\0%s\0' "$(git -C "$repo" rev-parse HEAD)"
        printf 'TRACKED\0'
        tracked_diff
        printf '\0UNTRACKED\0%d\0' "${#bridge_paths[@]}"
        for path in "${bridge_paths[@]}"; do
            mode_bits=$(stat -c '%a' "$repo/$path")
            content_sha=$(sha256sum "$repo/$path" | cut -d' ' -f1)
            printf 'FILE\0%s\0%s\0%s\0' "$path" "$mode_bits" "$content_sha"
        done
    } >"$output"
}

generate_patch()
{
    local output=$1 path rc
    tracked_diff >"$output"
    for path in "${bridge_paths[@]}"; do
        if git -C "$repo" "${git_config[@]}" diff --no-index "${diff_options[@]}" -- /dev/null "$path" >>"$output"; then
            die "untracked bridge produced no patch: $path"
        else
            rc=$?
            [[ $rc -eq 1 ]] || die "patch generation failed for $path (status $rc)"
        fi
    done
    [[ -s $output ]] || die 'refusing to publish an empty lsteamclient patch'
}

if [[ $mode == --write ]]; then
    write_lock=$metadata/.refresh-public-lsteamclient.lock
    mkdir -- "$write_lock" 2>/dev/null || die "another lsteamclient patch refresh may be active: $write_lock"
    owns_write_lock=1
    work_dir=$(mktemp -d "$metadata/.refresh-public-lsteamclient.XXXXXX") || die 'cannot create same-filesystem staging directory'
else
    temporary_root=${TMPDIR:-}
    if [[ -z $temporary_root ]]; then
        if [[ -d /data/data/com.termux/files/usr/tmp ]]; then
            temporary_root=/data/data/com.termux/files/usr/tmp
        else
            temporary_root=/tmp
        fi
    fi
    [[ $temporary_root == /* && $temporary_root != / && -d $temporary_root ]] || die "unsafe temporary directory: $temporary_root"
    work_dir=$(mktemp -d "$temporary_root/public-lsteamclient-patch.XXXXXX") || die 'cannot create temporary check directory'
fi

snapshot_before=$work_dir/snapshot.before
snapshot_after=$work_dir/snapshot.after
candidate=$work_dir/0001-termux-arm64-bridge.patch

collect_paths
write_snapshot "$snapshot_before"
generate_patch "$candidate"
collect_paths
write_snapshot "$snapshot_after"
cmp -s -- "$snapshot_before" "$snapshot_after" || die 'lsteamclient sources changed during snapshot; wait for DRM work to stop'

candidate_sha=$(sha256sum "$candidate" | cut -d' ' -f1)
patch_changed=0
hash_changed=0
cmp -s -- "$patch" "$candidate" || patch_changed=1
[[ $candidate_sha == "$locked_patch_sha" ]] || hash_changed=1

if [[ $mode == --check ]]; then
    ((patch_changed == 0)) || die 'public lsteamclient patch differs byte-for-byte from the stable source snapshot'
    ((hash_changed == 0)) || die "PATCH_SHA256 is stale (locked $locked_patch_sha, generated $candidate_sha)"
    printf 'Public lsteamclient patch: OK (%d tracked, %d bridge files, %s)\n' "${#tracked_paths[@]}" "${#bridge_paths[@]}" "$candidate_sha"
    exit 0
fi

lock_candidate=$work_dir/source.lock
awk -v sha="$candidate_sha" '
    BEGIN { count = 0 }
    /^PATCH_SHA256=/ {
        print "PATCH_SHA256=" sha
        count++
        next
    }
    { print }
    END { if (count != 1) exit 42 }
' "$lock" >"$lock_candidate" || die 'could not produce source.lock with only PATCH_SHA256 replaced'

if ((patch_changed)); then
    chmod --reference="$patch" "$candidate"
    mv -- "$candidate" "$patch"
fi
if ((hash_changed)); then
    chmod --reference="$lock" "$lock_candidate"
    mv -- "$lock_candidate" "$lock"
fi
if ((patch_changed || hash_changed)); then
    printf 'Updated public lsteamclient patch atomically: patch=%d hash=%d %s\n' "$patch_changed" "$hash_changed" "$candidate_sha"
else
    printf '%s\n' 'Public lsteamclient patch and hash already current; no files changed.'
fi
