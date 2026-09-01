# Reproducible Termux ARM64 `lsteamclient.so` build

This directory contains only this project's patch set and build recipes. It
does not contain Valve's Proton source, Steamworks SDK trees, or a prebuilt
`lsteamclient.so`.

`prepare-proton-source.sh` performs a sparse fetch from Valve's Proton tag
`proton-11.0-2`, verifies commit
`db9e6ffbf24a95b104fb699dd62532c70a2f9a51`, excludes every
`lsteamclient/steamworks_sdk_*` directory from the working tree, verifies and
applies the local patch, and fetches the one LGPL Wine header required by the
standalone build. The Wine header is pinned to Proton's Wine gitlink and is
verified by SHA-256.

On Termux, with `git`, `curl`, `clang`, `binutils`, and `hangover-wine`
installed, run:

```sh
bash public-source/lsteamclient/prepare-proton-source.sh
bash public-source/lsteamclient/build-bionic-lsteamclient.sh
```

The build also needs `steam-bridge/protocol.h` from this project. Its output is
`public-source/lsteamclient/work/build/lsteamclient.so`.

Useful overrides are `LSTEAM_WORK_ROOT`, `PROTON_CHECKOUT`,
`LSTEAM_DEPS_DIR`, `LSTEAM_BUILD_DIR`, `HANGOVER_WINE_ROOT`,
`LSTEAM_BRIDGE_INCLUDE`, `JOBS`, and `CXX`. A trusted Proton mirror can be used
with `PROTON_REPOSITORY_OVERRIDE`; the checked-out commit is still verified.

Valve's fetched files remain under their upstream terms, including the license
stored by Valve in `lsteamclient/LICENSE`. This patch set does not relicense
those files. The original bridge code is covered by the project's license.
