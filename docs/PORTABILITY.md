# Platform Boundaries and Consensus Invariance

Windows is a supported platform backend, not the STN-Chain architecture.

Consensus-relevant behavior must remain identical across supported operating
systems and processor architectures.

## Implemented layout

The existing directories are the architecture-equivalent layout; no duplicate
consensus implementations or cosmetic core directory migration is introduced.

| Location | Responsibility |
| --- | --- |
| includes/ | Portable core types and explicit service contracts |
| src/stn_*.c | Canonical codecs, validation, work, chain/fork logic, protocol/sync and storage coordination |
| src/main.c | Application composition and development-node entry point |
| platforms/stn_build_config.h | Sole compiler-macro OS/CPU detection |
| platforms/stn_backend.h | Deliberate runtime backend availability gate |
| platforms/windows/ | CNG, Winsock, NTFS and their Windows-specific declarations |
| platforms/linux/, platforms/macos/ | Explicit unavailable backend boundaries and future target directories |

Windows socket handles, wchar paths and Windows declaration headers now live
under platforms/windows. Core includes no Windows SDK header or host-selection
header. stn_platform.h retains only ISO C17, 8-bit-byte and exact-width integer
prerequisites. Application composition includes the backend gate; compiling
pure validation code does not require an OS-specific backend.

## Selection and qualification matrix

| OS | Detected architectures | Runtime backend | Qualification |
| --- | --- | --- | --- |
| Windows | x86, x64, ARM32, ARM64 | x64 CNG/Winsock/NTFS available; others fail explicitly | Release/x64, VS Community 2026, v145/MSVC 14.51.36231, SDK 10.0.26100.0 only |
| Linux | x86, x64, ARM32, ARM64 | Not implemented; application backend selection fails | None |
| macOS | x86, x64, ARM32, ARM64 | Not implemented; application backend selection fails | None; recognition is not a promise that an OS/CPU combination exists or is supported |
| Unknown/ambiguous | Unknown/ambiguous | Configuration error | None |

Existing macOS target folders reserve x64 and ARM64 only. ARM64EC mixed ABI is
explicitly rejected. Compiler spelling aliases map to the same host IDs.
Detection never selects a hashing algorithm, serialization rule, chain target
or validation branch. No host byte-order macro is needed or consulted.

The explicit backend gate prevents accidental Linux/macOS fallthrough into
Windows code, and prevents unqualified Windows architecture builds from being
mistaken for supported runtimes. Future qualification requires providing the
appropriate implementations and intentionally updating the gate/build matrix.
No fake provider is introduced to make an unsupported target link.

## Service contracts

| Service | Portable contract and ownership | Backend responsibility |
| --- | --- | --- |
| SHA-256 | stn_hash_provider, canonical domain span followed by byte span; unchanged digest on failure | Real byte-identical SHA-256; CNG today; no test fallback |
| Network | stn_peer_transport exact bounded send/receive; explicit failures and finite session deadline | Sockets, partial I/O, monotonic scheduling and close; Winsock today |
| Filesystem | stn_storage_provider bounded read and atomic complete replace | Paths, file handles, staging, flush and namespace replacement |
| Storage exclusion | Provider acquire/release surrounds read/validate/replace | Cooperating writer exclusion; Windows exclusive lock-file handle |
| Validation time | Caller-supplied context time and deterministic chain timestamps | No ambient clock in consensus |
| Transport time | Finite session deadline defined by transport contract | GetTickCount64/select inside Windows adapter, never consensus input |

No random/entropy service is currently used by runtime core, so none is invented.
There is no threaded core scheduler. Windows test harness threads remain test
infrastructure, not a hidden core dependency. New services must be introduced
only when a concrete caller requires them and remain outside consensus rules.

## Canonical behavior and integer safety

Wire fields use explicit uint8/16/32/64 representations and bytewise big-endian
reads/writes, not pointer casts into buffers. Hashes consume specified canonical
bytes, never structures, pointers, padding or process-local identities.
size_t describes in-process spans only; it is never persisted as a native field.
Lengths/counts are validated before copy/iteration. Existing maximum snapshot
(67,320,620 bytes) and message limits fit a 32-bit size_t. Work uses explicit
32-byte integers with bounded carry/division, not compiler-specific wide types.

Pointer-containing objects and padding are noncanonical state. New tests fill
objects with different padding patterns, assign equal fields at different
addresses and verify identical serialized bytes/transaction IDs. Unaligned
input offsets 0..7 produce identical decoded values and re-encoding. Exact
endian-sensitive uint64 fields, UINT32_MAX/SIZE_MAX rejection and target-work
vectors are checked. Existing independent SHA-256, ID, chain/fork, persistence
and mock transport/provider tests remain intact.

These checks expose layout assumptions on the current host. They do not replace
actual 32-bit, big-endian or ARM builds, nor establish cross-platform qualification.
All future implementations must reproduce the same independent byte/digest,
validation, work and fork fixtures before being marked supported.

Persisted format and peer framing remain unchanged. Atomic replacement and
crash durability are backend properties, not portable consensus truths. Each
new filesystem backend must satisfy the provider's old-or-new publication
contract and document its own durability limits. Current NTFS limitations,
including absence of a power-loss guarantee, remain unchanged.

## Enforcement and testing

The Visual Studio test project's pre-build step runs tools/check-platform.ps1.
It checks 12 simulated OS/CPU combinations, 12 backend-selection cases,
unknown/conflicting OS/CPU, ARM64EC rejection, four compiler spelling aliases,
and one source-boundary audit: 34 probes total. Expected compile failures must
contain their specific configuration diagnostics. These are compile-only macro
simulations using the current compiler, not foreign-architecture binaries.

The boundary audit rejects OS SDK includes or scattered host/architecture checks
in portable sources and includes. The test executable adds 93 invariance checks;
all earlier tests and the localhost SHA-256/NTFS integration also run. The
pre-build probes use provisioned Visual Studio tools and do not download tools.

Deferred: Linux/macOS implementations and qualification, ARM/x86 qualification,
public RPC/explorer APIs, wallets, contracts, mining/Stratum, difficulty adjustment,
economics and mempool. No protocol or software release version is changed.

## RPC follow-up

RPC codecs/dispatch and node-service snapshots now reside in portable src/ and
includes/. They contain no OS APIs, paths, native handles or struct wire copies.
The Windows loopback RPC runtime is now implemented; transport must stay
behind platform interfaces. See [RPC.md](RPC.md).


Mining-work identity hashes canonical bytes including the body with a zero nonce; no native structures, addresses or ambient time are serialized. The runnable Windows composition lives in platforms/windows/stn_app_windows.c and reuses CNG/NTFS/Winsock adapters. Tests cover input relocation and unaligned work; only Release/x64 is qualified.
