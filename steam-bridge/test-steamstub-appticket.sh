#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

root=$(cd -- "${BASH_SOURCE[0]%/*}" && pwd -P)
project=$(cd -- "$root/.." && pwd -P)
compiler=${MINGW32_CC:-i686-w64-mingw32-gcc}
source_file=$root/probe-steamstub-appticket.c
executable=$root/probe-steamstub-appticket.exe
result=$root/steamstub-appticket-probe-result.txt

build_probe() {
    if ! command -v -- "$compiler" >/dev/null 2>&1; then
        printf 'SteamStub AppTicket probe: SKIP (missing %s)\n' "$compiler" >&2
        return 77
    fi
    "$compiler" -std=c11 -O2 -Wall -Wextra -Werror \
        "$source_file" -o "$executable"
    file "$executable"
}

run_probe() {
    local appid=${SteamAppId:-39500}
    local compat_data=${STEAM_COMPAT_DATA_PATH:-$project/steam-home/Steam/steamapps/compatdata/$appid}
    local steam_home=$project/steam-home/Steam
    local probe_result

    if [[ ! -x $executable ]]; then
        build_probe
    fi
    rm -f -- "$result"
    timeout --signal=TERM --kill-after=3s 30s \
        env -u LD_PRELOAD \
            SteamAppId="$appid" \
            SteamGameId="$appid" \
            STEAM_COMPAT_APP_ID="$appid" \
            STEAM_COMPAT_DATA_PATH="$compat_data" \
            STEAM_COMPAT_CLIENT_INSTALL_PATH="$steam_home" \
            STEAM_COMPAT_LIBRARY_PATHS="$steam_home" \
            PROTON_BIONIC_STEAM_BRIDGE=proxy \
            PROTON_BIONIC_WINEDEBUG=-all \
            "$BASH" "$project/proton-bionic-tool/proton" run "$executable"
    probe_result=$(tr -d '\r' < "$result")
    [[ $probe_result == 'SteamStub AppTicket probe: PASS' ]]
    printf 'SteamStub AppTicket probe: PASS\n'
}

case ${1:-build} in
    build)
        build_probe
        ;;
    run)
        run_probe
        ;;
    *)
        printf 'usage: %s [build|run]\n' "$0" >&2
        exit 2
        ;;
esac
