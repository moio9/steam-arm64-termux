# lsteambridge

This is an open-source ABI bridge between Android Bionic and the native glibc
Steam client. It does not copy, load, or depend on GameNative binaries.

The glibc server loads the existing ARM64 `steamclient.so`, connects to the
already authenticated desktop Steam session, and listens on an
`AF_UNIX/SOCK_SEQPACKET` socket. The Android Bionic client proves that the same
socket can be reached from Hangover/Wine's host side.

Security properties:

- the socket is mode `0600`;
- the server accepts only the same Android UID using `SO_PEERCRED`;
- an existing non-socket or foreign-owned path is never replaced;
- the protocol has fixed-width fields, magic, version, opcode, and sequence;
- no password, refresh token, or Steam credential is transferred or stored;
  only the Steamworks API requests and bounded results needed by the game cross
  the socket.

Build and test while native Steam is running and logged in:

```sh
./steam-bridge/build.sh
./steam-bridge/smoke-test.sh
```

The bridge now includes method-specific marshalling for SteamUser, Friends,
Utils, UserStats, Apps, AppList, Matchmaking/lobbies and server queries,
GameSearch, legacy Networking, NetworkingMessages, NetworkingSockets,
RemoteStorage, UGC, HTTP, Inventory, Video, ParentalSettings, GameServer, and
GameServerStats. Legacy
interface layouts used by Source games are represented by separate vtables
instead of assuming that current slot numbers are compatible.

`SteamUser011` through `015` have separate, version-correct vtables for their
older voice and authentication layouts; `016` through `021` share the newer
compatible core layout. `SteamUser021` includes the legacy usage/data-folder calls, voice recording
control, availability, compressed capture, decompression, native sample rate,
auth, phone/2FA state, market eligibility, and duration-control calls. Voice
audio has a separate protocol capped at 64 KiB per request/response, with all
reported sizes checked before data is copied back into the Windows process.

`SteamClient013` through `SteamClient021`, plus `SteamClient023`, have
version-correct client vtables matching their real method order. In particular,
`SteamClient019` is kept separate from `020` because `SetLocalIPBinding` changed
from an IPv4 integer to an IPv4/IPv6 structure. Valve also added and removed
client getters between versions; treating every version as `SteamClient020` can
turn `RunFrame` or an interface lookup into a call with an incompatible
signature.

`STEAMHTTP_INTERFACE_VERSION001` through `003` perform requests through the
authenticated native Steam client and transfer headers and bodies in bounded
chunks. `STEAMINVENTORY_INTERFACE_V001` through `V003` support status,
result/property reads, serialization, definitions, prices, and inspection.
Inventory mutations such as consuming, exchanging, purchasing, or granting
items intentionally remain disabled; an ABI bridge must not turn an accidental
game call into an account mutation.

With a nonzero `SteamAppId`, the current ARM64 Steam client exposes its native
`SteamNetworkingMessages002`, `SteamNetworkingSockets012`, and
`SteamNetworkingUtils004` implementations.
The launcher keys the bridge process by AppID so a server inherited from a
different game is restarted instead of silently using the wrong networking
context. If NetworkingMessages is unavailable, Steam-ID identities still have
a compatibility fallback over legacy P2P.

`SteamNetworkingSockets012` and `013` use distinct bridge vtables because their
batch `SendMessages` signatures differ. IP/P2P creation without pointer-valued
options, connection and listen handles, socket pairs, individual and batch
send/receive ownership, status, identity, authentication, poll groups, names,
user data, and lane configuration are marshalled to the native Steam object.
Direct, batch, and poll-group message delivery were verified end to end with a
Windows i386 loopback probe. The WOW64 message-list and allocation conversions
are patched locally so 32-bit games keep valid message ownership across the
Wine/ARM64 boundary.

`SteamNetworkingUtils003` and `004` expose message allocation, relay and ping
status, POP/timestamp queries, FakeIP/address and identity conversion, plus
scalar/string configuration reads. Callback function pointers and other raw
pointer-valued options deliberately stay inside their originating process.
Remote relay/P2P interoperability still needs a two-device test.

Raw C++ pointers are never sent between the two ABIs or processes. Handles,
structures, strings, arrays, and asynchronous callback results are validated
and copied through the fixed-width protocol. Interfaces not yet implemented
return conservative local defaults; enable `LSTEAM_BRIDGE_TRACE` to identify a
new game's exact missing method before extending the protocol.

The full Bionic regression probe can be rebuilt and run with:

```sh
./steam-bridge/test-bionic-proxy.sh
```
