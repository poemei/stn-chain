# Component Architecture

Status: proposed engineering boundaries, not an implemented package layout.
See [DECISIONS.md](DECISIONS.md) for unresolved protocol choices.

## Components and responsibilities

| Component | Responsibility | Boundary |
| --- | --- | --- |
| Protocol core | Canonical decoding, record/block validation, authorization, deterministic state transitions, consensus checks | No network calls, filesystem access, device access, or ambient wall-clock reads during deterministic validation |
| Node | Coordinate peers, pending records, chain selection, synchronization, and accepted state | Treat local submissions and peer messages as untrusted; use the same consensus rules |
| Storage adapter | Durable blocks and state, indexes, recovery, reversible state changes | Storage representation is separate from consensus encoding |
| Network adapter | Framing, connections, peer exchange, bounded requests, block/record propagation | Transport success does not establish validity or authority |
| Submission/query interface | Receive records, expose chain state and inclusion status | Administrative operations require separate access controls |
| Integration adapters | Connect STN-Labz API, Sentinel, DevBot, and company tools | External services cannot be required to resolve consensus validity |
| Contract engine | Validate authorized contract transitions against chain state | Execution model and resource accounting remain open |
| Miner, if activated | Obtain work, search, submit results, discard stale work | Node independently validates all submitted work |
| Platform layer | Files, sockets, time, entropy, concurrency, device access | OS-specific implementation remains outside deterministic core |

These are logical responsibilities. Exact libraries, executables, public C
interfaces, and directory layout will follow the protocol decisions.

## Record processing

1. An application prepares a record and signs the defined canonical bytes.
2. A node performs bounded decoding, identity/authorization checks, and
   pending-pool admission checks.
3. A candidate block selects ordered records against a specific parent.
4. Consensus validation rechecks all included records against the applicable
   state. Pending-pool admission is not a substitute for block validation.
5. Acceptance durably associates the block with its state transition.
6. Peers receive announcements and fetch missing data; each verifies it.
7. Consumers receive inclusion and chain-position information and apply their
   chosen confirmation policy before external action.

Consensus validity and local admission policy must be distinguished. Nodes
may impose local queue policies without changing which blocks are valid.

## Determinism and portability

Consensus must specify integer widths, byte order, lengths, ordering,
encoding, domain-separated signing/hash inputs, limits, and overflow rules.
Do not serialize C structs directly or use platform-dependent native types
as a wire specification. Economic quantities, if introduced, require exact
integer units rather than floating-point arithmetic.

Validation depends on explicit input bytes, prior state, and protocol rules.
Any time rules must have specified validation context. Contract execution
cannot read local files, call arbitrary APIs, or consult local randomness.

Cryptographic implementations should sit behind narrow interfaces with
published test vectors. Algorithm and library choices remain open; a small
dependency count does not justify inventing cryptographic primitives.

Memory ownership, bounded allocation, error propagation, and thread/state
ownership must be documented before implementation interfaces are frozen.

## Chain state and recovery

The development implementation now has an explicit in-memory validation state
and bounded atomic sequence validator; see [CHAIN_STATE.md](CHAIN_STATE.md).
Bounded Windows snapshot storage/application now exists; see
[PERSISTENCE.md](PERSISTENCE.md). Scalable storage/recovery remain future work.

Genesis must be identical for participants in the same network. Loading a
file successfully is not evidence that the chain is valid. Define verified
startup state, recovery after interrupted writes, and how indexes rebuild.

Chain changes must have an ordered owner or equivalent synchronization.
Pending records should not be irretrievably removed before acceptance is
durable. Fork handling needs reversible state transitions and a defined
policy for reconsidering displaced records.

Consumers must distinguish provisional inclusion from their action threshold.
Reorganization handling for acknowledgments and external actions must be
specified; the chain cannot undo a physical action or retract a delivered
message simply by switching branches.

## Contract authority

The proposed first contract exercise is policy issuance, activation,
acknowledgment, and supersession. This is a demonstration scope, not a
commitment to exclude general-purpose contracts.

Authorization needs chain-verifiable issuer identities, roles, delegation,
rotation, and revocation rules. A company service may prepare a decision;
validators must establish its validity using available protocol data.

## Mining boundary

The version-3 development profile now verifies work and accumulates integer
chain work, using real Windows SHA-256; see [POW.md](POW.md). The miner and
hardware backends described below remain unimplemented. Legacy version 1
is explicitly non-PoW and cannot be accepted by the PoW chain profile.

If PoW is selected for activation, define a work interface that binds work
to a parent, candidate contents, target, and job identity. Define stale-work
cancellation and independently checked submission results.

Start with a CPU reference backend. GPU acceleration and ASIC support depend
on the selected algorithm and compatible devices; generic ASIC compatibility
is not promised. Hardware backends may operate concurrently where supported,
without changing consensus requirements.

## Content distribution

Separate signed metadata and commitments from payload placement. Public
inline content, encrypted content, and externally stored documents have
different availability and privacy properties. The eventual protocol must
specify which bytes validators require and how recipients obtain payloads.

## Operational sovereignty

Maintain the ability to build from STN-Labz-controlled source, operate nodes,
recover state, and retain engineering knowledge independently of a single
hosting, build, identity, or AI provider. Hosted development services may
assist but must not become an unnecessary runtime dependency.

For each significant dependency, record its requirement, licensing and
security fit, interface boundary, loss impact, and practical replacement or
recovery path. Review depth should match impact. Locally maintainable mature
cryptography is compatible with this objective; weaker bespoke cryptography
is not a sovereignty improvement. See the policy references in DECISIONS.md.

## Deployment boundary

The first implementation runs on an explicitly isolated development network.
A local block-production harness, if used before consensus is implemented,
must be labeled as such and must not be represented as a secure blockchain.
Public operation requires the consensus, authorization, recovery, and
resource-limit milestones in [ROADMAP.md](ROADMAP.md).

## Local fork evaluation

The core now revalidates two bounded complete PoW histories, compares calculated
work, discovers their shared prefix and publishes an atomic in-memory plan.
Equal work retains current. No active state is changed. See [FORK_CHOICE.md](FORK_CHOICE.md)
for fixed-target limitations, eligibility, detach/attach ranges and future replay coordination.

## Persistence coordination

The portable storage layer treats disk as untrusted canonical input and calls
existing validators/fork choice. It reloads active data, rejects stale plans,
and commits caller state only after provider replacement. Windows filesystem
operations remain in platforms/windows. No transaction, replay or mempool state
is implicitly changed; those future subsystems require coordinated application.

## Peer evidence and explicit recovery

Socket transport, framing/handshake, sync coordination, consensus and storage
remain separate. Bounded header hints avoid retransmitting validated prefixes;
full blocks and calculated work alone determine preference. Explicit recovery
can rebuild damaged snapshots without changing strict startup rejection.
See [PEER_PROTOCOL.md](PEER_PROTOCOL.md). No peer or peer majority is authoritative;
P2P is separate from future application RPC.

## Enforced platform boundary

Windows is a supported platform backend, not the STN-Chain architecture.

Consensus-relevant behavior must remain identical across supported operating
systems and processor architectures.

The current layout and compile-time selection gates are documented in
[PORTABILITY.md](PORTABILITY.md). OS-specific declarations and services live
under platforms/; portable core modules share all consensus/wire behavior.

## RPC service boundary

RPC semantics call explicit node/service interfaces, which use the portable
validators and immutable snapshots. Transport remains a separate platform
responsibility. RPC is the supported application/miner integration boundary;
external tools must not inspect persistence files as an authoritative interface.
P2P is node-to-node evidence exchange; RPC is application-to-node interaction.
No RPC client gains consensus authority and no node becomes authoritative merely
by exposing RPC. See [RPC.md](RPC.md) for implemented and deferred methods.

## In-memory pending store foundation

`stn_pending_init`, `stn_pending_insert`, `stn_pending_lookup`,
`stn_pending_remove`, `stn_pending_count`, `stn_pending_enumerate`, and
`stn_pending_clear` provide a local store bounded independently to 128 entries
and 256 KiB of copied canonical transaction bytes. The fixed entry table adds
bounded metadata overhead. Insert checks the supported canonical transaction
structure and derives its protocol transaction ID through the SHA-256 provider.
This low-level operation does not authenticate or authorize a publication;
admission policy remains the caller's responsibility.

Identity and enumeration use ascending unsigned canonical ID bytes, independent
of arrival order. Duplicate IDs return DUPLICATE, including when full. Entry or
byte exhaustion returns CAPACITY without eviction or other mutation; removal
releases capacity. Allocation failure also returns CAPACITY without mutation.
Lookup copies bytes and enumeration copies bounded pages of IDs into caller
buffers; neither returns an internal pointer. Insert's ID output changes only
on success. Short lookup buffers return CAPACITY with zero bytes written.

Initialize only fresh storage; initialization allocates nothing and cannot
fail. Clear frees all owned bytes and resets the store, serving as both reset
and destruction; repeated clear is safe. Never shallow-copy a live store or
modify its implementation fields. Calls and caller buffers require external
serialization and the disjoint-span contract in `stn_pending.h`. This foundation
increment adds no RPC, mining, assembly, gossip, or pending persistence behavior;
existing working-tree integrations are preserved without extension.

## Validated pending admission

Incoming canonical publications use `stn_pending_admit_transaction` (STNT) or
`stn_pending_admit` (STNR), not the structural store insert primitive. Transaction
decoding delegates to existing canonical codecs; both admission paths reuse
`stn_validate_intelligence_record` for envelope, intelligence schema, network,
configured timestamp, signature, authority, and replay-hook validation. Only
UNDER_CONTEXT proceeds. The supplied immutable context must describe the same
network and active snapshot as the caller's validated storage view; callers hold
external serialization throughout validation and insertion.

The existing protocol transaction ID identifies pending entries. After successful
validation, an exact pending ID returns DUPLICATE before capacity checks. A
different ID reusing a pending signer/nonce returns REPLAY. Admission also scans
the validated active history for that signer/nonce before insertion, supplementing
the required replay hook; no new index or persistent replay subsystem is added.
The shared store primitive owns allocation/copying and preserves its limits,
ordering, and rejection guarantees. No allocation occurs before validation.

Results reuse the existing small enum: ACCEPTED, DUPLICATE, CAPACITY, INVALID
(including malformed data), UNSUPPORTED, REPLAY, NETWORK, TIME, SIGNATURE,
AUTHORITY, UNAVAILABLE (unresolved prerequisites), and PROVIDER (internal/provider
failure). Record-validation details remain available in the existing report;
later replay/store failures are conveyed by the returned admission result.
Unsupported transaction, record, or intelligence versions fail closed.

Signature and authority enforcement uses supplied validation hooks; this pending
admission increment did not wire a production identity or authority provider.
Missing hooks cannot admit data. Positive admission tests use explicitly scripted
hooks and production SHA-256, proving orchestration rather than signature or
authority qualification. Phase 14 Block 1 now supplies the isolated identity
and signature foundation; admission wiring and authority remain later work.
Existing integration work is preserved; this increment adds no RPC, mining,
assembly, gossip, or pending-persistence integration.

## Windows node composition

### Accepted-block pending cleanup

Local solved-work acceptance and peer synchronization prepare a bounded removal
mask by deriving each included transaction's existing canonical ID. Only exact
pending-ID matches are removed; a different signature/ID sharing a signer and
nonce is not an inclusion match. Preparation is read-only and publishes no mask
on hash/decoding failure. Cleanup frees owned bytes and updates count/usage only
after atomic storage acceptance and active-state publication succeed.

Peer callers attach the node's store through optional `stn_peer_workspace.pending`
and serialize the entire synchronization with admission, templates and local
acceptance. A null pointer preserves store-independent peer operation. Successful
adoption reconciles included IDs across the newly active history; retained,
invalid, disconnected, and failed-persistence candidates never consume entries.
No additional workers or runtime peer orchestration are introduced.

Unmatched transactions are normal and unrelated pending entries survive. No
fallible step follows successful persistence before cleanup. Re-adding detached
transactions remains deferred, as do pending persistence and gossip.

Pending-derived candidate construction now consumes the store's canonical ID
enumeration under the same external serialization as admission. Existing
validation and active-history replay checks determine eligibility; canonical
encoding enforces the unchanged block limits. Template/work-context reads never
prune pending entries. Empty eligibility yields no work, and unchanged inputs
yield identical work identity. Explicit development fixtures remain isolated
from normal pending selection. See [MINING_WORK.md](MINING_WORK.md).

Canonical pending submissions are exposed by STNC 0x1005 through the existing
validated admission API. Protocol result values are explicitly mapped and only
accepted/duplicate IDs are returned; no validation internals are exposed by this
operation. STNC 0x1004 now supplies a fixed 16-byte count/usage/capacity summary.
Both use the existing serialized service dispatch boundary and current validated
history. See [RPC.md](RPC.md). No new mining or assembly integration is involved.

The Windows application composes existing storage and byte-transport adapters with the portable mining service. It provides loopback RPC and strict persisted startup; automatic P2P orchestration remains deferred. Explicit configured transaction content produces deterministic work. Submitted solutions use ordinary full fork evaluation and atomic storage application with production SHA-256. Mining origin grants no authority. See [MINING_WORK.md](MINING_WORK.md).

## Phase 10 — Chain ↔ Stratum Integration COMPLETE

Qualified on Windows Release/x64 (2026-09-09). Chain-issued work passes through
actual STNC 0x2002, deterministic STNM jobs, fixture miner results and Stratum's
STNC 0x2003 submission into independent Chain validation and persistence. Work
identity, full target and canonical candidate remain exact except the permitted
64-bit nonce. Invalid/stale results remain rejected; bounded sessions agree.

The final 125-check lifecycle stops Chain and Stratum once, recovers identical
accepted INFO and block bytes with Stratum absent, then starts a fresh coordinator.
Unavailable work yields no placeholder/cached current job. New height-two work
extends the recovered accepted tip with a new Chain work ID. Stratum coordinates
mining; STN Chain remains consensus authority. No production fix was necessary.

All 562 Phase 10 process checks (81/93/115/148/125), 1,544 Phase 9 checks,
1,130,094 Chain C checks, 27 parser checks, two session assertions and 34 build
probes pass. Release/x64 builds have zero warnings/errors. Identity and result
fixtures remain test-only; hardware, production identity, accounting, performance
and other platforms are not qualified. Phase 11 has not started.

## Activated required-target flow — Phase 11 Chunk 2

Accepted branch blocks rebuild a bounded current-window history in chain state.
stn_chain_required_target supplies both candidate validation and mining templates;
stn_target_next remains the sole adjustment calculation. A submitted target must
match before PoW can authorize a state transition. Persistence/reorg paths replay
canonical blocks and reconstruct the window rather than trust a local cache.
Cumulative work remains a checked per-block sum. STNC transports canonical target
bytes unchanged, and Stratum retains its coordinator role. See POW.md for evidence
and the compatibility consequences for earlier fixed-target histories.
## Phase 11 Chunk 3 — 320-bit work and development-history boundary

Authorized consensus rule (2026-09-10): cumulative work is an exact unsigned
320-bit integer encoded as exactly 40 big-endian bytes. Target/hash/work-ID widths
remain 32 bytes. The target domain remains 1 through 2^255-1, and per-block work
remains floor(2^256/(T+1)). At most 2^64 blocks, including height-zero genesis,
each contribute at most 2^255 work, so every supported history fits within 2^319.
No valid history needs saturation, wrapping, truncation or an artificial ceiling.
The generic addition API still rejects out-of-domain 320-bit overflow atomically;
that guard cannot be reached by valid cumulative work over supported heights.

stn_work now holds 40 canonical bytes. Addition visits every byte; target work
is zero-extended into that representation. Fork comparison compares all 40 bytes.
Genesis and target-one successors can accumulate past 2^256 with exact ordering.
Current-window difficulty and the authorized adjustment formula are unchanged.

STNC and STNP use wire version 2, explicitly rejecting version 1 rather than
silently interpreting its narrower work fields. STNC INFO is 184 bytes: work
at 104..143, target at 144..175, status at 176 and count at 180. SUBMIT_WORK success
is 80 bytes: block ID 32, height 8, work 40. STNP STATE is 84 payload bytes:
height 8, tip 32, work 40, count 4. All work fields are unsigned big-endian with
leading zeros required. Wrong lengths are rejected at the relevant message
boundary. Other payload layouts, STNM, target bytes and work-ID semantics remain.

Persistence stores canonical blocks rather than cumulative-work metadata, so its
format is unchanged. Reload and reorganization reconstruct exact 40-byte sums
from accepted history; no serialized difficulty/work cache becomes authoritative.
The Stratum client version and affected response bounds were updated, including
the historical adapter buffer. Mining job/result formats were not redesigned.

Pre-Phase-11 fixed-target chains were explicit development/test evidence, with
no established compatibility guarantee. If their targets violate the activated
rule, revalidation rejects TARGET at the actual boundary. They are not described
as byte-corrupted merely because consensus evolved. No rewriting, bypass,
automatic migration or alternate work ordering is introduced. Development history
that already satisfies current rules remains eligible for normal revalidation.

Qualification uses arithmetic boundaries and scripted block-ID fixtures for
otherwise computationally infeasible target-one blocks. The tests cover full
height range at target one, adjacent minimum targets, high-bit comparison,
addition boundary/failure atomicity, 61 minimum-target blocks, canonical storage
reconstruction, high-work STNC/P2P serialization and invalid lengths/old versions.
Real SHA-256 adjusted branches retain reorg preference after storage reload.
No hardware or production-identity qualification is implied.
## Phase 11 Chunk 4 — final qualification (2026-09-10)

Phase 11 deterministic difficulty adjustment is COMPLETE on Windows Release/x64. Branch ancestry determines validation and mining targets; actual P2P evidence is independently validated before 320-bit work comparison and atomic adoption. Two NTFS node states converge after a bounded partition and reconstruct identical canonical history on reopen. Existing process regressions separately prove Chain/Stratum restart and continued mining. This qualification adds tests only, not production orchestration or consensus design. See ROADMAP.md for exact evidence, scripted-fixture boundaries and deferred qualification.

## Phase 12 Block 3 — peer discovery

Discovery is a local orchestration input layered onto the existing peer session.
An established peer may answer one bounded STNP v2 `GET_PEERS` request when it
advertises capability bit 2. The response contains only configured IPv4 endpoint
evidence. The receiver validates the entire batch, omits its configured self
endpoint, and merges it through the existing deterministic 64-entry candidate
store. Block 2 remains the only outbound connection manager.

Discovery never changes accepted Chain state and never establishes authority,
trust, priority or consensus validity. All later synchronization follows the
existing validation, proof-of-work, target, cumulative-work, fork-choice and
persistence paths. No external bootstrap, recursive gossip, scoring, reputation,
banning or learned-peer database is part of this block. Blocks 1–4 complete the
bounded Phase 12 orchestration qualification; Phase 13 has not started.

## Phase 12 Block 4 — portable orchestration boundary qualification

The orchestration contract is platform-neutral ISO C. Candidate values, discovery
codecs, deterministic ordering, outbound state, failure statuses and caller-
supplied monotonic timestamps are defined in `includes/stn_peer.h` and `src/`.
They contain no Windows handles, Winsock types, filesystem paths, native socket
constants or scheduler assumptions. Native struct layout is never serialized.

Windows conversion, socket readiness, operation deadlines, thread creation and
shutdown remain in `platforms/windows/`. The core policy does not observe socket
arrival order or platform timer representation. Portability checks round-trip the
candidate/discovery contract and enforce the fixed pacing boundary. Linux, ARM and
macOS implementations remain future adapter/qualification work; this block adds
no unrelated backends. Blocks 1–4 complete the bounded Phase 12 orchestration
qualification; Phase 13 has not started.

## Phase 13 Block 1 — RPC framing preflight

The RPC stream boundary now uses portable `stn_rpc_payload_length` to validate
the fixed 24-byte header and declared payload bound before reading body bytes.
It deliberately leaves magic, version, method, shape and capability interpretation
to the existing codec and dispatcher. The Windows application supplies only
transport/session loops; no Windows type enters the portable helper. This is a
bounded resource-hardening baseline, not authentication, authorization or a
consensus change.

## Phase 13 Block 2 — bounded RPC receive sessions

Windows RPC transport transfers now set and clear the existing per-operation
deadline around each fixed header, bounded payload and response. The transport
continues to accept short reads, while a stalled or failed partial frame closes
only that session and frees its private buffers. This policy remains in the
platform adapter; portable parser and dispatch code has no Windows timer or
socket dependency.

## Phase 13 Block 3 — complete-frame session continuity

The Windows RPC loops keep request receive, dispatch, and response completion
as one sequential per-session cycle. The next request is not read until the
prior bounded response transfer finishes. Request and response storage remains
private to each client, with no global parser or result state; the portable
dispatcher and wire definitions are unchanged.

## Phase 13 Block 4 — deterministic protocol errors

Protocol-visible rejection remains in the portable RPC codec and dispatcher:
unsupported methods and complete requests with invalid known shapes produce
bounded canonical STNC responses, while transport failures stay in the Windows
adapter and never become protocol status values. Request identifiers remain
session-local correlation data; no authority or consensus path consumes them.

## Phase 13 Block 5 — RPC lifecycle ownership

RPC session state remains owned by the accepted client record and is released
by the existing reap/stop paths after the worker joins. Socket reuse creates a
new client record and cannot carry request, response, or parser state forward.
The lifecycle qualification stays in the Windows adapter; portable RPC and
consensus layers receive no platform lifecycle types.

## Phase 13 Block 6 — shutdown ordering

The Windows application transitions shutdown by stopping acceptance first,
closing the listener, then joining outbound and RPC worker resources. Active
sessions are interrupted through the adapter before their client records are
freed. No shutdown control or handle type enters portable RPC, chain, or
consensus code.

## Phase 13 Block 7 — concurrent resource bounds

The Windows accept loop continues to allocate session records and workers from
host resources without a protocol-level connection limit. Partial setup closes
the accepted peer and frees its record; completed workers are reclaimed through
the existing reap path. The qualification is adapter/runtime evidence only
and does not introduce a portable client-count policy.

## Phase 13 Block 8 — RPC integration closeout

Blocks 1–7 remain a single layered subsystem: portable framing, validation,
dispatch, status, request association, and ownership policy sit above
platform-specific sockets, workers, deadlines, and shutdown. Final integration
qualification found no Windows coupling in portable `includes/` or `src/`, and
Phase 14 identity/authority work remains outside this phase.

## Phase 14 Block 1 — identity/signature foundation

Portable identity semantics use canonical 32-byte public keys, fixed signing
statement construction, bounded 64-byte signatures, and explicit verification
results. The Ed25519-donna provider is isolated behind the identity API; native
provider formats do not enter protocol data. Signature validity is evidence of
key control only and does not answer authority.

## Phase 14 Block 2 — deterministic authority foundation

Authority consumes the Block 1 public-key identity plus fixed 32-byte versioned
action and context tokens. Canonical evidence is exactly a version byte,
subject identity, action token, and context token. Evaluation returns
AUTHORIZED, UNAUTHORIZED, or MALFORMED by exact comparison; absent evidence is
unauthorized and malformed or unsupported evidence fails closed. The primitive
does not infer authority from signatures, transport, storage, mining, peers, or
STN-LABZ participation, and does not implement organizational policy or
contracts.

## Phase 14 Block 3 — authority-grant provenance

Genesis declares a bounded, sorted authority-root set of canonical public keys.
Only those roots may issue grants, and only for `ISSUE_AUTHORITY_GRANT`. A
canonical 194-byte grant signs exactly one Block 2 evidence record with the
dedicated `STN-CHAIN:AUTHORITY:GRANT:1` domain. Parsing, signature verification,
and root recognition remain separate stages. A valid issuer signature from a
non-root is an INVALID_GRANT and cannot feed Block 2; only VALID_GRANT evidence
may proceed to authority evaluation. No delegation or key lifecycle behavior
is introduced.

## Phase 14 Block 5 — identity rotation

The portable rotation primitive advances one active identity to one canonical
replacement only when the current identity signs the exact old/new pair. It
stores accepted lineage edges, rejects conflicting successors and cycles, and
resolves the current identity deterministically. Genesis-root succession is
rejected and remains a separate protocol decision. Rotation does not migrate
authority grants or alter historical signature verification.

## Phase 14 Block 4 — authority-grant revocation

Complete canonical grants receive a 32-byte SHA-256 identifier. A 129-byte
revocation record signs the issuer and grant identifier under
`STN-CHAIN:AUTHORITY:REVOKE:1`. Only the original genesis-root issuer can
produce a valid revocation for that grant. Accepted-state reconstruction keeps
revocation monotonic for the active history, while reorganization rebuilds the
set from the replacement history. No local blacklist, immediate pending effect,
or persisted trust cache is authoritative.

## Phase 14 Block 6 — signed-action replay protection

Replay protection reuses the existing record nonce rule rather than introducing
a counter, timestamp, UUID, or wallet sequence. A replay identity is exactly
the canonical signer public key followed by its nonzero 32-byte record nonce.
The portable state is caller-owned accepted-history state; consuming an unseen
identity marks it FRESH, while a consumed identity returns REPLAY. Pending and
submitted evidence do not mutate this state. Reorganization and restart rebuild
the state by replaying the applicable accepted history.

## Phase 14 Block 7 — accepted-history lifecycle integration

The reserved transaction classes are now bounded and explicit: type 2 is an
authority grant, type 3 is a grant revocation, and type 4 is an identity
rotation. Type 5 remains rejected. Each lifecycle transaction carries the
existing canonical Phase 14 payload directly. A portable lifecycle state
reconstructor applies validated actions only while replaying accepted block and
transaction order. Fork choice remains independent; replacing accepted history
rebuilds grants, revocations, rotations, and replay state from that history.

## Phase 14 Block 8 — accepted lifecycle state integration

Chain state now carries the accepted lifecycle projection alongside the accepted
chain tip. Candidate validation clones that projection, applies lifecycle
transactions atomically, and publishes it only after block acceptance. The
Genesis Initial Identity Set is a distinct context input, limited to 16 sorted
unique canonical identities; it does not confer authority. Persistence, peer
validation, fork choice, restart, and reorganization remain history-derived.

## Phase 15 Block 1 — production record validation

STNT publication type 1 embeds the existing STNR envelope. Its canonical class
field selects the existing class-1 payload codec (52–1,390 bytes); the outer
envelope cap remains 65,536 payload bytes. A valid evidence digest is a commitment,
not proof that an external artifact exists or has application-specific meaning.

At the genesis-specific activation height in DECISIONS.md, Chain uses
`stn_lifecycle_check_publication` to verify exact canonical signed bytes, the
Phase 14 Ed25519 identity, exact versioned publication authority tokens, accepted
revocation/rotation and replay. It also derives the approved signature-independent
record ID. Transport origin and the old optional publication-validation pointer
cannot change these rules. There is no record freshness window.

Validation applies transaction order to a candidate-local lifecycle clone after
structural, linkage, commitment, target and PoW checks. A failure discards the
clone; success publishes its projection. Grant/replay clone storage grows from
the validated history counts with checked allocation arithmetic instead of the
former 16-entry lifetime ceiling. This does not change the separately authorized
genesis sets or existing revocation/rotation bounds. Snapshot lifetime/reclamation
outside rejected-candidate cleanup retains the existing Phase 14 ownership model;
unbounded-duration production resource qualification is not claimed here.

Pending admission and assembly observe accepted lifecycle state without consuming
it. Assembly rechecks production eligibility so an accepted replay, revocation
or rotation can invalidate an older pending entry. Full candidate validation
remains authoritative, including interactions between transactions in one block.
Accepted record existence is membership in accepted canonical block history;
there is no extra ID index, trust cache, or record lookup service in this block.
