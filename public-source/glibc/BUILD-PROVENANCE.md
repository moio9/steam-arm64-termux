# Build provenance

## Binary identity

The staged runtime selector `tgcompat-glibc/current` resolves to
`b8868b0a4cee25ebad811c4cb5ae7f5ab8c4091835d7fc8f9775e2fd9d497c3f`.
Its marker contains that same value, which is also the SHA-256 of the source
package binary `glibc_2.44_aarch64.deb` (9,842,100 bytes).

The package reports:

```text
Package: glibc
Version: 2.44
Architecture: aarch64
```

## Source derivation

The final package recipe is commit
`99eb8c36733165debd70255ce60c86d2155b61c1` of
`moio9/glibc-packages`. Its history is:

1. official `termux/glibc-packages` glibc 2.44 recipe
   `954c6b200aa001088fcc420550b9304dd81229b8`;
2. tgcompat overlay commit
   `094d759b4e9b56dff8bc4c3e55b95e01e0fcaf90`; and
3. serialized install-phase fix
   `99eb8c36733165debd70255ce60c86d2155b61c1`.

The overlay input repository was
`huntergdavis/termux-glibc-compat` commit
`8d63206ac60eb1106cb5303f1ac75f5a3bd60a62`. The generator includes both its
full tree and its `integration/termux-glibc` directory separately.

The recipe downloads GNU glibc 2.44 from GNU FTP and requires SHA-256
`37f600f2bef3c5e8300147059568b2a2e40a7ad6ccc65ce942556d49429cc667`.
The generator includes that unchanged upstream archive.

## CI record

- GitHub Actions run: `moio9/glibc-packages`, run `33036107159`
- successful AArch64 job: `98399024594`
- build interval: 2026-08-27 03:21:17Z to 03:31:15Z
- artifact ID/name: `9632218951` /
  `debs-aarch64-99eb8c36733165debd70255ce60c86d2155b61c1`
- build image:
  `ghcr.io/termux/package-builder-cgct@sha256:fd7b60a92c5f1cb9425239cc53058ecc2a6e4993501728f9e29ac6b7a7d58e66`
- build command: `./build-package.sh -I -a aarch64 --library glibc glibc`
- downloaded artifact tar: 13,127,680 bytes, SHA-256
  `89a2709024ea4ebdab79a54e3be6f229537d6e82b14ef057df0dbbefbbc2be7a`

The workflow's `get-build-package.sh` performed a depth-one clone of
`termux/termux-packages` master between 03:21:43Z and 03:21:45Z but did not
print the checked-out commit. GitHub's immutable commit timeline shows that the
branch tip throughout that interval was
`3afcb5be31f848c2705181f4ee7daf091de0f47b`; the source bundle pins and includes
that tree. This derivation is recorded explicitly instead of pretending that
the original CI log printed a commit it did not print.

The exact build image digest and all build-control source are preserved. A
future rebuild can still differ byte-for-byte because repository dependency
indexes and the GitHub-hosted runner were not content-addressed by the original
workflow. That does not change which source corresponds to the conveyed
binary; the binary SHA-256 above remains the release identity.
