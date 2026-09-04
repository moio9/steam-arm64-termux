# Licensing map

The top-level `LICENSE` applies only to original project code whose files do
not state another license.  It does not relicense third-party code, generated
interfaces, or downloaded components.

## Project code

The launcher scripts, native compatibility shims, and Steam bridge code written
for this project are distributed under the MIT License in `LICENSE` and
`steam-bridge/LICENSE`, to the extent that the project contributors own those
files.

The standalone tgcompat project is also MIT licensed.  Files integrated into a
glibc build carry their own `LGPL-2.1-or-later` notices and are distributed as
part of the modified glibc work under glibc's applicable terms.

## Patched glibc runtime

The binary bootstrap contains a modified GNU C Library runtime.  It must only
be published together with equivalent access to its complete corresponding
machine-readable source, including the exact GNU source release, Termux package
recipe, project modifications, build scripts/configuration, and applicable
GPL/LGPL license texts.  `SOURCE-PROVENANCE.md` and the glibc source-bundle
workflow pin those inputs. Verbatim GPL-2.0 and LGPL-2.1 texts are also carried
in `licenses/` beside the binary bootstrap.

## Valve Proton and lsteamclient

Valve Proton is a multi-license project.  Its top-level files use
`LICENSE.proton`, while the upstream `lsteamclient` directory contains the
Steamworks SDK license and SDK headers.  This project does not place a copied
Steamworks SDK source tree in its public repository or release assets.

The public workflow obtains the pinned Proton revision directly from Valve and
applies the project's separate patch set locally.  A patch does not relicense
the upstream files it modifies.  Project-authored additions remain subject to
any third-party rights required to compile or use them with Proton and
Steamworks.

## Hangover loader overlay

The APT repository's `hangover-wine-steamswap` package contains four rebuilt
LGPL Wine artifacts. The exact Hangover 11.16 revision, project patch, artifact
hashes, rebuild notes and LGPL-2.1 text are published with the project; the
base Hangover package remains an external dependency.

## Download-only components

Valve's Steam client, Ubuntu packages, DXVK, vkd3d-proton, the base Hangover
Wine package, and Box64/FEX components are not relicensed by this project.
The bootstrap either downloads them directly from their upstream publishers using locked hashes or
installs them through configured package repositories.  Their own license and
service terms continue to apply.

See `THIRD-PARTY-NOTICES.md` for upstream locations and component notices.  The
project name does not imply endorsement by Valve, Canonical, CodeWeavers, or
any other upstream project.
