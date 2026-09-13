# Signed Intelligence Record Proposal

Status: the development payload schema and validation-hook API remain available.
Phase 15 Block 1 integrates Phase 14 production signature, authority and replay
primitives at the activation boundary in DECISIONS.md. Addresses parts of O-003, O-006, and
O-007. Uses [ENCODING_PROPOSAL.md](ENCODING_PROPOSAL.md). All initial data is
synthetic. This is not the final Sentinel schema or company identity model.

The payload codec is implemented in includes/stn_intelligence.h and
src/stn_intelligence.c, with tests in tests/test_intelligence.c. It validates
schema and exact encoding, not signatures, source ownership, evidence contents,
network identity, authorization, or replay state. A nonzero evidence_digest
is a structurally admissible commitment; its referenced evidence is not read
or hashed by this codec.

At Phase 15 production activation, accepted Chain history supplies authority,
rotation, revocation and replay state. The hook-based time/context API below
remains a development interface; its local clock and optional callbacks do not
control production consensus. Issued and observed times remain signed assertions;
the containing accepted block supplies accepted time. No new Sentinel schema or
external evidence retrieval is implemented.

## Record type 1 payload

Fields occur in the following order. Integers and lengths follow the envelope
encoding rules. There are no optional fields or unrecognized extensions.

| Field | Encoding | Proposed meaning and validation |
| --- | --- | --- |
| schema_version | u16 | 1 |
| observed_at | u64 | Producer-asserted Unix seconds; no implicit local-clock check |
| severity | u8 | Integer 1 through 10 |
| classification_length | u16 | 1 through 64 |
| classification | length bytes | Lowercase ASCII letters, digits, underscore or hyphen; report category |
| source_length | u16 | 1 through 253 |
| source | length bytes | Lowercase ASCII domain-shaped source name as defined below; not proof of ownership |
| subject_length | u16 | 1 through 1024 |
| subject | length bytes | Printable ASCII 0x20 through 0x7e; opaque observed indicator |
| evidence_digest | 32 bytes | SHA-256 of exact externally retained evidence bytes; nonzero |

Payload size is 49 plus the three variable field lengths: 52 through 1,390
bytes. The type-specific parser rejects payloads outside that range and
requires exact field consumption. The larger envelope cap reserves no
permission to exceed this type's limit.

For this development profile, source labels are 1-63 bytes of a-z, 0-9 or
hyphen, separated by single dots. A label cannot begin or end with a hyphen.
No leading/trailing dot, empty label, wildcard, port, or URL is accepted.
Internationalized names require a future explicit mapping decision; validators
do not perform locale-dependent normalization or DNS lookups.

subject is preserved byte-for-byte. It does not yet prescribe IP, URL, file,
or account normalization. Therefore two differently spelled subjects remain
different assertions until a future typed-indicator schema defines equality.
Use reserved synthetic examples such as sensor.example and actor.example
in development fixtures.

The evidence digest proves a retrieved byte sequence matches the reference;
it neither establishes that the evidence is true nor guarantees retrieval.
Evidence location, confidentiality, access, retention, and distribution are
O-008 work. The first fixture supplies evidence separately; it is not a
production evidence distribution service.

## Signing identity versus organizational authority

A public key identifies the signing key, not a company, person, domain owner,
or authorized role by itself. The initial development context explicitly
lists permitted test public keys. This fixture is not the future network
identity registry and must be labeled as a development-only authorization
source.

The full identity/role/delegation design remains O-009. The record does not
currently express original sensor plus relaying API attestations. For the
first exercise, the test producer signs and the adapter relays unchanged
bytes. Whether production uses sensor signatures, API attestations, or both
requires a separate provenance decision. Do not silently re-sign a sensor
report and claim it remains the original sensor's signed record.

Private keys stay outside records, logs, and peer messages. Generate keys
and record nonces with a platform cryptographic entropy provider; no rand(),
time-derived keys, or deterministic production seeds. Fixed public test keys
are acceptable only in clearly labeled fixtures.

## Validation stages

1. Enforce input-size bounds and decode the exact envelope.
2. Parse and validate the entire payload schema without external lookups.
3. Match configured network ID and validate explicit time policy. Supported
   envelope version and record type are checked at stage 1.
4. Invoke signature verification over the exact domain-prefixed unsigned bytes using
   the protocol's qualified verification profile.
5. Check the signer is authorized in the explicit supplied prior state.
6. Check replay state, then return the recomputed record ID and proposed
   state change. Do not mutate accepted state during partial validation.

The host performs durable acceptance only when the encompassing block is
valid. Block validation repeats these checks; API acceptance is not authority
to bypass them. Provider errors, unknown keys, and malformed inputs fail
closed with distinct diagnostic categories.

Stages 1-3 and hook orchestration are implemented. Actual signature verification,
authority lookup, replay storage, record-ID hashing, and durable block acceptance
are not. See [VALIDATION_CONTEXT.md](VALIDATION_CONTEXT.md) for the exact current
statuses and time rules; the host/block description is future architecture.

## Replay and corrections

Propose uniqueness of (network_id, signer_public_key, record_nonce) in the
accepted chain state. An identical resubmission is a duplicate; a different
record using the same tuple is a conflict. Pending-pool duplication is a
local admission matter; canonical-chain uniqueness is a validation rule.
A new nonce deliberately permits a new assertion with otherwise equal data.

Store enough replay state to preserve the rule across restart. Roll it back
with displaced blocks during reorganizations. A record removed from the
canonical chain may become eligible again after revalidation; consumers need
idempotency and confirmation rules before acting on it.

Corrections must be new signed records referencing earlier records, not edits
to accepted bytes. Their type, issuer rights, and supersession behavior are
not implemented by type 1 and remain part of O-006.

## Required qualification cases

| Case | Required result |
| --- | --- |
| Known test key, valid signature, allowed signer, matching network, valid payload | Valid candidate with reproducible ID |
| Altered payload, source, nonce, issued_at, network, or type without re-signing | Reject |
| Valid signature from an unauthorized test key | Reject authorization |
| Same bytes resubmitted | Duplicate, no second state transition |
| Same signer/nonce with altered signed content | Reject conflict once tuple accepted |
| Short header, short signature, trailing bytes, oversized or inconsistent lengths | Reject parsing |
| Unsupported versions/types, zero nonce, invalid source or severity | Reject schema/envelope |
| Crypto provider unavailable or unexpected failure | Reject with provider error |
| Invalid/canonicality-edge Ed25519 encodings | Same result on every qualified provider |
| Restart and branch rollback | Replay state agrees with accepted branch |

Build byte-level known-answer fixtures and RFC 8032 provider tests before
claiming interoperability. The current examples specify behavior but are
not executed cryptographic validation evidence.

## Boundaries still open

The proposal deliberately has one signer and one intelligence payload.
Multi-party contracts, economic authorization, key revocation timing,
maximum lifetime, source attestation chains, encryption, and fee-bearing
submission need explicit formats. Envelope versioning provides a place to
change formats; it does not itself solve compatibility or activation.
