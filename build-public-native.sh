#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

root=$(cd -- "${BASH_SOURCE[0]%/*}" && pwd -P)
metadata=$root/public-source/native
source_epoch=${SOURCE_DATE_EPOCH:-1788134400}
android_cc=${ANDROID_CC:-clang}
glibc_runner=${GLIBC_RUNNER:-glibc-runner}
glibc_prefix=${GLIBC_PREFIX:-/data/data/com.termux/files/usr/glibc}
glibc_cc=${GLIBC_CC:-$glibc_prefix/bin/aarch64-linux-gnu-gcc}
glibc_cxx=${GLIBC_CXX:-$glibc_prefix/bin/aarch64-linux-gnu-g++}
allow_drift=${PUBLIC_NATIVE_ALLOW_TOOLCHAIN_DRIFT:-0}

usage() {
    printf 'usage: %s --check|/absolute/output-directory\n' "$0" >&2
    exit 2
}

die() {
    printf 'build-public-native: %s\n' "$*" >&2
    exit 1
}

case ${1:-} in
    --check)
        [[ $# -eq 1 ]] || usage
        mode=check
        output=
        ;;
    /*)
        [[ $# -eq 1 ]] || usage
        mode=build
        output=$1
        ;;
    *)
        usage
        ;;
esac

for tool in "$android_cc" "$glibc_runner" readelf sha256sum find sort xargs; do
    command -v -- "$tool" >/dev/null 2>&1 || die "missing tool: $tool"
done
[[ -x $glibc_cc ]] || die "missing glibc C compiler: $glibc_cc"
[[ -x $glibc_cxx ]] || die "missing glibc C++ compiler: $glibc_cxx"
[[ -f $metadata/source-lock.sha256 ]] || die 'missing source checksum lock'
[[ -f $metadata/source-trees.tsv ]] || die 'missing source tree lock'
[[ -f $metadata/artifacts.tsv ]] || die 'missing artifact inventory'

android_version=$($android_cc -dumpversion)
glibc_version=$($glibc_runner "$glibc_cc" -dumpfullversion -dumpversion)
if [[ $allow_drift != 1 ]]; then
    [[ $android_version == 21.1.8 ]] ||
        die "clang $android_version found; canonical version is 21.1.8 (set PUBLIC_NATIVE_ALLOW_TOOLCHAIN_DRIFT=1 for a non-canonical build)"
    [[ $glibc_version == 14.2.1 ]] ||
        die "glibc GCC $glibc_version found; canonical version is 14.2.1 (set PUBLIC_NATIVE_ALLOW_TOOLCHAIN_DRIFT=1 for a non-canonical build)"
fi

(cd -- "$root" && sha256sum --check --strict --quiet \
    public-source/native/source-lock.sha256) || die 'source checksum lock failed'

tree_digest() {
    local directory=$1
    (cd -- "$root" && find "$directory" -type f -print0 | LC_ALL=C sort -z |
        xargs -0 -r sha256sum | sha256sum | cut -d' ' -f1)
}

check_steamworks_sdk() {
    [[ -n ${STEAMWORKS_SDK_DIR:-} ]] ||
        die 'STEAMWORKS_SDK_DIR is required to build the bridge server; point it at a legally obtained Steamworks SDK 1.54 header directory'
    [[ $STEAMWORKS_SDK_DIR == /* ]] ||
        die 'STEAMWORKS_SDK_DIR must be an absolute path'
    sdk=$(cd -- "$STEAMWORKS_SDK_DIR" 2>/dev/null && pwd -P) ||
        die "invalid STEAMWORKS_SDK_DIR: $STEAMWORKS_SDK_DIR"
    for header in isteamclient.h isteamuser.h isteamfriends.h steam_api.h; do
        [[ -f $sdk/$header ]] || die "Steamworks SDK 1.54 header missing: $sdk/$header"
    done
    sdk_digest=$(cd -- "$sdk" && find . -type f -print0 | LC_ALL=C sort -z |
        xargs -0 -r sha256sum | sha256sum | cut -d' ' -f1)
    if [[ $allow_drift != 1 && $sdk_digest != 3c2d9ac8041c0b1e5a717ee521fea0db47f95f9d2e60e7eb10f00880f24c8c3e ]]; then
        die "Steamworks SDK tree is not the canonical SDK 1.54 input (set PUBLIC_NATIVE_ALLOW_TOOLCHAIN_DRIFT=1 for a non-canonical build)"
    fi
}

while IFS=$'\t' read -r directory expected; do
    [[ -n $directory && ${directory:0:1} != '#' ]] || continue
    [[ $directory != /* && $directory != *'..'* ]] ||
        die "unsafe source tree entry: $directory"
    [[ -d $root/$directory ]] || die "missing source tree: $directory"
    actual=$(tree_digest "$directory")
    [[ $actual == "$expected" ]] || die "source tree checksum failed: $directory"
done < "$metadata/source-trees.tsv"

inventory_count=0
while IFS=$'\t' read -r artifact state abi recipe sources expected; do
    [[ -n $artifact && ${artifact:0:1} != '#' ]] || continue
    [[ $artifact != /* && $artifact != *'..'* ]] ||
        die "unsafe artifact entry: $artifact"
    case $state in
        build|delegated|excluded|external)
            ;;
        *)
            die "invalid artifact state '$state' for $artifact"
            ;;
    esac
    if [[ $expected != - && -e $root/$artifact ]]; then
        actual=$(sha256sum "$root/$artifact" | cut -d' ' -f1)
        [[ $actual == "$expected" ]] || die "artifact checksum failed: $artifact"
    fi
    [[ $state != build ]] || ((++inventory_count))
done < "$metadata/artifacts.tsv"
[[ $inventory_count -eq 13 ]] || die "expected 13 directly built artifacts, found $inventory_count"

if [[ $mode == check ]]; then
    if [[ -n ${STEAMWORKS_SDK_DIR:-} ]]; then
        check_steamworks_sdk
        printf 'External Steamworks SDK input: OK (%s)\n' "$sdk_digest"
    else
        printf '%s\n' 'External Steamworks SDK input: not checked (set STEAMWORKS_SDK_DIR to validate it).'
    fi
    printf 'Public native source/toolchain inventory: OK (%d direct builds)\n' \
        "$inventory_count"
    printf '%s\n' 'Delegated builds: lsteamclient, tgcompat, patched glibc runtime.'
    exit 0
fi

check_steamworks_sdk

[[ ! -e $output ]] || die "refusing to overwrite output: $output"
output_parent=${output%/*}
[[ -d $output_parent ]] || die "output parent does not exist: $output_parent"
mkdir -- "$output"
mkdir -- "$output/steamrtarm64" "$output/steam-bridge"

export LC_ALL=C TZ=UTC SOURCE_DATE_EPOCH=$source_epoch
prefix_maps=(
    "-ffile-prefix-map=$root=."
    "-fdebug-prefix-map=$root=."
    "-fmacro-prefix-map=$root=."
)
glibc_cflags=(
    -std=c11 -O2 -Wall -Wextra -fPIC -fno-ident
    "${prefix_maps[@]}"
    -shared -Wl,--build-id=none -Wl,--as-needed
    -Wl,-z,relro,-z,now -Wl,--no-undefined
)

build_glibc_so() {
    local target=$1
    shift
    "$glibc_runner" "$glibc_cc" "${glibc_cflags[@]}" \
        "-frandom-seed=${target##*/}" "$@" -ldl -o "$output/$target"
}

cd -- "$root"
build_glibc_so steamrtarm64/steam-cert-shim.so \
    steam-cert-shim.c steam-cert-stat-shim.c \
    steam-cert-open-shim.c steam-cert-fopen64-shim.c
build_glibc_so steamrtarm64/steam-dlmopen-shim.so steam-dlmopen-shim-v2.c
build_glibc_so steamrtarm64/steam-helper-shim.so steam-helper-shim.c
build_glibc_so steamrtarm64/steam-module-shim.so steam-module-shim.c
build_glibc_so steamrtarm64/steam-open-shim.so steam-open-shim.c
build_glibc_so steamrtarm64/steam-sysv-shim.so -pthread steam-sysv-shim.c
build_glibc_so steamrtarm64/steam-x11-main-shim.so steam-x11-main-shim.c
build_glibc_so steam-legacy-tmp-shim.so steam-path-shim.c
build_glibc_so steam-proc-self-shim.so steam-proc-self-shim.c
build_glibc_so steam-unix-socket-shim.so steam-unix-socket-shim.c

"$glibc_runner" "$glibc_cc" -std=c11 -O3 -DNDEBUG -flto -fno-plt \
    -fno-ident -fno-semantic-interposition -ffunction-sections -fdata-sections \
    -Wall -Wextra -Werror -Wpedantic -Wformat=2 -Wshadow \
    "${prefix_maps[@]}" -frandom-seed=steam-native-lsof \
    "$root/reference-steamclienttermux/diagnostics/native-lsof.c" \
    -Wl,--build-id=none -Wl,-O2,--as-needed,--gc-sections,-z,relro,-z,now \
    -Wl,--dynamic-linker,"$glibc_prefix/lib/ld-linux-aarch64.so.1" \
    -o "$output/steam-native-lsof"

"$glibc_runner" "$glibc_cxx" -std=gnu++17 -O2 -Wall -Wextra \
    -fPIE -fno-ident "${prefix_maps[@]}" \
    -frandom-seed=lsteambridge-server -I"$root/steam-bridge" -I"$sdk" \
    "$root/steam-bridge/server.cpp" -pie -ldl \
    -Wl,--build-id=none -Wl,--as-needed -Wl,-z,relro,-z,now \
    -Wl,--no-undefined -Wl,-rpath,"$glibc_prefix/lib" \
    -o "$output/steam-bridge/lsteambridge-server"

"$android_cc" -std=c11 -O2 -Wall -Wextra -fPIE -fno-ident \
    "${prefix_maps[@]}" -frandom-seed=lsteambridge-client \
    -I"$root/steam-bridge" "$root/steam-bridge/client.c" -pie \
    -Wl,--build-id=none -Wl,--as-needed -Wl,-z,relro,-z,now \
    -Wl,--no-undefined -o "$output/steam-bridge/lsteambridge-client"

artifacts=(
    steamrtarm64/steam-cert-shim.so
    steamrtarm64/steam-dlmopen-shim.so
    steamrtarm64/steam-helper-shim.so
    steamrtarm64/steam-module-shim.so
    steamrtarm64/steam-open-shim.so
    steamrtarm64/steam-sysv-shim.so
    steamrtarm64/steam-x11-main-shim.so
    steam-legacy-tmp-shim.so
    steam-proc-self-shim.so
    steam-unix-socket-shim.so
    steam-native-lsof
    steam-bridge/lsteambridge-client
    steam-bridge/lsteambridge-server
)
for artifact in "${artifacts[@]}"; do
    readelf -h "$output/$artifact" | grep 'Machine:.*AArch64' >/dev/null ||
        die "wrong architecture: $artifact"
done
readelf -l "$output/steam-bridge/lsteambridge-client" |
    grep '/system/bin/linker64' >/dev/null || die 'bridge client is not a Bionic executable'
readelf -l "$output/steam-bridge/lsteambridge-server" |
    grep "$glibc_prefix/lib/ld-linux-aarch64.so.1" >/dev/null ||
    die 'bridge server is not a glibc executable'
readelf -l "$output/steam-native-lsof" |
    grep "$glibc_prefix/lib/ld-linux-aarch64.so.1" >/dev/null ||
    die 'steam-native-lsof is not a glibc executable'

{
    printf 'schema=1\nsource_date_epoch=%s\ncanonical=%s\n' \
        "$source_epoch" "$([[ $allow_drift == 1 ]] && printf no || printf yes)"
    printf 'android_cc=%s\nandroid_cc_version=%s\n' "$android_cc" "$android_version"
    printf 'glibc_cc=%s\nglibc_cc_version=%s\n' "$glibc_cc" "$glibc_version"
    printf 'glibc_cxx=%s\nglibc_runner=%s\n' "$glibc_cxx" "$glibc_runner"
    printf 'steamworks_sdk_sha256=%s\n' "$sdk_digest"
} > "$output/build-info.txt"

(
    cd -- "$output"
    : > SHA256SUMS
    for artifact in "${artifacts[@]}"; do
        sha256sum "$artifact" >> SHA256SUMS
    done
)
touch -d "@$source_epoch" "$output/build-info.txt" "$output/SHA256SUMS"
for artifact in "${artifacts[@]}"; do
    touch -d "@$source_epoch" "$output/$artifact"
done
printf 'Built %d reproducible native artifacts in %s\n' \
    "${#artifacts[@]}" "$output"
