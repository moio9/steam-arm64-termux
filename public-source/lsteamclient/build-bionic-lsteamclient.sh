#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

here=$(cd -- "${BASH_SOURCE[0]%/*}" && pwd -P)
project_root=$(cd -- "$here/../.." && pwd -P)
# shellcheck source=source.lock
. "$here/source.lock"

work_root=${LSTEAM_WORK_ROOT:-$here/work}
checkout=${PROTON_CHECKOUT:-$work_root/proton}
deps_dir=${LSTEAM_DEPS_DIR:-$work_root/deps}
build_dir=${LSTEAM_BUILD_DIR:-$work_root/build}
wine_root=${HANGOVER_WINE_ROOT:-/data/data/com.termux/files/usr/opt/hangover-wine}
bridge_include=${LSTEAM_BRIDGE_INCLUDE:-$project_root/steam-bridge}
jobs=${JOBS:-4}
cxx=${CXX:-clang++}
src=$checkout/lsteamclient
list_header=$deps_dir/include/wine/list.h
ntdll=$wine_root/lib/wine/aarch64-unix/ntdll.so
obj_dir=
output_tmp=
compile_pids=()

die()
{
    printf 'build-bionic-lsteamclient: %s\n' "$*" >&2
    exit 1
}

cleanup()
{
    local pid
    for pid in "${compile_pids[@]}"; do kill "$pid" 2>/dev/null || true; done
    for pid in "${compile_pids[@]}"; do wait "$pid" 2>/dev/null || true; done
    if [[ -n $output_tmp && -e $output_tmp ]]; then rm -f -- "$output_tmp"; fi
    if [[ -n $obj_dir && -d $obj_dir ]]; then rm -rf -- "$obj_dir"; fi
}
trap cleanup EXIT HUP INT TERM

[[ $jobs =~ ^[1-9][0-9]*$ ]] || die "invalid JOBS value: $jobs"
for command in "$cxx" git sha256sum file readelf mktemp; do
    command -v "$command" >/dev/null 2>&1 || die "missing command: $command"
done
[[ -d $checkout/.git ]] || die "run prepare-proton-source.sh first: $checkout"
[[ $(git -C "$checkout" rev-parse HEAD) == "$PROTON_COMMIT" ]] ||
    die 'Proton checkout is not at the locked commit'
[[ -f $src/bridge_proxy.inc ]] || die 'lsteamclient bridge patch is not applied'
[[ -f $bridge_include/protocol.h ]] ||
    die "missing bridge protocol header: $bridge_include/protocol.h"
[[ -f $list_header ]] || die "missing Wine list.h: $list_header"
printf '%s  %s\n' "$WINE_LIST_SHA256" "$list_header" |
    sha256sum -c - >/dev/null || die 'Wine list.h checksum mismatch'
[[ -d $wine_root/include/wine/windows ]] ||
    die "missing Hangover Wine headers below: $wine_root"
[[ -f $ntdll ]] || die "missing Hangover ntdll.so: $ntdll"

mkdir -p -- "$build_dir/include/wine"
install -m 0644 -- "$list_header" "$build_dir/include/wine/list.h"
obj_dir=$(mktemp -d "$build_dir/.objects.XXXXXX")
output_tmp=$(mktemp "$build_dir/.lsteamclient.so.XXXXXX")

flags=(
    -c -fPIC -O2 -std=gnu++17 -pthread
    -Wno-pragma-pack -Wno-format-extra-args
    -D__WINESRC__ -DNOMINMAX -DWINE_UNIX_LIB
    -DSTEAM_API_EXPORTS -Dprivate=public -Dprotected=public
    -DLSTEAM_BRIDGE_PROXY
    -I"$build_dir/include"
    -I"$wine_root/include"
    -I"$wine_root/include/wine/windows"
    -I"$bridge_include"
    -I"$src"
)

shopt -s nullglob
generated_sources=("$src"/cppISteam*.cpp)
shopt -u nullglob
((${#generated_sources[@]})) || die 'no cppISteam sources found'
sources=(
    "$src/unixlib.cpp"
    "$src/unixlib_generated.cpp"
    "$src/unix_steam_input_manual.cpp"
    "$src/unix_steam_networking_manual.cpp"
    "$src/unix_steam_remote_storage_manual.cpp"
    "$src/unix_steam_utils_manual.cpp"
    "$src/unix_steam_matchmaking_manual.cpp"
    "${generated_sources[@]}"
)
objects=()

wait_oldest()
{
    local pid=${compile_pids[0]}
    if ! wait "$pid"; then
        die "compiler process failed (pid $pid)"
    fi
    compile_pids=("${compile_pids[@]:1}")
}

LC_ALL=C
export LC_ALL
for source in "${sources[@]}"; do
    [[ -f $source ]] || die "missing source: $source"
    object=$obj_dir/${source##*/}.o
    objects+=("$object")
    "$cxx" "${flags[@]}" "$source" -o "$object" &
    compile_pids+=("$!")
    if ((${#compile_pids[@]} >= jobs)); then wait_oldest; fi
done
while ((${#compile_pids[@]})); do wait_oldest; done

"$cxx" -shared -pthread -Wl,-soname,lsteamclient.so -Wl,--no-undefined \
    -o "$output_tmp" "${objects[@]}" "$ntdll" -ldl
chmod 0755 "$output_tmp"
mv -- "$output_tmp" "$build_dir/lsteamclient.so"
output_tmp=

file "$build_dir/lsteamclient.so"
readelf -d "$build_dir/lsteamclient.so" | grep -E 'NEEDED|SONAME'
sha256sum "$build_dir/lsteamclient.so"
