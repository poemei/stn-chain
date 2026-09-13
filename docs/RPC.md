# RPC Core and Deterministic Node Interface


Current work representation is 320 bits / 40 canonical big-endian bytes (Phase 11
Chunk 3). This supersedes historical 256-bit work limits below. STNC/STNP version
2 carries widened work fields; block, target, hash and mining-job formats remain.

Status: portable codec/dispatch, snapshot queries, mining templates and atomic solved-work submission are implemented, with a Windows loopback TCP runtime. HTTP/JSON, public deployment and authentication remain deferred.

RPC is the supported application/miner integration boundary.
External tools must not read local persistence files as an authoritative interface.
P2P remains node-to-node consensus/evidence exchange.
RPC remains application-to-node interaction.
No RPC client gains consensus authority.
No node becomes authoritative because it exposes RPC.

## Protocol version 2

### Qualified STN-Stratum client

Phase 10 Chunk 1 qualifies the actual current STN-Stratum Windows server and
its client/transport sources for INFO (0x0001), MINING_TEMPLATE (0x2002), and
SUBMIT_WORK (0x2003). The server polls templates; INFO is exposed by the client
API. Stratum's payload bound now matches this specification, and it checks
correlation/framing, method response lengths, nested template length and v3 magic.
It preserves Chain base/work identities, target and immutable candidate bytes.

Accepted, rejected, stale, unavailable and transport failures remain distinct.
Five-second send/receive timeouts bound stalled socket operations; connection
establishment uses OS behavior. One automatic reconnect/retry is allowed for
reads only. A lost mutation reply remains an uncertain transport failure, never
an inferred acceptance or an automatic second submission. The actual server
recovers at its configured endpoint and coexists with other Chain RPC clients.

Reproduce with tools/test-stratum-interface.ps1 after building both projects;
the default Stratum checkout is C:\poes_projects\stn-stratum. Ports 18473 and
18475 must be free; the test never evicts existing services. Temporary Chain
state/logs are isolated and removed. Explicit development work fixtures qualify
the contract; no external miner or production identity qualification is implied.

### Phase 10 — Chain ↔ Stratum Integration COMPLETE

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

### Frame layout

Phase 10 Chunk 4 qualifies failure-state recovery with the actual Stratum server.
Failed/uncertain solved-work calls invalidate Stratum's current-job flags before
further distribution. Recovery consumes a fresh Chain template; identical
authoritative inputs retain identity and changed inputs replace it. Unavailable
work is never substituted with a cached current job. Partial miner frames are
discarded with their sessions. Existing polling/timeouts bound detection;
transport failure never means Chain acceptance. No Chain protocol change.

Phase 10 Chunk 3 qualifies actual miner-result forwarding: Stratum copies the
original 0x2002 candidate and changes only its defined big-endian 64-bit nonce
before 0x2003. The accepted block read back from Chain matches those bytes exactly;
forwarded insufficient PoW is rejected by Chain. No STNC format or consensus
rule changed. Miner-facing malformed/unknown/stale handling is documented in
Stratum PROTOCOL.md and exercised by -MinerResultOnly in the existing integration
script. Chain acceptance, not Stratum forwarding, remains authoritative.

Phase 10 Chunk 2 also qualifies the actual Stratum server's existing miner-job
mapping from 0x2002: its STNM job ID is exactly Chain's work ID, the full target
is copied, and the complete candidate block (including version/network/height/tip
and the initial 64-bit nonce) is unchanged. Three observation-only sessions
receive identical jobs. Changed pending content replaces the job; unavailable
work is not fabricated or resent as current. Stratum's PROTOCOL.md specifies the
84-byte job wrapper and fixed nonce rule. Chain wire and consensus are unchanged.

All multibyte fields are unsigned big-endian. Field order is fixed; no optional
implicit fields, locale-dependent text, floating point or struct serialization.

| Offset | Width | Meaning |
| --- | --- | --- |
| 0 | 4 | STNC magic (distinct from STNP peer framing and STNS storage) |
| 4 | 2 | Version 2 |
| 6 | 2 | Kind: 1 request, 2 response |
| 8 | 2 | Method identifier |
| 10 | 2 | Result code; requests require 0 |
| 12 | 8 | Client correlation ID; opaque unsigned value, not authorization |
| 20 | 4 | Exact payload length |
| 24 | length | Payload; trailing bytes prohibited |

Maximum payload is 1,051,948 bytes (68 + maximum canonical block); maximum
frame is 1,051,972. Minimum frame is 24. The largest supported successful
response is a mining template, 1,051,948 payload bytes, matching the solved-work request bound. Encoded sizes do not depend on pointers, padding or ABI.

Decode checks bounds before accessing variable spans. Known request/response
payload lengths are enforced. Embedded record/block bytes are evidence, whose
validation belongs to the existing validators, not a new RPC consensus rule.
Unknown methods in otherwise valid requests receive METHOD without reaching a
handler. Unsupported versions fail explicitly. Requests with nonzero result
codes and error responses carrying payload bytes reject.

Result codes: 0 OK, 1 INVALID, 2 VERSION, 3 METHOD, 4 FORBIDDEN,
5 UNAVAILABLE, 6 NOT_FOUND, 7 REJECTED, 8 PROVIDER, 9 CAPACITY, 10 STALE.
All error responses have an empty payload. Malformed/version-invalid requests
receive correlation ID/method zero, avoiding echoing unvalidated fields.
For valid requests, responses preserve the method and correlation ID.

Dispatch returns OK when it successfully encodes a response, including an error
response. Callers inspect its result code. Failed encoding leaves written=0.
Output scratch can change during dispatch; only the returned written range is
publishable. Decoder outputs remain unchanged on failure. Service errors cannot
leak partially produced payload bytes into the encoded response.

## Method and capability table

Capabilities are local policy bits: READ=1, SUBMISSION=2, ADMIN=4. A request
contains no capability claim. Unknown policy bits reject. A method requires
its own bit; admin does not implicitly grant read or submission permission.
This classification is not authentication. No remote policy is configured.

| Method | Capability | Request payload | Implemented result |
| --- | --- | --- | --- |
| 0x0001 INFO | READ | empty | 184-byte validated active chain summary |
| 0x0002 BLOCK_HEIGHT | READ | height u64 | Canonical full block or NOT_FOUND |
| 0x0003 BLOCK_ID | READ | ID 32 | Bounded scan by calculated block ID or NOT_FOUND |
| 0x1000 CHECK_INTELLIGENCE | READ | Whole STNR record, 180..65,716 bytes | 20-byte existing validation report; no admission |
| 0x1001 SUBMIT_INTELLIGENCE | SUBMISSION | Whole STNR record | Preserved legacy record-admission/report path in the configured pending service; snapshot-only adapter cannot admit |
| 0x1002 INTELLIGENCE_ID | READ | Identifier 32 | UNAVAILABLE; standalone record ID/index is not implemented |
| 0x1003 INTELLIGENCE_CURSOR | READ | Snapshot tip 32, block index u32, transaction index u32 | UNAVAILABLE; accepted-intelligence cursor behavior is reserved |
| 0x1004 PENDING | READ | empty | 16-byte pending count, entry capacity, byte usage, byte capacity |
| 0x1005 SUBMIT_TRANSACTION | SUBMISSION | Whole canonical STNT transaction, at most 65,728 bytes | 36-byte versioned admission result and canonical transaction ID |
| 0x2000 MINING_CONTEXT | READ | empty | 76-byte tip/target/height context, template availability=1 when configured (0 in snapshot-only adapter) |
| 0x2001 CHECK_WORK_BASE | READ | Base tip 32 | Current mining context if matching; otherwise STALE |
| 0x2002 MINING_TEMPLATE | READ | empty | Parent 32, work ID 32, block length u32, canonical block |
| 0x2003 SUBMIT_WORK | SUBMISSION | Base tip 32, template ID 32, block length u32, block bytes | Validated atomic acceptance: block ID 32, height u64, cumulative work 40; otherwise explicit error |
| 0x3000 ADMIN_CONTROL | ADMIN | empty | UNAVAILABLE; no administrative action is exposed |

SUBMIT_WORK requires the nested block length to match the remaining payload
and the current canonical block size bounds. Template identity and nonce mutation are defined in [MINING_WORK.md](MINING_WORK.md).
Reserved methods never return successful empty placeholders. Production identity
providers and administrative actions remain unimplemented.

## Canonical pending submission

SUBMIT_TRANSACTION (0x1005) calls `stn_pending_admit_transaction`; it never calls
the structural-only store insertion API. Its only request content is canonical
STNT bytes. The frame decoder rejects method payloads larger than 65,728 bytes
before admission. Smaller malformed content, including empty input, receives
the deterministic invalid-submission result. Global STNC bounds are unchanged.

A handled admission response has envelope code OK and exactly 36 payload bytes:
format u16=1 at offset 0, admission result u16 at offset 2, and canonical ID[32]
at offset 4. These explicit wire values are mapped independently from internal
enums:

| Value | Admission result |
| --- | --- |
| 0 | ACCEPTED |
| 1 | DUPLICATE |
| 2 | CAPACITY |
| 3 | INVALID (malformed content, wrong network, or invalid timestamp) |
| 4 | UNSUPPORTED |
| 5 | REPLAY |
| 6 | UNAUTHORIZED (signature or authority rejection) |
| 7 | PROVIDER_UNAVAILABLE (unresolved admission prerequisites) |
| 8 | INTERNAL_ERROR (provider/internal admission error) |

Only ACCEPTED and DUPLICATE include the canonical transaction ID; other results
contain 32 zero bytes. No payload, provider diagnostics, or validation-stage
report is echoed. Envelope errors such as FORBIDDEN, INVALID framing, storage
failure, or insufficient response capacity retain their existing empty-payload
semantics. Response capacity is checked before mutation. Admission rejection
does not prune or otherwise change pending entries or active chain state.

PENDING (0x1004) reuses the existing status operation with four u32 fields:
count at 0, entry capacity (128) at 4, owned byte usage at 8, byte capacity
(262,144) at 12. This replaces the earlier uncommitted ID-list response;
clients must expect exactly 16 bytes. No payload browsing or pagination is
exposed. Existing active-history reconciliation in the service is preserved.
The Windows listener holds its shared dispatch critical section across status,
admission, and chain operations; transport reads/writes remain per client.
The portable service requires the same external serialization.

Production signature/authority providers are not supplied. The actual node
fails closed; successful admission tests use explicitly scripted validation
hooks with real SHA-256. The legacy 0x1001 record/report operation is preserved;
new integrations should use the minimal canonical 0x1005 operation. No mining
template, nonce, assembly, pending persistence, or gossip behavior is added here.

## Node/service boundary and read consistency

RPC dispatch calls a service handler only after complete frame/method-shape
validation and capability checking. The handler receives a borrowed validated
request and bounded response capacity. A malformed request never reaches it.
An impossible service return code, oversized result or wrong success-payload
shape becomes PROVIDER. Services are trusted internal components obliged to
honor capacity and read-only/mutation contracts; they are not client plugins.

The supplied stn_node_service adapter owns no database or global state. The
node supplies an immutable complete history/context for the whole dispatch.
It validates persisted history incrementally before chain
queries and recalculates identifiers/work. Empty or unavailable snapshots
return UNAVAILABLE; no peer count, balance or other untracked status is invented.
This snapshot-only adapter does not mutate state. The configured mining service
uses storage providers and can atomically accept solved blocks.

INFO payload offsets: network 0..31, genesis 32..63, height u64 at 64,
tip 72..103, cumulative work 104..143, target 144..175, status u32 at 176,
block count u32 at 180. Status 1 means validated under current local development
rules, not full distributed consensus or intelligence semantic acceptance.
Height lookup compares the u64 value to the bounded count before size_t conversion.
ID lookup currently scans loaded history; no persistent index/search is claimed.

Each request uses one held snapshot. The node owns lifetime/exclusion. Different
requests can observe different tips; callers needing stable evidence should
use block identifiers and verify returned canonical blocks rather than assume
height lookups across requests describe one immutable chain.

## Intelligence integration

CHECK_INTELLIGENCE calls stn_validate_intelligence_record with the node's
existing explicit validation context. Its expected network must match the
chain context. Response is ten u16 values in fixed order: structure, payload,
network, time, signature, authority, replay, acceptance, envelope error,
payload error. Enum numeric values are the current validation-v1 values.
Later stages remain NOT_RUN when earlier stages stop. OK means the report was
returned, not that the report accepted the intelligence.

Missing production signature/authority/replay providers remain unresolved.
A successful test hook is not production verification. SUBMIT_INTELLIGENCE
uses the same validator; it never queues, persists, publishes or claims block
inclusion. Even fully validated input receives UNAVAILABLE because a durable
admission/mempool path does not exist. Callback/provider error maps to PROVIDER;
invalid input maps to REJECTED. Retrieval/search/cursors for semantically
accepted intelligence remain reserved, not fabricated from structural blocks.

This boundary is for STN API and Sentinel-family integration; applications do
not need filesystem or internal node access. An RPC response itself is not
independent evidence of reporter truth or organizational authorization.

## Mining integration and loopback transport

The configured `stn_mining_service` implements templates and solved submission;
the original immutable `stn_node_service` still returns UNAVAILABLE for those
methods. MINING_CONTEXT and CHECK_WORK_BASE return tip 32, target 32, tip height
u64, and template availability u32 (1 only when construction is available).

MINING_TEMPLATE success uses the same payload shape as SUBMIT_WORK: parent 32,
work ID 32, length u32, full canonical block. Both nested lengths are checked.
SUBMIT_WORK success is exactly 80 bytes: block ID 32, height u64, work 40.
Previously reserved method IDs acquire defined successes without changing RPC
version, framing or maximum payload. The largest success is now a full template,
1,051,948 payload bytes. Older codecs that reject these formerly unavailable
success shapes must be updated before using the mining methods.

See [MINING_WORK.md](MINING_WORK.md) for identity derivation, nonce region,
staleness, normal consensus validation, persistence and full runnable examples.
The portable mining service uses storage providers; OS access remains in adapters.
The snapshot-only query adapter remains immutable. Configured mining queries load
and validate a fresh snapshot per request, and solved submission alone may activate
state through existing atomic storage application.

The executable now supplies a separate loopback-only TCP RPC listener using the
existing Windows byte transport. RPC keeps its own STNC framing, method/capability
rules and port; no P2P handshake is used. Read the 24-byte header, bound the payload
length, then read exactly that many bytes. Responses use the same framing. Socket
chunks are bounded independently of frame size, so maximum RPC frames exceed the
old P2P frame bound without changing P2P. Bad magic/oversized framing closes the
connection; decoded protocol errors return RPC errors. Socket timeout is an idle I/O bound rather than a total-session lifetime. The
Windows runnable node accepts concurrent loopback clients so long-lived Stratum
and STN Core sessions can coexist. Each connection owns its framing buffers and
session. RPC dispatch into the current mining/storage service is deliberately
serialized at the service boundary so concurrent clients cannot race shared
consensus/persistence scratch state. Connection concurrency therefore does not
change consensus ordering or RPC wire semantics.

## Phase 13 Block 1 — framing preflight baseline

The portable `stn_rpc_payload_length` helper is the stream-reader preflight for
STNC. It accepts only an exact 24-byte header and returns its declared payload
length when that value is at most `STN_RPC_MAX_PAYLOAD`. It does not interpret
magic, version, kind, opcode, request shape or capabilities; those existing
dispatch rules retain their protocol-visible error behavior. A caller runs the
preflight before attempting to read the declared payload.

The Windows persistent-client and `--once` loops use this helper after receiving
the fixed header. Bad magic remains the existing immediate close behavior;
unsupported versions and methods still reach dispatch when their bounded payload
can be read and receive the existing deterministic error response. An oversized
declaration is rejected before any payload read. No allocation is based on an
untrusted value beyond the existing fixed frame bound, and no client/session or
consensus state is changed by preflight failure.

Three focused checks cover valid, truncated and maximum-plus-one declarations.
The helper and all parsing remain in portable `src/`/`includes/`; Windows socket
transfer remains in the platform adapter. Authentication, identity, authority,
TLS and API-account behavior remain Phase 14 or later concerns.

Core tests preserve all earlier 237 RPC checks and add mining tests. The real
executable smoke test covers fragmented headers, template retrieval, insufficient
PoW, successful submission, stale work, forbidden administration, bad version,
shutdown and persisted restart. Only Windows Release/x64 has been built/tested.

Deferred: public RPC deployment/authentication, accepted intelligence admission
and indexing, miners/Stratum, wallet/coin/economics, contract runtime, explorer,
mempool, difficulty adjustment, other platform qualification.

## Runtime qualification (2026-09-08)

STNC v1 methods and payloads remain unchanged. Executable tests keep 21 clients
connected, submit work on one while querying another, exercise connection churn
and more than 64 requests, and resume a partial payload after an idle-I/O poll.
A separate idle session remains usable beyond 60 seconds. These are synthetic
Core/Stratum-role clients; actual STN Core and stn-stratumd binaries were not tested.
New-connection socket/allocation/thread failures are isolated from existing
sessions; OS resource exhaustion was not deliberately induced. Node dispatch
remains serialized. Core borrowed scratch never reallocates without ownership.

## Phase 13 Block 2 — bounded incomplete-frame sessions

The Windows stream adapter applies the existing 60-second operation deadline to
each RPC header, payload and response transfer. Short reads are still
reassembled deterministically, but a partial frame that stalls, disconnects,
resets or otherwise fails is terminated and its session resources are released.
The deadline is per session and per frame; it is not shared parser state or a
global connection limit. A separate healthy client can continue to complete
requests while another client is stalled. Portable framing, dispatch,
consensus and authority behavior are unchanged.

## Phase 13 Block 3 — complete-frame session continuity

An established session consumes one complete request before dispatching and
finishing its matching response; only then does it read the next header. Each
iteration uses the session's bounded private request and response buffers, so
payload bytes and status from an earlier request cannot become the next
request. Focused runtime checks use distinct request identifiers on consecutive
requests and verify the returned association while an incomplete or failed
client remains isolated. Existing malformed-complete-request error and close
semantics are unchanged.

## Phase 13 Block 5 — lifecycle churn and reclamation

Each accepted RPC client owns its socket, request/response buffers, and worker
lifetime. Clean disconnects, disconnects before the first request, and failed
request sessions are reclaimed before their client record is reused or freed.
Repeated reconnects cannot inherit prior frame or response state, while an
independent healthy session continues to operate. The lifecycle policy does not
alter STNC status, consensus, persistence, or authority semantics.

## Phase 13 Block 6 — deterministic shutdown

RPC shutdown first marks the node stopping and closes the listening socket, so
no new sessions are accepted. Active client workers are then interrupted and
joined through the existing stop path; their private buffers and peer handles
are reclaimed before node cleanup completes. Shutdown does not alter accepted
chain state or persistence, and transport termination remains distinct from
protocol status.

## Phase 13 Block 7 — sustained concurrent sessions

RPC concurrency remains host-resource scaled rather than protocol-capped. A
new client owns its accepted peer, bounded buffers, and worker record; setup
failure closes and releases that partial session locally. Qualification covers
64 simultaneous clients, repeated complete requests, cleanup, and continued
service of an independent healthy session. No healthy client is evicted to
enforce an artificial global ceiling.

## Phase 13 Block 8 — final integration closeout

The final qualification composes Blocks 1–7 through the existing runtime:
bounded framing, deadlines, complete-frame sequencing, protocol errors,
disconnect/reconnect cleanup, listener-first shutdown, and concurrent clients.
Representative failures remain transport-local, valid completed operations keep
their existing atomicity, and no RPC transport result becomes consensus or
authority evidence. Phase 13 is complete for Windows Release/x64; Phase 14
Production Identity / Signatures / Authority is the next phase.

## Phase 13 Block 4 — deterministic protocol error behavior

Complete, bounded requests that name an unsupported method or violate a known
request shape receive the existing STNC error response deterministically. A
supported request may follow those responses in the same session where the
existing protocol permits continuation; response identifiers remain tied to
the decoded request. Malformed framing and transport failure remain separate:
they do not become application status values and still terminate only the
affected session when required. No status codes or wire fields were added.

## Phase 15 Block 1 — production publication admission

At the next candidate's activated height, SUBMIT_TRANSACTION publication payloads,
SUBMIT_INTELLIGENCE and CHECK_INTELLIGENCE use the accepted Phase 14 lifecycle
projection and corrected versioned publication tokens. The old development hooks
cannot authorize a production record or supply a freshness window. Invalid
signature/authority uses existing unauthorized submission status; malformed data,
replay and local provider failure use their existing result categories.
Admission and read-only checks do not consume accepted replay state. Pending
assembly rechecks production eligibility before constructing candidate evidence.

There are no STNC field, version, method, work-ID, nonce-range or Stratum changes.
Existing block queries continue to expose accepted canonical evidence. Dedicated
record lookup/cursor services remain unavailable. The default development lineage
retains its approved unsigned prefix through height 129; this does not provision
production publishing keys or grants for the executable.
