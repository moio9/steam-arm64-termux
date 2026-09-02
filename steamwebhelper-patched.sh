#!/data/data/com.termux/files/usr/bin/sh
set -eu

HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BASE=$(CDPATH= cd -- "$HERE/.." && pwd)
if [ ! -d "$BASE/steam-linux-libs" ]; then
    BASE=$(CDPATH= cd -- "$HERE/../.." && pwd)
fi
STEAM_LINUX_LIBS=$BASE/steam-linux-libs
export FONTCONFIG_FILE="$BASE/steam-fontconfig.conf"
export FONTCONFIG_PATH="$BASE"
export LD_LIBRARY_PATH="$BASE/tgcompat-glibc/current/lib:$STEAM_LINUX_LIBS:$HERE:$HERE/libs:/data/data/com.termux/files/usr/glibc/lib:/data/data/com.termux/files/usr/glibc/lib/pulseaudio"
export LIBGL_KOPPER_DISABLE=true
# Do not let a stale game launch environment classify Steam UI as the game.
unset SteamAppId SteamGameId SteamOverlayGameId
unset STEAM_COMPAT_APP_ID STEAM_COMPAT_DATA_PATH STEAM_COMPAT_INSTALL_PATH
unset STEAM_COMPAT_TOOL_PATHS STEAM_COMPAT_LIBRARY_PATHS STEAM_COMPAT_SHADER_PATH
unset STEAM_COMPAT_MEDIA_PATH STEAM_COMPAT_TRANSCODED_MEDIA_PATH
unset STEAM_COMPAT_PROTON STEAM_COMPAT_FEX_CONFIG STEAM_COMPAT_MOUNTS

exec "$HERE/steamwebhelper-patched" --disable-dev-shm-usage --no-sandbox --disable-gpu --disable-gpu-compositing "$@"
