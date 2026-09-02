#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

root=$(cd -- "${BASH_SOURCE[0]%/*}" && pwd -P)
client_root=${STEAM_ARM64_CLIENT_ROOT:-$root/valve-client}
[[ $client_root == /* ]] || {
    printf '%s\n' 'STEAM_ARM64_CLIENT_ROOT must be absolute.' >&2
    exit 1
}
cache_root=${STEAM_ARM64_DOWNLOAD_CACHE:-$root/download-cache}
bootstrap=$root/reference-steamclienttermux/scripts/bootstrap-steam-arm64-client.py
lock=$root/reference-steamclienttermux/config/steam-arm64-bootstrap-lock.json

[[ ${PREFIX:-} == /data/data/com.termux/files/usr ]] || {
    printf '%s\n' 'Run this bootstrap inside the standard Termux installation.' >&2
    exit 1
}
[[ $(uname -m) == aarch64 ]] || {
    printf '%s\n' 'The Steam ARM64 bootstrap requires an aarch64 device.' >&2
    exit 1
}
[[ -f $bootstrap && -f $lock ]] || {
    printf '%s\n' 'The public bootstrap scripts or Valve lock are missing.' >&2
    exit 1
}

command -v pkg >/dev/null || {
    printf '%s\n' 'pkg is unavailable; run this bootstrap inside Termux.' >&2
    exit 1
}
pkg install -y python patchelf curl zstd

python3 "$bootstrap" --lock "$lock" install \
    --cache "$cache_root/valve" --destination "$client_root"

# The locked Valve depots are distributed through ZIP payloads, whose Unix
# execute bits are not reliable after extraction on Android filesystems.
client_executables=(
    steam
    steamwebhelper
    steamsysinfo
    reaper
    gldriverquery
    vulkandriverquery
)
for name in "${client_executables[@]}"; do
    executable=$client_root/steamrtarm64/$name
    [[ -f $executable && ! -L $executable ]] || {
        printf 'Downloaded Valve executable is missing: %s\n' "$executable" >&2
        exit 1
    }
    chmod 0755 -- "$executable"
done

python3 "$root/fetch-ubuntu-runtime.py" --cache "$cache_root/ubuntu-debs"
"$root/fetch-proton-components.sh"

# These are project-built compatibility objects, not Valve files. Keep them in
# the public payload separately and install them over the end-user-fetched seed.
support_files=(
    steam-cert-shim.so
    steam-dlmopen-shim.so
    steam-helper-shim.so
    steam-module-shim.so
    steam-open-shim.so
    steam-sysv-shim.so
    steam-x11-main-shim.so
)
for name in "${support_files[@]}"; do
    source_file=$root/steamrtarm64/$name
    [[ -f $source_file && ! -L $source_file ]] || {
        printf 'Missing public runtime support object: %s\n' "$source_file" >&2
        exit 1
    }
    install -m 0755 -- "$source_file" "$client_root/steamrtarm64/$name"
done

STEAM_ARM64_CLIENT_ROOT=$client_root "$root/install-minimal-steam.sh" "$@"
python3 "$root/configure-steam-default-compat.py" \
    --config "$root/steam-home/Steam/config/config.vdf" \
    --marker "$root/.steam-arm64-default-compat-v1"
printf '%s\n' 'Public bootstrap complete. Start Steam with ./run-public-steam.sh'
