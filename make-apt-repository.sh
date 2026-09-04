#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail
umask 022

repository_epoch=${APT_REPOSITORY_EPOCH:-$(date +%s)}
[[ $repository_epoch =~ ^[0-9]+$ ]] || {
    printf '%s\n' 'APT_REPOSITORY_EPOCH must be numeric.' >&2
    exit 1
}

die() {
    printf 'make-apt-repository: %s\n' "$*" >&2
    exit 1
}

(($# >= 2)) || die 'usage: ./make-apt-repository.sh /absolute/package.deb [...] /absolute/new-directory'
last_index=$#
output=${!last_index}
debs=("${@:1:last_index-1}")
[[ $output == /* && ! -e $output && ! -L $output ]] ||
    die 'output must be an absolute new directory'
for tool in dpkg-deb sha256sum stat gzip install find touch date; do
    command -v "$tool" >/dev/null 2>&1 || die "missing tool: $tool"
done

binary_dir=$output/dists/stable/main/binary-aarch64
install -d -m 0755 "$binary_dir"
packages_file=$binary_dir/Packages
: >"$packages_file"

for deb in "${debs[@]}"; do
    [[ $deb == /* && -f $deb && ! -L $deb ]] || die "invalid package: $deb"
    package=$(dpkg-deb -f "$deb" Package)
    version=$(dpkg-deb -f "$deb" Version)
    architecture=$(dpkg-deb -f "$deb" Architecture)
    [[ $architecture == aarch64 ]] || die "unexpected architecture for $deb"
    case $package in
        steam-arm64|hangover-wine-steamswap) ;;
        *) die "unexpected package identity: $package" ;;
    esac
    filename=pool/main/${package:0:1}/$package/${package}_${version}_aarch64.deb
    install -d -m 0755 "$output/${filename%/*}"
    install -m 0644 "$deb" "$output/$filename"
    {
        dpkg-deb -f "$deb"
        printf 'Filename: %s\n' "$filename"
        printf 'Size: %s\n' "$(stat -c %s "$deb")"
        printf 'SHA256: %s\n\n' "$(sha256sum "$deb" | cut -d' ' -f1)"
    } >>"$packages_file"
    printf 'Package: %s %s %s\n' "$package" "$version" "$architecture"
done

gzip -n -9 -c "$packages_file" >"$binary_dir/Packages.gz"
release=$output/dists/stable/Release
{
    printf '%s\n' 'Origin: Steam ARM64 for Termux'
    printf '%s\n' 'Label: Steam ARM64 for Termux'
    printf '%s\n' 'Suite: stable'
    printf '%s\n' 'Codename: stable'
    printf 'Date: %s\n' "$(LC_ALL=C date -Ru -d "@$repository_epoch")"
    printf '%s\n' 'Architectures: aarch64'
    printf '%s\n' 'Components: main'
    printf '%s\n' 'Description: Native ARM64 Steam packages for Termux'
    printf '%s\n' 'SHA256:'
    for relative in main/binary-aarch64/Packages main/binary-aarch64/Packages.gz; do
        file=$output/dists/stable/$relative
        printf ' %s %16s %s\n' "$(sha256sum "$file" | cut -d' ' -f1)" \
            "$(stat -c %s "$file")" "$relative"
    done
} >"$release"

cat >"$output/index.html" <<'EOF'
<!doctype html>
<meta charset="utf-8">
<title>Steam ARM64 for Termux repository</title>
<h1>Steam ARM64 for Termux</h1>
<p>Configure this APT repository using the instructions in the
<a href="https://github.com/moio9/steam-arm64-termux">project repository</a>.</p>
EOF
: >"$output/.nojekyll"
find "$output" -exec touch -h -d "@$repository_epoch" {} +
printf 'Created APT repository: %s\n' "$output"
