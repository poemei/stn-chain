# Deterministic Mining Work and Runnable STN Chain Node

STN Chain mining is a consensus-integrated work interface.

Chain constructs deterministic candidate work, exposes that work through STNC, validates returned solved work, and activates accepted blocks through the same consensus path used for all other accepted Chain evidence.

Mining does not confer authority.

Stratum coordinates mining participation but does not own or reproduce Chain consensus.

```text
STN Chain
    ↓
Canonical Mining Work
    ↓
STNC
    ↓
STN-Stratum
    ↓
Miners
    ↓
STN-Stratum
    ↓
STNC Solved Work
    ↓
STN Chain Validation
    ↓
Accepted Chain State
```

The currently qualified production architecture includes:

- deterministic canonical block construction;
- pending-based candidate assembly;
- single-SHA-256 Proof-of-Work;
- branch-derived required target;
- deterministic difficulty adjustment;
- exact unsigned 320-bit cumulative chain work;
- deterministic work identity;
- 64-bit nonce contract;
- STNC mining RPC;
- STN Chain ↔ STN-Stratum integration;
- atomic accepted-block activation;
- persistence and restart reconstruction;
- P2P synchronization and reorganization;
- automatic P2P orchestration;
- hardened concurrent RPC operation;
- production identity, signatures, authority, revocation, rotation, and replay protection;
- lifecycle state integrated into accepted Chain state.

Windows Release/x64 is the current qualified execution environment.

Consensus-visible mining behavior remains platform-independent.

---

# Mining Authority Boundary

Mining supplies Proof-of-Work.

Mining does not determine protocol authority.

The following relationships are controlling:

```text
Mining Work != Authority

Hashrate != Authority

Miner Hardware != Authority

Stratum != Authority

Peer != Authority

Work Submission != Accepted Block

Share Acceptance != Chain Acceptance

Coin Ownership != Consensus Authority
```

ASIC, GPU, CPU, USB-ASIC, ARM, internal miners, solo miners, and pooled miners are subject to the same Chain consensus rules for submitted work.

STN-Stratum must accept mining participants according to protocol correctness and valid participation rather than minimum hashrate or hardware class.

Chain independently validates solved work regardless of its source.

---

# Chain ↔ Stratum Interface Dependency

The mining interface between STN Chain and STN-Stratum is a qualified cross-project contract.

The following are **Stratum-impacting interfaces**:

1. mining RPC envelope;
2. work identity or work-ID derivation;
3. target representation or target semantics;
4. nonce contract, including width, location, byte order, or mutable region;
5. solved-work request or response representation and semantics.

If STN Chain changes any of these:

> **STN-Stratum requires an interface review/update and requalification.**

Chain qualification alone is not sufficient after such a change.

```text
CHAIN MINING INTERFACE CHANGE
            ↓
     STRATUM IMPACT
            ↓
Interface Update / Verification
            ↓
STN-Stratum Requalification
```

This dependency must be identified during development before a changed mining interface is treated as complete.

---

# Runnable Node

The runnable Chain node exposes the binary STNC protocol.

Typical development execution uses:

```text
stn-chain.exe --dev --data stn-chain-dev.stns --rpc-port 18473
```

The normal development RPC endpoint is:

```text
127.0.0.1:18473
```

The protocol is binary STNC.

It is not:

- HTTP;
- JSON-RPC;
- Bitcoin RPC;
- Ethereum RPC;
- Stratum.

STN-Stratum is a separate service and consumes the documented STNC Chain-facing mining interface.

The Chain node remains responsible for consensus validation.

---

# Development Inputs

Explicit development inputs may be supplied using the supported development command-line interface.

Example:

```text
stn-chain.exe --dev --genesis genesis.block --transaction selected.stnt --data chain.stns --rpc-port 18473
```

Inputs are canonical binary Chain evidence rather than text or hexadecimal representations.

Development fixtures are test evidence.

They do not establish:

- production authority;
- monetary value;
- application meaning;
- trusted intelligence;
- privileged transaction selection.

Normal production behavior derives eligible candidate content from validated pending evidence and accepted Chain state.

---

# Persistence

Chain persistence stores canonical accepted Chain evidence.

Existing storage is validated before becoming authoritative.

Corruption does not become an empty or trusted Chain.

Persistence behavior follows the established rule:

```text
Stored Evidence
      ↓
Validation
      ↓
Accepted State
```

not:

```text
Stored Bytes
      ↓
Automatic Authority
```

Accepted block activation and persistence remain atomic according to the qualified storage contract.

A persistence failure must not publish an unpersisted accepted state.

Startup, recovery, adoption, and reorganization reconstruct the authoritative state from validated accepted history.

Phase 14 lifecycle state is also reconstructed from accepted canonical history rather than from an independently authoritative identity or authority database.

---

# Pending Candidate Selection

Normal candidate construction uses validated pending evidence.

Pending entries are enumerated deterministically according to their qualified canonical ordering.

Candidate construction applies the current accepted Chain context and applicable candidate-local validation.

Eligible transactions are selected according to existing protocol bounds and deterministic ordering.

Current candidate construction preserves the established maximum:

```text
16 transactions
```

and the established canonical block-body limits.

A candidate is not accepted state.

```text
Pending
    ↓
Candidate
    ↓
Mining Work
    ↓
Solved Work
    ↓
Consensus Validation
    ↓
Accepted Block
```

Pending entries are not removed merely because:

- a template was requested;
- work was distributed;
- a miner received a job;
- a miner searched a nonce;
- a candidate was constructed.

Accepted pending cleanup occurs only through the qualified accepted-state path.

Failed validation, stale work, failed persistence, and rejected activation must not incorrectly consume unrelated pending evidence.

---

# Production Lifecycle Transactions

Phase 14 integrated identity and authority lifecycle transactions into accepted Chain state.

The qualified transaction mapping includes:

```text
TYPE 1  STN_TX_PUBLICATION
TYPE 2  STN_TX_AUTHORITY_GRANT
TYPE 3  STN_TX_AUTHORITY_REVOKE
TYPE 4  STN_TX_IDENTITY_ROTATE
TYPE 5  RESERVED / REJECTED
```

Lifecycle transactions are not accepted merely because their outer transaction structure is valid.

Candidate-local validation applies the applicable:

- canonical validation;
- signature validation;
- authority validation;
- grant validation;
- revocation validation;
- identity-lineage validation;
- rotation validation;
- replay validation;
- genesis-root rules;
- genesis-initial-identity rules;
- capacity rules.

Rejected candidates cannot mutate authoritative:

- identity state;
- authority state;
- revocation state;
- replay state.

Pending lifecycle evidence remains non-authoritative.

Accepted Chain state owns the corresponding accepted lifecycle state atomically.

---

# Genesis Identity and Authority

The Genesis Authority Root Set and Genesis Initial Identity Set are separate consensus concepts.

```text
Genesis Authority Root
!=
Genesis Initial Identity

Identity
!=
Authority
```

The Genesis Initial Identity Set is bounded to:

```text
16 canonical identities
```

Its identities are canonical, sorted, and unique.

Membership establishes an authorized origin for production identity-rotation lineage.

It does not independently grant authority.

Identity rotation does not implicitly inherit authority grants.

Genesis-root succession remains prohibited unless future consensus law explicitly changes that rule.

---

# Canonical Mining Template

The mining service constructs a canonical candidate block from the current validated Chain state.

Candidate construction derives its relevant consensus values from that state, including:

- network;
- parent/tip;
- next height;
- required target;
- deterministic timestamp behavior;
- canonical transaction body;
- body commitment;
- current validation context.

No wall-clock or process-order dependency may alter consensus-visible candidate construction where the protocol requires deterministic behavior.

The template nonce begins at zero.

Nonce zero is a starting value, not an assertion that the candidate satisfies Proof-of-Work.

---

# Difficulty and Required Target

Mining no longer uses a permanently fixed production target.

Phase 11 introduced deterministic difficulty adjustment.

Consensus parameters are:

```text
TARGET_BLOCK_INTERVAL       = 60 seconds
DIFFICULTY_ADJUSTMENT_WINDOW = 60 blocks
EXPECTED_WINDOW_TIMESPAN    = 3600 seconds
MINIMUM_TIMESPAN            = 900 seconds
MAXIMUM_TIMESPAN            = 14400 seconds
```

Adjustment occurs at qualifying 60-block boundaries.

For adjustment height `H`:

```text
H >= 60
H % 60 == 0
```

Historical measurement uses the applicable accepted branch history.

The required target is derived deterministically from the previous target and clamped observed timespan.

Conceptually:

```text
new_target =
    floor(
        previous_target
        *
        clamped_actual_timespan
        /
        3600
    )
```

with:

```text
900 <= clamped_actual_timespan <= 14400
```

The target must remain within the qualified consensus range.

Missing required target history fails closed.

Non-increasing historical timestamps use the qualified minimum-timespan behavior.

Candidate construction and candidate validation derive the same required target from the applicable accepted branch.

A miner or Stratum server does not choose the Chain target.

---

# Cumulative Chain Work

Fork choice uses exact cumulative Chain work.

Cumulative work is represented as an exact unsigned:

```text
320-bit integer
```

with canonical:

```text
40-byte big-endian representation
```

There is no saturation, wraparound, or artificial cumulative-work ceiling.

Mining work, difficulty, and accepted history feed the existing deterministic fork-choice rules.

Authority records, identity records, application records, miners, peers, and Stratum do not add branch weight.

```text
Valid Proof-of-Work
        ↓
Per-Block Work
        ↓
Exact Cumulative Work
        ↓
Fork Choice
```

---

# Mining Work Envelope

The qualified mining response and solved-work request use the established canonical mining payload:

| Offset | Bytes | Meaning |
| --- | ---: | --- |
| 0 | 32 | Current parent/tip ID |
| 32 | 32 | Deterministic work ID |
| 64 | 4 | Canonical block length, unsigned big-endian |
| 68 | block length | Complete canonical candidate block |

Any proposed change to this envelope is a **Stratum-impacting interface change** and requires STN-Stratum interface review/update and requalification.

---

# Work Identity

The qualified work identity is:

```text
work_id =
    SHA256(
        "STN-CHAIN:WORK:ID:1"
        || 00
        || complete_zero_nonce_block
    )
```

The domain terminator in this qualified work-ID rule is exactly one `0x00` byte.

Work identity includes the complete canonical zero-nonce candidate block, including its body.

It is not merely a header identifier.

Equivalent deterministic candidate context produces the same work identity.

A change to selected candidate content that changes canonical candidate bytes changes work identity.

Work identity is independent of:

- miner;
- Stratum connection;
- process ID;
- filesystem path;
- wall clock;
- thread;
- socket arrival order.

Any change to work-ID derivation is a **Stratum-impacting interface change**.

---

# Nonce Contract

The qualified mining nonce is one unsigned:

```text
64-bit big-endian integer
```

Only canonical block offsets:

```text
152..159
```

may be modified by the miner during nonce search.

Within the mining RPC payload, these correspond to:

```text
220..227
```

given the established envelope.

All other candidate bytes remain immutable.

The nonce is not a Bitcoin-style 32-bit nonce.

Changing:

- nonce width;
- nonce byte order;
- nonce offset;
- mutable byte region;
- nonce interpretation

is a **Stratum-impacting interface change** and requires STN-Stratum requalification.

---

# Target Representation

The required target is represented as a canonical:

```text
32-byte unsigned big-endian value
```

at canonical block offsets:

```text
120..151
```

and therefore mining-payload offsets:

```text
188..219
```

under the established mining envelope.

Do not reverse target bytes.

Do not use Bitcoin compact-target notation.

The target is derived by Chain consensus.

Any change to target representation or mining-facing target semantics is a **Stratum-impacting interface change**.

---

# Proof-of-Work

STN Chain Proof-of-Work remains single SHA-256.

The qualified block-ID / PoW operation uses:

```text
SHA256(
    "STN-CHAIN:BLOCK:ID:1"
    || 00
    || header[0..167]
)
```

The digest is interpreted as an unsigned big-endian integer.

A block satisfies Proof-of-Work when:

```text
H <= T
```

where:

```text
H = interpreted block hash
T = required target
```

STN Chain does not use Bitcoin SHA256d.

SHA256d ASIC compatibility is not implied by the Chain consensus algorithm.

---

# Solved-Work Submission

Solved work is returned through the qualified STNC solved-work path.

The miner changes only the authorized nonce bytes.

On submission, Chain:

1. loads or obtains the current validated accepted state;
2. verifies that the work still refers to the applicable parent;
3. reconstructs or validates the deterministic candidate context;
4. verifies the work identity;
5. verifies immutable candidate bytes;
6. validates the submitted nonce and Proof-of-Work;
7. performs ordinary candidate validation;
8. applies applicable transaction, identity, authority, replay, target, and Chain rules;
9. persists the accepted state atomically;
10. publishes the new accepted Chain state only after successful activation.

A changed accepted tip may make previously issued work stale.

Tip changes caused by:

- local solved work;
- peer extension;
- reorganization;
- recovery/adoption

invalidate obsolete work according to the same consensus-visible rules.

There is no privileged local-miner acceptance route.

---

# Solved-Work Response

Successful solved-work submission returns the qualified solved-work result containing:

```text
block ID
accepted height
cumulative work
```

The established successful payload is:

```text
32-byte block ID
8-byte accepted height
32-byte cumulative-work field in the qualified mining RPC response
```

The mining RPC response contract is distinct from the canonical 40-byte cumulative-work representation used by consensus state where applicable.

Do not infer permission to change the qualified mining response from internal cumulative-work representation changes.

Any change to the solved-work response format or semantics is a **Stratum-impacting interface change** and requires STN-Stratum interface review/update and requalification.

Failure responses continue to use the qualified STNC error behavior.

---

# STN Chain ↔ STN-Stratum

STN-Stratum consumes Chain's mining interface.

The relationship is:

```text
STN Chain
    ↓
STNC Work
    ↓
STN-Stratum
    ↓
STNM / Miner-Facing Job
    ↓
Miner
```

and:

```text
Miner Solution
    ↓
STN-Stratum
    ↓
STNC Solved Work
    ↓
STN Chain
    ↓
Consensus Validation
```

Phase 10 qualified the real Chain ↔ STN-Stratum integration.

Stratum does not:

- inspect Chain persistence;
- construct authoritative Chain history;
- choose the required target;
- determine fork choice;
- bypass candidate validation;
- grant authority;
- declare a block accepted.

Stratum coordinates miners.

Chain decides.

---

# Shares and Chain Acceptance

Miner-facing shares and Chain blocks are different concepts.

A Stratum share may demonstrate valid mining participation without independently becoming an accepted Chain block.

Likewise:

```text
Share Accepted by Stratum
!=
Block Accepted by Chain
```

STN-Stratum's miner-facing participation rules must not redefine Chain Proof-of-Work or consensus acceptance.

All solved blocks submitted to Chain remain subject to ordinary Chain validation.

---

# Internal Mining

Internal or Core-coordinated mining must use the same logical Chain work and solved-work contract.

There is no privileged internal-miner consensus route.

The intended device-selection architecture supports:

```text
USB-ASIC
    ↓ fallback
GPU
    ↓ fallback
CPU
```

with applicable platform qualification.

CPU fallback in STN Core is intended to operate below the established default two-percent duty boundary.

Mining implementation and device detection belong to the relevant miner/Core components rather than Chain consensus.

Regardless of source:

```text
Same Work
+
Same Valid Solution
+
Same Accepted State
=
Same Consensus Result
```

---

# P2P Interaction

P2P synchronization and mining remain separate interfaces.

A peer may provide candidate Chain evidence.

A miner may provide solved work.

Neither source is authoritative.

Automatic P2P orchestration provides bounded deterministic peer discovery, selection, fallback, reconnect, synchronization, and recovery behavior through the qualified networking architecture.

A tip change resulting from accepted P2P history invalidates obsolete mining work just as a locally accepted block does.

No origin-specific mining validity rule exists.

---

# RPC Runtime

STNC is a production Chain interface used by components including STN-Stratum and STN Core.

The qualified RPC runtime includes:

- bounded frame validation;
- payload preflight;
- deterministic request/response association;
- per-frame idle I/O deadlines;
- long-lived healthy sessions;
- concurrent clients;
- connection churn;
- deterministic shutdown;
- malformed-request rejection.

The current Windows implementation uses platform-specific networking behind the platform abstraction.

Consensus-visible STNC semantics must remain identical across future qualified platform backends.

No arbitrary application-defined client ceiling is part of Chain consensus.

Host resources determine practical connection capacity.

---

# Identity, Signatures, and Authority

Phase 14 established production identity and authority.

The qualified model includes:

- canonical 32-byte Ed25519 identities;
- PureEd25519 RFC 8032 verification;
- strict canonical signature validation;
- deterministic authority evidence;
- Genesis Authority Root Set;
- authority grants;
- authority-grant revocation;
- Genesis Initial Identity Set;
- identity/key rotation;
- signed-action replay protection;
- accepted-history lifecycle reconstruction;
- atomic lifecycle state with accepted Chain state.

Mining does not bypass any applicable identity or authority rule for transactions included in a candidate.

A miner's ability to produce valid Proof-of-Work does not authorize invalid transaction evidence.

---

# Production Records

Phase 15 introduces production record integration on top of the qualified Chain and Phase 14 trust foundation.

Production records remain canonical evidence.

Their acceptance must follow:

```text
Canonical Record
      ↓
Signature
      ↓
Authority
      ↓
Replay Validation
      ↓
Candidate Validation
      ↓
Proof-of-Work / Block Validation
      ↓
Accepted Chain State
```

Mining does not interpret application meaning.

Miners search canonical work supplied by Chain.

Application-specific interpretation remains outside the mining interface.

---

# Record Identity

Production record identity is distinct from transaction/witness identity.

The authorized production record identifier is:

```text
record_id =
    SHA256(
        "STN-CHAIN:RECORD:ID:1"
        ||
        canonical_unsigned_record_bytes
    )
```

The domain is the exact ASCII bytes:

```text
STN-CHAIN:RECORD:ID:1
```

The C terminal NUL is excluded.

The signature is excluded from the record-ID input.

Therefore:

```text
Record Identity
!=
Transaction Identity

Record Identity
!=
Witness Identity

Record Identity
!=
Replay Identity
```

Different valid witnesses over identical canonical unsigned record evidence identify the same production record.

This record-ID rule does not alter mining work identity.

---

# Reorganization and Mining

Fork choice remains based on the qualified cumulative-work rules.

When a competing valid branch becomes preferred:

```text
old accepted history
        ↓
common ancestor
        ↓
replacement valid history
        ↓
higher qualifying cumulative work
        ↓
reorganization
        ↓
new accepted state
```

Accepted lifecycle and production-record state follow the selected accepted history.

Mining work bound to the abandoned tip becomes stale.

Authority state does not choose the branch.

Record contents do not choose the branch.

Miner identity does not choose the branch.

```text
Fork Choice
    ↓
Accepted History
    ↓
Identity / Authority / Record State
```

---

# Platform Architecture

Consensus-visible mining behavior is platform-independent.

Portable Chain logic must not depend on:

- Winsock;
- NTFS;
- Windows thread scheduling;
- CNG-native serialized formats;
- registry;
- pointer width;
- struct padding;
- native integer representation;
- filesystem enumeration order;
- locale;
- wall-clock scheduling.

Platform-specific services remain behind defined abstractions.

Windows Release/x64 is currently qualified.

Linux, macOS, ARM64, ARM32, and other targets require their own applicable platform qualification.

Windows qualification does not authorize Windows-specific consensus behavior.

---

# Current Development Boundary

Phases 1 through 14 are complete.

The completed foundation includes:

```text
Phase 1   Canonical Data and Validation
Phase 2   Chain State / PoW / Fork Choice
Phase 3   Persistence and Recovery
Phase 4   P2P Synchronization
Phase 5   Platform Abstraction
Phase 6   RPC and Runnable Node
Phase 7   Mining Templates and Solved Work
Phase 8   Runtime and History Reconciliation
Phase 9   Pending Transactions
Phase 10  Chain ↔ Stratum Integration
Phase 11  Difficulty Adjustment
Phase 12  Automatic P2P Orchestration
Phase 13  RPC Production Hardening
Phase 14  Production Identity / Signatures / Authority
```

Phase 15 is:

```text
Production Record Integration
```

At the beginning of Phase 15, the production record identifier has been authorized as described above.

Phase 15 does not alter the qualified mining contract unless an explicit future consensus decision does so.

---

# Mining Interface Change Control

Before completing any Chain development increment that touches mining, perform the following impact check:

```text
STRATUM IMPACT CHECK
```

Determine whether the change modifies:

```text
[ ] Mining RPC envelope
[ ] Work identity / work-ID derivation
[ ] Target representation
[ ] Target semantics exposed to Stratum
[ ] Nonce width
[ ] Nonce byte order
[ ] Nonce offset / mutable region
[ ] Solved-work request
[ ] Solved-work response
[ ] Solved-work status semantics
```

If every item is unchanged:

```text
STN-Stratum interface requalification:
NOT REQUIRED by mining-interface change
```

If any item changes:

```text
STN-Stratum interface impact:
YES

Required:
1. Update/verify STN-Stratum interface implementation.
2. Requalify STN-Stratum against the changed Chain contract.
3. Do not declare the cross-project mining interface qualified
   based solely on Chain tests.
```

This check is mandatory because Chain and Stratum are independently developed components joined by a qualified protocol boundary.

---

# Qualification Expectations

Mining qualification must preserve evidence for:

- deterministic candidate construction;
- deterministic work identity;
- exact nonce mutability;
- target representation;
- required-target derivation;
- valid and invalid Proof-of-Work;
- stale work;
- malformed solved-work requests;
- immutable-byte enforcement;
- pending selection;
- accepted pending cleanup;
- lifecycle transaction validation;
- atomic persistence;
- restart reconstruction;
- peer extension;
- reorganization;
- recovery/adoption;
- concurrent RPC clients;
- long-lived RPC sessions;
- Chain ↔ Stratum interface compatibility.

Consensus-visible changes require applicable requalification.

Mining-interface changes additionally trigger the STN-Stratum impact rule defined above.

---

# Remaining Mining-Related Scope

Mining-related work still outside the Chain consensus implementation may include, as directed:

- additional miner hardware implementations;
- GPU mining backends;
- USB-ASIC integration;
- additional CPU/ARM miner qualification;
- device discovery;
- miner performance work;
- pool share accounting;
- payout accounting;
- issuance and rewards;
- treasury behavior;
- wallet integration;
- additional operating-system and architecture qualification.

Economics, issuance, rewards, and treasury rules belong to their designated development phase and must not be invented by mining implementation.

Contract execution likewise remains separate from the mining interface.

---

# Controlling Principle

The mining architecture remains intentionally small:

```text
Chain constructs deterministic work.

Stratum distributes work.

Miners search work.

Stratum returns solutions.

Chain validates solutions.

Consensus determines accepted state.
```

No miner, Stratum server, peer, application, wallet, or infrastructure operator becomes authority merely by participating in that process.

> **Small. Deterministic. Easy to Use.**

> **Evidence ≠ Accepted State**

> **Mining ≠ Authority**

> **Stratum Coordinates. Chain Decides.**