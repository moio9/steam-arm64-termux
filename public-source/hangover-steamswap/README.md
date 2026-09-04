# Hangover 11.16 Steam client swap

This patch ports Proton Wine's steamclient export swap to Hangover 11.16 and
makes it safe for ARM64/WOW64. It uses fixed-width module fields across the
32/64-bit Unix-call boundary, exposes the call in the WOW64 table, attaches
and pins the replacement module, skips Valve's unresolved imports and
entrypoint, and preserves shared writable image mappings required by the
Steam client.

Apply `patches/0001-steamclient-swap-arm64.patch` to Hangover commit
`628d1aeb09803d3fa9170023e7203060e955bfe8`, configure the normal Termux
Hangover build with i386, aarch64 and arm64ec PE targets, then build:

```sh
make -C build -j4 server/wineserver dlls/ntdll/ntdll.so \
  dlls/ntdll/i386-windows/ntdll.dll \
  dlls/ntdll/aarch64-windows/ntdll.dll
```

`source.lock` records the expected source, patch and output hashes. The
overlay package contains the four rebuilt LGPL Wine artifacts and installs
only on top of `hangover-wine (= 11.16)`.
