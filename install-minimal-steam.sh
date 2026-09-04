#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

root=$(cd -- "${BASH_SOURCE[0]%/*}" && pwd -P)
check_only=0
skip_packages=0
while (($#)); do
    case $1 in
        --check) check_only=1 ;;
        --skip-packages) skip_packages=1 ;;
        -h|--help)
            printf 'usage: %s [--check] [--skip-packages]\n' "$0"
            printf '%s\n' '  --check          validate the payload and installed prerequisites only'
            printf '%s\n' '  --skip-packages  build/install the custom bridge without running pkg'
            exit 0
            ;;
        *)
            printf 'unknown option: %s\n' "$1" >&2
            exit 2
            ;;
    esac
    shift
done

termux_prefix=/data/data/com.termux/files/usr
if [[ ${PREFIX:-} != "$termux_prefix" ]]; then
    printf 'This package requires the standard Termux PREFIX: %s\n' "$termux_prefix" >&2
    exit 1
fi
if [[ $(uname -m) != aarch64 ]]; then
    printf 'This package requires an aarch64 Android device (found %s).\n' "$(uname -m)" >&2
    exit 1
fi

client_root=${STEAM_ARM64_CLIENT_ROOT:-$root}
[[ $client_root == /* && -d $client_root ]] || {
    printf 'Invalid Steam client root: %s\n' "$client_root" >&2
    exit 1
}
client_root=$(cd -- "$client_root" && pwd -P)

required_files=(
    "$root/run-steam.sh"
    "$root/run-steam-tgcompat.sh"
    "$root/steamwebhelper-patched.sh"
    "$root/steam-native-lsof"
    "$root/steam-fontconfig.conf"
    "$root/configure-steam-default-compat.py"
    "$client_root/steamrtarm64/steam"
    "$client_root/steamrtarm64/steamclient.so"
    "$client_root/steamrtarm64/steamui.so"
    "$client_root/steamrtarm64/chromehtml.so"
    "$client_root/steamrtarm64/steamwebhelper"
    "$client_root/steamrtarm64/steamsysinfo"
    "$client_root/steamrtarm64/reaper"
    "$client_root/steamrtarm64/gldriverquery"
    "$client_root/steamrtarm64/vulkandriverquery"
    "$root/steam-linux-libs/MANIFEST.tsv"
    "$root/ubuntu-runtime-lock.json"
    "$root/fetch-ubuntu-runtime.py"
    "$root/steam-linux-official-packages.txt"
    "$root/tgcompat-glibc/current/lib/ld-linux-aarch64.so.1"
    "$root/reference-termux-glibc-compat/build/tgcompatd"
    "$root/reference-termux-glibc-compat/build/libtgcompat-exec.so"
    "$root/reference-termux-glibc-compat/build/libtgcompat-robust.so"
    "$root/reference-termux-glibc-compat/scripts/tgcompat-session.sh"
    "$root/steam-bridge/lsteambridge-server"
    "$root/steam-bridge/lsteambridge-client"
    "$root/steam-bridge/proton-functions.sh"
    "$root/steam-bridge/run-server.sh"
    "$root/proton-bionic-tool/proton"
    "$root/proton-bionic-tool/steam-runtime-steam-remote"
    "$root/proton-bionic-tool/compatibilitytool.vdf"
    "$root/proton-bionic-fex-tool/proton"
    "$root/proton-bionic-fex-tool/compatibilitytool.vdf"
    "$root/proton-bionic-fex-tool/toolmanifest.vdf"
    "$root/proton-bionic-tool/vendor/dxvk-1.10.3/x32/d3d9.dll"
    "$root/proton-bionic-tool/vendor/dxvk-1.10.3/x64/dxgi.dll"
    "$root/proton-bionic-tool/vendor/vkd3d-proton-3.0.1/x86/d3d12.dll"
    "$root/proton-bionic-tool/vendor/vkd3d-proton-3.0.1/x64/d3d12.dll"
    "$root/public-source/lsteamclient/prepare-proton-source.sh"
    "$root/public-source/lsteamclient/build-bionic-lsteamclient.sh"
    "$root/public-source/lsteamclient/source.lock"
    "$root/public-source/lsteamclient/patches/0001-termux-arm64-bridge.patch"
    "$root/public-source/hangover-steamswap/source.lock"
    "$root/public-source/hangover-steamswap/patches/0001-steamclient-swap-arm64.patch"
    "$root/steam-sdkarm64/steam-launch-wrapper"
    "$root/steam-legacy-tmp-shim.so"
    "$root/steam-proc-self-shim.so"
    "$root/steam-unix-socket-shim.so"
    "$client_root/steamrtarm64/steam-sysv-shim.so"
)
for path in "${required_files[@]}"; do
    [[ -e $path ]] || {
        printf 'missing package component: %s\n' "$path" >&2
        exit 1
    }
done
while IFS=$'\t' read -r library _kind _source; do
    [[ -n $library ]] || continue
    [[ -e $root/steam-linux-libs/$library ||
       -L $root/steam-linux-libs/$library ]] || {
        printf 'missing library from MANIFEST.tsv: %s\n' "$library" >&2
        exit 1
    }
done < "$root/steam-linux-libs/MANIFEST.tsv"

if (( ! check_only && ! skip_packages )); then
    command -v pkg >/dev/null || {
        printf '%s\n' 'pkg is unavailable; run this installer from Termux.' >&2
        exit 1
    }
    pkg install -y x11-repo tur-repo glibc-repo

    packages=(
        bash coreutils diffutils findutils patchelf perl
        curl python zstd git clang binutils file
        glibc-runner bash-glibc ca-certificates-glibc
    )
    while IFS= read -r package; do
        [[ -n $package ]] && packages+=("$package")
    done < "$root/steam-linux-official-packages.txt"
    pkg install -y "${packages[@]}"
fi

required_hangover_packages=(
    hangover-wine hangover-wine-steamswap hangover-wowbox64
    hangover-libarm64ecfex hangover-libwow64fex
)
for package_name in "${required_hangover_packages[@]}"; do
    package_version=$(dpkg-query -W -f='${Version}' "$package_name" 2>/dev/null || true)
    case $package_name:$package_version in
        hangover-wine:11.16|hangover-wine-steamswap:11.16-1|\
        hangover-wowbox64:11.16|hangover-libarm64ecfex:11.16|\
        hangover-libwow64fex:11.16) ;;
        *)
            printf 'Required Hangover package version is missing: %s\n' "$package_name" >&2
            printf '%s\n' 'Install the locked Hangover 11.16 stack before running Steam ARM64.' >&2
            exit 1
            ;;
    esac
done

required_commands=(bash cmp patchelf perl sha256sum)
if (( ! check_only )); then
    required_commands+=(git curl clang++ file readelf mktemp)
fi
for command_name in "${required_commands[@]}"; do
    command -v "$command_name" >/dev/null || {
        printf 'Required command is unavailable: %s\n' "$command_name" >&2
        exit 1
    }
done

wine_root=$termux_prefix/opt/hangover-wine
wine_launcher=/data/data/com.termux/files/home/bin/hangover-wine
vulkan_icd=$termux_prefix/share/vulkan/icd.d/freedreno_icd.aarch64.json
lsteam_target=$wine_root/lib/wine/aarch64-unix/lsteamclient.so
lsteam_pe_target=$wine_root/lib/wine/i386-windows/lsteamclient.dll
lsteam_public=$root/public-source/lsteamclient
lsteam_patch=$lsteam_public/patches/0001-termux-arm64-bridge.patch
# shellcheck source=/dev/null
. "$lsteam_public/source.lock"
[[ ${PATCH_SHA256:-} =~ ^[0-9a-f]{64}$ ]] || {
    printf '%s\n' 'Invalid lsteamclient patch checksum in source.lock.' >&2
    exit 1
}
lsteam_work_root=${LSTEAM_WORK_ROOT:-$lsteam_public/work/${PATCH_SHA256:0:16}}
lsteam_checkout=${PROTON_CHECKOUT:-$lsteam_work_root/proton}
lsteam_build_dir=${LSTEAM_BUILD_DIR:-$lsteam_work_root/build}
lsteam_artifact=$lsteam_build_dir/lsteamclient.so
lsteam_pe_artifact=$lsteam_build_dir/lsteamclient.dll
export LSTEAM_WORK_ROOT=$lsteam_work_root
export PROTON_CHECKOUT=$lsteam_checkout
export LSTEAM_BUILD_DIR=$lsteam_build_dir
printf '%s  %s\n' "$PATCH_SHA256" "$lsteam_patch" |
    sha256sum -c - >/dev/null || {
        printf '%s\n' 'The public lsteamclient patch does not match source.lock.' >&2
        exit 1
    }

hangover_public=$root/public-source/hangover-steamswap
hangover_lock=$hangover_public/source.lock
hangover_patch=$hangover_public/patches/0001-steamclient-swap-arm64.patch
(
    # Keep the Hangover lock variables separate from the lsteamclient lock.
    # shellcheck source=/dev/null
    . "$hangover_lock"
    printf '%s  %s\n%s  %s\n%s  %s\n%s  %s\n%s  %s\n' \
        "$PATCH_SHA256" "$hangover_patch" \
        "$WINESERVER_SHA256" "$wine_root/bin/wineserver" \
        "$NTDLL_SO_SHA256" "$wine_root/lib/wine/aarch64-unix/ntdll.so" \
        "$NTDLL_I386_SHA256" "$wine_root/lib/wine/i386-windows/ntdll.dll" \
        "$NTDLL_AARCH64_SHA256" "$wine_root/lib/wine/aarch64-windows/ntdll.dll" |
        sha256sum -c - >/dev/null
) || {
    printf '%s\n' 'Hangover 11.16 is missing the Steam DRM loader overlay.' >&2
    printf '%s\n' 'Install hangover-wine-steamswap=11.16-1 from the project APT repository.' >&2
    exit 1
}

[[ -x $wine_launcher && -x $wine_root/bin/wineserver &&
   -f $wine_root/lib/wine/i386-windows/lsteamclient.dll &&
   -f $wine_root/lib/wine/i386-windows/steam.exe &&
   -f $wine_root/lib/wine/aarch64-windows/wowbox64.dll &&
   -f $wine_root/lib/wine/aarch64-windows/libarm64ecfex.dll &&
   -f $wine_root/lib/wine/aarch64-windows/libwow64fex.dll ]] || {
    printf '%s\n' 'Hangover Wine is not installed correctly.' >&2
    exit 1
}
[[ -f $vulkan_icd ]] || {
    printf 'Vulkan driver is missing: %s\n' "$vulkan_icd" >&2
    printf '%s\n' 'Install your normal Termux Turnip driver; this package does not replace it.' >&2
    exit 1
}

if (( check_only )) && { [[ ! -f $lsteam_artifact ]] || [[ -L $lsteam_artifact ]] ||
                              [[ ! -f $lsteam_pe_artifact ]] || [[ -L $lsteam_pe_artifact ]]; }; then
    printf 'The locally built lsteamclient artifact is missing: %s\n' \
        "$lsteam_artifact" >&2
    printf '%s\n' 'Run ./install-minimal-steam.sh to fetch its pinned source and build it locally.' >&2
    exit 1
fi
if (( check_only )) && ! cmp -s -- "$lsteam_artifact" "$lsteam_target"; then
    printf '%s\n' 'The installed Hangover lsteamclient bridge does not match this package.' >&2
    printf '%s\n' 'Run ./install-minimal-steam.sh to install it.' >&2
    exit 1
fi
if (( check_only )) && ! cmp -s -- "$lsteam_pe_artifact" "$lsteam_pe_target"; then
    printf '%s\n' 'The installed Hangover lsteamclient PE module does not match this package.' >&2
    printf '%s\n' 'Run ./install-minimal-steam.sh to install it.' >&2
    exit 1
fi

if (( ! check_only )); then
    # lsteamclient is built locally from the pinned public patch workflow.  The
    # public bootstrap deliberately carries neither a prebuilt lsteamclient.so
    # nor Valve's Proton/Steamworks source trees.
    if [[ -d $lsteam_checkout/.git ]]; then
        # A retained checkout is accepted only when the current package patch
        # is present exactly.  This prevents an upgraded package from silently
        # rebuilding an older checkout.
        if ! git -C "$lsteam_checkout" apply --reverse --check "$lsteam_patch"; then
            printf 'Existing Proton checkout does not match the current public patch: %s\n' \
                "$lsteam_checkout" >&2
            printf '%s\n' 'Choose a fresh LSTEAM_WORK_ROOT (or remove this generated work directory) and retry.' >&2
            exit 1
        fi
    elif [[ -e $lsteam_checkout ]]; then
        printf 'Incomplete Proton checkout blocks the local build: %s\n' \
            "$lsteam_checkout" >&2
        printf '%s\n' 'Choose a fresh LSTEAM_WORK_ROOT (or remove this generated work directory) and retry.' >&2
        exit 1
    else
        "$lsteam_public/prepare-proton-source.sh"
    fi
    "$lsteam_public/build-bionic-lsteamclient.sh"
    [[ -f $lsteam_artifact && ! -L $lsteam_artifact &&
       -f $lsteam_pe_artifact && ! -L $lsteam_pe_artifact ]] || {
        printf 'Local lsteamclient build did not produce both artifacts:\n%s\n%s\n' \
            "$lsteam_artifact" "$lsteam_pe_artifact" >&2
        exit 1
    }

    install -d -- "${lsteam_target%/*}"
    if [[ -f $lsteam_target && ! -e $lsteam_target.before-steam-arm64 ]]; then
        cp -p -- "$lsteam_target" "$lsteam_target.before-steam-arm64"
    fi
    install -m 0755 -- "$lsteam_artifact" "$lsteam_target"
    if [[ -f $lsteam_pe_target && ! -e $lsteam_pe_target.before-steam-arm64 ]]; then
        cp -p -- "$lsteam_pe_target" "$lsteam_pe_target.before-steam-arm64"
    fi
    install -m 0644 -- "$lsteam_pe_artifact" "$lsteam_pe_target"
    chmod 0700 "$root/run-steam.sh" "$root/run-steam-tgcompat.sh" \
        "$root/steamwebhelper-patched.sh" "$root/proton-bionic-tool/proton" \
        "$root/proton-bionic-tool/steam-runtime-steam-remote" \
        "$root/proton-bionic-fex-tool/proton" \
        "$root/steam-bridge/run-server.sh"
fi

printf '%s\n' 'Steam ARM64 minimal package: OK'
printf 'Valve client root: %s\n' "$client_root"
printf 'locally built lsteamclient: %s\n' "$(sha256sum "$lsteam_artifact" | cut -d' ' -f1)"
if [[ -f $lsteam_target ]]; then
    printf 'installed bridge: %s\n' "$(sha256sum "$lsteam_target" | cut -d' ' -f1)"
fi
printf '%s\n' 'Run ./run-steam.sh after Termux:X11 and your audio server are started.'
