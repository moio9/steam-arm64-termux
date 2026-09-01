#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

root=$(cd -- "${BASH_SOURCE[0]%/*}" && pwd -P)
project=$(cd -- "$root/.." && pwd -P)
client_root=${STEAM_ARM64_CLIENT_ROOT:-$project}
[[ $client_root == /* && -d $client_root/steamrtarm64 ]] || {
    printf 'lsteambridge: invalid Steam client root: %s\n' "$client_root" >&2
    exit 1
}
socket=${LSTEAM_BRIDGE_SOCKET:-/data/data/com.termux/files/usr/tmp/lsteambridge.sock}
log=${LSTEAM_BRIDGE_LOG:-$root/server.log}
pidfile=${LSTEAM_BRIDGE_PIDFILE:-$root/server.pid}
appidfile=${LSTEAM_BRIDGE_APPID_FILE:-$root/server.appid}
appid=${SteamAppId:-${SteamGameId:-${STEAM_COMPAT_APP_ID:-0}}}
# Steam normally exports /tmp/dumps. Android applications cannot create /tmp,
# so keep Breakpad output inside the bridge directory regardless of the parent
# Steam environment.
dump_dir=$root/dumps
mkdir -p -- "$dump_dir"

if [[ -f $pidfile ]]; then
    old_pid=$(<"$pidfile")
    if [[ $old_pid =~ ^[0-9]+$ ]] && kill -0 "$old_pid" 2>/dev/null &&
       LSTEAM_BRIDGE_SOCKET=$socket "$root/lsteambridge-client" status >/dev/null 2>&1; then
        old_appid=0
        [[ -f $appidfile ]] && old_appid=$(<"$appidfile")
        if [[ $old_appid == "$appid" ]]; then
            printf 'lsteambridge: already running pid=%s appid=%s\n' \
                "$old_pid" "$appid"
            exit 0
        fi
        LSTEAM_BRIDGE_SOCKET=$socket "$root/lsteambridge-client" stop \
            >/dev/null 2>&1 || true
        for _ in {1..50}; do
            kill -0 "$old_pid" 2>/dev/null || break
            sleep 0.05
        done
    fi
fi

rm -f -- "$pidfile" "$appidfile"
server_env=(env -u LD_PRELOAD \
    LD_PRELOAD="$client_root/steamrtarm64/steam-sysv-shim.so:$client_root/steamrtarm64/steam-path-shim.so:$project/reference-termux-glibc-compat/build/libtgcompat-robust.so" \
    LD_LIBRARY_PATH="$root:$client_root/steamrtarm64:/data/data/com.termux/files/usr/glibc/lib" \
    BREAKPAD_DUMP_LOCATION="$dump_dir" \
    TGCOMPAT_ROBUST_LIST=1 \
    STEAM_ARM64_TMP_ROOT=/data/data/com.termux/files/usr/tmp \
    STEAM_TMP=/data/data/com.termux/files/usr/tmp \
    SteamAppId="$appid" \
    SteamGameId="$appid" \
    STEAM_COMPAT_APP_ID="$appid" \
    LSTEAM_BRIDGE_SOCKET="$socket" \
    LSTEAM_BRIDGE_WORKDIR="$root")
if [[ ${LSTEAM_BRIDGE_FOREGROUND:-0} == 1 ]]; then
    exec "${server_env[@]}" "$root/lsteambridge-server" \
        "$client_root/steamrtarm64/steamclient-patched.so"
fi
"${server_env[@]}" "$root/lsteambridge-server" \
    "$client_root/steamrtarm64/steamclient-patched.so" \
    >>"$log" 2>&1 &
pid=$!
printf '%s\n' "$pid" >"$pidfile"
printf '%s\n' "$appid" >"$appidfile"

for _ in {1..50}; do
    if [[ -S $socket ]] &&
       LSTEAM_BRIDGE_SOCKET=$socket "$root/lsteambridge-client" status >/dev/null 2>&1; then
        LSTEAM_BRIDGE_SOCKET=$socket "$root/lsteambridge-client" status
        exit 0
    fi
    kill -0 "$pid" 2>/dev/null || break
    sleep 0.1
done
printf 'lsteambridge: failed to start; see %s\n' "$log" >&2
tail -20 "$log" >&2 || true
exit 1
