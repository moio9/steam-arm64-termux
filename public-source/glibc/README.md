# glibc complete corresponding source

This directory defines the complete-corresponding-source bundle for the exact
patched glibc binary shipped by the ARM64 Termux Steam bootstrap. The binary is
`glibc_2.44_aarch64.deb`, SHA-256
`b8868b0a4cee25ebad811c4cb5ae7f5ab8c4091835d7fc8f9775e2fd9d497c3f`.

The generated source archive contains:

- the original GNU glibc 2.44 source archive, verified against its recipe hash;
- clean exports of both the original and final `glibc-packages` trees;
- a full binary-safe patch from the original recipe to the final recipe;
- the exact `termux-glibc-compat` tree that supplied the tgcompat overlay;
- the exact `termux-packages` build scripts used by CI;
- the overlay as a separately visible directory;
- governing license texts, the source lock, build provenance, and rebuild
  instructions; and
- a SHA-256 manifest covering every regular file in the source bundle.

No source is fetched by `--check`:

```sh
./make-glibc-source-package.sh --check
```

The public repository also provides the manually dispatched GitHub Actions
workflow `.github/workflows/glibc-runtime.yml`. It checks out both packaging
repositories at the commits and tree objects recorded in `source-lock.env`,
uses the locked Termux builder image, rebuilds the AArch64 package, validates
its Debian metadata, and uploads the result with a SHA-256/provenance record.
The workflow labels its output as a rebuild candidate: it does not publish a
release, and the candidate still has to pass the tgcompat probes on Android.

Because the historical CI consumed package indexes that were not immutable, a
new build is not assumed to be byte-identical. Manual dispatch offers an
optional `require_historical_hash` gate when exact reproduction is desired.

Fetching is an explicit maintainer action. It verifies the upstream glibc
archive hash and every Git commit/tree identity before caching anything:

```sh
./make-glibc-source-package.sh --fetch
./make-glibc-source-package.sh --build
```

Verify a generated or downloaded archive with:

```sh
./make-glibc-source-package.sh --verify \
  glibc-complete-source-2.44-tgcompat-b8868b0a.tar.zst
```

The `.sha256` file next to the archive is the release checksum. Publish the
source archive and checksum beside every release that conveys the matching
patched glibc binary. Do not substitute a newer checkout: the source lock is
tied to one binary hash.
