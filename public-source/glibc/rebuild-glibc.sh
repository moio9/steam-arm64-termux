#!/bin/sh
set -eu

usage() {
    printf 'Usage: %s ABSOLUTE_EMPTY_WORKDIR [--run]\n' "$0" >&2
}

die() {
    printf 'rebuild-glibc: %s\n' "$*" >&2
    exit 1
}

[ "$#" -ge 1 ] && [ "$#" -le 2 ] || {
    usage
    exit 2
}

workdir=$1
run_build=${2:-}
[ "$run_build" = '' ] || [ "$run_build" = '--run' ] || {
    usage
    exit 2
}
[ "${workdir#/}" != "$workdir" ] || die 'work directory must be absolute'

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)
source_root=$script_dir/sources
final_source=$source_root/glibc-packages-final
termux_source=$source_root/termux-packages
glibc_archive=$source_root/glibc-2.44.tar.xz

[ -d "$final_source/gpkg/glibc" ] || die 'final glibc-packages source is missing'
[ -f "$termux_source/build-package.sh" ] || die 'termux-packages source is missing'
[ -f "$glibc_archive" ] || die 'GNU glibc source archive is missing'

actual_sha=$(sha256sum "$glibc_archive" | awk '{print $1}')
[ "$actual_sha" = '37f600f2bef3c5e8300147059568b2a2e40a7ad6ccc65ce942556d49429cc667' ] ||
    die "GNU glibc source hash mismatch: $actual_sha"

if [ -e "$workdir" ]; then
    [ -d "$workdir" ] || die 'work path exists and is not a directory'
    [ -z "$(find "$workdir" -mindepth 1 -maxdepth 1 -print -quit)" ] ||
        die 'work directory is not empty'
else
    mkdir -p -- "$workdir"
fi

cp -a -- "$final_source"/. "$workdir"/
for build_input in build-package.sh clean.sh packages x11-packages \
        root-packages scripts ndk-patches; do
    [ -e "$termux_source/$build_input" ] ||
        die "termux-packages input is missing: $build_input"
    cp -a -- "$termux_source/$build_input" "$workdir/$build_input"
done
mkdir -p -- "$workdir/upstream-source"
cp -p -- "$glibc_archive" "$workdir/upstream-source/glibc-2.44.tar.xz"

printf 'Prepared pinned build tree: %s\n' "$workdir"
printf 'Verified upstream source: %s\n' "$workdir/upstream-source/glibc-2.44.tar.xz"

if [ "$run_build" = '--run' ]; then
    command -v docker >/dev/null 2>&1 || die 'docker is required for --run'
    cd -- "$workdir"
    TERMUX_BUILDER_IMAGE_NAME='ghcr.io/termux/package-builder-cgct' \
        ./scripts/run-docker.sh ./build-package.sh -I -a aarch64 \
        --library glibc glibc
else
    printf '%s\n' 'Build not started; pass --run after preparing the pinned Docker image.'
fi
