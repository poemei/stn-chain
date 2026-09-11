# Development Peer Protocol, Synchronization and Recovery


Current work representation is 320 bits / 40 canonical big-endian bytes (Phase 11
Chunk 3). This supersedes historical 256-bit work limits below. STNC/STNP version
2 carries widened work fields; block, target, hash and mining-job formats remain.

Status: implemented bounded protocol/server handling, synchronous client sync,
explicit recovery coordination and Windows Winsock transport. Windows localhost
integration is tested, including real SHA-256 and NTFS persistence. No public
node service, automatic discovery, RPC or Internet qualification is provided.

## No peer is authoritative

Network identity, advertised height/tip/work and capabilities are claims.
Only locally validated canonical blocks and calculated cumulative work determine
preference. A handshake is compatibility checking, not peer authentication.
An advertised maximum work value cannot make a weaker chain win. Repeated
weaker peers cannot outvote stronger validated evidence. Sequential sync calls
compare against the latest persisted chain under storage exclusion.

This selects the strongest eligible evidence encountered, not a proof that the
global strongest chain was found. Eclipse/Sybil resistance and peer discovery
are not implemented. Existing fixed-target rules remain unchanged; shorter /
higher-work and longer / lower-work relationships are arithmetic qualification
from the fork suite, not eligible same-policy development chains. See
[FORK_CHOICE.md](FORK_CHOICE.md) for that explicit scope resolution.

## Frame and messages

All integers are unsigned big-endian. No raw C struct serialization is used.

| Offset | Width | Rule |
| --- | --- | --- |
| 0 | 4 | STNP magic |
| 4 | 2 | Protocol version 2 only |
| 6 | 2 | Type 1 through 8; unknown types rejected |
| 8 | 4 | Payload byte length, at most 1,051,884 |
| 12 | declared length | Exact payload; no trailing bytes in a decoded frame |

Read the fixed header first and check limits before reading payload. Caller
buffers are fixed/capacity-bounded; span allocation is checked against supplied candidate capacity before using peer counts.
TCP is a stream: exact-transfer adapters handle partial reads/writes. Coalesced
frames stay in the stream for subsequent framed reads. Incomplete frames fail;
there is no resynchronization scan that could reinterpret malformed bytes.

| Type | Payload and semantics |
| --- | --- |
| 1 HELLO | network ID 32, genesis ID 32, capabilities u32=1 (snapshot) or 3 (snapshot + discovery); echoed only on exact compatibility |
| 2 STATE | Empty request; response height u64, tip ID 32, cumulative work 40, block count u32 (84 bytes) |
| 3 GET_HEADERS | start index u32, count u32; count 1..64 within server snapshot |
| 4 HEADERS | start u32, count u32, exactly count canonical 168-byte headers |
| 5 GET_BLOCK | Full-history index u32 |
| 6 BLOCK | Requested index u32 followed by exactly one full canonical block |
| 7 GET_PEERS | Empty request; one bounded discovery request per established session |
| 8 PEERS | Count u16 (0..64), then count IPv4 octets and u16 ports, all big-endian; maximum 386 payload bytes |

HELLO must precede other requests. Capability 1 denotes this exact bounded
snapshot exchange; unknown capability combinations fail rather than being
silently downgraded. STATE count must be nonzero u32 and height=count-1. A peer can
lie consistently about both; subsequent evidence still must validate. No node
identifier is required: connection scope supplies the session. There is no
persistent identity, authentication, encryption or reputation system.

### Phase 12 Block 3 — peer discovery

An established session may issue one `GET_PEERS` request when HELLO capability
bit `2` is present. The response is a bounded `PEERS` payload: a two-byte count
followed by six bytes per endpoint (four IPv4 octets and a two-byte port). The
maximum is 64 entries / 386 payload bytes, matching the local candidate bound.
Zero entries are valid. Discovery is demand-driven and does not trigger recursive
requests or broadcasts.

The serving node emits its configured candidate set in canonical address-then-port
order, omitting an explicitly supplied self endpoint. It never emits raw internal
state or peer metadata. The receiver parses the entire payload into a temporary
candidate set, validates every endpoint using Block 1 rules, sorts independently
of sender order, omits its configured self endpoint, merges through the existing
candidate API, and commits only after the full batch succeeds. Malformed length or
count, invalid address/port, duplicate, and capacity failures leave the destination
unchanged; existing candidates are never evicted.

Discovery supplies connection candidates only. It confers no trust, authority,
priority, authentication, consensus validity, or Chain-state validity. The same
Block 2 outbound lane consumes newly admitted candidates; synchronization evidence
still passes the existing validation, PoW, target, work, fork-choice and persistence
rules. No DNS, seed, multicast, LAN, NAT, HTTP, gossip, scoring, banning or
learned-peer database is implemented. Blocks 1–4 complete the bounded Phase 12
orchestration qualification; Phase 13 is not included.

The server answers from an immutable full snapshot and revalidates it before
serving. It never advertises raw supplied state metadata. Type, range, length
or handshake errors invalidate the session; callers close failed connections.
No ERROR response is needed for this initial fail-and-close protocol.

## Synchronization state machine

1. Explicit caller connects to a configured IPv4 endpoint through transport.
2. Load a complete valid local chain or establish recovery-prefix evidence.
3. Exchange HELLO, request STATE, reject incompatible/malformed claims.
4. Fetch headers in pages of at most 64. They are hints for avoiding downloads, not work
   evidence. Compare their exact bytes with independently validated local
   prefix headers, stopping at the first difference.
5. Reuse full local blocks only for that common prefix; fetch one indexed full
   block at a time for the suffix. Each returned header must equal the earlier
   supplied header. Validate the full chain incrementally from deterministic
   genesis, including reused blocks, and copy it into independent scratch.
6. Under storage exclusion, reread current disk evidence, recompute fork choice,
   and atomically adopt only an eligible preferred candidate (or the narrowly
   defined recovery case below). Commit memory after persistence succeeds.

This is full-block validation with header-assisted reuse, not header-only
consensus. Header claims never supply accepted work. It avoids redownloading
matching local full blocks but still revalidates the complete bounded history.
Divergence uses existing fork/common-ancestor logic at final adoption.
Equal/lower work retains the healthy active chain. Advertised tip/work do not
participate in preference and are not trusted as identity for the fetched chain.
False availability fails when the requested evidence is missing or invalid.

Only one request is outstanding. Returned block index and response type must
match the request; duplicates/unexpected responses fail. Server snapshots must
remain stable for a session. A changing peer snapshot that cannot match its
headers is rejected; a later session can retry explicitly.

## Explicit recovery mode

Strict stn_storage_load behavior is preserved: corruption never activates a
prefix. A caller uses successful strict load for normal startup. On failure,
it keeps the node inactive (EMPTY context state) and explicitly invokes sync /
recovery instead. Recovery results are not accepted state until publication.

stn_storage_recovery_read is a separate evidence API. Recognizable storage-v1
framing is scanned in order with checked lengths; each block is validated from
EMPTY through all current chain stages. Stop at the first malformed or invalid
block. Never scan past damage to salvage a later suffix. If framing/root is
unusable, or storage is missing, the evidence prefix is empty. Unknown framing
is not guessed. Invalid checksums do not authorize bypassing block validation:
checksum-only corruption may leave a complete independently valid prefix.

Provider/hash errors are failures, not proof of corrupt history. Capacity/read
failures do not authorize replacement. No recovered prefix is automatically
activated. It is used to avoid downloads and as a minimum-work evidence floor.

stn_storage_adopt re-reads evidence under exclusion so a concurrent valid update
cannot be blindly overwritten. For a healthy chain, existing greater-work-only
fork choice applies. For corrupt storage, candidate work must exceed prefix
work, except an equal-work candidate with the exact same prefix tip can restore
that identical validated history. Equal-work divergent branches and lower-work
rollback reject. Empty-prefix recovery requires a complete valid chain starting
at the configured exact genesis. No peer can patch bytes or bypass local rules.

The complete rebuilt snapshot is encoded and replaced atomically. Validation,
transport or persistence failure leaves disk and caller state unchanged; an
inactive caller stays inactive. A successful repair removes corrupt suffix data
by replacing the entire snapshot, not by incrementally editing damaged blocks.
The existing staging and Windows durability limitations remain unchanged.
There is no automatic promotion of leftover staging files or backup fallback.

## Bounds and transport failures

- Full history: limited by actual supplied buffer capacity and v1 u32 indexing, not a validation batch.
- Maximum headers per response: 64; maximum blocks per response: one.
- Maximum payload: 1,051,884 bytes; frame: 1,051,896 bytes.
- No lifetime request ceiling; each individual request remains bounded.
- One outstanding request; no peer manager or unbounded peer list.
- No malformed-message tolerance: close the failed connection.
- Fixed caller-owned frame, candidate and storage buffers; no buffer growth.
- Windows session deadline: explicit 1..60,000 milliseconds, including partial
  transport activity. Slow trickle bytes do not reset it. OS time is transport
  scheduling only and never enters consensus validation.

Winsock uses nonblocking sockets and finite select waits, handles short I/O,
checks connection errors and distinguishes timeout, orderly disconnect and I/O
failure. A failed stream must be closed rather than resumed. The caller owns
connection lifetimes and serializes state/workspace access. TCP reset is I/O
failure; orderly close before an exact read completes is DISCONNECTED.

References: [Winsock select](https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-select)
and [Winsock send](https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-send).
The adapter connects to explicit IPv4 addresses; DNS, bootstrap and discovery
are absent. The listener deliberately binds loopback only. No public listener
is opened by the scaffold. Production deployments need separate lifecycle,
peer selection, fairness, authentication/privacy and resource review.

## Tests and deferred work

Tests cover independent framing, truncation/magic/version/type/size failures,
wrong handshake network, fabricated summary work/tip/availability, extension,
stronger/divergent/equal/weaker branches, prefix reuse, invalid PoW/target/
transactions, unexpected responses, timeout/disconnect, hash failures,
persistence failure, corrupt-prefix and root rebuild, checksum-only repair,
failed repair atomicity, multiple weaker peers and all 64 development blocks.

The Windows two-endpoint localhost test exchanges HELLO/STATE/headers/block,
validates real SHA-256 PoW, atomically updates an NTFS file and reloads matching
state. Actual idle accept timeout and refused connection are also tested.
Other transport faults primarily use deterministic mock transports. No Internet
connection, firewall change, power-cut test or non-Windows qualification occurs.

P2P serves consensus evidence exchange only. Public/application RPC, explorer
APIs, wallet/client administration and integration endpoints remain separate
and unimplemented. Also deferred: scalable chain storage/sync, public listening,
automatic discovery/gossip, signatures/authority/replay coordination, mempool,
mining/Stratum, contracts, automatic difficulty, coin/treasury economics, gas/fees.

## Longer-history qualification (2026-09-08)

Synchronization now pages headers while incrementally validating the full history.
Storage uses the complete-history fork evaluator for adoption; the original bounded
fork API remains available for batch callers. Regression tests cover 130 blocks,
64-block prefix reuse with a 66-block catch-up, an 80-block common-prefix reorg,
disconnect/reconnect, weaker subsequent peers, truncation retaining 129 valid
blocks, and a complete rebuild from invalid root framing. All failing sync paths
release allocated view/span ownership without activating partial evidence.

Windows exact-transfer tests retain a partial read across short idle polls. Failed
streams must still be closed. The executable's RPC loop additionally retains frame
progress across header/body and chunk boundaries. Automatic P2P connection/discovery
orchestration is still not part of the runnable RPC server. Longer-history protocol
coordination is tested in the existing deterministic peer harness; existing real
localhost/CNG/NTFS peer coverage is preserved.

## Phase 11 Chunk 4 — final qualification (2026-09-10)

Phase 11 final qualification uses actual loopback Winsock HELLO/STATE/header/block exchange between two independent NTFS-backed node states. Divergent histories cross height 60 and converge by independent target/work validation. Wrong-target evidence remains ineligible despite encoded-target-valid PoW and fabricated maximal work. Separate scripted-hash transport/reconstruction covers sums beyond 256 bits. No protocol or production changes were needed. State teardown/reload is in-process; separate existing process regressions qualify Chain/Stratum restart. Automatic P2P orchestration and Internet operation remain unqualified. See ROADMAP.md.

## Phase 12 Block 1 — local peer candidates

`stn_peer_endpoint` and `stn_peer_candidates` in `stn_peer.h` provide a local,
zero-initialized candidate set. The existing transport accepts explicit IPv4
text and a port; it has no reusable platform-neutral endpoint value type.
The new value uses four IPv4 octets in network order and a numeric uint16_t port.
There is no text parser, socket conversion, IPv6 extension or protocol identifier.
The value is not a wire format: do not serialize structure memory or padding.

`stn_peer_candidate_add` maintains ascending unsigned address-octet order,
then ascending numeric port. Enumerate `entries[0..count)`; callers serialize
access and must not edit entries/count. The bound is 64, matching the project's
small bounded-batch convention in scale but defined independently as local
resource policy. No allocation, eviction, source scoring, clock or randomness
is involved. No removal API is needed for this admission/enumeration block.

Results use existing peer statuses: OK inserts; RETAINED identifies a duplicate,
including at capacity; ARGUMENT rejects null arguments, invalid store invariants
or unusable endpoints; CAPACITY rejects a new endpoint when full. Non-OK leaves
the entire store unchanged. Duplicate identity compares address and port fields,
never pointer values or structure padding. Same address/different port is distinct.

Zero ports, IPv4 0/8, multicast 224/4 and reserved 240/4 (including limited
broadcast) are ineligible. Loopback, private and link-local unicast are allowed.
Subnet-directed broadcasts require interface context and are not inferred here.
Eligibility does not establish reachability, authentication or successful handshake.
Configuration, bootstrap, previous-peer and future discovery adapters can all
submit the same value; no source metadata or source preference is required.
Network/protocol compatibility remains the existing handshake's responsibility.

The implementation is ISO C in the existing peer core, without platform calls,
connection attempts or any access to accepted-state inputs. Tests compare three
insertion orders over a full set, port boundaries, malformed/duplicate/full-store
atomicity, corrupted-store rejection and identical block-validation results before
and after candidate operations. Existing consensus/P2P regressions remain passing.
Windows Release/x64 is the only qualified platform. Automatic orchestration,
retry/reconnect policy, discovery and Block 2 remain outside this foundation.
## Phase 12 Block 2 — automatic outbound management

`stn_peer_outbound` owns one local outbound lane and a private immutable snapshot
of Block 1 candidates. Initialize once while inactive, serialize calls, and close
before destruction. The connector interface owns only this lane's resources.
This is not an application-wide connection limit or a consensus preference.

The first monotonic-time step tries the lowest canonical endpoint. At most one
attempt or session refresh starts per 5,000 milliseconds. A failure closes even
partial resources, clears transport state and advances to the next endpoint,
wrapping after the final candidate. Calls before the interval (including a
backward timestamp) do no work. Empty sets do nothing. No eviction, scoring,
discovery or wall-clock ordering is used; candidates remain unchanged.

A successful connection runs the existing synchronization/handshake path. Only
successful validation/adoption or valid retained evidence marks it connected.
Subsequent refreshes use the same transport without repeating HELLO; decoding,
branch validation, target/PoW checks, work reconstruction and storage fork choice
remain the existing implementation. No advertised state is authoritative.
Any session error disposes the connection; accepted state is not rolled back.
The original fresh-connection `stn_peer_sync` API remains unchanged.

Windows conversion/socket operations remain in the existing backend. An optional
operation deadline bounds automatic outbound I/O, including partial frames;
zero preserves prior inbound idle-I/O behavior. The runtime uses a one-second
socket idle bound and five-second operation budget (one in-flight wait may
extend detection by up to one idle interval). No socket protocol was changed.

The executable accepts repeated `--peer IPv4:PORT` arguments. One optional worker
uses canonical order, private scratch and the existing dispatch lock for sync,
accepted state and pending pruning. RPC may wait for that bounded operation;
its sessions and inbound P2P handling are not replaced. Shutdown joins the worker
before freeing its state. `--once` with peers is rejected. Without peers, startup
behavior is unchanged. No P2P listening/discovery service is added to the RPC app.
The three scratch buffers each use max(8 MiB, startup storage capacity), plus one
existing frame buffer. Exceeding workspace capacity fails explicitly; no unbounded
peer-driven allocation or consensus-visible history limit is introduced.

Qualification: 121 policy checks, 44 actual Winsock checks, and 30 executable
checks. These cover canonical fallback, persistent sessions, loss/reconnect,
wrong evidence, partial cleanup/deadline, retry pacing, independent inbound P2P
connections and established RPC/pending isolation. The executable test serves
canonical genesis evidence and deliberately partial replies; ordinary C network
checks additionally exercise valid adoption. Windows Release/x64 only.
