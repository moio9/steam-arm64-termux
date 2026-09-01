#!/data/data/com.termux/files/usr/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
client_root=${STEAM_ARM64_CLIENT_ROOT:-$root/valve-client}
if [ ! -x "$client_root/steamrtarm64/steam" ]; then
    printf '%s\n' 'Valve client is not installed; run ./bootstrap-public-steam.sh first.' >&2
    exit 1
fi
export STEAM_ARM64_CLIENT_ROOT=$client_root
exec "$root/run-steam.sh" "$@"
