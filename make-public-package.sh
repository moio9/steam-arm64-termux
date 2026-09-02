#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail
umask 022

root=$(cd -- "${BASH_SOURCE[0]%/*}" && pwd -P)
source_epoch=${SOURCE_DATE_EPOCH:-1788134400}
[[ $source_epoch =~ ^[0-9]+$ ]] || {
    printf '%s\n' 'SOURCE_DATE_EPOCH must be numeric.' >&2
    exit 1
}
mode=build
case ${1:-} in
    --check)
        [[ $# -eq 1 ]] || exit 2
        mode=check
        destination=
        ;;
    --verify)
        [[ $# -eq 1 ]] || exit 2
        mode=verify
        destination=
        ;;
    /*)
        [[ $# -eq 1 ]] || exit 2
        destination=$1
        ;;
    *)
        printf 'usage: %s --check|--verify|/absolute/output.tar.zst\n' "$0" >&2
        exit 2
        ;;
esac

runtime_revision=$(readlink -- "$root/tgcompat-glibc/current")
case $runtime_revision in
    ''|/*|*/*|.|..)
        printf 'invalid tgcompat runtime revision: %s\n' "$runtime_revision" >&2
        exit 1
        ;;
esac
runtime=tgcompat-glibc/$runtime_revision

support_objects=(
    steam-cert-shim.so
    steam-dlmopen-shim.so
    steam-helper-shim.so
    steam-module-shim.so
    steam-open-shim.so
    steam-sysv-shim.so
    steam-x11-main-shim.so
)
contents=(
    LICENSE PACKAGE-VERSION README.md THIRD-PARTY-NOTICES.md
    CONTRIBUTING.md LICENSING.md SOURCE-PROVENANCE.md
    licenses/GPL-2.0.txt licenses/LGPL-2.1.txt
    bootstrap-public-steam.sh configure-steam-default-compat.py
    install-minimal-steam.sh
    make-public-package.sh make-public-release.sh make-public-source-tree.sh
    make-glibc-source-package.sh build-public-native.sh
    refresh-public-lsteamclient-patch.sh refresh-public-native-locks.sh
    generate-ubuntu-runtime-lock.py
    run-public-steam.sh run-steam.sh run-steam-tgcompat.sh
    fetch-ubuntu-runtime.py ubuntu-runtime-lock.json
    fetch-proton-components.sh steam-linux-official-packages.txt
    steamwebhelper-patched.sh steam-fontconfig.conf steam-native-lsof
    steam-legacy-tmp-shim.so steam-proc-self-shim.so
    steam-unix-socket-shim.so
    steam-sdkarm64/steam-launch-wrapper
    reference-steamclienttermux/LICENSE
    reference-steamclienttermux/config/steam-arm64-bootstrap-lock.json
    reference-steamclienttermux/scripts/bootstrap-steam-arm64-client.py
    reference-steamclienttermux/diagnostics/native-lsof.c
    reference-termux-glibc-compat
    tgcompat-glibc/current "$runtime/lib" "$runtime/etc"
    "$runtime/.tgcompat-package-sha256"
    proton-bionic-tool/COMPONENTS.md
    proton-bionic-tool/proton proton-bionic-tool/steam-runtime-steam-remote
    proton-bionic-tool/compatibilitytool.vdf
    proton-bionic-tool/toolmanifest.vdf
    proton-bionic-fex-tool/proton
    proton-bionic-fex-tool/compatibilitytool.vdf
    proton-bionic-fex-tool/toolmanifest.vdf
    proton-hangover-glibc-tool/proton
    proton-hangover-glibc-tool/compatibilitytool.vdf
    proton-hangover-glibc-tool/toolmanifest.vdf
    steam-bridge/README.md steam-bridge/LICENSE steam-bridge/build.sh
    steam-bridge/client.c steam-bridge/server.cpp steam-bridge/protocol.h
    steam-bridge/proton-functions.sh steam-bridge/run-server.sh
    steam-bridge/lsteambridge-client steam-bridge/lsteambridge-server
    public-source/glibc public-source/native public-source/lsteamclient
    .gitattributes .gitignore
)
native_sources=(
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
)
contents+=("${native_sources[@]}")
for path in "$root"/steam-bridge/*_native.inc; do
    [[ -f $path ]] && contents+=("${path#"$root/"}")
done
for object in "${support_objects[@]}"; do
    contents+=("steamrtarm64/$object")
done

for item in "${contents[@]}"; do
    [[ -e $root/$item || -L $root/$item ]] || {
        printf 'missing public component: %s\n' "$item" >&2
        exit 1
    }
done

# Fail closed if the upstream locks no longer describe download-only payloads.
python3 - "$root" <<'PY'
import json
import hashlib
import pathlib
import sys

root = pathlib.Path(sys.argv[1])
valve = json.loads((root / "reference-steamclienttermux/config/steam-arm64-bootstrap-lock.json").read_text())
if valve.get("redistribution") is not False:
    raise SystemExit("Valve lock no longer forbids redistribution")
ubuntu = json.loads((root / "ubuntu-runtime-lock.json").read_text())
if ubuntu.get("schema") != 1 or not ubuntu.get("packages") or not ubuntu.get("libraries"):
    raise SystemExit("Ubuntu runtime lock is incomplete")
for package in ubuntu["packages"]:
    if not package.get("url", "").startswith("https://snapshot.ubuntu.com/ubuntu/"):
        raise SystemExit("Ubuntu lock contains a non-snapshot URL")

lsteam_root = root / "public-source/lsteamclient"
lock_values = {}
for line in (lsteam_root / "source.lock").read_text().splitlines():
    if "=" in line and not line.lstrip().startswith("#"):
        key, value = line.split("=", 1)
        lock_values[key] = value
expected_patch = lock_values.get("PATCH_SHA256", "")
patch = lsteam_root / "patches/0001-termux-arm64-bridge.patch"
actual_patch = hashlib.sha256(patch.read_bytes()).hexdigest()
if len(expected_patch) != 64 or actual_patch != expected_patch:
    raise SystemExit("public lsteamclient patch checksum is stale")
PY

for forbidden in \
    steam-home valve-client steam-linux-libs download-cache legacycompat \
    clientui graphics public resource steam steamui steamrtarm64/steam \
    steamrtarm64/steamclient.so steamrtarm64/steamui.so \
    proton-bionic-tool/vendor proton-lsteamclient-src \
    lsteamclient-bionic-build
do
    for selected in "${contents[@]}"; do
        if [[ $selected == "$forbidden" || $selected == "$forbidden"/* ]]; then
            printf 'forbidden upstream payload selected: %s\n' "$selected" >&2
            exit 1
        fi
    done
done

excludes=(
    '*/.git' '*/.git/*' '*/__pycache__' '*/__pycache__/*'
    '*/work' '*/work/*' '*/.work' '*/.work/*'
    '*/cache' '*/cache/*' '*/.cache' '*/.cache/*'
    '*/download-cache' '*/download-cache/*'
    '*/output' '*/output/*' '*/out' '*/out/*'
    '*/steamworks_sdk_*' '*/steamworks_sdk_*/*'
    'proton-lsteamclient-src' 'proton-lsteamclient-src/*'
    'lsteamclient-bionic-build' 'lsteamclient-bionic-build/*'
    '*.orig' '*.rej' '*.log'
    '*/.env' '*/.env.*' '*/.netrc' '*/.npmrc'
    '*/credentials' '*/credentials/*' '*/secrets' '*/secrets/*'
    '*/id_rsa' '*/id_rsa.pub' '*/id_ed25519' '*/id_ed25519.pub'
    '*.pem' '*.key' '*.p12' '*.pfx'
    'reference-termux-glibc-compat/build/*.o'
    'reference-termux-glibc-compat/build/*.a'
    'reference-termux-glibc-compat/build/test-*'
    'reference-termux-glibc-compat/build/pthread-basic'
    'reference-termux-glibc-compat/build/robust-list'
    'reference-termux-glibc-compat/build/sysv-semaphore'
    'reference-termux-glibc-compat/build/sysv-shm'
)

cd -- "$root"
# Apply the prefix to archive member names only. GNU tar otherwise rewrites
# symlink targets as well and would break tgcompat-glibc/current after extract.
tar_args=(--zstd --sort=name --mtime="@$source_epoch" --format=gnu
    --owner=0 --group=0 --numeric-owner --mode='u+rwX,go+rX,go-w'
    --transform='flags=r;s,^,steam-arm64-termux/,')
for pattern in "${excludes[@]}"; do
    tar_args+=(--exclude="$pattern")
done

if [[ $mode == check ]]; then
    printf 'Public package manifest: OK (%s selected roots)\n' "${#contents[@]}"
    printf '%s\n' 'Valve, Ubuntu, DXVK, vkd3d-proton, Proton and Steamworks SDK inputs are not redistributed.'
    printf '%s\n' 'lsteamclient is built locally from the pinned public patch workflow.'
    exit 0
fi
if [[ $mode == verify ]]; then
    tar "${tar_args[@]}" -cf /dev/null "${contents[@]}"
    printf '%s\n' 'Public package traversal: OK'
    exit 0
fi

[[ ! -e $destination && ! -e $destination.sha256 ]] || {
    printf 'refusing to overwrite destination or checksum: %s\n' "$destination" >&2
    exit 1
}
tar "${tar_args[@]}" -cf "$destination" "${contents[@]}"
archive_sha=$(sha256sum "$destination" | cut -d' ' -f1)
printf '%s  %s\n' "$archive_sha" "${destination##*/}" > "$destination.sha256"
chmod 0644 -- "$destination" "$destination.sha256"
printf 'created public bootstrap %s (%s)\n' \
    "$destination" "$(du -h "$destination" | cut -f1)"
