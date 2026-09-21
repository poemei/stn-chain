# Native typed addresses - Phase 18 Chunk 1

This standalone ISO C17 component identifies entities. It grants no authority,
proves no ownership, contains no executable behavior and changes no existing
identity, signature, consensus, persistence, mining or STNC representation.

| Type | Prefix | Complete length | Abbreviated length |
| --- | --- | --- | --- |
| Chain identity/person | `stn0_` | 69 | 16 |
| Contract | `stnc0_` | 70 | 17 |
| Wallet namespace only | `stnw0_` | 70 | 17 |

Lengths exclude the terminating C NUL. The type is part of the address identity;
never discard it when comparing or resolving entities across namespaces.

## Exact derivation and encoding

Identifier = SHA-256 of the exact caller-supplied canonical byte span. No address
prefix, additional domain, length field or implicit string terminator is hashed.
Identical bytes produce identical identifiers across types, with distinct typed
addresses. A byte explicitly included in the span, including NUL, is hashed.
The empty span is valid. All providers enforce a maximum span of UINT32_MAX bytes
to match the existing Windows SHA-256 provider; excess length is rejected before
reading input. No additional cryptographic implementation or dependency is used.

This component does not choose future identity, contract or wallet source schemas.
The caller must supply canonical serialization, not native structs, pointers,
platform-endian integers or ad-hoc JSON. Mapping existing identities to addresses
is not activated by this foundation.

Complete text is prefix plus exactly 64 lowercase hexadecimal digits. There is
no whitespace trimming, case folding, checksum, implicit normalization or alternate
encoding. Zero-valued identifiers are structurally valid; syntax validation does
not prove that an entity exists. Unknown prefixes/types are rejected.

Abbreviation is the prefix followed by exactly five dots and the last six hex
digits. It is non-unique presentation only. Full-address decode/validation rejects
it. Records, protocol operations, authoritative lookup and identity comparisons
must always retain the complete typed address.

## API

`includes/stn_address.h` exposes:

- `stn_address_derive`: canonical bytes to typed 32-byte identifier.
- `stn_address_encode`: typed identifier to complete text.
- `stn_address_decode`: exact complete text to type and 32-byte identifier.
- `stn_address_validate`: complete structural validation.
- `stn_address_abbreviate`: typed identifier to display-only text.

Text output capacities include NUL. Written lengths exclude it. Maximum buffers
are 71 bytes for full text and 18 for abbreviated text. Inputs and outputs must
be disjoint. On failure outputs remain unchanged, except `*written` becomes zero.
No heap allocation, global mutable state or executable contract logic is present.

## Fixed vectors (all three prefixes)

| Source bytes (hex) | SHA-256 identifier |
| --- | --- |
| `616263` | `ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad` |
| `616264` | `a52d159f262b2c6ddb724a61840befc36eb30c88877a4030b65cbe86298449c9` |
| empty | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` |
| `61626300` | `dc1114cd074914bd872cc1f9a23ec910ea2203bc79779ab2e17da25782a624fc` |

For `616263`, identity text is
`stn0_ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad`
and abbreviation is `stn0_.....0015ad`.

## Qualification

Windows: build the existing Release/x64 solution, then run
`build\x64\Release\stn-chain-tests.exe --address`. The normal test runner also
includes the address checks. Linux: `make test-address` compiles the same fixed
vectors against the existing OpenSSL SHA-256 backend; `make` includes the component
in the node. Neither command requires a running Chain or stored history.

Windows results are recorded in CHANGELOG. Linux execution was not available on
the editing host; cross-platform execution parity is not claimed until that run
occurs. ARM qualification remains deferred. No additional phase behavior is added.
