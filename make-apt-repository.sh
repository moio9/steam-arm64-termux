#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail
umask 022

deb=${1:-}
output=${2:-}
source_epoch=${SOURCE_DATE_EPOCH:-1788134400}

die() {
    printf 'make-apt-repository: %s\n' "$*" >&2
    exit 1
}

[[ $deb == /* && -f $deb && ! -L $deb && $output == /* ]] ||
    die 'usage: ./make-apt-repository.sh /absolute/package.deb /absolute/new-directory'
[[ ! -e $output && ! -L $output ]] || die "refusing to overwrite: $output"
for tool in dpkg-deb sha256sum stat gzip install find touch; do
    command -v "$tool" >/dev/null 2>&1 || die "missing tool: $tool"
done

package=$(dpkg-deb -f "$deb" Package)
version=$(dpkg-deb -f "$deb" Version)
architecture=$(dpkg-deb -f "$deb" Architecture)
[[ $package == steam-arm64 && $architecture == aarch64 ]] ||
    die 'unexpected package identity'
filename=pool/main/s/steam-arm64/steam-arm64_${version}_aarch64.deb
binary_dir=$output/dists/stable/main/binary-aarch64
install -d -m 0755 -- "$output/${filename%/*}" "$binary_dir"
install -m 0644 -- "$deb" "$output/$filename"

{
    dpkg-deb -f "$deb"
    printf 'Filename: %s\n' "$filename"
    printf 'Size: %s\n' "$(stat -c %s "$deb")"
    printf 'SHA256: %s\n\n' "$(sha256sum "$deb" | cut -d' ' -f1)"
} > "$binary_dir/Packages"
gzip -n -9 -c "$binary_dir/Packages" > "$binary_dir/Packages.gz"

release=$output/dists/stable/Release
{
    printf '%s\n' 'Origin: Steam ARM64 for Termux'
    printf '%s\n' 'Label: Steam ARM64 for Termux'
    printf '%s\n' 'Suite: stable'
    printf '%s\n' 'Codename: stable'
    printf '%s\n' 'Architectures: aarch64'
    printf '%s\n' 'Components: main'
    printf '%s\n' 'Description: Native ARM64 Steam packages for Termux'
    printf '%s\n' 'SHA256:'
    for relative in main/binary-aarch64/Packages main/binary-aarch64/Packages.gz; do
        file=$output/dists/stable/$relative
        printf ' %s %16s %s\n' "$(sha256sum "$file" | cut -d' ' -f1)" \
            "$(stat -c %s "$file")" "$relative"
    done
} > "$release"

cat > "$output/index.html" <<'EOF'
<!doctype html>
<meta charset="utf-8">
<title>Steam ARM64 for Termux repository</title>
<h1>Steam ARM64 for Termux</h1>
<p>Configure this APT repository using the instructions in the
<a href="https://github.com/moio9/steam-arm64-termux">project repository</a>.</p>
EOF
: > "$output/.nojekyll"
find "$output" -exec touch -h -d "@$source_epoch" {} +
printf 'Created APT repository: %s\nPackage: %s %s %s\n' \
    "$output" "$package" "$version" "$architecture"
