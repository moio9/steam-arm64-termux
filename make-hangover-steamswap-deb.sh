#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail
umask 022

root=$(cd -- "${BASH_SOURCE[0]%/*}" && pwd -P)
source_epoch=${SOURCE_DATE_EPOCH:-1788134400}
[[ $source_epoch =~ ^[0-9]+$ ]] || { printf '%s\n' 'SOURCE_DATE_EPOCH must be numeric.' >&2; exit 1; }
export SOURCE_DATE_EPOCH=$source_epoch
source_dir=${HANGOVER_SOURCE_DIR:-${HOME:?HOME is required}/hangover-wine-11.16-src}
build_dir=${HANGOVER_BUILD_DIR:-$source_dir/build-steamswap}
output=${1:-}
meta=$root/public-source/hangover-steamswap
stage=

die() {
    printf 'make-hangover-steamswap-deb: %s\n' "$*" >&2
    exit 1
}
cleanup() {
    if [[ -n $stage && -d $stage && ! -L $stage &&
          $stage == /data/data/com.termux/files/usr/tmp/hangover-steamswap-deb.* ]]; then
        rm -rf -- "$stage"
    fi
}
trap cleanup EXIT HUP INT TERM

[[ $output == /* ]] || die 'usage: ./make-hangover-steamswap-deb.sh /absolute/new-output.deb'
[[ ! -e $output && ! -L $output ]] || die "refusing to overwrite: $output"
[[ -d $source_dir/.git && ! -L $source_dir ]] || die "missing Hangover source: $source_dir"
. "$meta/source.lock"
[[ $(git -C "$source_dir" rev-parse HEAD) == "$HANGOVER_COMMIT" ]] ||
    die 'Hangover checkout is not at the locked commit'
printf '%s  %s\n' "$PATCH_SHA256" "$meta/patches/0001-steamclient-swap-arm64.patch" |
    sha256sum -c - >/dev/null || die 'Hangover patch checksum mismatch'
git -C "$source_dir" apply --reverse --check "$meta/patches/0001-steamclient-swap-arm64.patch" ||
    die 'Hangover checkout does not contain the locked patch'

wineserver=$build_dir/server/wineserver
ntdll_so=$build_dir/dlls/ntdll/ntdll.so
ntdll_i386=$build_dir/dlls/ntdll/i386-windows/ntdll.dll
ntdll_aarch64=$build_dir/dlls/ntdll/aarch64-windows/ntdll.dll
printf '%s  %s\n%s  %s\n%s  %s\n%s  %s\n' \
    "$WINESERVER_SHA256" "$wineserver" \
    "$NTDLL_SO_SHA256" "$ntdll_so" \
    "$NTDLL_I386_SHA256" "$ntdll_i386" \
    "$NTDLL_AARCH64_SHA256" "$ntdll_aarch64" |
    sha256sum -c - >/dev/null || die 'rebuilt Hangover artifacts do not match source.lock'

stage=$(mktemp -d /data/data/com.termux/files/usr/tmp/hangover-steamswap-deb.XXXXXX)
pkg=$stage/root
prefix=data/data/com.termux/files/usr
install -d -m 0755 "$pkg/DEBIAN" "$pkg/$prefix/opt/hangover-wine/bin" \
    "$pkg/$prefix/opt/hangover-wine/lib/wine/aarch64-unix" \
    "$pkg/$prefix/opt/hangover-wine/lib/wine/i386-windows" \
    "$pkg/$prefix/opt/hangover-wine/lib/wine/aarch64-windows" \
    "$pkg/$prefix/share/doc/hangover-wine-steamswap"
install -m 0755 "$wineserver" "$pkg/$prefix/opt/hangover-wine/bin/wineserver"
install -m 0755 "$ntdll_so" "$pkg/$prefix/opt/hangover-wine/lib/wine/aarch64-unix/ntdll.so"
install -m 0644 "$ntdll_i386" "$pkg/$prefix/opt/hangover-wine/lib/wine/i386-windows/ntdll.dll"
install -m 0644 "$ntdll_aarch64" "$pkg/$prefix/opt/hangover-wine/lib/wine/aarch64-windows/ntdll.dll"
install -m 0644 "$meta/README.md" "$meta/source.lock" \
    "$meta/patches/0001-steamclient-swap-arm64.patch" \
    "$pkg/$prefix/share/doc/hangover-wine-steamswap/"
size=$(du -sk "$pkg/$prefix" | cut -f1)
cat >"$pkg/DEBIAN/control" <<EOF
Package: hangover-wine-steamswap
Version: 11.16-1
Architecture: aarch64
Maintainer: moio9 <noreply@github.com>
Installed-Size: $size
Depends: hangover-wine (= 11.16)
Replaces: hangover-wine (<= 11.16)
Section: x11
Priority: optional
Homepage: https://github.com/moio9/steam-arm64-termux
Description: Hangover 11.16 Steam client loader overlay for ARM64
 Installs the Wine loader fixes required by legacy Steam DRM under WOW64.
EOF
find "$pkg" -exec touch -h -d "@$source_epoch" {} +
dpkg-deb --root-owner-group --build "$pkg" "$output" >/dev/null
printf 'Created %s\nSHA-256: %s\n' "$output" "$(sha256sum "$output" | cut -d' ' -f1)"
