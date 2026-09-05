#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail
umask 022

root=$(cd -- "${BASH_SOURCE[0]%/*}" && pwd -P)
source_epoch=${SOURCE_DATE_EPOCH:-1788134400}
steamclienttermux_revision=dee92b8432424e873011542e6926b12651bc823c
tgcompat_revision=8d63206ac60eb1106cb5303f1ac75f5a3bd60a62
stage=
keep_stage=0

usage() {
    printf 'usage: %s --check|/absolute/new-directory\n' "$0" >&2
    exit 2
}

die() {
    printf 'make-public-source-tree: %s\n' "$*" >&2
    exit 1
}

cleanup() {
    if [[ $keep_stage == 0 && -n $stage && -d $stage ]]; then
        rm -rf -- "$stage"
    fi
}
trap cleanup EXIT HUP INT TERM

case ${1:-} in
    --check)
        [[ $# -eq 1 ]] || usage
        mode=check
        destination=
        temp_base=${TMPDIR:-/tmp}
        [[ $temp_base == /* && -d $temp_base ]] ||
            die "TMPDIR must name an existing absolute directory: $temp_base"
        stage=$(mktemp -d "$temp_base/steam-arm64-source-check.XXXXXX")
        ;;
    /*)
        [[ $# -eq 1 ]] || usage
        mode=build
        destination=$1
        destination_base=${destination##*/}
        destination_parent=${destination%/*}
        case $destination_base in
            ''|.|..) die "unsafe destination: $destination" ;;
        esac
        case $destination in
            *'/../'*|*'/./'*|*/..|*/.) die "destination must not contain dot components: $destination" ;;
        esac
        [[ -d $destination_parent ]] ||
            die "destination parent does not exist: $destination_parent"
        destination_parent=$(cd -- "$destination_parent" && pwd -P)
        destination=$destination_parent/$destination_base
        [[ ! -e $destination && ! -L $destination ]] ||
            die "refusing to overwrite destination: $destination"
        stage=$(mktemp -d "$destination_parent/.steam-arm64-source-stage.XXXXXX")
        ;;
    *)
        usage
        ;;
esac

for tool in git install find sort sha256sum python3 tar touch mktemp xargs chmod mv rm wc; do
    command -v -- "$tool" >/dev/null 2>&1 || die "missing tool: $tool"
done
[[ $source_epoch =~ ^[0-9]+$ ]] || die 'SOURCE_DATE_EPOCH must be an integer'

# Every copied workspace file is named here. A missing required entry is an
# error; adding a file anywhere in the development tree cannot add it to the
# public repository implicitly.
files=(
    'LICENSE|LICENSE|0644'
    'PACKAGE-VERSION|PACKAGE-VERSION|0644'
    'README.md|README.md|0644'
    'LICENSING.md|LICENSING.md|0644'
    'SOURCE-PROVENANCE.md|SOURCE-PROVENANCE.md|0644'
    'THIRD-PARTY-NOTICES.md|THIRD-PARTY-NOTICES.md|0644'
    'CONTRIBUTING.md|CONTRIBUTING.md|0644'
    'licenses/GPL-2.0.txt|licenses/GPL-2.0.txt|0644'
    'licenses/LGPL-2.1.txt|licenses/LGPL-2.1.txt|0644'
    '.gitattributes|.gitattributes|0644'
    '.gitignore|.gitignore|0644'
    '.github/workflows/glibc-runtime.yml|.github/workflows/glibc-runtime.yml|0644'

    'bootstrap-public-steam.sh|bootstrap-public-steam.sh|0755'
    'configure-steam-default-compat.py|configure-steam-default-compat.py|0755'
    'build-public-native.sh|build-public-native.sh|0755'
    'fetch-proton-components.sh|fetch-proton-components.sh|0755'
    'fetch-ubuntu-runtime.py|fetch-ubuntu-runtime.py|0755'
    'generate-ubuntu-runtime-lock.py|generate-ubuntu-runtime-lock.py|0755'
    'install-minimal-steam.sh|install-minimal-steam.sh|0755'
    'make-termux-deb.sh|make-termux-deb.sh|0755'
    'make-hangover-steamswap-deb.sh|make-hangover-steamswap-deb.sh|0755'
    'make-apt-repository.sh|make-apt-repository.sh|0755'
    'setup-apt-repository.sh|setup-apt-repository.sh|0755'
    'make-glibc-source-package.sh|make-glibc-source-package.sh|0755'
    'make-public-package.sh|make-public-package.sh|0755'
    'make-public-release.sh|make-public-release.sh|0755'
    'make-public-source-tree.sh|make-public-source-tree.sh|0755'
    'refresh-public-lsteamclient-patch.sh|refresh-public-lsteamclient-patch.sh|0755'
    'refresh-public-native-locks.sh|refresh-public-native-locks.sh|0755'
    'run-public-steam.sh|run-public-steam.sh|0755'
    'run-steam.sh|run-steam.sh|0755'
    'run-steam-tgcompat.sh|run-steam-tgcompat.sh|0755'
    'steamwebhelper-patched.sh|steamwebhelper-patched.sh|0755'
    'steam-fontconfig.conf|steam-fontconfig.conf|0644'
    'steam-linux-official-packages.txt|steam-linux-official-packages.txt|0644'
    'ubuntu-runtime-lock.json|ubuntu-runtime-lock.json|0644'

    'packaging/steam-arm64/release.lock|packaging/steam-arm64/release.lock|0644'
    'packaging/steam-arm64/steam-arm64|packaging/steam-arm64/steam-arm64|0755'
    'packaging/steam-arm64/steam-arm64-setup|packaging/steam-arm64/steam-arm64-setup|0755'
    'packaging/steam-arm64/steam-arm64.desktop|packaging/steam-arm64/steam-arm64.desktop|0644'
    'packaging/steam-arm64/steam-arm64-termux.svg|packaging/steam-arm64/steam-arm64-termux.svg|0644'

    'steam-cert-fopen64-shim.c|steam-cert-fopen64-shim.c|0644'
    'steam-cert-open-shim.c|steam-cert-open-shim.c|0644'
    'steam-cert-shim.c|steam-cert-shim.c|0644'
    'steam-cert-stat-shim.c|steam-cert-stat-shim.c|0644'
    'steam-dlmopen-shim-v2.c|steam-dlmopen-shim-v2.c|0644'
    'steam-helper-shim.c|steam-helper-shim.c|0644'
    'steam-module-shim.c|steam-module-shim.c|0644'
    'steam-open-shim.c|steam-open-shim.c|0644'
    'steam-path-shim.c|steam-path-shim.c|0644'
    'steam-proc-self-shim.c|steam-proc-self-shim.c|0644'
    'steam-sysv-shim.c|steam-sysv-shim.c|0644'
    'steam-unix-socket-shim.c|steam-unix-socket-shim.c|0644'
    'steam-x11-main-shim.c|steam-x11-main-shim.c|0644'

    'steam-sdkarm64/steam-launch-wrapper|steam-sdkarm64/steam-launch-wrapper|0755'
    'proton-bionic-tool/COMPONENTS.md|proton-bionic-tool/COMPONENTS.md|0644'
    'proton-bionic-tool/compatibilitytool.vdf|proton-bionic-tool/compatibilitytool.vdf|0644'
    'proton-bionic-tool/proton|proton-bionic-tool/proton|0755'
    'proton-bionic-tool/steam-runtime-steam-remote|proton-bionic-tool/steam-runtime-steam-remote|0755'
    'proton-bionic-tool/toolmanifest.vdf|proton-bionic-tool/toolmanifest.vdf|0644'
    'proton-bionic-fex-tool/compatibilitytool.vdf|proton-bionic-fex-tool/compatibilitytool.vdf|0644'
    'proton-bionic-fex-tool/proton|proton-bionic-fex-tool/proton|0755'
    'proton-bionic-fex-tool/toolmanifest.vdf|proton-bionic-fex-tool/toolmanifest.vdf|0644'
    'box64-native-tool/box64-native|box64-native-tool/box64-native|0755'
    'box64-native-tool/compatibilitytool.vdf|box64-native-tool/compatibilitytool.vdf|0644'
    'box64-native-tool/toolmanifest.vdf|box64-native-tool/toolmanifest.vdf|0644'
    'proton-hangover-glibc-tool/compatibilitytool.vdf|proton-hangover-glibc-tool/compatibilitytool.vdf|0644'
    'proton-hangover-glibc-tool/proton|proton-hangover-glibc-tool/proton|0755'
    'proton-hangover-glibc-tool/toolmanifest.vdf|proton-hangover-glibc-tool/toolmanifest.vdf|0644'

    'steam-bridge/LICENSE|steam-bridge/LICENSE|0644'
    'steam-bridge/README.md|steam-bridge/README.md|0644'
    'steam-bridge/build.sh|steam-bridge/build.sh|0755'
    'steam-bridge/client.c|steam-bridge/client.c|0644'
    'steam-bridge/protocol.h|steam-bridge/protocol.h|0644'
    'steam-bridge/server.cpp|steam-bridge/server.cpp|0644'
    'steam-bridge/proton-functions.sh|steam-bridge/proton-functions.sh|0755'
    'steam-bridge/run-server.sh|steam-bridge/run-server.sh|0755'
    'steam-bridge/smoke-test.sh|steam-bridge/smoke-test.sh|0755'
    'steam-bridge/test-bionic-proxy.sh|steam-bridge/test-bionic-proxy.sh|0755'
    'steam-bridge/test-steamstub-appticket.sh|steam-bridge/test-steamstub-appticket.sh|0755'
    'steam-bridge/callback-smoke-mock.cpp|steam-bridge/callback-smoke-mock.cpp|0644'
    'steam-bridge/probe-html.c|steam-bridge/probe-html.c|0644'
    'steam-bridge/probe-matchmaking.c|steam-bridge/probe-matchmaking.c|0644'
    'steam-bridge/probe-steamstub-appticket.c|steam-bridge/probe-steamstub-appticket.c|0644'
    'steam-bridge/applist_native.inc|steam-bridge/applist_native.inc|0644'
    'steam-bridge/apps_native.inc|steam-bridge/apps_native.inc|0644'
    'steam-bridge/client_native.inc|steam-bridge/client_native.inc|0644'
    'steam-bridge/friends_native.inc|steam-bridge/friends_native.inc|0644'
    'steam-bridge/gamesearch_native.inc|steam-bridge/gamesearch_native.inc|0644'
    'steam-bridge/gameserver_native.inc|steam-bridge/gameserver_native.inc|0644'
    'steam-bridge/gameserver_stats_native.inc|steam-bridge/gameserver_stats_native.inc|0644'
    'steam-bridge/html_native.inc|steam-bridge/html_native.inc|0644'
    'steam-bridge/http_native.inc|steam-bridge/http_native.inc|0644'
    'steam-bridge/inventory_native.inc|steam-bridge/inventory_native.inc|0644'
    'steam-bridge/lobby_native.inc|steam-bridge/lobby_native.inc|0644'
    'steam-bridge/matchmaking_native.inc|steam-bridge/matchmaking_native.inc|0644'
    'steam-bridge/networking_messages_native.inc|steam-bridge/networking_messages_native.inc|0644'
    'steam-bridge/networking_native.inc|steam-bridge/networking_native.inc|0644'
    'steam-bridge/networking_sockets_native.inc|steam-bridge/networking_sockets_native.inc|0644'
    'steam-bridge/networking_utils_native.inc|steam-bridge/networking_utils_native.inc|0644'
    'steam-bridge/parental_native.inc|steam-bridge/parental_native.inc|0644'
    'steam-bridge/remote_storage_native.inc|steam-bridge/remote_storage_native.inc|0644'
    'steam-bridge/ugc_native.inc|steam-bridge/ugc_native.inc|0644'
    'steam-bridge/user_native.inc|steam-bridge/user_native.inc|0644'
    'steam-bridge/user_stats_native.inc|steam-bridge/user_stats_native.inc|0644'
    'steam-bridge/utils_native.inc|steam-bridge/utils_native.inc|0644'
    'steam-bridge/video_native.inc|steam-bridge/video_native.inc|0644'
    'steam-bridge/voice_native.inc|steam-bridge/voice_native.inc|0644'

    'public-source/glibc/.gitignore|public-source/glibc/.gitignore|0644'
    'public-source/glibc/BUILD-PROVENANCE.md|public-source/glibc/BUILD-PROVENANCE.md|0644'
    'public-source/glibc/README.md|public-source/glibc/README.md|0644'
    'public-source/glibc/REBUILD.md|public-source/glibc/REBUILD.md|0644'
    'public-source/glibc/rebuild-glibc.sh|public-source/glibc/rebuild-glibc.sh|0755'
    'public-source/glibc/source-lock.env|public-source/glibc/source-lock.env|0644'
    'public-source/lsteamclient/README.md|public-source/lsteamclient/README.md|0644'
    'public-source/lsteamclient/build-bionic-lsteamclient.sh|public-source/lsteamclient/build-bionic-lsteamclient.sh|0755'
    'public-source/lsteamclient/prepare-proton-source.sh|public-source/lsteamclient/prepare-proton-source.sh|0755'
    'public-source/lsteamclient/source.lock|public-source/lsteamclient/source.lock|0644'
    'public-source/lsteamclient/patches/0001-termux-arm64-bridge.patch|public-source/lsteamclient/patches/0001-termux-arm64-bridge.patch|0644'
    'public-source/hangover-steamswap/README.md|public-source/hangover-steamswap/README.md|0644'
    'public-source/hangover-steamswap/source.lock|public-source/hangover-steamswap/source.lock|0644'
    'public-source/hangover-steamswap/patches/0001-steamclient-swap-arm64.patch|public-source/hangover-steamswap/patches/0001-steamclient-swap-arm64.patch|0644'
    'public-source/native/artifacts.tsv|public-source/native/artifacts.tsv|0644'
    'public-source/native/source-lock.sha256|public-source/native/source-lock.sha256|0644'
    'public-source/native/source-trees.tsv|public-source/native/source-trees.tsv|0644'
    'public-source/native/toolchains.tsv|public-source/native/toolchains.tsv|0644'
)

# These orchestrators may be added while the release work is in progress. They
# remain opt-in by exact filename; no wildcard is used.
optional_files=(
    'build-lsteamclient-public.sh|build-lsteamclient-public.sh|0755'
)

copy_file() {
    local source_name=$1 destination_name=$2 mode_bits=$3
    [[ $source_name != /* && $destination_name != /* ]] ||
        die "absolute allowlist entry: $source_name -> $destination_name"
    case /$source_name/ in
        *'/../'*|*'/./'*) die "unsafe source allowlist entry: $source_name" ;;
    esac
    case /$destination_name/ in
        *'/../'*|*'/./'*) die "unsafe destination allowlist entry: $destination_name" ;;
    esac
    [[ -f $root/$source_name && ! -L $root/$source_name ]] ||
        die "missing or unsafe allowlisted source: $source_name"
    install -D -m "$mode_bits" -- "$root/$source_name" "$stage/$destination_name"
}

for entry in "${files[@]}"; do
    IFS='|' read -r source_name destination_name mode_bits <<< "$entry"
    copy_file "$source_name" "$destination_name" "$mode_bits"
done
for entry in "${optional_files[@]}"; do
    IFS='|' read -r source_name destination_name mode_bits <<< "$entry"
    [[ ! -e $root/$source_name && ! -L $root/$source_name ]] ||
        copy_file "$source_name" "$destination_name" "$mode_bits"
done

# Copy only these tracked paths from the exact upstream revisions. In
# particular, neither repository's .git directory, build tree nor untracked
# files can enter the staged source tree.
steamclienttermux_repo=$root/reference-steamclienttermux
tgcompat_repo=$root/reference-termux-glibc-compat
install -d -m 0755 -- "$stage/reference-steamclienttermux"
install -d -m 0755 -- "$stage/reference-termux-glibc-compat"
if [[ -d $steamclienttermux_repo/.git && -d $tgcompat_repo/.git ]]; then
    [[ $(git -C "$steamclienttermux_repo" rev-parse HEAD) == "$steamclienttermux_revision" ]] ||
        die 'reference-steamclienttermux is not at its locked revision'
    [[ $(git -C "$tgcompat_repo" rev-parse HEAD) == "$tgcompat_revision" ]] ||
        die 'reference-termux-glibc-compat is not at its locked revision'
    git -C "$steamclienttermux_repo" archive --format=tar "$steamclienttermux_revision" -- \
        LICENSE config/steam-arm64-bootstrap-lock.json \
        scripts/bootstrap-steam-arm64-client.py diagnostics/native-lsof.c |
        tar -xf - -C "$stage/reference-steamclienttermux"
    git -C "$tgcompat_repo" archive --format=tar "$tgcompat_revision" -- \
        .github/workflows/ci.yml .gitignore LICENSE Makefile README.md \
        benchmarks docs/ARCHITECTURE.md docs/BASELINE.md docs/PERFORMANCE.md \
        docs/ROADMAP.md include integration probes scripts src tests |
        tar -xf - -C "$stage/reference-termux-glibc-compat"
else
    # Public checkouts vendor these locked snapshots without nested .git
    # directories. Their bytes are then protected by the outer repository.
    git -C "$root" diff --quiet -- reference-steamclienttermux \
        reference-termux-glibc-compat || die 'vendored reference snapshot is modified'
    tar -C "$steamclienttermux_repo" -cf - \
        LICENSE config/steam-arm64-bootstrap-lock.json \
        scripts/bootstrap-steam-arm64-client.py diagnostics/native-lsof.c |
        tar -xf - -C "$stage/reference-steamclienttermux"
    tar -C "$tgcompat_repo" -cf - \
        .github/workflows/ci.yml .gitignore LICENSE Makefile README.md \
        benchmarks docs/ARCHITECTURE.md docs/BASELINE.md docs/PERFORMANCE.md \
        docs/ROADMAP.md include integration probes scripts src tests |
        tar -xf - -C "$stage/reference-termux-glibc-compat"
fi

# Normalize metadata so two runs with the same source bytes produce the same
# Git worktree and checksum manifest.
find "$stage" -type d -exec chmod 0755 {} +
find "$stage" -type f -exec touch -d "@$source_epoch" {} +

python3 - "$stage" <<'PY'
import pathlib
import re
import sys

root = pathlib.Path(sys.argv[1])
binary_suffixes = {
    ".7z", ".a", ".apk", ".bin", ".bz2", ".class", ".deb", ".dll",
    ".dylib", ".elf", ".exe", ".gif", ".gz", ".ico", ".jar", ".jpeg",
    ".jpg", ".o", ".pdf", ".png", ".pyc", ".pyo", ".rpm", ".so",
    ".sqsh", ".tar", ".wasm", ".xz", ".zip", ".zst",
}
forbidden_parts = {
    ".git", "__pycache__", "build", "cache", "dist", "download-cache",
    "lsteamclient-bionic-build", "node_modules", "out", "package",
    "proton-lsteamclient-src", "steam-home", "steam-linux-libs", "steamapps",
    "steamrtarm64", "tgcompat-glibc", "valve-client", "vendor", "work",
}
credential_names = {
    ".env", ".netrc", ".npmrc", ".pypirc", "config.vdf", "id_ed25519",
    "id_rsa", "loginusers.vdf", "registry.vdf",
}
secret_patterns = (
    re.compile(rb"-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----"),
    re.compile(rb"\bgh[pousr]_[A-Za-z0-9]{30,}\b"),
    re.compile(rb"\bgithub_pat_[A-Za-z0-9_]{20,}\b"),
    re.compile(rb"\bAKIA[0-9A-Z]{16}\b"),
    re.compile(rb"\bxox[baprs]-[A-Za-z0-9-]{20,}\b"),
    re.compile(rb"\b7656119[0-9]{10}\b"),
)
magic_prefixes = (
    b"\x7fELF", b"MZ", b"PK\x03\x04", b"\x1f\x8b", b"\xfd7zXZ\x00",
    b"(\xb5/\xfd", b"7z\xbc\xaf'\x1c", b"BZh", b"\x89PNG\r\n\x1a\n",
    b"GIF87a", b"GIF89a", b"%PDF-",
)

files = []
for path in sorted(root.rglob("*")):
    relative = path.relative_to(root)
    parts = relative.parts
    lowered = {part.lower() for part in parts}
    if path.is_symlink():
        raise SystemExit(f"symlink is forbidden: {relative}")
    if path.is_dir():
        continue
    if not path.is_file():
        raise SystemExit(f"non-regular entry is forbidden: {relative}")
    if lowered & forbidden_parts:
        raise SystemExit(f"forbidden path component: {relative}")
    if any(part.lower().startswith("steamworks_sdk") for part in parts):
        raise SystemExit(f"Steamworks SDK path is forbidden: {relative}")
    name_lower = path.name.lower()
    if name_lower in credential_names or name_lower.startswith("ssfn"):
        raise SystemExit(f"credential path is forbidden: {relative}")
    suffixes = {suffix.lower() for suffix in path.suffixes}
    if suffixes & binary_suffixes:
        raise SystemExit(f"binary/archive suffix is forbidden: {relative}")
    data = path.read_bytes()
    if b"\x00" in data or any(data.startswith(prefix) for prefix in magic_prefixes):
        raise SystemExit(f"binary content is forbidden: {relative}")
    try:
        data.decode("utf-8")
    except UnicodeDecodeError as error:
        raise SystemExit(f"non-UTF-8 source file: {relative}: {error}") from error
    for pattern in secret_patterns:
        if pattern.search(data):
            raise SystemExit(f"possible embedded credential: {relative}")
    if path.suffix.lower() == ".patch":
        if re.search(
            rb"^(?:diff --git|---|\+\+\+).*steamworks_sdk", data,
            re.MULTILINE | re.IGNORECASE,
        ):
            raise SystemExit(f"patch references a Steamworks SDK path: {relative}")
        if b"GIT binary patch" in data:
            raise SystemExit(f"binary patch payload is forbidden: {relative}")
    files.append(relative)

# Fail closed if a new local server include was added without being added to
# the source allowlist. Steamworks SDK headers are intentionally external.
server = (root / "steam-bridge/server.cpp").read_text(encoding="utf-8")
for include in re.findall(r'^#include\s+"([^"]+)"', server, re.MULTILINE):
    if include == "protocol.h" or include.endswith("_native.inc"):
        if not (root / "steam-bridge" / include).is_file():
            raise SystemExit(f"missing allowlisted local bridge include: {include}")

if not files:
    raise SystemExit("staged source tree is empty")
print(f"source scan: OK ({len(files)} UTF-8 text files)")
PY

(
    cd -- "$stage"
    find . -type f ! -name SOURCE-TREE-SHA256SUMS -print0 |
        LC_ALL=C sort -z | xargs -0 sha256sum > SOURCE-TREE-SHA256SUMS
)
chmod 0644 -- "$stage/SOURCE-TREE-SHA256SUMS"
touch -d "@$source_epoch" "$stage/SOURCE-TREE-SHA256SUMS"
find "$stage" -depth -exec touch -d "@$source_epoch" {} +

file_count=$(find "$stage" -type f | wc -l)
if [[ $mode == check ]]; then
    printf 'Public source tree allowlist and scan: OK (%s files)\n' "$file_count"
    exit 0
fi

mv -- "$stage" "$destination"
stage=
keep_stage=1
printf 'Created public source tree: %s (%s files)\n' "$destination" "$file_count"
