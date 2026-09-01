#!/bin/sh
set -eu
umask 022

die() {
    printf 'make-glibc-source-package: %s\n' "$*" >&2
    exit 1
}

usage() {
    cat >&2 <<'EOF'
Usage:
  ./make-glibc-source-package.sh --check [--cache DIR]
  ./make-glibc-source-package.sh --fetch [--cache DIR]
  ./make-glibc-source-package.sh --build [--cache DIR] [--output FILE]
  ./make-glibc-source-package.sh --verify ARCHIVE [--cache DIR]

--check never accesses the network. Missing fetchable inputs are reported but
do not make the local identity check fail. --fetch is the only network mode.
EOF
}

root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)
metadata_dir=$root/public-source/glibc
lock_file=$metadata_dir/source-lock.env
[ -f "$lock_file" ] || die "missing source lock: $lock_file"
# shellcheck disable=SC1090
. "$lock_file"

default_cache_base=${XDG_CACHE_HOME:-${HOME:-/tmp}/.cache}
source_cache=${GLIBC_SOURCE_CACHE:-$default_cache_base/steam-arm64-termux/glibc-source}
mode=
verify_archive=
output_archive=

while [ "$#" -gt 0 ]; do
    case $1 in
        --check|--fetch|--build)
            [ -z "$mode" ] || die 'select exactly one operation'
            mode=${1#--}
            ;;
        --verify)
            [ -z "$mode" ] || die 'select exactly one operation'
            mode=verify
            shift
            [ "$#" -gt 0 ] || die '--verify requires an archive path'
            verify_archive=$1
            ;;
        --cache)
            shift
            [ "$#" -gt 0 ] || die '--cache requires a directory'
            source_cache=$1
            ;;
        --output)
            shift
            [ "$#" -gt 0 ] || die '--output requires a file'
            output_archive=$1
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            usage
            die "unknown argument: $1"
            ;;
    esac
    shift
done

[ -n "$mode" ] || {
    usage
    exit 2
}
[ "${source_cache#/}" != "$source_cache" ] ||
    die '--cache must be an absolute path'

bundle_name=glibc-complete-source-$BUNDLE_VERSION
[ -n "$output_archive" ] || output_archive=$root/$bundle_name.tar.zst
glibc_source_cache=$source_cache/$GLIBC_SOURCE_FILENAME
glibc_packages_cache=$source_cache/repositories/glibc-packages.git
tgcompat_cache=$source_cache/repositories/termux-glibc-compat.git
termux_packages_cache=$source_cache/repositories/termux-packages.git

require_command() {
    command -v "$1" >/dev/null 2>&1 || die "required command is missing: $1"
}

validate_hex() {
    value=$1
    label=$2
    expected_length=$3
    case $value in
        *[!0-9a-f]*) die "$label is not lowercase hexadecimal" ;;
    esac
    [ "${#value}" -eq "$expected_length" ] ||
        die "$label has the wrong length"
}

validate_lock() {
    [ "$LOCK_SCHEMA_VERSION" = 1 ] || die 'unsupported source-lock schema'
    [ "$GLIBC_VERSION" = 2.44 ] || die 'unexpected glibc version in source lock'
    validate_hex "$GLIBC_SOURCE_SHA256" GLIBC_SOURCE_SHA256 64
    validate_hex "$GLIBC_PACKAGES_BASE_COMMIT" GLIBC_PACKAGES_BASE_COMMIT 40
    validate_hex "$GLIBC_PACKAGES_BASE_TREE" GLIBC_PACKAGES_BASE_TREE 40
    validate_hex "$GLIBC_PACKAGES_OVERLAY_COMMIT" GLIBC_PACKAGES_OVERLAY_COMMIT 40
    validate_hex "$GLIBC_PACKAGES_OVERLAY_TREE" GLIBC_PACKAGES_OVERLAY_TREE 40
    validate_hex "$GLIBC_PACKAGES_FINAL_COMMIT" GLIBC_PACKAGES_FINAL_COMMIT 40
    validate_hex "$GLIBC_PACKAGES_FINAL_TREE" GLIBC_PACKAGES_FINAL_TREE 40
    validate_hex "$TGCOMPAT_COMMIT" TGCOMPAT_COMMIT 40
    validate_hex "$TGCOMPAT_TREE" TGCOMPAT_TREE 40
    validate_hex "$TERMUX_PACKAGES_COMMIT" TERMUX_PACKAGES_COMMIT 40
    validate_hex "$TERMUX_PACKAGES_TREE" TERMUX_PACKAGES_TREE 40
    validate_hex "$BINARY_PACKAGE_SHA256" BINARY_PACKAGE_SHA256 64
    validate_hex "$RUNTIME_REVISION" RUNTIME_REVISION 64
    [ "$BINARY_PACKAGE_SHA256" = "$RUNTIME_REVISION" ] ||
        die 'runtime revision and binary package hash differ'
}

file_sha256() {
    sha256sum "$1" | awk '{print $1}'
}

verify_file() {
    file=$1
    expected_sha=$2
    expected_size=${3:-}
    [ -f "$file" ] && [ ! -L "$file" ] || die "missing regular file: $file"
    actual_sha=$(file_sha256 "$file")
    [ "$actual_sha" = "$expected_sha" ] ||
        die "SHA-256 mismatch for $file: $actual_sha"
    if [ -n "$expected_size" ]; then
        actual_size=$(stat -c %s "$file")
        [ "$actual_size" = "$expected_size" ] ||
            die "size mismatch for $file: $actual_size"
    fi
}

verify_worktree_commit() {
    checkout=$1
    commit=$2
    tree=$3
    label=$4
    [ -d "$checkout/.git" ] || return 1
    git -C "$checkout" cat-file -e "$commit^{commit}" 2>/dev/null || return 1
    actual_tree=$(git -C "$checkout" rev-parse "$commit^{tree}")
    [ "$actual_tree" = "$tree" ] || die "$label tree mismatch in $checkout"
    return 0
}

verify_bare_commit() {
    repository=$1
    commit=$2
    tree=$3
    label=$4
    [ -d "$repository" ] || return 1
    git --git-dir="$repository" cat-file -e "$commit^{commit}" 2>/dev/null || return 1
    actual_tree=$(git --git-dir="$repository" rev-parse "$commit^{tree}")
    [ "$actual_tree" = "$tree" ] || die "$label tree mismatch in cache"
    return 0
}

select_repository() {
    checkout=$1
    cache=$2
    commit=$3
    tree=$4
    label=$5
    if verify_worktree_commit "$checkout" "$commit" "$tree" "$label"; then
        printf 'worktree:%s\n' "$checkout"
    elif verify_bare_commit "$cache" "$commit" "$tree" "$label"; then
        printf 'bare:%s\n' "$cache"
    else
        return 1
    fi
}

repository_archive() {
    repository_spec=$1
    commit=$2
    destination=$3
    temporary_tar=$4
    repository_kind=${repository_spec%%:*}
    repository_path=${repository_spec#*:}
    mkdir -p -- "$destination"
    case $repository_kind in
        worktree) git -C "$repository_path" archive --format=tar \
            --output="$temporary_tar" "$commit" ;;
        bare) git --git-dir="$repository_path" archive --format=tar \
            --output="$temporary_tar" "$commit" ;;
        *) die "invalid repository source: $repository_spec" ;;
    esac
    tar -xf "$temporary_tar" -C "$destination"
    rm -f -- "$temporary_tar"
}

repository_diff() {
    repository_spec=$1
    old_commit=$2
    new_commit=$3
    output=$4
    repository_kind=${repository_spec%%:*}
    repository_path=${repository_spec#*:}
    case $repository_kind in
        worktree) git -C "$repository_path" diff --binary --full-index \
            --no-renames "$old_commit" "$new_commit" > "$output" ;;
        bare) git --git-dir="$repository_path" diff --binary --full-index \
            --no-renames "$old_commit" "$new_commit" > "$output" ;;
        *) die "invalid repository source: $repository_spec" ;;
    esac
    [ -s "$output" ] || die 'generated glibc-packages patch is empty'
}

check_runtime_identity() {
    runtime_selector_path=$root/$RUNTIME_SELECTOR
    [ -L "$runtime_selector_path" ] || die "runtime selector is missing: $RUNTIME_SELECTOR"
    actual_revision=$(readlink "$runtime_selector_path")
    [ "$actual_revision" = "$RUNTIME_REVISION" ] ||
        die "runtime selector mismatch: $actual_revision"
    marker=$root/tgcompat-glibc/$actual_revision/$RUNTIME_MARKER
    [ -f "$marker" ] && [ ! -L "$marker" ] || die "runtime marker is missing: $marker"
    IFS= read -r marker_revision < "$marker"
    [ "$marker_revision" = "$RUNTIME_REVISION" ] || die 'runtime marker mismatch'

    binary_package=$root/$BINARY_PACKAGE_PATH
    verify_file "$binary_package" "$BINARY_PACKAGE_SHA256" "$BINARY_PACKAGE_SIZE"
    if command -v dpkg-deb >/dev/null 2>&1; then
        [ "$(dpkg-deb -f "$binary_package" Package)" = "$BINARY_PACKAGE_NAME" ] ||
            die 'binary package name mismatch'
        [ "$(dpkg-deb -f "$binary_package" Version)" = "$BINARY_PACKAGE_VERSION" ] ||
            die 'binary package version mismatch'
        [ "$(dpkg-deb -f "$binary_package" Architecture)" = "$BINARY_PACKAGE_ARCHITECTURE" ] ||
            die 'binary package architecture mismatch'
    fi

    artifact_tar=$root/$CI_ARTIFACT_TAR_PATH
    if [ -f "$artifact_tar" ]; then
        verify_file "$artifact_tar" "$CI_ARTIFACT_TAR_SHA256" "$CI_ARTIFACT_TAR_SIZE"
    fi
    printf 'runtime_identity=ok sha256=%s\n' "$RUNTIME_REVISION"
}

report_sources() {
    missing=0
    if [ -f "$glibc_source_cache" ]; then
        verify_file "$glibc_source_cache" "$GLIBC_SOURCE_SHA256"
        printf 'glibc_source=available %s\n' "$glibc_source_cache"
    else
        printf 'glibc_source=missing_fetchable %s\n' "$glibc_source_cache"
        missing=1
    fi

    glibc_checkout=$root/reference-glibc-packages
    if verify_worktree_commit "$glibc_checkout" "$GLIBC_PACKAGES_FINAL_COMMIT" \
            "$GLIBC_PACKAGES_FINAL_TREE" glibc-packages &&
            verify_worktree_commit "$glibc_checkout" "$GLIBC_PACKAGES_BASE_COMMIT" \
            "$GLIBC_PACKAGES_BASE_TREE" glibc-packages-base; then
        [ "$(git -C "$glibc_checkout" rev-parse HEAD)" = "$GLIBC_PACKAGES_FINAL_COMMIT" ] ||
            die 'reference-glibc-packages HEAD is not the locked final commit'
        [ -z "$(git -C "$glibc_checkout" status --porcelain --untracked-files=no)" ] ||
            die 'reference-glibc-packages has tracked modifications'
        printf 'glibc_packages=available %s\n' "$glibc_checkout"
    elif verify_bare_commit "$glibc_packages_cache" "$GLIBC_PACKAGES_FINAL_COMMIT" \
            "$GLIBC_PACKAGES_FINAL_TREE" glibc-packages &&
            verify_bare_commit "$glibc_packages_cache" "$GLIBC_PACKAGES_BASE_COMMIT" \
            "$GLIBC_PACKAGES_BASE_TREE" glibc-packages-base; then
        printf 'glibc_packages=available %s\n' "$glibc_packages_cache"
    else
        printf 'glibc_packages=missing_fetchable %s\n' "$glibc_packages_cache"
        missing=1
    fi

    tgcompat_checkout=$root/reference-termux-glibc-compat
    if verify_worktree_commit "$tgcompat_checkout" "$TGCOMPAT_COMMIT" \
            "$TGCOMPAT_TREE" tgcompat; then
        [ "$(git -C "$tgcompat_checkout" rev-parse HEAD)" = "$TGCOMPAT_COMMIT" ] ||
            die 'reference-termux-glibc-compat HEAD is not the locked commit'
        [ -z "$(git -C "$tgcompat_checkout" status --porcelain --untracked-files=no)" ] ||
            die 'reference-termux-glibc-compat has tracked modifications'
        printf 'tgcompat=available %s\n' "$tgcompat_checkout"
    elif verify_bare_commit "$tgcompat_cache" "$TGCOMPAT_COMMIT" "$TGCOMPAT_TREE" tgcompat; then
        printf 'tgcompat=available %s\n' "$tgcompat_cache"
    else
        printf 'tgcompat=missing_fetchable %s\n' "$tgcompat_cache"
        missing=1
    fi

    if verify_bare_commit "$termux_packages_cache" "$TERMUX_PACKAGES_COMMIT" \
            "$TERMUX_PACKAGES_TREE" termux-packages; then
        printf 'termux_packages=available %s\n' "$termux_packages_cache"
    else
        printf 'termux_packages=missing_fetchable %s\n' "$termux_packages_cache"
        missing=1
    fi
    if [ "$missing" -eq 0 ]; then
        printf '%s\n' 'source_cache_complete=yes'
    else
        printf '%s\n' 'source_cache_complete=no (run --fetch before --build)'
    fi
}

fetch_file() {
    url=$1
    destination=$2
    expected_sha=$3
    if [ -f "$destination" ]; then
        verify_file "$destination" "$expected_sha"
        return
    fi
    mkdir -p -- "$(dirname -- "$destination")"
    temporary_download=$(mktemp "$destination.part.XXXXXX")
    if command -v curl >/dev/null 2>&1; then
        if ! curl --fail --location --retry 3 --output "$temporary_download" "$url"; then
            rm -f -- "$temporary_download"
            die "download failed: $url"
        fi
    elif command -v wget >/dev/null 2>&1; then
        if ! wget -O "$temporary_download" "$url"; then
            rm -f -- "$temporary_download"
            die "download failed: $url"
        fi
    else
        rm -f -- "$temporary_download"
        die 'curl or wget is required for --fetch'
    fi
    actual_sha=$(file_sha256 "$temporary_download")
    [ "$actual_sha" = "$expected_sha" ] || {
        rm -f -- "$temporary_download"
        die "downloaded SHA-256 mismatch for $url: $actual_sha"
    }
    mv -- "$temporary_download" "$destination"
}

fetch_commit() {
    repository=$1
    url=$2
    commit=$3
    tree=$4
    label=$5
    if verify_bare_commit "$repository" "$commit" "$tree" "$label"; then
        return
    fi
    mkdir -p -- "$(dirname -- "$repository")"
    if [ ! -d "$repository" ]; then
        git init --bare "$repository" >/dev/null
    fi
    git --git-dir="$repository" fetch --no-tags --depth=1 "$url" "$commit"
    verify_bare_commit "$repository" "$commit" "$tree" "$label" ||
        die "fetched repository lacks locked $label commit"
}

fetch_all() {
    require_command git
    require_command sha256sum
    fetch_file "$GLIBC_SOURCE_URL" "$glibc_source_cache" "$GLIBC_SOURCE_SHA256"
    fetch_commit "$glibc_packages_cache" "$GLIBC_PACKAGES_REPOSITORY" \
        "$GLIBC_PACKAGES_BASE_COMMIT" "$GLIBC_PACKAGES_BASE_TREE" glibc-packages-base
    fetch_commit "$glibc_packages_cache" "$GLIBC_PACKAGES_REPOSITORY" \
        "$GLIBC_PACKAGES_OVERLAY_COMMIT" "$GLIBC_PACKAGES_OVERLAY_TREE" glibc-packages-overlay
    fetch_commit "$glibc_packages_cache" "$GLIBC_PACKAGES_REPOSITORY" \
        "$GLIBC_PACKAGES_FINAL_COMMIT" "$GLIBC_PACKAGES_FINAL_TREE" glibc-packages-final
    fetch_commit "$tgcompat_cache" "$TGCOMPAT_REPOSITORY" \
        "$TGCOMPAT_COMMIT" "$TGCOMPAT_TREE" tgcompat
    fetch_commit "$termux_packages_cache" "$TERMUX_PACKAGES_REPOSITORY" \
        "$TERMUX_PACKAGES_COMMIT" "$TERMUX_PACKAGES_TREE" termux-packages
    printf 'source_cache_complete=yes cache=%s\n' "$source_cache"
}

cleanup_dir=
cleanup() {
    if [ -n "$cleanup_dir" ] && [ -d "$cleanup_dir" ] && [ ! -L "$cleanup_dir" ]; then
        case $cleanup_dir in
            "${TMPDIR:-/tmp}"/glibc-source-package.*) rm -rf -- "$cleanup_dir" ;;
        esac
    fi
}
trap cleanup EXIT HUP INT TERM

verify_archive_file() {
    archive=$1
    [ -f "$archive" ] && [ ! -L "$archive" ] || die "archive is missing: $archive"
    require_command zstd
    require_command tar
    require_command sha256sum
    cleanup_dir=$(mktemp -d "${TMPDIR:-/tmp}/glibc-source-package.XXXXXX")
    archive_tar=$cleanup_dir/source.tar
    zstd -q -d -c -- "$archive" > "$archive_tar"
    tar -tf "$archive_tar" > "$cleanup_dir/members.txt"
    while IFS= read -r member; do
        case $member in
            "$bundle_name"|"$bundle_name"/*) ;;
            *) die "archive contains an unexpected path: $member" ;;
        esac
        case /$member/ in
            *'/../'*) die "archive contains an unsafe path: $member" ;;
        esac
    done < "$cleanup_dir/members.txt"
    tar -xf "$archive_tar" -C "$cleanup_dir"
    bundle_root=$cleanup_dir/$bundle_name
    [ -d "$bundle_root" ] && [ ! -L "$bundle_root" ] || die 'archive root is invalid'
    cmp "$bundle_root/SOURCE-LOCK.env" "$lock_file" >/dev/null ||
        die 'archive source lock differs from this verifier'
    for required_file in README.md BUILD-PROVENANCE.md REBUILD.md rebuild-glibc.sh \
            SOURCE-MANIFEST.sha256 sources/$GLIBC_SOURCE_FILENAME \
            patches/glibc-packages-base-to-final.patch \
            licenses/glibc-COPYING.LIB \
            licenses/glibc-COPYING.LESSERv2 \
            licenses/glibc-COPYINGv2 licenses/glibc-COPYINGv3 \
            licenses/glibc-LICENSES \
            licenses/glibc-packages-LICENSE.md licenses/tgcompat-LICENSE \
            licenses/termux-packages-LICENSE.md; do
        [ -s "$bundle_root/$required_file" ] || die "archive lacks $required_file"
    done
    while IFS= read -r manifest_line; do
        manifest_path=${manifest_line#*  }
        [ "$manifest_path" != "$manifest_line" ] || die 'malformed source manifest line'
        case $manifest_path in
            ./*) ;;
            *) die "unsafe source manifest path: $manifest_path" ;;
        esac
        case /$manifest_path/ in
            *'/../'*) die "unsafe source manifest path: $manifest_path" ;;
        esac
    done < "$bundle_root/SOURCE-MANIFEST.sha256"
    (cd "$bundle_root" && sha256sum -c SOURCE-MANIFEST.sha256 >/dev/null)
    verify_file "$bundle_root/sources/$GLIBC_SOURCE_FILENAME" "$GLIBC_SOURCE_SHA256"
    grep -F "TERMUX_PKG_SHA256=$GLIBC_SOURCE_SHA256" \
        "$bundle_root/sources/glibc-packages-final/gpkg/glibc/build.sh" >/dev/null ||
        die 'final recipe does not pin the included glibc source'
    grep -F 'tgcompat-client' \
        "$bundle_root/sources/glibc-packages-final/gpkg/glibc/build.sh" >/dev/null ||
        die 'final recipe lacks the tgcompat overlay'
    printf 'archive_verification=ok sha256=%s file=%s\n' \
        "$(file_sha256 "$archive")" "$archive"
    cleanup
    cleanup_dir=
}

build_archive() {
    require_command git
    require_command tar
    require_command zstd
    require_command sha256sum
    require_command stat
    check_runtime_identity
    verify_file "$glibc_source_cache" "$GLIBC_SOURCE_SHA256"

    glibc_spec=$(select_repository "$root/reference-glibc-packages" \
        "$glibc_packages_cache" "$GLIBC_PACKAGES_FINAL_COMMIT" \
        "$GLIBC_PACKAGES_FINAL_TREE" glibc-packages-final) ||
        die 'locked glibc-packages source is unavailable; run --fetch'
    # The selected repository must also contain the exact unmodified base.
    glibc_repository_kind=${glibc_spec%%:*}
    glibc_repository_path=${glibc_spec#*:}
    if [ "$glibc_repository_kind" = worktree ]; then
        verify_worktree_commit "$glibc_repository_path" "$GLIBC_PACKAGES_BASE_COMMIT" \
            "$GLIBC_PACKAGES_BASE_TREE" glibc-packages-base ||
            die 'selected glibc-packages source lacks its base commit'
    else
        verify_bare_commit "$glibc_repository_path" "$GLIBC_PACKAGES_BASE_COMMIT" \
            "$GLIBC_PACKAGES_BASE_TREE" glibc-packages-base ||
            die 'selected glibc-packages source lacks its base commit'
    fi
    tgcompat_spec=$(select_repository "$root/reference-termux-glibc-compat" \
        "$tgcompat_cache" "$TGCOMPAT_COMMIT" "$TGCOMPAT_TREE" tgcompat) ||
        die 'locked tgcompat source is unavailable; run --fetch'
    termux_spec=$(select_repository "$root/nonexistent-termux-packages-checkout" \
        "$termux_packages_cache" "$TERMUX_PACKAGES_COMMIT" \
        "$TERMUX_PACKAGES_TREE" termux-packages) ||
        die 'locked termux-packages source is unavailable; run --fetch'

    case $output_archive in
        /*) ;;
        *) output_archive=$PWD/$output_archive ;;
    esac
    [ ! -e "$output_archive" ] && [ ! -L "$output_archive" ] ||
        die "refusing to overwrite output: $output_archive"
    mkdir -p -- "$(dirname -- "$output_archive")"

    cleanup_dir=$(mktemp -d "${TMPDIR:-/tmp}/glibc-source-package.XXXXXX")
    bundle_root=$cleanup_dir/$bundle_name
    mkdir -p -- "$bundle_root/sources" "$bundle_root/patches" "$bundle_root/licenses"
    cp -p -- "$metadata_dir/README.md" "$bundle_root/README.md"
    cp -p -- "$metadata_dir/BUILD-PROVENANCE.md" "$bundle_root/BUILD-PROVENANCE.md"
    cp -p -- "$metadata_dir/REBUILD.md" "$bundle_root/REBUILD.md"
    cp -p -- "$metadata_dir/rebuild-glibc.sh" "$bundle_root/rebuild-glibc.sh"
    chmod 0755 "$bundle_root/rebuild-glibc.sh"
    cp -p -- "$lock_file" "$bundle_root/SOURCE-LOCK.env"
    cp -p -- "$glibc_source_cache" "$bundle_root/sources/$GLIBC_SOURCE_FILENAME"

    repository_archive "$glibc_spec" "$GLIBC_PACKAGES_BASE_COMMIT" \
        "$bundle_root/sources/glibc-packages-base" "$cleanup_dir/base.tar"
    repository_archive "$glibc_spec" "$GLIBC_PACKAGES_FINAL_COMMIT" \
        "$bundle_root/sources/glibc-packages-final" "$cleanup_dir/final.tar"
    repository_archive "$tgcompat_spec" "$TGCOMPAT_COMMIT" \
        "$bundle_root/sources/termux-glibc-compat" "$cleanup_dir/tgcompat.tar"
    repository_archive "$termux_spec" "$TERMUX_PACKAGES_COMMIT" \
        "$bundle_root/sources/termux-packages" "$cleanup_dir/termux-packages.tar"
    repository_diff "$glibc_spec" "$GLIBC_PACKAGES_BASE_COMMIT" \
        "$GLIBC_PACKAGES_FINAL_COMMIT" \
        "$bundle_root/patches/glibc-packages-base-to-final.patch"
    cp -a -- "$bundle_root/sources/termux-glibc-compat/integration/termux-glibc" \
        "$bundle_root/sources/tgcompat-glibc-overlay"

    # glibc 2.44 stores COPYING.LIB as a symlink to COPYING.LESSERv2 in the
    # release tarball.  `tar -xO` emits no bytes for an archive symlink, so
    # copy the target text explicitly and retain the other license/notices
    # files under unambiguous names.
    for license_name in COPYING.LESSERv2 COPYINGv2 COPYINGv3 LICENSES; do
        tar -xJOf "$glibc_source_cache" \
            "glibc-$GLIBC_VERSION/$license_name" \
            > "$bundle_root/licenses/glibc-$license_name"
    done
    cp -p -- "$bundle_root/licenses/glibc-COPYING.LESSERv2" \
        "$bundle_root/licenses/glibc-COPYING.LIB"
    cp -p -- "$bundle_root/sources/glibc-packages-final/LICENSE.md" \
        "$bundle_root/licenses/glibc-packages-LICENSE.md"
    cp -p -- "$bundle_root/sources/termux-glibc-compat/LICENSE" \
        "$bundle_root/licenses/tgcompat-LICENSE"
    cp -p -- "$bundle_root/sources/termux-packages/LICENSE.md" \
        "$bundle_root/licenses/termux-packages-LICENSE.md"

    (
        cd "$bundle_root"
        find . -type f ! -name SOURCE-MANIFEST.sha256 -print0 |
            LC_ALL=C sort -z | xargs -0 sha256sum
    ) > "$bundle_root/SOURCE-MANIFEST.sha256"
    (cd "$bundle_root" && sha256sum -c SOURCE-MANIFEST.sha256 >/dev/null)

    normalized_tar=$cleanup_dir/$bundle_name.tar
    tar --sort=name --mtime='@0' --owner=0 --group=0 --numeric-owner \
        --format=gnu -cf "$normalized_tar" -C "$cleanup_dir" "$bundle_name"
    zstd -q -19 -f -o "$output_archive" -- "$normalized_tar"
    output_sha=$(file_sha256 "$output_archive")
    printf '%s  %s\n' "$output_sha" "$(basename -- "$output_archive")" \
        > "$output_archive.sha256"
    cleanup
    cleanup_dir=
    verify_archive_file "$output_archive"
    printf 'source_archive=%s\nsource_archive_sha256=%s\n' \
        "$output_archive" "$output_sha"
}

validate_lock
case $mode in
    check)
        require_command git
        require_command sha256sum
        require_command stat
        check_runtime_identity
        report_sources
        printf '%s\n' 'check=ok (no network access performed)'
        ;;
    fetch) fetch_all ;;
    build) build_archive ;;
    verify) verify_archive_file "$verify_archive" ;;
    *) die "internal error: unsupported mode $mode" ;;
esac
