# Chain Persistence and Atomic Application


Current work representation is 320 bits / 40 canonical big-endian bytes (Phase 11
Chunk 3). This supersedes historical 256-bit work limits below. STNC/STNP version
2 carries widened work fields; block, target, hash and mining-job formats remain.

Status: implemented ISO C17 storage coordination and a Windows x64 local NTFS
adapter. The runnable node uses this as its current canonical block store. The
format is still whole-snapshot persistence, not the final scalable/indexed store.

## Persisted representation

Storage version 1 is independent of block/software versions. All integers
are fixed-width unsigned big-endian. No C structs or derived state are stored.

| Offset | Width | Meaning |
| --- | --- | --- |
| 0 | 4 | STNS magic |
| 4 | 2 | Storage version 1 |
| 6 | 2 | Flags 0; unknown flags rejected |
| 8 | 4 | Nonzero block count (u32) |
| 12 onward | repeated | u32 block length followed by exact canonical block bytes |
| final 32 bytes | 32 | SHA-256 of storage domain followed by all preceding file bytes |

The domain is ASCII STN-CHAIN:STORAGE:1 followed by one NUL. Each block is
364 through 1,051,880 bytes. Storage history is no longer coupled to the
64-block validation/fork batch constant. Counts and lengths are checked against
the actual file before advancing spans; truncation, overflow and trailing bytes
reject. The runnable node sizes its workspace from the existing file and grows
it as accepted blocks require.

The checksum detects accidental corruption, not malicious replacement: anyone
can recompute it. Complete chain validation is required even when it matches.
No height, tip, target cache or cumulative-work claims are persisted. Block
headers carry their normal consensus fields; state is reconstructed from them.

## Startup and provider boundary

stn_storage_load acquires provider exclusion, reads into caller-owned scratch,
checks framing/checksum, initializes EMPTY and validates canonical blocks
incrementally in chain order. The 64-block batch limit is not a history limit. Structural and
body checks, network, exact genesis, links, timestamps, target, PoW and work
must pass. Only then does it publish a view/state. Views borrow scratch bytes.

Missing storage returns NOT_FOUND; it is not an accepted empty chain. Empty
files and count zero are invalid. Explicit stn_storage_create may install a
validated nonempty complete history only when storage is absent. It refuses
any existing file, including corruption. There is no prefix salvage or silent
repair. Legacy non-PoW contexts are unsupported by this persistence profile.

The portable provider contract supplies acquire, release, bounded read and
atomic replace. Exclusion covers read, validation and publication together.
All participants must honor it. Replace success means the complete new snapshot
was published; failure must leave the old snapshot authoritative. A provider
cannot report failure after committing. Unsupported/misbehaving providers are
not made safe by consensus validation. No filesystem paths or Windows APIs
enter the consensus core.

Scratch may change on failure. Caller-visible view/state objects remain
unchanged. Encode failure leaves written length zero; uncommitted scratch must
not be consumed as a valid file. Callers provide disjoint live buffers and
immutable candidate/context inputs. No allocation derives from disk lengths;
small callers may receive CAPACITY instead of reading a valid large snapshot.

## Extensions and reorganizations

Both use stn_storage_apply. Under provider exclusion it reloads and independently
validates the currently stored history, checks every caller active-state field
against reconstructed state, then reruns fork evaluation against the full
candidate history. The supplied plan must match every recalculated field,
including ranges, tips and work. Forged/stale plans reject. Equal/lower work
retains active; only a strictly preferred candidate may replace storage.

The full resulting history is validated and encoded before provider replace.
Memory changes only after replacement succeeds; no partial detach/attach state
is exposed. A process stopping after replacement but before memory assignment
restarts by validating the complete new disk snapshot. This is not a concurrent
in-process state manager: the caller serializes access to its active object.

Detach/attach ranges remain available in the caller's plan and histories.
Application does not discard/resubmit transactions, change confirmation indexes,
manage replay state or invent a mempool. Future node state must coordinate those
changes transactionally before those subsystems can use this application path.
Local chain acceptance still excludes production signatures, authority and
cross-block replay validation, exactly as before this increment.

## Windows replacement and durability limits

The adapter accepts bounded ordinary paths on local fixed NTFS volumes. It
uses an exclusive .lock file handle for cooperating operations; Windows
releases that handle on process termination. The lock file may remain and is
reopened on the next operation. Use one canonical path and an application-owned
trusted directory. External file/directory substitution and alias-based writers
are outside the contract. Network shares and non-NTFS volumes are refused.

The adapter creates a sibling .stage file with CREATE_NEW, writes the complete
snapshot, checks FlushFileBuffers and close, then performs a same-volume
MoveFileExW replacement with REPLACE_EXISTING and WRITE_THROUGH. Copy/delete
fallback is disabled. A failed pre-publication operation leaves the active
file alone; only staging created by that call is eligible for cleanup.
Successful rename is the publication point; no fallible action follows it.

References: [FlushFileBuffers](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-flushfilebuffers)
and [MoveFileExW](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-movefileexw).
The implementation flushes staged file data and relies on same-volume NTFS
namespace replacement for old-or-new visibility. It does not prove directory
metadata durability under power loss, defeat faulty disk caches, or guarantee
recovery from hardware/media damage. WRITE_THROUGH is not claimed as a general
power-loss guarantee. Power-cut testing has not been performed.

A process interrupted before promotion can leave .stage. Startup never loads
or promotes it; the authoritative file alone is validated. Subsequent writes
fail closed while staging remains, requiring explicit operator inspection and
removal. Failed cleanup can likewise leave staging. There is no automatic
recovery, backup fallback, or filesystem repair. If the authoritative file is
corrupt, startup refuses activation even if some prefix or staging data is valid.

## Qualification and limits

Windows Release/x64 tests cover deterministic save/load and reconstruction,
genesis and multi-block snapshots, all 64 development blocks, every truncation
of a three-block snapshot, malformed format/count/length, trailing bytes,
wrong network/genesis, body/transaction/link/target/PoW corruption, duplicate
transactions with recomputed checksums, and preserved output on failure.

Failure-injection tests exercise acquisition, staging, partial write, flush,
promotion and read failures for extensions/reorganizations. Actual NTFS tests
exercise save/reopen, writer exclusion, leftover staging, denied replacement,
reorganization, corrupt startup rejection, and real SHA-256 genesis/extension.
Tests use private temporary files and remove their own files on success.
Non-Windows adapters, cross-platform tests and power-cut guarantees are absent.

Whole-snapshot rewriting and full startup validation remain current limitations.
They are correctness-first behavior, not a 64-block lifetime ceiling. Scalable
indexing, pruning and a future streaming/segmented store remain separate work.
Ordinary mined extension now uses a dedicated validate-current-tip + atomic
replace path rather than whole-history fork-choice evaluation.

Deferred: networking/RPC/discovery/propagation, automatic target adjustment,
mining/Stratum, wallets, contracts, rewards/issuance/treasury, fees/gas, mempool
and transaction confirmation/replay coordination.

## Authorized peer-recovery follow-up

Strict load still refuses corruption. Separate recovery evidence/adoption APIs
now support validated-prefix reuse and complete atomic reconstruction; see
[PEER_PROTOCOL.md](PEER_PROTOCOL.md). The earlier absence of recovery describes
the persistence-only increment. Provider failures and oversized reads never
authorize repair. Staging recovery and power-loss guarantees remain absent.

## Runtime/history reconciliation (2026-09-08)

Ordinary extension reuses the already validated canonical prefix under the storage
lock, validates the single candidate, updates the count/checksum in independent
scratch, and atomically publishes before changing active state. It no longer
allocates a second block-span history or revalidates it through encode. Recovery
of a truncated tail scans the available complete prefix instead of discarding it
because the declared count no longer fits. Complete-history adoption can now
reorganize/rebuild beyond the bounded fork API's 64-block limit.

Storage views own their dynamically allocated span tables; callers release them.
Runtime buffers are explicitly malloc-owned; other callers receive CAPACITY rather
than an unsafe realloc of borrowed memory. Snapshot replacement and its u32 count
format remain unchanged. Full-history reads and snapshot rewrite cost remain;
streaming/segmented storage is intentionally deferred, not claimed implemented.

## Phase 14 lifecycle reconstruction

Persistence continues to store canonical accepted blocks. Lifecycle authority,
identity, revocation, and replay state is derived by replaying that history;
no sidecar lifecycle database is authoritative. Reorganization replaces the
accepted history and therefore replaces the derived lifecycle state.
