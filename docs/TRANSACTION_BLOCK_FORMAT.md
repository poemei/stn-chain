# Transaction and Block Development Format

Status: implemented bounded codecs and structural checks. Development format
version 1 is not a software release version or a finalized consensus protocol.
The header table describes legacy v1. Version 3 activates target and nonce
without changing offsets; see [POW.md](POW.md). Version 2 remains rejected.
All multibyte integers are unsigned, fixed-width, big-endian. Lengths count
bytes. No struct-memory serialization, floating point, allocation, implicit
normalization, or automatic clock reads are used.

## Record relationship and transaction classes

One publication transaction deliberately wraps exactly one existing canonical
STNR record. The record is unchanged and carries network ID, publisher key,
nonce, publication timestamp, payload, and signature. The small wrapper adds
transaction class and framing without duplicating those fields.

| Class | Value | Current behavior |
| --- | --- | --- |
| Intelligence publication | 1 | Wrap exactly one structurally valid STNR record |
| Coin transfer | 2 | Reserved; rejected |
| Contract action | 3 | Reserved; rejected |
| Organization/governance action | 4 | Reserved; rejected |
| Treasury action | 5 | Reserved; rejected |

Other values are rejected. Future classes need defined payloads, identities,
authorization/signing domains, and activation before being accepted. They
must not reinterpret an existing intelligence signature as authorization for
another action. Today version/type are fixed: the outer wrapper adds no new
signing authority. No fees, gas, coin balances, issuance rules, or lifetime
supply ceiling are introduced.

## Transaction wire layout

| Offset | Width | Field | Development rule |
| --- | --- | --- | --- |
| 0 | 4 | magic | STNT, hex 53 54 4e 54 |
| 4 | 2 | version | 1 |
| 6 | 2 | type | 1 only; other classes reserved |
| 8 | 4 | record_length | 180 through 65,716 |
| 12 | record_length | record | Exact existing canonical STNR bytes |

Transaction length must equal 12 + record_length, without trailing bytes.
Minimum is 192 bytes; maximum is 65,728 bytes. Empty transaction content is
prohibited. A minimum-size inner record with no intelligence payload can pass
structural checks but fails the separate intelligence-payload schema. This
preserves the existing distinction between structure and semantics.

stn_transaction_validate_structure performs framing and inner record
structural checks. It does not invoke semantic validation or the validation
context. An eventual publication dispatcher must separately call
stn_validate_intelligence_record on the wrapped record. A transaction ID is
not evidence that those checks passed.

## Transaction identifiers

    transaction_id = SHA256(ASCII("STN-CHAIN:TX:ID:1") || 00 || transaction_bytes)

The domain is 18 bytes including its terminal zero. Hash all canonical
transaction bytes, including the inner signature. This differs deliberately
from the proposed record ID, which excludes the signature. A transaction ID
identifies the transported witness as well as the record. Different witnesses
may produce different transaction IDs for the same logical record; the
record's network/signer/nonce replay policy still applies independently.

stn_hash_provider is the injectable interface for a SHA-256 provider.
It receives domain and bytes as separate spans representing their concatenation.
Windows CNG now supplies production SHA-256; see POW.md for qualification.
Missing provider/function returns UNRESOLVED. Provider failure or unexpected
status returns PROVIDER_ERROR. Digest output stays unchanged unless successful.
Earlier tests use an explicitly noncryptographic deterministic provider, never
a production fallback. The PoW suite adds real SHA-256 known-answer checks.

## Block header wire layout

| Offset | Width | Field | Development rule |
| --- | --- | --- | --- |
| 0 | 4 | magic | STNB, hex 53 54 4e 42 |
| 4 | 2 | version | 1 |
| 6 | 2 | flags | 0; unknown modes rejected |
| 8 | 32 | network_id | Preserved; expected identity checked outside structure |
| 40 | 32 | previous_hash | Zero only at height 0; nonzero at later heights |
| 72 | 8 | height | Explicit position; no ancestry verification |
| 80 | 8 | timestamp | Explicit Unix seconds; no time policy in this codec |
| 88 | 32 | transaction_commitment | Preserved; provider-dependent equality check is separate |
| 120 | 32 | reserved_target | All zero; target encoding/PoW inactive |
| 152 | 8 | reserved_work_nonce | Zero; work validation inactive |
| 160 | 4 | transaction_count | 1 through 16 |
| 164 | 4 | body_length | Exact serialized body length |

Header length is exactly 168 bytes. Reserved work fields do not implement
PoW or difficulty in version 1 and cannot be used to claim valid work there.
Version 3 explicitly activates their meaning as documented in POW.md. Zero timestamps and
zero commitments are structurally representable, not promises of validity.
The previous-hash convention alone does not prove ancestry.

## Block body and limits

The body is an ordered sequence, repeated transaction_count times:

    uint32 transaction_length
    uint8  transaction_bytes[transaction_length]

Every transaction must pass transaction structural validation. Reject an
invalid count, invalid transaction size, a short prefix, malformed embedded
transaction, truncated body, or trailing bytes. Order is preserved.

| Bound | Development value |
| --- | --- |
| Transactions per block | 1 through 16 |
| Maximum transaction | 65,728 bytes |
| Minimum body | 196 bytes (one minimum transaction and its length) |
| Maximum body | 1,051,712 bytes |
| Minimum complete block | 364 bytes |
| Maximum complete block | 1,051,880 bytes |

The header also checks body_length against the count-derived minimum and
maximum before body parsing. Complete length must equal 168 + body_length.
These are conservative development constants, not immutable protocol limits.
Count and per-item limits bound both work and memory. The integrity checker
uses at most 16 32-byte IDs (512 bytes) for duplicate checks; codecs borrow
input spans or write into caller-provided buffers.

## Body commitment and duplicate IDs

    body_commitment = SHA256(ASCII("STN-CHAIN:BLOCK:BODY:1") || 00 || body_bytes)

The domain is 23 bytes including its terminal zero. The body has exact
length-prefix framing, making its transaction order and boundaries explicit.
The count must match parsing; therefore the same canonical body cannot claim
a different count. This is a linear body commitment, not a Merkle tree and
not an inclusion-proof implementation. It commits full transaction bytes,
including witnesses. It is not a header hash or a block identifier.

stn_block_check_integrity first performs structural checks, then obtains
transaction IDs from the supplied provider, rejects equal IDs, and checks the
computed body commitment against the header. Structurally valid duplicate
transactions may be encoded and decoded; the separate integrity check rejects
them when IDs are available. Provider absence remains UNRESOLVED. Provider
collisions also cause duplicate-ID rejection; provider qualification matters.

The header fields are not authenticated by this body commitment. Block-header
hashing and work validation are separate chain/PoW APIs. Signing remains
unimplemented. Matching body commitment is
not consensus acceptance or authorization.

## APIs and ownership

- Transaction: encode, decode, validate_structure, and provider-based ID.
- Header: encode, decode, and validate_structure.
- Body: encode from at most 16 transaction spans, validate_structure, and
  provider-based commitment.
- Complete block: encode, decode, validate_structure, and separate integrity
  checks.

Inputs must designate their stated readable byte ranges; output capacity must
be writable. Borrowed decoded spans require input lifetime and immutability.
Input spans, result objects, written-length storage, and output buffers must
not overlap where encoding/copying occurs. Failure preserves output objects
or bytes; encoders reset written length to zero. No in-place encoding is
promised. Callers must not concurrently mutate input or provider state.

## Deterministic fixtures and qualification

Independent byte fixtures specify a minimum transaction and a one-transaction
block, with fixed timestamps, previous-hash and commitment patterns. Tests
also exercise a fixed height-zero/zero-parent development shape. No runtime
or timestamp-generated genesis exists and no fixture is final network genesis.

Release tests cover exact re-encoding, maximum sizes, every byte truncation of
minimum and maximum transactions/blocks, invalid versions/classes/nonces,
malformed lengths/counts/reserved fields, failure output preservation, provider
status handling, deterministic IDs, witness participation, duplicates,
commitment equality, and nested intelligence -> record -> transaction -> block
round trips. These original hash tests validate the boundary with doubles;
the later PoW suite additionally validates real SHA-256 and independent IDs.

## Unimplemented and stopping point

A subsequent authorized increment implements network agreement, local chain
state, sequential ancestry checks, and provider-bound block IDs; see
[CHAIN_STATE.md](CHAIN_STATE.md). Full transaction semantics, signatures,
final genesis, and distributed consensus remain unimplemented. Production
hashing, fixed-target PoW and cumulative work are now implemented; see POW.md.
A future coordinator must supply semantic/stateful acceptance separately.

Difficulty adjustment, chain selection, reorganizations, mining rewards, coin
economics, wallets, networking, RPC, and smart-contract execution remain
unimplemented. This increment stops with the codecs, structural checks,
provider-bound integrity interfaces, documentation, and regression tests.

## Phase 14 lifecycle transaction mapping

The existing transaction header remains the canonical envelope. Type 2 carries
exactly the 194-byte authority grant, type 3 exactly the 129-byte revocation,
and type 4 exactly the 129-byte identity rotation. Type 5 remains reserved and
invalid. Lifecycle payload bytes are not normalized or re-encoded. Accepted
lifecycle state is reconstructed from persisted block history in canonical
block and transaction order.

## Phase 14 Block 8 accepted-state integration

Transaction types 2, 3, and 4 retain their existing direct canonical payloads.
When such a transaction appears in a candidate block, Chain validation applies
it to a temporary lifecycle state clone in canonical transaction order. The
clone becomes accepted state only with the block; failed validation leaves the
prior state unchanged. The Genesis Initial Identity Set is carried by Chain
context as a separate sorted, duplicate-free set of at most 16 canonical
32-byte identities and is used only for rotation-lineage origin validation.

## Phase 15 Block 1 — publication semantics

Type 1, its 12-byte STNT header, and the embedded STNR bytes are unchanged.
The class is the existing u16 big-endian field at STNR offset 6; only class 1 is
supported. Record ID hashes the exact unsigned STNR bytes under the approved
21-byte domain without a NUL (ENCODING_PROPOSAL.md). Transaction and block
commitments still include the full signature witness; record ID is not a replay ID.

Production publication validation activates by exact genesis lineage and block
height as specified in DECISIONS.md. A successful codec call alone does not
establish signature, authority or acceptance. Production requires the Phase 14
signature and exact matching accepted, unrevoked publication grant, active
identity lineage and fresh signer-plus-nonce replay identity. Pending contents
cannot establish authority. Existing preactivation development bytes remain
subject to their historical structural and active PoW rules.

## Phase 15 Block 2 — returned publication evidence

GET_ACCEPTED_RECORD (STNC 0x0004) returns the exact accepted STNT publication
bytes, including the complete signed STNR witness. It does not re-encode the
record, substitute the transaction ID for record ID, or add an application
envelope. RPC.md specifies the external 76-byte record-ID/block-location/length
prefix. Transaction, record, block and mining formats themselves are unchanged.
Historical query eligibility is stricter than legacy historical acceptance and
is governed by the Operations decision in DECISIONS.md.
