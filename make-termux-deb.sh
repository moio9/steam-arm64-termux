#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail
umask 022

root=$(cd -- "${BASH_SOURCE[0]%/*}" && pwd -P)
packaging=$root/packaging/steam-arm64
# shellcheck source=packaging/steam-arm64/release.lock
. "$packaging/release.lock"
source_epoch=${SOURCE_DATE_EPOCH:-1788134400}
cache=${STEAM_ARM64_PACKAGE_CACHE:-$root/download-cache/termux-package}
output=${1:-}
stage=

die() {
    printf 'make-termux-deb: %s\n' "$*" >&2
    exit 1
}

cleanup() {
    if [[ -n $stage && -d $stage && ! -L $stage &&
          $stage == /data/data/com.termux/files/usr/tmp/steam-arm64-deb.* ]]; then
        rm -rf -- "$stage"
    fi
}
trap cleanup EXIT HUP INT TERM

[[ $output == /* ]] || die 'usage: ./make-termux-deb.sh /absolute/new-output.deb'
[[ ! -e $output && ! -L $output ]] || die "refusing to overwrite: $output"
[[ $PACKAGE_VERSION =~ ^[0-9][0-9.]*$ && $PACKAGE_REVISION =~ ^[1-9][0-9]*$ ]]
[[ $BOOTSTRAP_SHA256 =~ ^[0-9a-f]{64}$ && $BOOTSTRAP_SIZE =~ ^[1-9][0-9]*$ ]]
for tool in curl dpkg-deb install sha256sum stat mktemp; do
    command -v "$tool" >/dev/null 2>&1 || die "missing tool: $tool"
done

mkdir -p -- "$cache" "${output%/*}"
bootstrap=$cache/$BOOTSTRAP_FILENAME
if [[ ! -f $bootstrap || -L $bootstrap ||
      $(stat -c %s "$bootstrap" 2>/dev/null || true) != "$BOOTSTRAP_SIZE" ||
      $(sha256sum "$bootstrap" 2>/dev/null | cut -d' ' -f1) != "$BOOTSTRAP_SHA256" ]]; then
    partial=$bootstrap.partial
    rm -f -- "$partial"
    curl --fail --location --retry 3 --proto '=https' --tlsv1.2 \
        --output "$partial" "$BOOTSTRAP_URL"
    [[ $(stat -c %s "$partial") == "$BOOTSTRAP_SIZE" &&
       $(sha256sum "$partial" | cut -d' ' -f1) == "$BOOTSTRAP_SHA256" ]] || {
        rm -f -- "$partial"
        die 'downloaded bootstrap failed its locked identity check'
    }
    mv -- "$partial" "$bootstrap"
fi

stage=$(mktemp -d /data/data/com.termux/files/usr/tmp/steam-arm64-deb.XXXXXX)
pkgroot=$stage/root
prefix_rel=data/data/com.termux/files/usr
install -d -m 0755 -- "$pkgroot/DEBIAN" \
    "$pkgroot/$prefix_rel/bin" \
    "$pkgroot/$prefix_rel/share/steam-arm64-termux" \
    "$pkgroot/$prefix_rel/share/applications" \
    "$pkgroot/$prefix_rel/share/icons/hicolor/scalable/apps"
install -m 0755 -- "$packaging/steam-arm64" "$pkgroot/$prefix_rel/bin/steam-arm64"
install -m 0755 -- "$packaging/steam-arm64-setup" "$pkgroot/$prefix_rel/bin/steam-arm64-setup"
install -m 0644 -- "$packaging/steam-arm64.desktop" \
    "$pkgroot/$prefix_rel/share/applications/steam-arm64-termux.desktop"
install -m 0644 -- "$packaging/steam-arm64-termux.svg" \
    "$pkgroot/$prefix_rel/share/icons/hicolor/scalable/apps/steam-arm64-termux.svg"
install -m 0644 -- "$bootstrap" \
    "$pkgroot/$prefix_rel/share/steam-arm64-termux/$BOOTSTRAP_FILENAME"

installed_size=$(du -sk "$pkgroot/$prefix_rel" | cut -f1)
cat > "$pkgroot/DEBIAN/control" <<EOF
Package: steam-arm64
Version: $PACKAGE_VERSION-$PACKAGE_REVISION
Architecture: aarch64
Maintainer: moio9 <noreply@github.com>
Installed-Size: $installed_size
Depends: bash, coreutils, curl, python, zstd, patchelf
Section: games
Priority: optional
Homepage: https://github.com/moio9/steam-arm64-termux
Description: Native ARM64 Steam bootstrap and desktop integration for Termux
 Downloads locked upstream components on first launch. Valve client files,
 games, credentials and the Steamworks SDK are not included in this package.
EOF
cat > "$pkgroot/DEBIAN/postinst" <<'EOF'
#!/data/data/com.termux/files/usr/bin/sh
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database /data/data/com.termux/files/usr/share/applications >/dev/null 2>&1 || true
fi
printf '%s\n' 'Steam ARM64 installed. Open “Steam ARM64” from your desktop menu or run steam-arm64.'
EOF
chmod 0755 "$pkgroot/DEBIAN/postinst"

find "$pkgroot" -exec touch -h -d "@$source_epoch" {} +
dpkg-deb --root-owner-group --build "$pkgroot" "$output" >/dev/null
[[ $(dpkg-deb -f "$output" Package) == steam-arm64 ]]
[[ $(dpkg-deb -f "$output" Version) == "$PACKAGE_VERSION-$PACKAGE_REVISION" ]]
[[ $(dpkg-deb -f "$output" Architecture) == aarch64 ]]
printf 'Created %s\nSHA-256: %s\n' "$output" "$(sha256sum "$output" | cut -d' ' -f1)"
