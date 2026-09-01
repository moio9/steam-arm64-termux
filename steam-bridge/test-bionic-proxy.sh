#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

root=$(cd -- "${BASH_SOURCE[0]%/*}" && pwd -P)
project=$(cd -- "$root/.." && pwd -P)
appid=${SteamAppId:-224260}
prefix=${STEAM_COMPAT_DATA_PATH:-$project/steam-home/Steam/steamapps/compatdata/$appid}
trace=${LSTEAM_BRIDGE_TRACE:-$root/bionic-proxy-probe-trace.log}

if command -v i686-w64-mingw32-gcc >/dev/null 2>&1; then
    i686-w64-mingw32-gcc -std=c11 -O2 -Wall -Wextra \
        "$root/probe-user-friends.c" -o "$root/probe-user-friends.exe"
fi

set +e
timeout --signal=TERM --kill-after=3s 30s env -u LD_PRELOAD \
    SteamAppId="$appid" \
    SteamGameId="$appid" \
    STEAM_COMPAT_APP_ID="$appid" \
    STEAM_COMPAT_DATA_PATH="$prefix" \
    PROTON_BIONIC_STEAM_BRIDGE=proxy \
    LSTEAM_BRIDGE_TRACE="$trace" \
    /data/data/com.termux/files/usr/bin/bash \
        "$project/proton-bionic-tool/proton" run \
        "$root/probe-user-friends.exe"
probe_status=$?
set -e
if ((probe_status != 0 && probe_status != 124)); then
    printf 'lsteambridge probe launcher failed: %d\n' "$probe_status" >&2
    exit "$probe_status"
fi

result=$root/steam-bridge-probe-result.txt
grep -q '^logged_on=1 ' "$result"
grep -Eq '^user_legacy=SteamUser011:1/1/[0-9]+ SteamUser012:1/1/[0-9]+ SteamUser013:1/1/[0-9]+ SteamUser014:1/1/[0-9]+ SteamUser015:1/1/[0-9]+' "$result"
grep -Eq '^user_extended=folder:[01] .* voice_available:[0-9]+ .* voice_get:[0-9]+ .* sample:[1-9][0-9]* phone:[01]/[01]/[01]/[01]' "$result"
grep -q '^network_messages=.* sent:1 received:1 ' "$result"
grep -q '^network_sockets=pair:1 .* sent:1 .* received:1 .* poll_sent:1 poll_received:1 .* close:1/1 destroy_group:1' "$result"
grep -q '^network_utils=alloc4:1 alloc3:1 .* ip_parse:1 .* identity_parse:1 .* config_result:1 .* direct:1' "$result"
grep -Eq '^network_send_many=v12_pair:1 submitted:1 result:[1-9][0-9]* received:1 payload12:lsteambridge-many-v12 v13_pair:1 submitted:1 result:[1-9][0-9]* received:1 payload13:lsteambridge-many-v13 close12:1/1 close13:1/1' "$result"
grep -q '^client_layout=SteamClient013 .* runframe:ok' "$result"
grep -q '^client_layout=SteamClient017 .* runframe:ok' "$result"
grep -q '^client_layout=SteamClient018 .* runframe:ok' "$result"
grep -q '^client_layout=SteamClient019 .* runframe:ok' "$result"
grep -q '^client_layout=SteamClient021 .* runframe:ok' "$result"
grep -q '^client_layout=SteamClient023 .* runframe:ok' "$result"

printf 'lsteambridge Bionic proxy probe: OK\n'
sed -n '1,24p' "$result"
