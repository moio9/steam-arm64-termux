#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail
umask 022

root=$(cd -- "${BASH_SOURCE[0]%/*}" && pwd -P)
mode=
destination=
cache=
temporary=

die()
{
    printf 'make-public-release: %s\n' "$*" >&2
    exit 1
}

usage()
{
    cat >&2 <<'EOF'
Usage:
  ./make-public-release.sh --check [--cache ABSOLUTE_DIRECTORY]
  ./make-public-release.sh --fetch-sources [--cache ABSOLUTE_DIRECTORY]
  ./make-public-release.sh --build ABSOLUTE_NEW_DIRECTORY [--cache ABSOLUTE_DIRECTORY]

The build mode verifies project-owned native binaries against clean rebuilds,
creates a redistribution-safe bootstrap, stages a clean source snapshot, and
creates the complete corresponding-source archive for the patched glibc.
STEAMWORKS_SDK_DIR must point to a legally obtained SDK 1.54 header directory;
the SDK is validated but is never copied into a release asset.
EOF
    exit 2
}

while (($#)); do
    case $1 in
        --check|--fetch-sources)
            [[ -z $mode ]] || usage
            mode=${1#--}
            shift
            ;;
        --build)
            [[ -z $mode ]] || usage
            mode=build
            shift
            (($#)) || usage
            destination=$1
            shift
            ;;
        --cache)
            shift
            (($#)) || usage
            cache=$1
            shift
            ;;
        -h|--help)
            usage
            ;;
        *)
            usage
            ;;
    esac
done
[[ -n $mode ]] || usage
if [[ -n $cache && $cache != /* ]]; then
    die '--cache must be an absolute path'
fi

package_identity=$(<"$root/PACKAGE-VERSION")
read -r package_name package_version trailing <<<"$package_identity"
[[ $package_name == steam-arm64-termux && -n $package_version && -z ${trailing:-} ]] ||
    die 'PACKAGE-VERSION has an unexpected format'
[[ $package_version =~ ^[0-9A-Za-z][0-9A-Za-z._+-]*$ ]] ||
    die "unsafe package version: $package_version"
source_epoch=${SOURCE_DATE_EPOCH:-1788134400}
[[ $source_epoch =~ ^[0-9]+$ ]] || die 'SOURCE_DATE_EPOCH must be numeric'
export SOURCE_DATE_EPOCH=$source_epoch

glibc_args=()
[[ -z $cache ]] || glibc_args+=(--cache "$cache")

cleanup()
{
    if [[ -n $temporary && -d $temporary && ! -L $temporary ]]; then
        case ${temporary##*/} in
            .steam-arm64-release.*) rm -rf -- "$temporary" ;;
        esac
    fi
}
trap cleanup EXIT HUP INT TERM

check_patch_snapshot()
{
    local helper lock patch expected actual source base
    helper=$root/refresh-public-lsteamclient-patch.sh
    lock=$root/public-source/lsteamclient/source.lock
    patch=$root/public-source/lsteamclient/patches/0001-termux-arm64-bridge.patch
    [[ -f $helper && -f $lock && -f $patch ]] ||
        die 'public lsteamclient patch workflow is incomplete'
    if [[ -e $root/proton-lsteamclient-src/.git ]]; then
        "$helper" --check
        return
    fi
    expected=$(awk -F= '$1 == "PATCH_SHA256" {print $2}' "$lock")
    [[ $expected =~ ^[0-9a-f]{64}$ ]] || die 'invalid lsteamclient patch checksum lock'
    actual=$(sha256sum "$patch" | cut -d' ' -f1)
    [[ $actual == "$expected" ]] ||
        die "lsteamclient patch checksum is stale: $actual"

    # In a maintainer checkout, catch bridge additions that were not captured
    # in the public patch. The clean public repository intentionally omits this
    # expanded Valve-derived development tree, so consumers skip this portion.
    source=$root/proton-lsteamclient-src/lsteamclient
    if [[ -d $source ]]; then
        while IFS= read -r base; do
            grep -F -- "+++ b/lsteamclient/$base" "$patch" >/dev/null ||
                die "public lsteamclient patch does not contain $base"
        done < <(find "$source" -maxdepth 1 -type f -name 'bridge_*.inc' -printf '%f\n' | LC_ALL=C sort)
        if find "$source" -maxdepth 1 -type f \( -name '*.orig' -o -name '*.rej' \) -print -quit |
                grep -q .; then
            die 'maintainer lsteamclient source contains .orig or .rej debris'
        fi
    fi
}

run_checks()
{
    local scripts script
    scripts=(
        bootstrap-public-steam.sh
        install-minimal-steam.sh
        make-public-package.sh
        make-glibc-source-package.sh
        build-public-native.sh
        refresh-public-lsteamclient-patch.sh
        refresh-public-native-locks.sh
        make-public-source-tree.sh
        public-source/lsteamclient/prepare-proton-source.sh
        public-source/lsteamclient/build-bionic-lsteamclient.sh
    )
    for script in "${scripts[@]}"; do
        [[ -f $root/$script ]] || die "missing public script: $script"
        bash -n "$root/$script"
    done
    check_patch_snapshot
    "$root/make-public-package.sh" --check
    "$root/make-glibc-source-package.sh" --check "${glibc_args[@]}"
    "$root/build-public-native.sh" --check
    "$root/make-public-source-tree.sh" --check
    printf '%s\n' 'Public release preflight: OK'
}

case $mode in
    check)
        run_checks
        exit 0
        ;;
    fetch-sources)
        "$root/make-glibc-source-package.sh" --fetch "${glibc_args[@]}"
        exit 0
        ;;
    build) ;;
    *) die "internal error: unsupported mode $mode" ;;
esac

[[ $destination == /* ]] || die 'release destination must be absolute'
[[ ! -e $destination && ! -L $destination ]] ||
    die "refusing to overwrite release destination: $destination"
destination_parent=${destination%/*}
[[ -n $destination_parent ]] || destination_parent=/
mkdir -p -- "$destination_parent"

run_checks
[[ -n ${STEAMWORKS_SDK_DIR:-} ]] ||
    die 'STEAMWORKS_SDK_DIR is required for a release build'

temporary=$(mktemp -d "$destination_parent/.steam-arm64-release.XXXXXX")
native_verify=$temporary/.native-verification
"$root/build-public-native.sh" "$native_verify"

while IFS=$'\t' read -r artifact state _abi _recipe _sources locked_sha; do
    [[ -n $artifact && $artifact != \#* && $state == build ]] || continue
    [[ -f $root/$artifact ]] || die "missing packaged native artifact: $artifact"
    [[ -f $native_verify/$artifact ]] ||
        die "clean native build omitted artifact: $artifact"
    actual_sha=$(sha256sum "$root/$artifact" | cut -d' ' -f1)
    [[ $actual_sha == "$locked_sha" ]] ||
        die "packaged native artifact is not locked: $artifact ($actual_sha)"
    cmp -s -- "$root/$artifact" "$native_verify/$artifact" ||
        die "packaged native artifact differs from clean rebuild: $artifact"
done < "$root/public-source/native/artifacts.tsv"

bootstrap_name=$package_name-$package_version-bootstrap.tar.zst
glibc_source_name=$package_name-$package_version-glibc-complete-source.tar.zst
source_name=$package_name-$package_version-source.tar.zst
source_tree=$temporary/$package_name-$package_version-source

"$root/make-public-package.sh" "$temporary/$bootstrap_name"
"$root/make-glibc-source-package.sh" --build "${glibc_args[@]}" --output "$temporary/$glibc_source_name"
"$root/make-public-source-tree.sh" "$source_tree"

source_tar=$temporary/.source.tar
tar --sort=name --mtime="@$source_epoch" --owner=0 --group=0 --numeric-owner --format=gnu -cf "$source_tar" -C "$temporary" "${source_tree##*/}"
zstd -q -19 -o "$temporary/$source_name" -- "$source_tar"

rm -rf -- "$native_verify" "$source_tree"
rm -f -- "$source_tar"

cat > "$temporary/RELEASE-MANIFEST.txt" <<EOF
Package: $package_name
Version: $package_version
Format: public download-only bootstrap plus source assets

$bootstrap_name
  Project runtime payload. Valve Steam, Ubuntu packages, DXVK, vkd3d-proton,
  Steamworks SDK files, games, credentials, and prebuilt lsteamclient.so are
  not included.

$glibc_source_name
  Complete corresponding source for the modified glibc runtime in the
  bootstrap, including locked upstream sources, patches, build metadata, and
  licenses.

$source_name
  Clean public project source snapshot suitable for publication.
EOF

(
    cd -- "$temporary"
    sha256sum "$bootstrap_name" "$glibc_source_name" "$source_name" > SHA256SUMS
    sha256sum -c SHA256SUMS >/dev/null
)
touch -d "@$source_epoch" "$temporary/RELEASE-MANIFEST.txt" "$temporary/SHA256SUMS" "$temporary/$source_name"

mv -- "$temporary" "$destination"
temporary=
printf 'Created public release directory: %s\n' "$destination"
printf 'Verify with: (cd %q && sha256sum -c SHA256SUMS)\n' "$destination"
