# Third-party components

The public bootstrap package does not contain Valve's Steam client, Ubuntu
runtime libraries, DXVK, vkd3d-proton, or a copied Steamworks SDK. Components
needed at install time are downloaded on the end user's device from their
upstream locations and checked against pinned revisions, sizes, or SHA-256
hashes as applicable.

- Steam client: downloaded directly from Valve under the Steam Subscriber
  Agreement and Steam Client license. Redistribution is explicitly disabled in
  the client lock file.
- Ubuntu 24.04 ARM64 packages: downloaded from Canonical's Ubuntu Snapshot
  Service. The bootstrap extracts each package's Debian copyright file into
  `THIRD-PARTY-NOTICES/ubuntu/` on the user's device.
- DXVK 1.10.3: zlib/libpng license; fetched from the upstream DXVK release.
- vkd3d-proton 3.0.1: LGPL-2.1; fetched from the upstream vkd3d-proton release.
- Hangover Wine 11.9 loader overlay: LGPL Wine artifacts rebuilt from the
  pinned Hangover source and the public
  `public-source/hangover-steamswap` patch. The APT overlay package includes
  the patch, source lock and rebuild notes alongside the four modified files.
- Wine/Proton `lsteamclient` compatibility code: the public repository carries
  a project patch and a locked preparation/build workflow. The unmodified
  Proton input is obtained directly from Valve. A copied `steamworks_sdk_*`
  tree and the locally built `lsteamclient.so` are not release payloads.
- Steamworks SDK headers: not redistributed. Maintainers rebuilding the native
  bridge provide a legally obtained SDK 1.54 header directory locally; its
  expected content digest is recorded by the native build workflow.
- tgcompat: MIT; its license and complete source are included in the source
  repository and in the glibc corresponding-source bundle where applicable.
- GNU C Library runtime: LGPL-2.1-or-later. The bootstrap includes the exact
  patched runtime. A matching, independently checksummed complete-source
  archive contains GNU glibc 2.44, both Termux recipe revisions, the project
  diff and overlay, tgcompat, build metadata, and the applicable license texts.

This notice is informational and is not a substitute for the license text
shipped with each component.
