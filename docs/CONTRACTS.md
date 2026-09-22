# STN Chain Contracts

## Deterministic Addressed Agreements

STN Chain contracts are deterministic addressed agreements.

They are not arbitrary executable programs.

The Contract Engine is designed around the same engineering requirements as the
rest of STN Chain:

**Small. Deterministic. Easy to use.**

A contract exists as canonical protocol data interpreted under explicit STN Chain
rules. Independent nodes must reach the same result from the same canonical
contract data and accepted Chain state.

The governing principle remains:

> Evidence does not determine truth. Consensus does.

A contract, signature, participant, application, miner, Stratum server or peer
may provide evidence. None independently determines accepted Chain state.

---

## Contract Model

Conceptually, an STN Chain contract consists of:

    Contract =
        Participants
        + Required Fields
        + Authority
        + Sequence
        + Signatures
        + State

The Contract Engine uses protocol-defined contract objects and deterministic
state transitions rather than a general-purpose execution environment.

Contract objects use the `stnc0_` namespace.

Participants may use `stn0_` identity addresses.

The object classes remain strictly separate:

    Identity != Contract != Wallet

An identity identifies a participant.

A contract address identifies an agreement.

A wallet address identifies a wallet.

None of these identifiers by itself establishes ownership, authentication,
signature validity, authority, economic entitlement or consensus acceptance.

---

## What STN Chain Contracts Are Not

STN Chain does not use contracts as unrestricted applications running inside
consensus.

The Contract Engine does not introduce:

    NO EVM
    NO GENERAL-PURPOSE VM
    NO ARBITRARY BYTECODE
    NO ARBITRARY SCRIPT EXECUTION
    NO GAS

Contract behavior is native, bounded and deterministic.

New contract behavior is introduced through explicit protocol rules rather than
through arbitrary user-supplied executable code.

---

## Contract Addressing

Canonical contract objects use the typed contract namespace:

    stnc0_<64 lowercase hexadecimal characters>

The address identifier is derived through the established STN Chain typed-address
foundation.

For Contract v1, the exact canonical contract bytes are supplied as the address
derivation source.

The address layer deterministically applies SHA-256 to those exact source bytes.

It does not silently add:

- namespace text to the hash input;
- domain text to the hash input;
- an implicit NUL;
- case normalization;
- whitespace normalization;
- salts;
- native C structure representation.

The resulting address identifies that exact canonical contract object.

A contract address does not replace participant identities, signatures,
authority, contract state or consensus validation.

---

## Contract v1 Canonical Representation

Contract v1 uses the canonical `STCT` representation.

All integer fields are encoded in big-endian order.

| Field | Size | Meaning |
| --- | ---: | --- |
| Magic | 4 bytes | `STCT` |
| Version | 2 bytes | Contract format version |
| Type | 2 bytes | Protocol-defined contract type |
| Sequence | 8 bytes | Contract sequence value |
| Created At | 8 bytes | Contract creation timestamp value |
| State | 2 bytes | Protocol-defined contract state |
| Participant Count | 2 bytes | Number of participant entries |
| Terms Length | 4 bytes | Length of canonical terms bytes |
| Participants | N × 34 bytes | Canonical participant entries |
| Terms | Variable | Exact contract terms bytes |

The fixed Contract v1 header is 32 bytes.

Contract v1 is bounded to:

- 32 participants;
- 65,536 bytes of terms.

The structural codec performs no allocation and does not depend on native
structure serialization.

---

## Participants

Each canonical participant entry is exactly 34 bytes:

| Field | Size |
| --- | ---: |
| Identity identifier | 32 bytes |
| Role | 2 bytes |

The identity field is the 32-byte identifier underlying the participant's
canonical `stn0_` identity address.

The role is encoded as an unsigned 16-bit big-endian protocol value.

Canonical participant bytes are not cast onto native C structures. Decoding
retains a borrowed canonical byte span, and individual participants may be
extracted into native participant objects through the Contract API.

This prevents platform alignment, padding and native-endian behavior from
becoming part of the protocol.

---

## Contract Types

Contract v1 currently defines the following protocol type identifiers:

| Type | Value |
| --- | ---: |
| Generic | 1 |
| Work Offer | 2 |
| Contributor Agreement | 3 |
| Policy | 4 |
| Organizational Decision | 5 |
| Service Agreement | 6 |

These values identify protocol-defined contract classes.

They do not create a general-purpose contract language.

---

## Contract States

Contract v1 defines the following state identifiers:

| State | Value |
| --- | ---: |
| Draft | 1 |
| Issued | 2 |
| Review | 3 |
| Approvals | 4 |
| Attestation | 5 |
| Executed | 6 |
| Rejected | 7 |
| Revoked | 8 |
| Closed | 9 |

A conceptual agreement lifecycle may therefore look like:

    DRAFT
    -> ISSUED
    -> REVIEW
    -> APPROVALS
    -> ATTESTATION
    -> EXECUTED

The presence of a state identifier in the canonical format does not by itself
authorize every transition between states.

Permitted transitions are Contract Engine policy and must be evaluated
deterministically.

---

## Participant Roles

Contract v1 defines the following participant roles:

| Role | Value |
| --- | ---: |
| Participant | 1 |
| Issuer | 2 |
| Recipient | 3 |
| Approver | 4 |
| Attestor | 5 |

A role describes a participant's relationship to an agreement.

A role does not by itself prove identity, signature validity or authority.

---

## Contract Actions

The STN Chain contract model anticipates explicit protocol actions such as:

- `CREATE_CONTRACT`;
- `AMEND_CONTRACT`;
- `APPROVE`;
- `REJECT`;
- `EXECUTE`;
- `REVOKE`;
- `CLOSE`.

These actions describe the Contract Engine model.

The current Contract v1 structural foundation does not yet implement accepted
contract actions or state-transition enforcement.

When those semantics are introduced, the Chain must evaluate the applicable
participant identity, signer authority, signature, current state, permitted
transition, sequence and duplicate-action rules under deterministic protocol
rules.

---

## Identity, Signatures and Authority

Identity, signatures and authority remain separate concerns.

Cryptographic validity asks:

> Did this key sign this canonical content?

Authority validation asks:

> Was this signer permitted to perform this action?

A valid signature does not automatically establish authority.

Likewise, inclusion as a contract participant does not automatically grant
authority to perform every action on that contract.

The Contract Engine builds on the existing STN Chain production identity,
Ed25519 signature and scoped-authority foundations rather than duplicating those
systems inside the structural contract codec.

Contract authority may represent multiple independent organizations.

Organizational identity must remain explicit.

---

## Structural Validation

The Contract v1 codec validates the canonical structural representation.

Structural validation includes the defined format boundaries such as:

- `STCT` magic;
- supported Contract v1 version;
- supported contract type;
- supported contract state;
- participant-count bound;
- terms-length bound;
- exact encoded length;
- supported participant roles.

Structural validity does not establish:

- participant ownership;
- signature validity;
- signer authority;
- a permitted state transition;
- legal validity;
- economic entitlement;
- accepted Chain state.

Those properties belong to their respective protocol layers.

---

## Platform Independence

Contract representation is consensus-visible protocol data.

For identical canonical contract input and accepted history, every qualified STN
Chain implementation must interpret the contract identically.

Canonical contract bytes therefore never depend on:

- native structure padding;
- native alignment;
- native integer byte order;
- operating-system data types;
- compiler-specific structure layout.

The Contract v1 codec uses explicit canonical bytes and big-endian integer
encoding.

---

## Current Phase 18 Status

Phase 18 is the active Contract Engine feature phase.

### Chunk 1 — Address Foundation

The typed address foundation is complete and qualified on Windows x64 and Linux
x64 for the recorded scope.

It establishes:

- `stn0_` identity addresses;
- `stnc0_` contract addresses;
- `stnw0_` wallet-namespace addresses;
- deterministic derivation;
- full text encoding and decoding;
- structural validation;
- 32-byte identifier access;
- display-only abbreviation.

The STNC identity-address integration is implemented separately through STNC v2
method `0x0009 DERIVE_ADDRESS`. Its current RPC operation accepts identity
address derivation only.

### Chunk 2 — Canonical Contract Foundation

The Contract v1 structural foundation is implemented.

It establishes:

- canonical `STCT` framing;
- explicit big-endian fields;
- bounded participants;
- bounded terms;
- protocol-defined contract types;
- protocol-defined contract states;
- protocol-defined participant roles;
- portable canonical participant encoding;
- safe participant extraction without native-structure wire casting;
- structural validation;
- deterministic `stnc0_` derivation from canonical Contract v1 bytes.

Chunk 2 implementation and qualification are separate.

No build, runtime or cross-platform qualification result is claimed here for
Chunk 2 until that evidence is recorded.

---

## Not Yet Implemented by Chunk 2

The canonical Contract v1 foundation does not by itself add:

- contract transaction admission;
- accepted-history contract integration;
- STNC contract methods;
- contract persistence semantics;
- contract state-transition enforcement;
- signature application to contract actions;
- authority evaluation for contract actions;
- duplicate approval prevention;
- amendment processing;
- execution processing;
- contract settlement;
- wallet behavior;
- economics.

Those capabilities require later bounded Contract Engine increments.

Phase 19 remains responsible for authoritative economic semantics, including
wallet behavior, balances, miner compensation, issuance, transfers and contract
settlement where those features are ultimately defined.

Staging contract and wallet-address prerequisites does not give them economic
meaning before the Economy protocol defines that meaning.

---

## Implementation Boundary

The current Contract v1 implementation is intentionally small.

The structural codec answers:

> Are these exact bytes a structurally valid canonical Contract v1 object?

The typed address layer answers:

> What deterministic `stnc0_` identifier corresponds to these exact canonical
> contract bytes?

Existing identity and cryptographic foundations answer separate questions about
signatures and authority.

Future Contract Engine state-transition logic will answer whether an action is
permitted against accepted contract state.

Consensus ultimately determines whether resulting evidence becomes accepted
Chain state.

Keeping those responsibilities separate prevents a contract parser from quietly
becoming an execution engine, authority system, wallet or consensus substitute.

---

## Design Rule

STN Chain contracts follow the same rule as the rest of the network:

**Small. Deterministic. Easy to use.**

The Contract Engine will grow through explicit protocol-defined behavior and
bounded, independently qualified increments.

It will not grow by importing general-purpose blockchain execution baggage.
