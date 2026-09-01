#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

root=$(cd -- "${BASH_SOURCE[0]%/*}" && pwd -P)
vendor=$root/proton-bionic-tool/vendor
cache=${STEAM_ARM64_DOWNLOAD_CACHE:-$root/download-cache}/upstream
mkdir -p -- "$cache" "$vendor"

dxvk_dll=$vendor/dxvk-1.10.3/x32/d3d9.dll
vkd3d_dll=$vendor/vkd3d-proton-3.0.1/x86/d3d12.dll
if [[ -f $dxvk_dll && ! -L $dxvk_dll &&
      -f $vkd3d_dll && ! -L $vkd3d_dll &&
      $(sha256sum "$dxvk_dll" | cut -d' ' -f1) == \
          b6cfa2cd62af73b80d461085d126004b0e22dd3944c9246c58e3a68e747b56b6 &&
      $(sha256sum "$vkd3d_dll" | cut -d' ' -f1) == \
          098f4f07182773b7420fe7e2558ec537fdac300489cfde5f52e1df5678ac5ef3 ]]; then
    printf '%s\n' 'Locked DXVK and vkd3d-proton components: OK'
    exit 0
fi

fetch_locked() {
    local url=$1 expected=$2 destination=$3 partial
    if [[ -f $destination && ! -L $destination ]] &&
       [[ $(sha256sum "$destination" | cut -d' ' -f1) == "$expected" ]]; then
        return
    fi
    partial=$destination.partial
    rm -f -- "$partial"
    curl --fail --location --proto '=https' --tlsv1.2 \
        --output "$partial" "$url"
    [[ $(sha256sum "$partial" | cut -d' ' -f1) == "$expected" ]] || {
        rm -f -- "$partial"
        printf 'Checksum mismatch for %s\n' "$url" >&2
        exit 1
    }
    mv -- "$partial" "$destination"
}

dxvk_archive=$cache/dxvk-1.10.3.tar.gz
vkd3d_archive=$cache/vkd3d-proton-3.0.1.tar.zst
fetch_locked \
    https://github.com/doitsujin/dxvk/releases/download/v1.10.3/dxvk-1.10.3.tar.gz \
    8d1a3c912761b450c879f98478ae64f6f6639e40ce6848170a0f6b8596fd53c6 \
    "$dxvk_archive"
fetch_locked \
    https://github.com/HansKristian-Work/vkd3d-proton/releases/download/v3.0.1/vkd3d-proton-3.0.1.tar.zst \
    3cf2315522af5e43605ef6d3c41dad91387040bf97199934f3f7ab76caaa2f0c \
    "$vkd3d_archive"

if [[ ! -f $vendor/dxvk-1.10.3/x32/d3d9.dll ]]; then
    [[ ! -e $vendor/dxvk-1.10.3 && ! -L $vendor/dxvk-1.10.3 ]] || {
        printf '%s\n' 'Refusing an incomplete existing DXVK directory.' >&2
        exit 1
    }
    tar -xzf "$dxvk_archive" -C "$vendor"
fi
if [[ ! -f $vendor/vkd3d-proton-3.0.1/x86/d3d12.dll ]]; then
    [[ ! -e $vendor/vkd3d-proton-3.0.1 &&
       ! -L $vendor/vkd3d-proton-3.0.1 ]] || {
        printf '%s\n' 'Refusing an incomplete existing vkd3d-proton directory.' >&2
        exit 1
    }
    tar --zstd -xf "$vkd3d_archive" -C "$vendor"
fi

[[ $(sha256sum "$vendor/dxvk-1.10.3/x32/d3d9.dll" | cut -d' ' -f1) == \
   b6cfa2cd62af73b80d461085d126004b0e22dd3944c9246c58e3a68e747b56b6 ]]
[[ $(sha256sum "$vendor/vkd3d-proton-3.0.1/x86/d3d12.dll" | cut -d' ' -f1) == \
   098f4f07182773b7420fe7e2558ec537fdac300489cfde5f52e1df5678ac5ef3 ]]
printf '%s\n' 'Locked DXVK and vkd3d-proton components: OK'
