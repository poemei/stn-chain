# Deterministic Development Chain State


Current work representation is 320 bits / 40 canonical big-endian bytes (Phase 11
Chunk 3). This supersedes historical 256-bit work limits below. STNC/STNP version
2 carries widened work fields; block, target, hash and mining-job formats remain.


Current Phase 11 Chunk 2 supersedes fixed-target-only statements below: validation
and mining use the same activated branch-derived required target; persistence
and fork evaluation rebuild its history. Per-block work remains checked and
summed. See [POW.md](POW.md#phase-11-chunk-2--required-target-consensus-activated).

Status: implemented local structural/linkage validation with provider-bound
integrity. Not distributed consensus, semantic record acceptance, persistent
chain storage, or signature verification. The original version-1 profile below
now coexists with the explicit version-3 PoW profile documented in
[POW.md](POW.md). Windows CNG supplies production SHA-256.

## State and context

stn_chain_state stores network_id, genesis_id, tip_id (32 bytes each), uint64
height and timestamp, and a canonical has_tip flag (0 or 1). It has no wire
serialization. It also stores current_target and cumulative_work (32 bytes
each), zero for EMPTY and legacy version 1. It is an in-memory validation
result, not a database schema.

stn_chain_context supplies expected network ID, exact immutable development
genesis bytes and length, the existing stn_hash_provider, and optional
pow_policy. NULL policy explicitly selects legacy v1; a supplied fixed-target
policy requires v3 throughout, including genesis. Context and
provider behavior must remain stable for a validation operation. No global
mutable state, floating point, allocations, or ambient clock is used.

stn_chain_initialize checks that the supplied anchor is structurally a
height-zero block on the expected network, then creates canonical EMPTY state.
It does not accept genesis or claim its commitment is valid. EMPTY has zero
IDs, height and timestamp, has_tip=0, and the configured network identity.
Submitting genesis through candidate validation performs the required checks.

Nonempty prior state must originate from successful validation under the same
context/provider. Consistency checks reject malformed flags, wrong network,
zero tip IDs, wrong genesis ID, and metadata inconsistent with genesis. They
cannot prove that an arbitrary caller-supplied tip/height pair has a valid
historical prefix. Never load serialized metadata and treat it as trusted.
To verify untrusted loaded history, initialize EMPTY and validate the blocks
through the same sequence API used for newly supplied data.

## Block identity

    block_id = SHA256(ASCII("STN-CHAIN:BLOCK:ID:1") || 00 || canonical_header)

The domain is 21 bytes including its terminal zero. canonical_header is the
existing exact 168-byte header. The full block must be structurally valid
before stn_chain_block_id calls the provider. Body commitment binds the ordered
body; candidate validation separately verifies it. This avoids duplicating the
body in the header hash. No wire-format change is made.

Missing provider is UNRESOLVED; error or unsupported provider status fails
closed. Digest output remains unchanged on failure. Candidate validation
reserves the all-zero block ID as invalid to preserve the zero-parent genesis
convention. The Windows production provider and independent digest fixtures
are documented in POW.md. Earlier tests retain explicit noncryptographic
doubles for boundary testing; these are never production fallbacks.

## Candidate stages

stn_chain_validate_candidate returns a report and writes resulting state only
on complete local success. The stages are:

1. Structure: full existing block/header/body/transaction structural checks.
2. Link: prior-state consistency, expected block network, exact genesis for
   EMPTY or expected next height/parent/timestamp for nonempty state.
3. Target: version-3 target must match the explicit fixed policy; NOT_RUN in v1.
4. Body: existing provider-based duplicate-ID and transaction-commitment checks;
   each wrapped record's network must also match the context.
5. Identifier: compute candidate block ID through the provider, reject zero ID.
6. PoW: require block ID <= target in v3; NOT_RUN in legacy v1.
7. Work: add floor(2^256 / (target + 1)) with checked 256-bit arithmetic in v3;
   NOT_RUN in legacy v1. Prior work must match the fixed-target prefix formula.

Each active stage reports PASS, REJECT, UNRESOLVED, ERROR, or NOT_RUN. Stop at
the first non-PASS stage. No required unresolved/failed stage can produce
UNDER_CONTEXT. That final status means local development linkage/integrity
only. It does not mean full consensus acceptance, authorization, signature
verification, or truth of an intelligence report.

No record/intelligence semantics are smuggled into structural block parsing.
The separate stn_validate_intelligence_record layer still owns time/signature/
authority/replay context for records. This increment does not call production
providers for those functions or claim those checks occurred. There is no
coin-balance authority, organizational authority source, fee, gas, or token
policy in chain linkage.

## Link and timestamp rules

For EMPTY state, the candidate must match the configured genesis byte-for-byte,
including its witness/body. A timestamp-generated genesis is never created.
Tests use a fixed non-final anchor with time zero and synthetic data. No final
network genesis or network launch policy has been selected.

For nonempty state:

- Candidate height must equal prior height + 1. UINT64_MAX prior height rejects
  further extension without wrapping.
- previous_hash must equal the prior tip ID.
- Candidate timestamp must be greater than or equal to the previous timestamp.
  Equality and zero are permitted where the ordering rule permits them.
- No upper wall-clock bound, median time, or difficulty/time adjustment exists.

These are explicitly development rules. Record publication-time rules remain
separate. Timestamp and height checks use comparisons and guarded addition.

## Full sequence and atomic behavior

stn_chain_validate_sequence processes 0 through 64 block spans in order from
explicit prior state. For a complete chain, begin EMPTY and include genesis.
For a suffix, use a previously validated prefix state. The batch limit is a
development per-call resource bound, not a lifetime chain-height limit.

A local temporary state advances only after a candidate succeeds. Caller output
is assigned once, after the entire requested sequence succeeds. Candidate and
sequence APIs allow prior==out; failure still leaves it unchanged. Other
input/output overlaps and concurrent mutations are outside the API contract.
Inputs are borrowed; no block contents are copied or allocated by chain code.

An empty sequence validates context and prior-state consistency, returns the
unchanged state, and runs no block stages. Empty history does not imply an
accepted genesis. Batches above 64 are rejected before reading their spans.
Longer history can be validated in bounded chunks by a caller holding temporary
state; callers requiring all-or-nothing validation of an entire longer history
must publish that state only after all chunks succeed.

Duplicate IDs are rejected within each block under the existing integrity
rules. Cross-block duplicate transactions and record nonce replay are not
tracked by this minimal state. Reusing data across blocks is not prevented by
this increment; future stateful semantic/replay validation must enforce that
policy before production acceptance.

## Failure report

The report contains stage statuses, final acceptance, a chain-specific reason,
underlying data/provider status, failing input index, and optional failing height.
Sequence validation stops at the first failure. A candidate failure has index 0
once candidate processing begins; invalid context/arguments use SIZE_MAX.
Sequence failures associated with a block replace the index with its batch index.

A height is reported only when the header itself decoded structurally. A bad
body can therefore report its valid header's height; a truncated/bad header
reports height_available=0 rather than inventing a height. Successful calls
report failing_index=SIZE_MAX and no failing height. Pre-batch state/argument
errors are not misrepresented as a failure of block zero.

## Validation and remaining work

Release/x64 checks include fixed independent genesis bytes, single/multi-block
sequences, all candidate truncations, 64-block batches, first-failure stability,
wrong parent/height/network/version, malformed embedded transactions, duplicate
IDs, commitment mismatches, absent/failing hash providers, unchanged outputs,
loaded-byte copies, exact genesis mismatch, zero identifiers, timestamp equality
and uint64 boundaries. All earlier codec and context tests remain active.

Implemented: explicit local state, sequential linkage, candidate and sequence
validation, provider-bound block IDs, deterministic development timestamps,
atomic caller-state updates, and diagnostic reports.

Unverified: independent cryptographic security review, non-Windows builds,
semantic transaction acceptance, real signatures, and trusted replay/authority
state. Development fixtures and providers are not production mechanisms.

Implemented follow-up: production SHA-256, fixed development targets, PoW
verification, and accumulated work; see POW.md for qualification and limits.

Implemented follow-up: work-based fork evaluation and atomic in-memory plans; see [FORK_CHOICE.md](FORK_CHOICE.md).
Implemented follow-up: bounded snapshot persistence and atomic extension/
reorganization application; see [PERSISTENCE.md](PERSISTENCE.md). Loaded data
is revalidated from EMPTY before activation.

Deferred: difficulty adjustment, orphan handling, peer agreement, propagation,
scalable storage, wallets, RPC, contracts, fees/gas/issuance, mining rewards,
and miner integration. Local validation is not full distributed consensus.
Bounded peer synchronization now supplies untrusted candidate evidence to these
same rules. Explicit recovery keeps validated prefixes inactive until complete
replacement succeeds. See [PEER_PROTOCOL.md](PEER_PROTOCOL.md).

## Mining application

The node now creates deterministic next-block templates and submits solved evidence through existing full-history validation, fork evaluation and atomic persistence. No empty-block exemption, reward transaction, timestamp rule, signature provider or replay rule is introduced. Tip changes from any accepted source invalidate old-parent work. See [MINING_WORK.md](MINING_WORK.md).
