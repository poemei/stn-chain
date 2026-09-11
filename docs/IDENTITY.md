# Production identity and signatures

Phase 14 Block 1 authorizes PureEd25519 as defined by RFC 8032. The canonical
public key is exactly 32 bytes of compressed Edwards25519 encoding. The Chain
identity is that canonical public-key value; no username, account, machine,
network, filesystem, certificate, or provider handle participates in identity.

The canonical signature is exactly 64 bytes, `R || S`, with canonical point and
scalar encodings. The signing statement is the fixed domain
`STN-CHAIN:RECORD:SIGN:1`, one zero byte, and the canonical unsigned record
bytes. No native structures, text encodings, or prehashed/ctx/ph variants are
accepted.

`stn_identity_verify` returns `STN_IDENTITY_VALID`,
`STN_IDENTITY_INVALID`, or `STN_IDENTITY_MALFORMED`. Length, canonical-encoding,
unsupported, and small-order failures are malformed and fail closed; a valid
encoding with a wrong signature is invalid. Verification proves key control
over the statement only.

**Signature ≠ Authority.** A valid signature does not establish organizational,
contract, governance, mining, or protocol authority. Authority remains a
separate deterministic Phase 14 concern.

The current provider is the isolated public-domain Ed25519-donna implementation
under `src/crypto/ed25519_donna/`. It is compiled behind `stn_identity` and
does not expose provider-native formats. Private keys are not stored or logged.
Other platform providers must reproduce the same canonical acceptance profile.

## Authority boundary

Phase 14 Block 2 supplies a separate deterministic authority primitive. Its
subject is this same canonical public-key identity. Authority evidence grants a
versioned, fixed-width action token in a versioned context token; the evaluator
compares subject, action, and context exactly. Evidence is not accepted because
it arrived through a trusted transport, was persisted, was signed by somebody,
or was produced by mining. **Signature ≠ Authority.** Authority is explicit,
scoped, deterministic, and fail-closed. This block defines no organizational
roles, policy taxonomy, delegation, rotation, revocation, wallet, or contract
lifecycle.

## Authority-grant provenance

Phase 14 Block 3 defines the genesis-declared authority root set. Roots are
canonical 32-byte public keys in ascending order, with no duplicates and a
maximum of 16 entries. Root status grants only `ISSUE_AUTHORITY_GRANT` and is
not inferred from organizational ownership, mining, peers, transport, or local
configuration.

The canonical grant envelope is 194 bytes: version, issuer identity, the
canonical 97-byte Block 2 authority evidence, and one 64-byte signature. Its
signing statement is the domain `STN-CHAIN:AUTHORITY:GRANT:1`, a zero byte, the
grant version, issuer, and evidence. A grant is valid only when its signature
verifies and its issuer is present in the applicable genesis root set. A valid
issuer signature without root authority is an invalid grant. No recursive
delegation, rotation, revocation, expiry, or local root override is defined.
