#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

HERE=$(cd -- "$(dirname -- "$0")" && pwd -P)
CLIENT_ROOT=${STEAM_ARM64_CLIENT_ROOT:-$HERE}
[[ $CLIENT_ROOT == /* && -d $CLIENT_ROOT ]] || {
  printf 'Invalid Steam client root: %s\n' "$CLIENT_ROOT" >&2
  exit 1
}
CLIENT_ROOT=$(cd -- "$CLIENT_ROOT" && pwd -P)
STEAM_DIR=$CLIENT_ROOT/steamrtarm64
GLIBC_PREFIX=$HERE/tgcompat-glibc/current
MAIN_GLIBC=$PREFIX/glibc
TGCOMPAT_REPO=$HERE/reference-termux-glibc-compat
STEAM_LINUX_LIBS=${STEAM_ARM64_LINUX_LIBS:-$HERE/steam-linux-libs}
[[ $STEAM_LINUX_LIBS == /* ]] || {
    printf '%s\n' 'STEAM_ARM64_LINUX_LIBS must be absolute.' >&2
    exit 1
}
STEAM_HOME=$HERE/steam-home
RUNTIME_DIR=$HERE/steam-tgcompat-run
BROKER_SOCKET=$RUNTIME_DIR/broker.sock

[[ -x $GLIBC_PREFIX/lib/ld-linux-aarch64.so.1 ]]
[[ -x $MAIN_GLIBC/lib/ld-linux-aarch64.so.1 ]]
[[ -x $TGCOMPAT_REPO/build/tgcompatd ]]
[[ -s $STEAM_LINUX_LIBS/MANIFEST.tsv ]]

# Older development runs created absolute links into a PRoot Ubuntu tree.
# Replace those aliases with the self-contained minimal runtime before any
# Steam process starts; no PRoot installation is needed after packaging.
while IFS=$'\t' read -r library _kind _source; do
  [[ -L $STEAM_DIR/$library ]] || continue
  case $(readlink -- "$STEAM_DIR/$library") in
    *proot-distro*) ln -sfn "../steam-linux-libs/$library" "$STEAM_DIR/$library" ;;
  esac
done < "$STEAM_LINUX_LIBS/MANIFEST.tsv"

install -d -m 0700 "$STEAM_HOME" "$STEAM_HOME/.steam" \
  "$STEAM_HOME/Steam" "$STEAM_HOME/steam" "$RUNTIME_DIR" "$RUNTIME_DIR/shm" \
  "$RUNTIME_DIR/dumps" "$RUNTIME_DIR/fontconfig-cache" \
  "$HERE/compatibilitytools.d" "$HERE/steam-sdkarm64" \
  "$HERE/steam-overlay-null/bin64"
ln -sfn "$STEAM_HOME/Steam" "$STEAM_HOME/.steam/steam"
ln -sfn "$CLIENT_ROOT" "$STEAM_HOME/.steam/root"
ln -sfn "$HERE/steam-sdkarm64" "$STEAM_HOME/.steam/sdkarm64"
ln -sfn "$STEAM_DIR/steamclient-patched.so" \
  "$HERE/steam-sdkarm64/steamclient.so"
ln -sfn "$MAIN_GLIBC/lib/libdl.so.2" \
  "$HERE/steam-overlay-null/bin64/gameoverlayrenderer.so"
ln -sfn "$HERE/steam-overlay-null/bin64" "$STEAM_HOME/.steam/bin64"
ln -sfn "$HERE/proton-bionic-tool" \
  "$HERE/compatibilitytools.d/Proton-Bionic-Termux"
ln -sfn "$HERE/proton-bionic-tool" \
  "$STEAM_HOME/Steam/compatibilitytools.d/Proton-Bionic-Termux"
# The updater resolves its bundled fonts relative to the patched executable.
ln -sfn ../clientui "$STEAM_DIR/clientui"
ln -sfn "$CLIENT_ROOT/clientui" "$STEAM_HOME/Steam/clientui"
# libedit is fetched from the pinned Ubuntu snapshot rather than copied from
# Valve's optional runtime.
ln -sfn "$STEAM_LINUX_LIBS/libedit.so.2" "$STEAM_DIR/libedit.so.2"

export DISPLAY=${DISPLAY:-:0}
export HOME=$STEAM_HOME
export XDG_RUNTIME_DIR=$RUNTIME_DIR
export TMPDIR=/data/data/com.termux/files/usr/tmp
export STEAM_TMP=$PREFIX/tmp
export TGCOMPAT_SOCKET=$BROKER_SOCKET
export TGCOMPAT_ROBUST_LIST=1
export BREAKPAD_DUMP_LOCATION=$RUNTIME_DIR/dumps
export LANG=C.UTF-8
export LC_ALL=C.UTF-8
export STEAM_ARM64_TMP_ROOT=$PREFIX/tmp
export STEAM_ARM64_SHM_ROOT=$RUNTIME_DIR/shm
export STEAM_ARM64_LSOF=$HERE/steam-native-lsof
export STEAM_ARM64_CLIENT_ROOT=$CLIENT_ROOT
export SSL_CERT_FILE=$MAIN_GLIBC/etc/ssl/certs/ca-certificates.crt
export SSL_CERT_DIR=$MAIN_GLIBC/etc/ssl/certs
export CURL_CA_BUNDLE=$SSL_CERT_FILE
export FONTCONFIG_FILE=$HERE/steam-fontconfig.conf
export FONTCONFIG_PATH=$HERE
export GALLIUM_DRIVER=llvmpipe
export LIBGL_ALWAYS_SOFTWARE=1
export VK_DRIVER_FILES=$STEAM_DIR/vk_swiftshader_icd.json
export VK_ICD_FILENAMES=$VK_DRIVER_FILES
unset LD_PRELOAD LD_LIBRARY_PATH GLIBC_LD_LIBRARY_PATH
unset SteamAppId SteamGameId STEAM_COMPAT_APP_ID
unset STEAM_COMPAT_DATA_PATH STEAM_COMPAT_INSTALL_PATH STEAM_COMPAT_TOOL_PATHS

# Current ARM64 client metadata advertises this network callback even when the
# native bridge does not implement it.  Let SteamUI take its built-in fallback
# instead of remaining forever in "Waiting for network".
NETWORK_UI_CHUNK=$CLIENT_ROOT/steamui/chunk~2dcc5aaf7.js
if [[ -f $NETWORK_UI_CHUNK ]]; then
  perl -0pi -e '
    s/const ([A-Za-z_][A-Za-z0-9_]*)=\(0,([A-Za-z_][A-Za-z0-9_]*)\.Dp\)\("System\.Network\.RegisterForDeviceChanges"\);/const $1=(0,$2.Dp)("System.Network.RegisterForDeviceChanges")\&\&"function"==typeof SteamClient.System.Network.RegisterForDeviceChanges;/;
    s/\(0,([A-Za-z_][A-Za-z0-9_]*)\.Dp\)\("System\.Network\.GetProxyInfo"\)\&\&SteamClient\.System\.Network\.GetProxyInfo\(\)/(0,$1.Dp)("System.Network.GetProxyInfo")\&\&"function"==typeof SteamClient.System.Network.GetProxyInfo\&\&SteamClient.System.Network.GetProxyInfo()/;
    s/\(0,([A-Za-z_][A-Za-z0-9_]*)\.Dp\)\("System\.Network\.RegisterForConnectivityTestChanges"\)\&\&SteamClient\.System\.Network\.RegisterForConnectivityTestChanges\(/(0,$1.Dp)("System.Network.RegisterForConnectivityTestChanges")\&\&"function"==typeof SteamClient.System.Network.RegisterForConnectivityTestChanges\&\&SteamClient.System.Network.RegisterForConnectivityTestChanges(/;
  ' "$NETWORK_UI_CHUNK"
fi

if ! bash "$TGCOMPAT_REPO/scripts/tgcompat-session.sh" status >/dev/null 2>&1; then
  bash "$TGCOMPAT_REPO/scripts/tgcompat-session.sh" start
fi

STEAM_BIN=$STEAM_DIR/steam-patched
STEAMUI_BIN=$STEAM_DIR/steamui-patched.so
STEAMCLIENT_BIN=$STEAM_DIR/steamclient-patched.so
CHROMEHTML_BIN=$STEAM_DIR/chromehtml-patched.so

cp -p "$STEAM_DIR/steam" "$STEAM_BIN"
cp -p "$STEAM_DIR/steamui.so" "$STEAMUI_BIN"
cp -p "$STEAM_DIR/steamclient.so" "$STEAMCLIENT_BIN"
cp -p "$STEAM_DIR/chromehtml.so" "$CHROMEHTML_BIN"
cp -p "$STEAM_DIR/steamsysinfo" "$STEAM_DIR/steamsysinfP"
cp -p "$HERE/steamwebhelper-patched.sh" "$STEAM_DIR/steamwebhelpeP.sh"
cp -p "$STEAM_DIR/steamwebhelper" "$STEAM_DIR/steamwebhelper-patched"
cp -p "$TGCOMPAT_REPO/build/libtgcompat-robust.so" "$STEAM_DIR/libtgcompat-robust.so"
cp -p "$TGCOMPAT_REPO/build/libtgcompat-exec.so" "$STEAM_DIR/libtgcompat-exec.so"
cp -p "$HERE/steam-legacy-tmp-shim.so" "$STEAM_DIR/steam-tmp-shim.so"
cp -p "$HERE/steam-proc-self-shim.so" "$STEAM_DIR/steam-proc-self-shim.so"
cp -p "$HERE/steam-unix-socket-shim.so" "$STEAM_DIR/steam-unix-socket-shim.so"
[[ -f $STEAM_DIR/libedit.so.2 ]]
# Keep only the /tmp redirect now that SysV semaphores live inside patched libc.
cp -p "$STEAM_DIR/steam-tmp-shim.so" "$STEAM_DIR/steam-path-shim.so"

RPATH="$STEAM_LINUX_LIBS:\$ORIGIN:\$ORIGIN/libs:$MAIN_GLIBC/lib:$MAIN_GLIBC/lib/pulseaudio"
CEF_RPATH="$GLIBC_PREFIX/lib:$STEAM_LINUX_LIBS:\$ORIGIN:\$ORIGIN/libs:$MAIN_GLIBC/lib:$MAIN_GLIBC/lib/pulseaudio"
LIBRARY_PATH="$STEAM_LINUX_LIBS:$STEAM_DIR:$STEAM_DIR/libs:$MAIN_GLIBC/lib:$MAIN_GLIBC/lib/pulseaudio"
CHILD_PRELOAD="$STEAM_DIR/steam-path-shim.so:$STEAM_DIR/libtgcompat-robust.so:$STEAM_DIR/libtgcompat-exec.so"

export TGCOMPAT_LD_SO="$MAIN_GLIBC/lib/ld-linux-aarch64.so.1"
export TGCOMPAT_LIBRARY_PATH="$LIBRARY_PATH"
export TGCOMPAT_EXEC_LD_PRELOAD="$CHILD_PRELOAD"
export TGCOMPAT_EXEC_SHELL="$MAIN_GLIBC/bin/sh"

cp -p "$MAIN_GLIBC/bin/bash" "$HERE/steam-sdkarm64/bash"
patchelf --set-interpreter "$MAIN_GLIBC/lib/ld-linux-aarch64.so.1" \
  --force-rpath --set-rpath "$STEAM_DIR:$RPATH" \
  "$HERE/steam-sdkarm64/bash"
sed -i "1c\\#!$HERE/steam-sdkarm64/bash" \
  "$HERE/steam-sdkarm64/steam-launch-wrapper"
chmod 700 "$HERE/steam-sdkarm64/bash" \
  "$HERE/steam-sdkarm64/steam-launch-wrapper"

# These helpers are invoked by a shell, so Android's kernel must be able to
# resolve their interpreter before the normal exec-boundary shim can help.
for helper in \
  "$STEAM_DIR/reaper" \
  "$STEAM_DIR/gldriverquery" \
  "$STEAM_DIR/vulkandriverquery"
do
  patchelf --set-interpreter "$MAIN_GLIBC/lib/ld-linux-aarch64.so.1" \
    --force-rpath --set-rpath "$RPATH" "$helper"
done
if ! patchelf --print-needed "$STEAM_DIR/reaper" | grep -qx libtgcompat-exec.so; then
  patchelf --add-needed libtgcompat-exec.so "$STEAM_DIR/reaper"
fi
patchelf --force-rpath --set-rpath "$RPATH" "$STEAMUI_BIN"
patchelf --add-needed steam-helper-shim.so "$STEAMUI_BIN"
patchelf --add-needed steam-module-shim.so "$STEAMUI_BIN"
patchelf --add-needed steam-dlmopen-shim.so "$STEAMUI_BIN"
sed -i 's/steamsysinfo/steamsysinfP/g' "$STEAMUI_BIN"
sed -i 's/steamwebhelper\.sh/steamwebhelpeP.sh/g' "$STEAMCLIENT_BIN" "$CHROMEHTML_BIN"
patchelf --force-rpath --set-rpath "$RPATH" "$STEAMCLIENT_BIN" "$CHROMEHTML_BIN"

patchelf --set-interpreter "$MAIN_GLIBC/lib/ld-linux-aarch64.so.1" \
  --force-rpath --set-rpath "$RPATH" "$STEAM_DIR/steamsysinfP"
patchelf --add-needed steam-tmp-shim.so "$STEAM_DIR/steamsysinfP"
chmod 700 "$STEAM_DIR/steamwebhelpeP.sh"

patchelf --set-interpreter "$GLIBC_PREFIX/lib/ld-linux-aarch64.so.1" \
  --force-rpath --set-rpath "$CEF_RPATH" "$STEAM_DIR/steamwebhelper-patched"
patchelf --add-needed steam-tmp-shim.so "$STEAM_DIR/steamwebhelper-patched"
patchelf --add-needed steam-unix-socket-shim.so "$STEAM_DIR/steamwebhelper-patched"
patchelf --add-needed libtgcompat-robust.so "$STEAM_DIR/steamwebhelper-patched"

patchelf --set-interpreter "$MAIN_GLIBC/lib/ld-linux-aarch64.so.1" \
  --force-rpath --set-rpath "$RPATH" \
  --add-needed steam-path-shim.so \
  --add-needed steam-helper-shim.so \
  --add-needed steam-proc-self-shim.so \
  --add-needed steam-unix-socket-shim.so \
  --add-needed steam-sysv-shim.so \
  --add-needed steam-cert-shim.so \
  --add-needed steam-dlmopen-shim.so \
  --add-needed steam-open-shim.so \
  --add-needed libtgcompat-exec.so \
  --add-needed libtgcompat-robust.so "$STEAM_BIN"

export STEAMUI_PATCHED=$STEAMUI_BIN
export STEAMCLIENT_PATCHED=$STEAMCLIENT_BIN
export CHROMEHTML_PATCHED=$CHROMEHTML_BIN
export TGCOMPAT_PROC_SELF_EXE=$STEAM_BIN
cd "$CLIENT_ROOT"
exec "$MAIN_GLIBC/lib/ld-linux-aarch64.so.1" --inhibit-cache \
  --library-path "$LIBRARY_PATH" --argv0 "$STEAM_BIN" "$STEAM_BIN" \
  -no-cef-sandbox -cef-disable-gpu -chromeosnopreallocate -noverifyfiles \
  -no-child-update-ui "$@"
