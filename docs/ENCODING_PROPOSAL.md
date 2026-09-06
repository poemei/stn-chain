# Canonical Record Encoding Proposal

Status: adopted for the experimental structural codec; addresses part of O-002. Not a released
protocol, block format, network frame, or commitment to future compatibility.

Implementation: includes/stn_record.h and src/stn_record.c. Successful codec
return establishes structural validity only. Payload schema, network matching,
signature verification, record IDs, authorization, and replay state are later
validation stages. Synthetic test signatures are deliberately not verified.
The C profile requires size_t to cover at least the 32-bit unsigned range.

## Encoding rules

Use a fixed-order binary envelope with explicitly sized unsigned integers.
All multibyte integers use big-endian order. Lengths count octets. Byte arrays
are literal bytes, with no separators, implicit padding, terminators, native
struct layout, varints, or optional omitted fields.

The decoder must consume exactly one complete record. Reject short input,
extra trailing bytes, unknown versions/types, impossible lengths, and
unsupported suites. Do not normalize or repair malformed signed input.
JSON may be an integration/display format but is not the signing format.

Proposed development limit: 65,536 payload bytes. This is a concrete candidate
for testing, not a throughput claim or final network resource budget.
Changing it later requires an explicit protocol decision.

## Envelope version 1

| Offset | Bytes | Field | Proposed rule |
| --- | --- | --- | --- |
| 0 | 4 | magic | ASCII STNR: 53 54 4e 52 |
| 4 | 2 | envelope_version | 1 |
| 6 | 2 | record_type | 1 = experimental intelligence report; other values rejected for now |
| 8 | 32 | network_id | Must equal validation context's configured network ID |
| 40 | 32 | signer_public_key | Ed25519 public-key encoding |
| 72 | 32 | record_nonce | Signer-generated random bytes; must not be all zero |
| 104 | 8 | issued_at | Unsigned Unix seconds, producer assertion |
| 112 | 4 | payload_length | 0 through 65,536 at envelope level; type rules may require more |
| 116 | N | payload | Exact bytes of the type-specific schema |
| 116 + N | 64 | signature | PureEd25519 signature |

Total size is 180 + N bytes; maximum is 65,716 bytes. Envelope version 1
fixes the signature suite. Algorithm agility requires a specified future
format/activation rather than an unbounded per-record algorithm selector.

## Signed bytes and record identity

Let U be bytes [0, 116 + N), the entire record except the signature.
Let ASCII literals below contain exactly the displayed characters, without
quotes, newline, or trailing NUL. The explicit 00 is a single zero byte.

    signing_message = ASCII("STN-CHAIN:RECORD:SIGN:1") || 00 || U
    signature       = Ed25519.Sign(private_key, signing_message)
    record_id       = SHA256(ASCII("STN-CHAIN:RECORD:ID:1") || 00 || U)

The signing domain is 24 bytes including 00; the ID domain is 22 bytes
including 00. A different domain distinguishes hashing from signing.
All payload bytes and the network, type, signer, nonce, lengths, and times
are bound. Never trust a separately supplied ID instead of recomputing it.

The record ID excludes the signature so a second signature representation
cannot create a different logical record. Signature validation is always
required even when the ID matches an existing entry. A later block-format
proposal must explicitly commit to full validated record contents and
signature witnesses; no Merkle construction is selected here.

## Parsing discipline

Check the fixed header is present before reading payload_length. Check the
limit, then require N to equal input_length minus 180 after proving the
input is at least 180 bytes. Avoid unchecked length additions. Use bounded
views into immutable caller-owned input; do not allocate based on unchecked
lengths. Do not cast unaligned buffers to integer or struct pointers.

An encoder checks all values and output capacity before writing. Failure
returns an explicit error and no successful record length. No network or
filesystem access is involved in encoding or decoding.

issued_at is not an ordering authority and must not be checked against an
implicit local clock. Any accepted timestamp range requires explicit,
deterministic context under the eventual consensus rules.

## Byte-layout examples (not signed records)

    uint16(1) = 00 01
    uint32(256) = 00 00 01 00
    uint64(1) = 00 00 00 00 00 00 00 01
    envelope prefix = 53 54 4e 52 00 01 00 01

These examples exercise layout only. They are not positive signature vectors
and must never be labeled valid network records.

## Alternatives and unresolved scope

Canonical CBOR could provide extensibility but requires defining its exact
accepted subset and rejecting alternative encodings. JSON needs strict
canonicalization and numeric/text rules. Fixed fields are proposed for the
small initial envelope because all signed bytes are easy to specify.

This document does not define block size, block headers, Merkle trees,
transport framing, multi-signature authorization, coin transactions, or VM
bytecode. Future record kinds need their own schemas and activation rules.
See [SIGNED_RECORD_PROPOSAL.md](SIGNED_RECORD_PROPOSAL.md).
