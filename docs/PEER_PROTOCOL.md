# Development Peer Protocol, Synchronization and Recovery

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
| 4 | 2 | Protocol version 1 only |
| 6 | 2 | Type 1 through 6; unknown types rejected |
| 8 | 4 | Payload byte length, at most 1,051,884 |
| 12 | declared length | Exact payload; no trailing bytes in a decoded frame |

Read the fixed header first and check limits before reading payload. Caller
buffers are fixed/capacity-bounded; span allocation is checked against supplied candidate capacity before using peer counts.
TCP is a stream: exact-transfer adapters handle partial reads/writes. Coalesced
frames stay in the stream for subsequent framed reads. Incomplete frames fail;
there is no resynchronization scan that could reinterpret malformed bytes.

| Type | Payload and semantics |
| --- | --- |
| 1 HELLO | network ID 32, genesis ID 32, capabilities u32=1; echoed only on exact compatibility |
| 2 STATE | Empty request; response height u64, tip ID 32, cumulative work 32, block count u32 (76 bytes) |
| 3 GET_HEADERS | start index u32, count u32; count 1..64 within server snapshot |
| 4 HEADERS | start u32, count u32, exactly count canonical 168-byte headers |
| 5 GET_BLOCK | Full-history index u32 |
| 6 BLOCK | Requested index u32 followed by exactly one full canonical block |

HELLO must precede other requests. Capability 1 denotes this exact bounded
snapshot exchange; unknown capability combinations fail rather than being
silently downgraded. STATE count must be nonzero u32 and height=count-1. A peer can
lie consistently about both; subsequent evidence still must validate. No node
identifier is required: connection scope supplies the session. There is no
persistent identity, authentication, encryption or reputation system.

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
