# Steam ARM64 for Termux — minimal runtime

> The monolithic development archive contains Valve files and is for local
> testing only. A public release must use `bootstrap-public-steam.sh`, which
> downloads the locked ARM64 seed directly from Valve into `valve-client/`.
> Valve binaries must not be mirrored or attached to a project release.

This tree runs Valve's native ARM64 Steam client directly through glibc on
Android and launches x86 Windows games through the Bionic Hangover/Box64
compatibility tool. PRoot is not used by Steam, CEF, Wine, the game, or the
Steamworks bridge.

The package assumes that Termux, Termux:X11, an audio server, and a working
Termux Vulkan/Turnip driver are already installed. It deliberately does not
bundle or replace Android applications or GPU drivers.

## Install and run

### Termux package and desktop menu

Configure the project APT repository once, then install and update Steam like a
normal Termux package:

```sh
curl -fsSL https://raw.githubusercontent.com/moio9/steam-arm64-termux/main/setup-apt-repository.sh | bash
pkg install steam-arm64
```

The package installs the `steam-arm64` command, a **Steam ARM64** entry in the
XFCE/LXQt application menu, and an original project icon. The first launch opens
the verified bootstrap in a terminal; later launches open Steam directly.
User data and downloaded Valve files remain below
`~/.local/share/steam-arm64-termux`, so removing the package never deletes a
Steam account, installed games, or credentials. The APT package also installs
the version-locked `hangover-wine-steamswap` overlay required by legacy Steam
DRM; it intentionally depends on `hangover-wine 11.16`.

Steam registers two Bionic compatibility choices for Windows games:

- **Proton Bionic Box64 (Termux)** — the tested default;
- **Proton Bionic FEX (Termux)** — the same Wine prefix, selecting
  `libwow64fex.dll` instead of `wowbox64.dll` through `HODLL`.

If the separately built `hangover-glibc-11.9` runtime is present, Steam also
registers **Proton ARM64 glibc (Termux)**. It remains optional because that
custom runtime is not bundled with the small bootstrap package.

For a direct `.deb` installation without adding the repository, download the
`steam-arm64_<version>_aarch64.deb` release asset and run
`pkg install ./steam-arm64_<version>_aarch64.deb`.

### Manual bootstrap

The Git repository and GitHub-generated `Source code` archives intentionally
contain no runtime binaries, so they are not installers. Download the named
`steam-arm64-termux-<version>-bootstrap.tar.zst` release asset and its adjacent
checksum, then run. If the Releases page has no such asset yet, the project is
still development-only and no supported public installer has been published.

```sh
sha256sum -c steam-arm64-termux-<version>-bootstrap.tar.zst.sha256
tar --zstd -xf steam-arm64-termux-<version>-bootstrap.tar.zst
cd steam-arm64-termux
./bootstrap-public-steam.sh
./run-public-steam.sh
```

The bootstrap downloads the locked Valve client and other download-only
upstream components before calling `install-minimal-steam.sh`. In a fully
populated maintainer/development tree, the lower-level equivalent is:

```sh
./install-minimal-steam.sh
./run-steam.sh
```

The public bootstrap verifies Valve's manifest, archive size and SHA-256 before
safe extraction. Login and client updates remain entirely inside Valve's client.
The Ubuntu compatibility libraries are fetched from Canonical's immutable
snapshot service as 123 locked ARM64 packages; all archive sizes and SHA-256
values are recorded in `ubuntu-runtime-lock.json`. DXVK and vkd3d-proton are
likewise fetched from their upstream GitHub releases and checked against pinned
hashes. None of these upstream binaries are mirrored in the public archive.

The installer obtains ordinary dependencies with `pkg`, prepares the pinned
Valve Proton source without copying any `steamworks_sdk_*` directory, builds
the custom Unix and PE `lsteamclient` bridge modules locally, installs them
into Hangover, and validates the existing Vulkan ICD. It does not install a
Linux distribution or `proot-distro`.

Use `./install-minimal-steam.sh --check` to validate an existing installation,
or `--skip-packages` when all official packages are already installed and only
the custom bridge should be installed.

For a Windows game, select **Proton Bionic (Termux)** in the game's Steam
compatibility properties. The tested default is Box64 plus DXVK 1.10.3; this
older DXVK is intentional because DXVK 3.0.2 required `shaderInt64` and crashed
or rejected the Adreno 740 Turnip path used in testing.
Only this tested compatibility tool is registered by the minimal package;
development and legacy Proton experiments are intentionally omitted.

## Architecture

- Valve ARM64 Steam and CEF run under the Termux glibc loader plus `tgcompat`.
- `steam-linux-libs` is assembled locally from verified Ubuntu snapshot `.deb`
  files and contains the ABI-coherent Linux closure needed by Steam. Its
  generated manifest lists every file and origin.
- Hangover Wine and its Box64 backend come from `pkg`. The project APT
  repository supplies a small, source-locked Hangover 11.16 loader overlay for
  legacy Steam DRM; the base Hangover package is not bundled.
- `lsteambridge` uses a same-UID, mode-0600 Unix socket to connect Wine's
  Bionic `lsteamclient` proxy to the authenticated native Steam session.
- No password, login token, or Steam credential crosses the bridge or belongs
  in a redistributable archive; bounded Steamworks API results are returned to
  the game on demand.

## Steamworks status

The bridge implements the paths exercised by No More Room in Hell, including
SteamUser/auth tickets, Friends (including legacy 013/014 layouts), Apps
(including legacy 005), Utils/callbacks, UserStats, Matchmaking/lobbies and
server queries, legacy Networking, NetworkingMessages002,
NetworkingSockets012/013, NetworkingUtils003/004, RemoteStorage, UGC 014/015,
GameServer, and GameServerStats. Individual, batch, and poll-group networking
messages pass the Windows i386 loopback regression probe. NMRiH has reached
authenticated menus, map loading, hosting, joining, and gameplay through this
build.

SteamUser011-015 use separate legacy ABI layouts, while SteamUser021 voice
control/capture/decompression uses the real native Steam
session through a separate 64 KiB-bounded transport. The regression probe also
checks the real voice sample rate, userdata folder, and phone/2FA state without
starting microphone recording.

Known limitations are the Steam overlay, anti-cheat drivers, and HTMLSurface's
embedded browser/MOTD. Steamworks is a versioned C++ ABI, so games using other
interface revisions can still require additional per-method marshalling.

## Creating a redistributable archive

Run all release checks and create the public assets with:

```sh
./make-public-release.sh --check
./make-public-release.sh --fetch-sources
STEAMWORKS_SDK_DIR=/absolute/path/to/sdk/public/steam \
  ./make-public-release.sh --build /absolute/path/to/new-release-directory
```

The SDK path is used only to reproduce and compare the project-owned native
bridge binary. It is never copied into an archive. The release directory
contains a download-only bootstrap and a separately checksummed complete-source
archive for the modified glibc runtime.

To create only the public download-only bootstrap, use:

```sh
./make-public-package.sh /absolute/path/steam-arm64-termux-bootstrap.tar.zst
```

This archive contains the project-built compatibility code, public build
workflows and licenses, and the pinned upstream manifests. It contains no
Valve client, copied Steamworks SDK, Ubuntu `.deb`, DXVK, vkd3d-proton, or
prebuilt `lsteamclient.so`. Use `--check` for a quick allowlist check or
`--verify` to traverse every selected file without creating an archive. A
binary release must be accompanied by the matching glibc complete-source asset
created by `make-glibc-source-package.sh` or the release orchestrator above.

The old monolithic generator is retained only for private development backups.
It refuses to package Valve files unless explicitly acknowledged and its output
must not be published or shared:

```sh
STEAM_ARM64_LOCAL_PROPRIETARY_OK=1 ./make-minimal-package.sh \
  /storage/emulated/0/Download/steam-arm64-termux-local-only.tar.zst
```

The archive excludes `steam-home` (credentials, userdata and installed games),
Steam's download cache in `package`, logs, source/build trees, old compatibility
experiments, Android apps, Vulkan drivers, and the unused DXVK 3.0.2 payload.
It also excludes roughly 119 MB of patched Steam executables and temporary
runtime links; these are reproduced from the original Valve files on launch.
The bundled `tgcompat` tree contains only its session launcher and built runtime
objects; its tests and development sources remain in the source repository.

To validate the archive manifest without writing the large archive:

```sh
./make-minimal-package.sh --check
```

For a full read-only traversal of every file that would enter the archive, use
`./make-minimal-package.sh --verify`. This performs more I/O but writes no
archive.
