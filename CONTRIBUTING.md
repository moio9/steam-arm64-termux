# Contributing

This project is an experimental compatibility layer for running the official
ARM64 Steam client and legitimately owned games on Termux. Keep changes small,
reviewable, and reproducible.

## Never commit private or redistributable inputs

Do not commit or attach:

- Steam login data, sentry files, cookies, account identifiers, auth tickets,
  private keys, or unredacted user logs;
- Valve client binaries, installed games, Ubuntu package payloads, DXVK or
  vkd3d-proton release binaries;
- a copied `steamworks_sdk_*` tree or locally built `lsteamclient.so`;
- build caches, Wine prefixes, core dumps, `.orig`/`.rej` files, or test output.

Upstream components must stay download-only and be pinned by revision, size,
and SHA-256 where the upstream format permits it.

## Steamworks bridge boundary

Bridge methods may marshal calls to the user's real, authenticated Steam
session. They must not fabricate ownership, CD keys, account identity,
authentication tickets, entitlements, or anti-cheat state. Do not add
credential interception or a Steam service reimplementation. Tests should use
the caller's own account and software and should log only bounded,
non-sensitive diagnostics.

## Before proposing a change

Run the source-only checks:

```sh
bash -n ./*.sh public-source/glibc/*.sh public-source/lsteamclient/*.sh
./make-public-package.sh --check
./make-glibc-source-package.sh --check
./build-public-native.sh --check
./make-public-source-tree.sh --check
```

Native bridge changes also require a clean reproducible rebuild with a legally
obtained Steamworks SDK 1.54 header directory supplied through
`STEAMWORKS_SDK_DIR`. `lsteamclient` changes must be regenerated as the
standalone public patch, applied to the locked pristine Proton revision, and
built successfully without an SDK tree in the public checkout.

After the implementation has stopped changing, maintainers refresh and verify
the two publication locks explicitly:

```sh
./refresh-public-lsteamclient-patch.sh --write
./refresh-public-lsteamclient-patch.sh --check
./refresh-public-native-locks.sh --write
./refresh-public-native-locks.sh --check
```

The top-level `LICENSE` covers original project contributions unless a file
states another license. Contributors must only submit material they have the
right to license and distribute. Upstream-derived files retain their upstream
terms.
