# Source and build provenance

This document identifies the exact upstream revisions used by the current
`steam-arm64-termux 0.1.6` release.  A public binary release must
be generated from these revisions or update this file, its lock files, and all
corresponding-source archives together.

## Project inputs

| Component | Upstream | Pinned revision |
| --- | --- | --- |
| Valve Proton source used for `lsteamclient` patches | <https://github.com/ValveSoftware/Proton> | `db9e6ffbf24a95b104fb699dd62532c70a2f9a51` (`proton-11.0-2`) |
| Hangover Wine source used for the Steam-swap loader overlay | <https://github.com/AndreRH/wine> | `628d1aeb09803d3fa9170023e7203060e955bfe8` (`hangover-11.16`) |
| Termux glibc package recipe and tgcompat integration | <https://github.com/moio9/glibc-packages> | `99eb8c36733165debd70255ce60c86d2155b61c1`, branch `tgcompat-glibc-2.44` |
| Termux glibc packages base | <https://github.com/termux/glibc-packages> | `954c6b200aa001088fcc420550b9304dd81229b8` |
| tgcompat source | <https://github.com/huntergdavis/termux-glibc-compat> | `8d63206ac60eb1106cb5303f1ac75f5a3bd60a62` |
| Steam ARM64 bootstrap helper | <https://github.com/huntergdavis/steamclienttermux> | `dee92b8432424e873011542e6926b12651bc823c` |
| GNU C Library | <https://ftp.gnu.org/gnu/libc/glibc-2.44.tar.xz> | SHA-256 `37f600f2bef3c5e8300147059568b2a2e40a7ad6ccc65ce942556d49429cc667` |

The staged tgcompat glibc runtime is selected by
`tgcompat-glibc/current`.  For this development build its content-addressed
directory and originating package SHA-256 are both:

```text
b8868b0a4cee25ebad811c4cb5ae7f5ab8c4091835d7fc8f9775e2fd9d497c3f
```

The fork commits after the Termux base are:

- `094d759b4e9b56dff8bc4c3e55b95e01e0fcaf90`: tgcompat integration;
- `99eb8c36733165debd70255ce60c86d2155b61c1`: serialized glibc install phases.

## Download-only inputs

These files are fetched from their owners on the user's device and are not
redistributed by this project:

- Valve's ARM64 Steam client, pinned by
  `reference-steamclienttermux/config/steam-arm64-bootstrap-lock.json`;
- Ubuntu ARM64 packages, pinned by `ubuntu-runtime-lock.json`;
- DXVK 1.10.3 and vkd3d-proton 3.0.1, pinned by
  `fetch-proton-components.sh`;
- the base Hangover Wine 11.16 and its Box64/FEX packages, installed from
  configured Termux package repositories.

The project APT repository separately distributes four rebuilt LGPL Wine
loader artifacts in `hangover-wine-steamswap`. Their exact source revision,
patch and output hashes are recorded in
`public-source/hangover-steamswap/source.lock`.

## Publication rule

Do not publish a binary bootstrap unless the same release also provides the
complete corresponding source archive for the patched glibc runtime, its
checksum, all applicable license texts, and the scripts/configuration needed
to rebuild it.  Do not place Valve's Steam client, Steam credentials, game
files, or copied Steamworks SDK source trees in this repository or its release
assets.
