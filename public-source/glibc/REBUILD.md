# Rebuilding the patched glibc package

The source bundle is self-contained for the program source and build-control
source. A rebuild still requires the Linux/Termux package-builder environment,
the Android NDK selected by that tooling, and its build dependencies.

After extracting the complete-source archive, prepare a working checkout with:

```sh
./rebuild-glibc.sh /absolute/path/to/empty-work-directory
```

This copies the final patched `glibc-packages` snapshot, overlays the exact
`termux-packages` build files in precisely the same way as
`get-build-package.sh`, verifies the included GNU glibc archive, and places it
in `upstream-source/`. It does not start Docker unless `--run` is supplied.

For the closest reconstruction of the CI build, install Docker and run:

```sh
docker pull \
  ghcr.io/termux/package-builder-cgct@sha256:fd7b60a92c5f1cb9425239cc53058ecc2a6e4993501728f9e29ac6b7a7d58e66
docker tag \
  ghcr.io/termux/package-builder-cgct@sha256:fd7b60a92c5f1cb9425239cc53058ecc2a6e4993501728f9e29ac6b7a7d58e66 \
  ghcr.io/termux/package-builder-cgct:latest
./rebuild-glibc.sh /absolute/path/to/empty-work-directory --run
```

The original recipe URL and SHA-256 are unchanged. The included
`sources/glibc-2.44.tar.xz` is the exact input and can be used to seed the
Termux builder's source cache for an offline rebuild. `rebuild-glibc.sh` keeps
a verified copy at `upstream-source/glibc-2.44.tar.xz` instead of guessing a
host-specific cache path.

Expected package identity for the historical build:

```text
filename: glibc_2.44_aarch64.deb
size:     9842100
sha256:   b8868b0a4cee25ebad811c4cb5ae7f5ab8c4091835d7fc8f9775e2fd9d497c3f
```

The historical hash proves which binary this source accompanies. A rebuild is
not promised to be byte-identical because the original workflow consumed live
package indexes and an unpinned GitHub runner image; see `BUILD-PROVENANCE.md`.

For a hosted rebuild, manually dispatch `Rebuild patched glibc runtime` in the
project's GitHub Actions page. That workflow pins the source commits, verifies
their Git tree identities, and pulls the recorded builder image by digest. Its
artifact is deliberately a candidate rather than a release: validate it with
the on-device scripts in `reference-termux-glibc-compat` before packaging it.
