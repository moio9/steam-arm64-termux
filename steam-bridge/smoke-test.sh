#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

root=$(cd -- "${BASH_SOURCE[0]%/*}" && pwd -P)
project=$(cd -- "$root/.." && pwd -P)
socket=/data/data/com.termux/files/usr/tmp/lsteambridge-smoke-$(id -u).sock
log=$root/smoke-server.log
dump_dir=$root/dumps
server_pid=

mkdir -p -- "$dump_dir"

cleanup() {
    if [[ -n $server_pid ]]; then
        LSTEAM_BRIDGE_SOCKET=$socket "$root/lsteambridge-client" stop >/dev/null 2>&1 || true
        wait "$server_pid" 2>/dev/null || true
    fi
}
trap cleanup EXIT INT TERM HUP

env -u LD_PRELOAD \
    LD_PRELOAD="$project/steamrtarm64/steam-sysv-shim.so:$project/steamrtarm64/steam-path-shim.so:$project/reference-termux-glibc-compat/build/libtgcompat-robust.so" \
    LD_LIBRARY_PATH="$root:$project/steamrtarm64:/data/data/com.termux/files/usr/glibc/lib" \
    BREAKPAD_DUMP_LOCATION="$dump_dir" \
    TGCOMPAT_ROBUST_LIST=1 \
    STEAM_ARM64_TMP_ROOT=/data/data/com.termux/files/usr/tmp \
    STEAM_TMP=/data/data/com.termux/files/usr/tmp \
    SteamAppId=224260 \
    SteamGameId=224260 \
    STEAM_COMPAT_APP_ID=224260 \
    LSTEAM_BRIDGE_SOCKET="$socket" \
    LSTEAM_BRIDGE_WORKDIR="$root" \
    "$root/lsteambridge-server" "$project/steamrtarm64/steamclient-patched.so" \
    >"$log" 2>&1 &
server_pid=$!

for _ in {1..50}; do
    [[ -S $socket ]] && break
    kill -0 "$server_pid" 2>/dev/null || break
    sleep 0.1
done

LSTEAM_BRIDGE_SOCKET=$socket "$root/lsteambridge-client" status
LSTEAM_BRIDGE_SOCKET=$socket "$root/lsteambridge-client" ping
LSTEAM_BRIDGE_SOCKET=$socket "$root/lsteambridge-client" client-bind-any
LSTEAM_BRIDGE_SOCKET=$socket "$root/lsteambridge-client" stop
wait "$server_pid"
server_pid=
[[ ! -e $socket ]]
