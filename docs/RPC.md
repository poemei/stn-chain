# RPC Core and Deterministic Node Interface

Status: portable codec/dispatch, snapshot queries, mining templates and atomic solved-work submission are implemented, with a Windows loopback TCP runtime. HTTP/JSON, public deployment and authentication remain deferred.

RPC is the supported application/miner integration boundary.
External tools must not read local persistence files as an authoritative interface.
P2P remains node-to-node consensus/evidence exchange.
RPC remains application-to-node interaction.
No RPC client gains consensus authority.
No node becomes authoritative because it exposes RPC.

## Protocol version 1

All multibyte fields are unsigned big-endian. Field order is fixed; no optional
implicit fields, locale-dependent text, floating point or struct serialization.

| Offset | Width | Meaning |
| --- | --- | --- |
| 0 | 4 | STNC magic (distinct from STNP peer framing and STNS storage) |
| 4 | 2 | Version 1 |
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
| 0x0001 INFO | READ | empty | 176-byte validated active chain summary |
| 0x0002 BLOCK_HEIGHT | READ | height u64 | Canonical full block or NOT_FOUND |
| 0x0003 BLOCK_ID | READ | ID 32 | Bounded scan by calculated block ID or NOT_FOUND |
| 0x1000 CHECK_INTELLIGENCE | READ | Whole STNR record, 180..65,716 bytes | 20-byte existing validation report; no admission |
| 0x1001 SUBMIT_INTELLIGENCE | SUBMISSION | Whole STNR record | Existing validation, then UNAVAILABLE for otherwise valid/unresolved submission; invalid evidence REJECTED |
| 0x1002 INTELLIGENCE_ID | READ | Identifier 32 | UNAVAILABLE; standalone record ID/index is not implemented |
| 0x1003 INTELLIGENCE_CURSOR | READ | Snapshot tip 32, block index u32, transaction index u32 | UNAVAILABLE; accepted-intelligence cursor behavior is reserved |
| 0x2000 MINING_CONTEXT | READ | empty | 76-byte tip/target/height context, template availability=1 when configured (0 in snapshot-only adapter) |
| 0x2001 CHECK_WORK_BASE | READ | Base tip 32 | Current mining context if matching; otherwise STALE |
| 0x2002 MINING_TEMPLATE | READ | empty | Parent 32, work ID 32, block length u32, canonical block |
| 0x2003 SUBMIT_WORK | SUBMISSION | Base tip 32, template ID 32, block length u32, block bytes | Validated atomic acceptance: block ID 32, height u64, cumulative work 32; otherwise explicit error |
| 0x3000 ADMIN_CONTROL | ADMIN | empty | UNAVAILABLE; no administrative action is exposed |

SUBMIT_WORK requires the nested block length to match the remaining payload
and the current canonical block size bounds. Template identity and nonce mutation are defined in [MINING_WORK.md](MINING_WORK.md).
Reserved methods never return successful empty placeholders. Full authentication,
administrative actions and actual submission admission remain unimplemented.

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
tip 72..103, cumulative work 104..135, target 136..167, status u32 at 168,
block count u32 at 172. Status 1 means validated under current local development
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
SUBMIT_WORK success is exactly 72 bytes: block ID 32, height u64, work 32.
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
