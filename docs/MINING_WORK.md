# Deterministic mining work and runnable development node

Implemented on Windows Release/x64. This is a node-side work interface, not a
miner, Stratum server, public network, wallet, or coin. Production CNG SHA-256
and the existing v3 block rules validate submitted work regardless of its source.

## Run locally

Build Release | x64 in Visual Studio, then double-click `run-dev.cmd` in the
repository root. It starts `build\x64\Release\stn-chain.exe` with:

```
stn-chain.exe --dev --data stn-chain-dev.stns --rpc-port 18473
```

The listener is **127.0.0.1:18473**, binary RPC v1 `STNC` (see [RPC.md](RPC.md)).
It is not HTTP/JSON-RPC, Bitcoin RPC, Ethereum RPC, or Stratum. stn-stratumd must
implement this documented node-facing protocol; compatibility with an existing
stratumd miner-facing lifecycle is not qualified. The actual current STN-Stratum
server and production STNC client are now qualified for the Chain-facing contract
in Phase 10 Chunk 1; see RPC.md and tools/test-stratum-interface.ps1.
There is no background mining. Ctrl+C requests shutdown. Socket timeouts are
idle I/O bounds; a healthy connection is no longer killed by a fixed total
session lifetime.

`--dev` explicitly selects the existing fixed real-SHA256 regression genesis
and its single structural test transaction. The fixture has **no semantically
validated intelligence**, production signatures, admission policy, or monetary
value. Its repeat use is an explicit test configuration, not transaction selection
or production replay policy. The first template is already solved at nonce 0;
nonce 1 fails PoW and nonce 2 also solves it. These are fixed test vectors, not
a search implementation. Later templates require valid work supplied by a client.

For explicit operator-selected development inputs:

```
stn-chain.exe --dev --genesis genesis.block --transaction selected.stnt --data chain.stns --rpc-port 18473
```

Supply canonical v3 genesis bytes and one canonical STNT transaction, not text
or hexadecimal. Network and fixed target come from the exact genesis, which is
fully validated including PoW. The selected transaction is reused until restart
with a different explicit input in this explicit development mode.
Normal mode uses `--genesis genesis.block --data chain.stns --rpc-port 18473`
without `--dev` or `--transaction`, selecting eligible pending publications.
Without eligible content the core returns UNAVAILABLE:
current consensus requires at least one transaction, so empty blocks are invalid.
This remains a structural development chain; signature/authority/replay admission
is not silently added to the existing block validator.

Data paths require an existing trusted directory on local fixed NTFS. Missing
storage is created once; existing storage is fully revalidated. Corruption fails
startup and is never treated as an empty chain. The app does not automatically
invoke recovery or P2P synchronization. It reloads disk on each request, so an
external writer using the existing storage contract is observed on the next call.
`--rpc-port 0` chooses and prints a free port. `--once` exits after one connection
and is used by `tools/test-node.ps1`. No arguments print usage.

Chain history is no longer bounded by the 64-block validation batch constant,
and the runnable node grows its storage work buffers as history grows. RPC keeps
connections alive while I/O remains active rather than enforcing a 64-request or
60-second total-session fixture. The loopback listener supports resource-limited concurrent clients with serialized dispatch. Public binding, authentication and peer discovery remain deferred.

## Canonical template

### Pending candidate selection

When a pending store is configured, candidate construction uses its ascending
unsigned canonical-ID enumeration, then the existing canonical body encoder.
It reuses record validation against the immutable current context and excludes
active-history signer/nonce replays. Missing or mismatched context fails closed;
ineligible entries are skipped without eviction. Provider errors fail the build.
The caller must externally serialize the complete operation with admission and
chain activation, as the Windows RPC dispatcher already does.

Eligible transactions are selected in that order up to the existing limit of
16 transactions and 1,051,712 body bytes. Selection stops if the next eligible
transaction cannot fit the protocol body limit. A short caller buffer returns
CAPACITY rather than silently choosing different content. Current pending bounds
(256 KiB total canonical transaction bytes) make the protocol byte ceiling
unreachable before the store/count bounds; the byte guard remains enforced.

No eligible transactions returns UNAVAILABLE because empty blocks are prohibited.
No filler, fees, rewards, arrival-time priority, or fixture fallback is introduced.
Template and work-context queries do not remove pending entries, including ones
made ineligible by active history. Existing cleanup in other operations is
preserved without extension in this increment.

Unchanged tip, target, deterministic timestamp, validation snapshot and pending
set produce identical candidate bytes and work ID. A changed selected candidate
invalidates old pending-derived work under the existing STALE handling; a change
outside the selected set need not change the work ID. The 64-bit nonce region and
nonce-only submission rules remain unchanged. Explicit `--dev` fixture support
remains; normal construction requires no selected.stnt. The executable still
needs production identity providers to admit eligible signed publications; test
hooks demonstrating selection do not supply those providers.

### Header and work identity

Successful local acceptance prepares canonical-ID pending removals before the
atomic storage transition and applies them only after success. SUBMIT_WORK no
longer performs preliminary pending pruning, so validation, storage, or stale
activation failures preserve the complete pending state. Multiple included IDs
are removed together; absent IDs are harmless and unrelated entries retain their
bytes/order. Peer synchronization supplies the equivalent post-adoption cleanup
when its workspace is attached to the node's pending store. Template construction
continues to remove nothing. Detached-block requeue is not implemented.

`stn_mining_service` loads and fully validates persisted history, then constructs
one v3 block using its network, tip ID, next height, fixed target, and tip timestamp.
The timestamp is deliberately unchanged (existing rules allow equality); no wall
clock or process state contributes. The explicit canonical body supplies count,
length, and calculated body commitment. Structural/network/integrity checks run
before publication. The template nonce is zero. Zero nonce need not satisfy PoW.
Capacity and cumulative-work overflow fail closed. No rewards or fees are inserted.

The response and solved-work request share this payload:

| Offset | Bytes | Meaning |
| --- | ---: | --- |
| 0 | 32 | Current parent/tip ID |
| 32 | 32 | Deterministic work ID |
| 64 | 4 | Canonical block length, unsigned big-endian |
| 68 | block length | Complete canonical candidate block |

```
work_id = SHA256("STN-CHAIN:WORK:ID:1" || 00 || complete_zero_nonce_block)
```

The NUL is exactly one byte. Work identity includes the body, not just the header.
Only **block offsets 152..159** (payload 220..227) may change: one unsigned
64-bit big-endian nonce. All other bytes and the work ID must match exactly.
The 32-byte unsigned big-endian target is at block offsets 120..151 (payload
188..219). Do not reverse hash bytes or use Bitcoin compact-target notation.

PoW remains single SHA-256 of `"STN-CHAIN:BLOCK:ID:1" || 00 || header[0..167]`.
Interpret the digest as unsigned big-endian and require H <= T. Header offset
152 is not a Bitcoin 32-bit nonce, and SHA256d ASIC compatibility is not claimed.

Independent first development work ID:
`59f6d10b443dcf2bf0bb37f021b3c5644a66e71839be8380d956cbf2ca4b76c7`.
Nonce-zero child block ID:
`35eb9fa251a3eeca9412b3c23159fbdbf26d874fa9af8969341d2ef67d8c8726`.

## Submission and atomicity

Send the returned payload to RPC 0x2003, changing only nonce bytes. The node
reloads the current snapshot, rejects a different parent as STALE, rebuilds the
canonical template, and verifies its work ID and every immutable byte. An unknown
ID or changed content under the same parent is REJECTED. Malformed nested lengths
are INVALID. No cache or list of previously issued jobs is required. Repeated
identical context/body produces the same work ID across restarts and relocations.

Solved local work is validated directly against the current accepted tip using
the ordinary candidate validator, including real SHA-256, PoW, body, network,
link, timestamp and target rules. `stn_storage_extend` reloads under exclusion,
requires the persisted state to still equal the active state, revalidates the
candidate, and atomically replaces storage before publishing the new active state.
Fork choice remains reserved for actual competing branches rather than ordinary
one-block extension.
An intervening accepted write produces STALE; failed replacement leaves both the
previous persisted snapshot and this service's accepted-state output unchanged.
Successful response is block ID 32, accepted height u64, cumulative work 32 (72 bytes).
Failure responses carry no payload.

Tip changes caused by local work, peer extension, reorganization, or recovered-chain
activation invalidate work identically: no origin-specific branch exists. Repair
that restores the exact same tip and configured content does not create a new job.
The cached `active` field records successful local submissions; validation and
queries always reload disk rather than trust that cache after another writer acts.

Stratum distributes the canonical work; miners search it; STN-Chain validates
solutions. Internal miners can call this same logical service using the same
payload. Neither needs access to storage files, private state, nor candidate
construction logic. There is no privileged internal-miner acceptance route.

## Verification and remaining scope

The C suite tests independent work bytes/ID, repeated/relocated inputs, unaligned
submissions, every immutable byte, fixed valid and invalid real PoW, malformed
requests, RPC responses, local extension, reorganization, recovered activation,
intervening storage writes, and replacement failure without partial activation.
`tools/test-node.ps1` starts the real executable, fragments TCP headers, exercises
work retrieval/submission/errors, stops it, and verifies NTFS persistence on restart.
Pointer-width independence is a serialization property, not an x86/ARM build claim.

Deferred: CPU/GPU/ASIC mining and detection, internal/background mining, Stratum/
stn-stratumd, shares/payouts, wallet/coin/issuance/rewards/treasury, fees/gas/economics,
difficulty adjustment, mempool/admission, contract runtime, explorer, public RPC
deployment/authentication, and additional OS/architecture qualification.

## Long-running server reconciliation (2026-09-08)

The executable has been tested through height 70 using independently fixed
SHA-256 solutions, including work at height 71 after restart. Scratch starts at
actual required size and grows; externally changed storage is probed before
loading. Borrowed core buffers never implicitly realloc. A solution binds to the
freshly validated snapshot, not a stale service cache left by another writer.

Ordinary extension validates the submitted block against the accepted tip and
uses atomic snapshot replacement; it does not assemble a second history or invoke
fork choice. The existing persisted prefix is still revalidated under exclusion
and snapshot bytes are still copied/hashed. Streaming/indexed persistence and
avoiding full-history reads remain future work; dynamic allocation is not a claim
of constant-memory chain operation.

Tests keep 21 TCP clients connected, exercise churn and more than 64 requests,
retain a partial STNC payload across the 60-second idle poll, and verify another
idle client remains usable after that poll. Client threads own frame buffers;
shared node operations serialize. Failed new-client resources do not stop the
listener. Completed sessions are reclaimed during accepts and idle polling.
Socket interruption is separated from close/join to avoid shutdown races.

Pending accepted submissions/block selection remain deferred in this bounded
runtime/history increment. Production signature/authority/replay admission is
still unavailable; no fixture transaction is promoted to authenticated pending
intelligence. The configured transaction remains explicit development content.
