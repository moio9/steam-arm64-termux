#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

prefix=/data/data/com.termux/files/usr
source_file=$prefix/etc/apt/sources.list.d/steam-arm64-termux.list
source_line='deb [arch=aarch64 trusted=yes] https://raw.githubusercontent.com/moio9/steam-arm64-termux/gh-pages stable main'
legacy_source_line='deb [arch=aarch64 trusted=yes] https://moio9.github.io/steam-arm64-termux stable main'

[[ ${PREFIX:-} == "$prefix" ]] || {
    printf '%s\n' 'Run this repository setup inside standard Termux.' >&2
    exit 1
}
[[ $(uname -m) == aarch64 ]] || {
    printf '%s\n' 'The Steam ARM64 repository requires an aarch64 device.' >&2
    exit 1
}

if [[ -e $source_file || -L $source_file ]]; then
    [[ -f $source_file && ! -L $source_file ]] || {
        printf 'Refusing to replace an unexpected repository file: %s\n' "$source_file" >&2
        exit 1
    }
    case $(<"$source_file") in
        "$source_line") ;;
        "$legacy_source_line") printf '%s\n' "$source_line" > "$source_file" ;;
        *)
            printf 'Refusing to replace an unexpected repository file: %s\n' "$source_file" >&2
            exit 1
            ;;
    esac
else
    printf '%s\n' "$source_line" > "$source_file"
fi

pkg update
printf '%s\n' 'Steam ARM64 repository configured.'
printf '%s\n' 'Install with: pkg install steam-arm64'
