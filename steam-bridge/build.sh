#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

root=$(cd -- "${BASH_SOURCE[0]%/*}" && pwd -P)
glibc_bin=/data/data/com.termux/files/usr/glibc/bin
sdk=${LSTEAM_SDK_DIR:-$root/../proton-lsteamclient-src/lsteamclient/steamworks_sdk_154}

glibc-runner "$glibc_bin/aarch64-linux-gnu-g++" -O2 -Wall -Wextra -std=gnu++17 \
    -I"$root" -I"$sdk" "$root/server.cpp" -ldl \
    -Wl,-rpath,/data/data/com.termux/files/usr/glibc/lib \
    -o "$root/lsteambridge-server"

clang -O2 -Wall -Wextra -std=c11 -I"$root" "$root/client.c" \
    -o "$root/lsteambridge-client"

file "$root/lsteambridge-server" "$root/lsteambridge-client"
