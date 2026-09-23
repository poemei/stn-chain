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

## STNC Core
Utilizing Contracts **WILL** be implemented in *STNC Core* which will give the user a Graphical User Interface to a Contract.

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

These actions are implemented as canonical Contract action transactions and are
evaluated by the Chain against accepted Contract state.

For each action, the Chain evaluates the applicable participant identity,
Ed25519 signature, scoped signer authority, immutable Contract lineage, current
state, permitted transition, exact next sequence and approval-vote rules under
deterministic protocol rules. Candidate failure does not partially mutate the
previous accepted Contract snapshot.

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

## Contract Identity and Lineage

The exact canonical DRAFT is the immutable Contract origin.

Its canonical bytes determine the Contract's stable `stnc0_` identifier.
Accepted lifecycle progression changes sequence and state, but does not derive a
new Contract address at each state.

A later action belongs to the same Contract lineage only when the immutable
DRAFT fields remain identical. Version, type, creation value, participants and
terms cannot be changed while claiming the original Contract identity.

This separates stable Contract identity from mutable accepted state.

---

## Contract Action Transaction

Contract actions use STNT transaction type 5 with the following canonical
payload fields:

| Field | Size |
| --- | ---: |
| Version | 2 bytes |
| Action | 2 bytes |
| Sequence | 8 bytes |
| Contract Length | 4 bytes |
| Authority Length | 4 bytes |
| Actor | 32 bytes |
| Signature | 64 bytes |
| Canonical Contract | Contract Length |
| Authority Evidence | Authority Length |

All integer fields are big-endian. The fixed action header is 116 bytes.
Current scoped authority evidence is the existing fixed 97-byte Chain authority
representation.

The action signature authenticates the exact canonical current Contract bytes,
canonical actor identity, action and sequence. Authority is evaluated separately
against the immutable canonical DRAFT.

---

## Majority Approval

Approval is not established merely because one participant submits an APPROVE
action.

Eligible voters are the unique participants in the canonical DRAFT whose role is
`APPROVER`.

A duplicate APPROVER identity is invalid. A Contract with no eligible approvers
cannot establish approval authority.

The required approval threshold is strict majority:

    floor(eligible_approvers / 2) + 1

Each eligible identity may contribute one accepted approval vote for a Contract
origin. Duplicate votes are rejected.

An accepted vote advances the Contract sequence. While the vote count remains
below the threshold the Contract is in APPROVALS. Reaching the threshold advances
the Contract to ATTESTATION.

The qualified three-approver fixture therefore requires two accepted votes:

    REVIEW/2
    -> APPROVE #1
    -> APPROVALS/3
    -> APPROVE #2
    -> ATTESTATION/4

Consensus acceptance remains a Chain protocol decision. The vote set is evidence
evaluated under that protocol; no participant independently determines accepted
Chain state.

---

## Accepted State and Recovery

Contract accepted state is owned by the Chain state snapshot.

For each bounded Contract entry the snapshot retains the stable Contract
identifier, independently owned canonical DRAFT evidence, current accepted
Contract state, eligible/required approval counts and accepted vote evidence.

Candidate evaluation is atomic. A failed Contract action or other candidate
failure does not partially mutate the prior accepted Contract snapshot.

Contract state is reconstructible from accepted block history through the
existing Chain history-reconstruction path. It is not loaded from a separate
trusted Contract-state file.

Cross-platform qualification has demonstrated reconstruction through majority
approval. Replaying the accepted history reconstructs the same canonical DRAFT
identity, three eligible approvers, majority threshold two, two accepted votes
and `ATTESTATION/4`.

---

## Qualified Lifecycle

The current qualified primary path is:

    DRAFT/0
    -> CREATE
    -> ISSUED/1
    -> AMEND
    -> REVIEW/2
    -> APPROVE
    -> APPROVALS/3
    -> APPROVE
    -> ATTESTATION/4
    -> EXECUTE
    -> EXECUTED/5

Qualified terminal branches also include:

    REVIEW/2 -> REJECTED/3 -> CLOSED/4
    REVIEW/2 -> REVOKED/3

Authority evidence accepted earlier in canonical candidate transaction order may
authorize a later Contract action. Evidence appearing later in the candidate
cannot retroactively authorize an earlier action.

Stale Contract state/sequence is rejected without changing accepted state.

---

## Current Phase 18 Status

The Contract Engine is implemented and qualified on Windows x64 and Linux x64
for the current bounded scope:

- typed Contract addressing;
- canonical Contract v1 representation;
- Contract action transaction codec and STNT type 5;
- stable canonical-DRAFT identity and lineage;
- Ed25519 action authentication;
- scoped authority evaluation;
- deterministic lifecycle transitions;
- strict-majority approval by eligible APPROVER participants;
- duplicate-vote prevention in the Contract consensus primitive;
- bounded snapshot-owned accepted state;
- candidate failure atomicity and snapshot isolation;
- stale-state rejection;
- canonical same-block authority ordering;
- accepted-history reconstruction through majority approval.

The Contract-capable Chain build was installed and the deployed Chain daemon was
restarted on 2026-09-22.

Phase 18 remains ACTIVE. Application-facing STNC Contract methods are not claimed
complete by this document. ARM qualification is not claimed.

Phase 19 remains responsible for native economic semantics. The Contract Engine
does not introduce balances, issuance, rewards, gas or settlement economics.

No VM, EVM, arbitrary bytecode or arbitrary script execution is introduced.

---

## Implementation Boundary

The Contract Engine remains deliberately bounded.

The canonical codec answers:

> Are these exact bytes a structurally valid canonical Contract v1 object?

The typed address layer answers:

> What stable `stnc0_` identifier corresponds to the exact canonical DRAFT?

The identity/signature layer answers:

> Did this actor authenticate this exact Contract action and sequence?

The authority layer answers:

> Is this actor permitted to perform this action for this Contract origin?

The Contract consensus/state layer answers:

> Is the action valid against current accepted Contract state, and if it is an
> approval, does the accepted vote set reach the deterministic majority threshold?

Chain consensus determines whether that evidence becomes accepted state.

Keeping these responsibilities separate prevents the Contract Engine from
becoming a general-purpose VM, wallet, economic system or substitute authority.

---

## Design Rule

STN Chain contracts follow the same rule as the rest of the network:

**Small. Deterministic. Easy to use.**

The Contract Engine will grow through explicit protocol-defined behavior and
bounded, independently qualified increments.

It will not grow by importing general-purpose blockchain execution baggage.
