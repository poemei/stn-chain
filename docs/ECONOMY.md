# STNC Economic Model — Chain Development Handoff

**Status:** Economic design direction  
**Date:** September 23, 2026  
**Audience:** STN Chain development  
**Scope:** Native currency, mining compensation, qualifying-share definition, issuance authority, and deterministic economic state

---

## 1. Purpose

This document defines the emerging STNC economic model so Chain development can account for the intended economic state and mining-compensation rules.

This document is a development handoff. It establishes the economic direction described below, but it does not by itself activate issuance on the current Chain. Consensus-visible implementation must be introduced through explicit protocol work, deterministic validation, persistence, reorganization handling, and tests.

The design follows the existing STN Chain authority model:

    Evidence
    -> Canonical Representation
    -> Deterministic Validation
    -> Deterministic Consensus Rules
    -> Accepted State

STN-Stratum may measure and verify qualifying mining participation. STN Chain remains authoritative over accepted issuance, balances, supply, and accepted economic history.

---

## 2. Native Unit

The native currency is:

    STNC

The smallest economic unit is:

    0.01 STNC = 1 economic unit

All consensus-visible accounting uses integer economic units.

No floating-point value is permitted in consensus accounting.

Examples:

    1 unit   = 0.01 STNC
    10 units = 0.10 STNC
    100 units = 1.00 STNC
    101 units = 1.01 STNC

STNC decimal notation is presentation. Consensus state stores and operates on integer units.

---

## 3. Wallet Namespace

Wallet addresses use the existing dedicated namespace:

    stnw0_

Mining identity and wallet identity remain distinct:

    stn0_...
        Mining / Chain identity

    stnw0_...
        Wallet

A mining identity MUST NOT be treated as a wallet merely because it participated in mining.

The protocol relationship between a compensated mining identity and its destination wallet must be explicit and deterministic before economic issuance is activated.

---

## 4. Mining Compensation

STNC issuance is created only through protocol-recognized mining compensation.

Two compensation events are defined.

### 4.1 Qualifying Share Reward

Each accepted qualifying share creates:

    1 economic unit
    0.01 STNC

### 4.2 Accepted Block Reward

Each accepted block solution creates:

    100 economic units
    1.00 STNC

### 4.3 Block Solver Compensation

A proof satisfying the Chain target necessarily also satisfies the easier qualifying-share target.

The block solver therefore receives both compensation events:

    Qualifying share reward      1 unit = 0.01 STNC
    Accepted block reward      100 units = 1.00 STNC
                               ----------------------
    Total                      101 units = 1.01 STNC

The share reward is not replaced by the block reward.

---

## 5. Qualifying Share

A qualifying share is a unique proof-of-work result produced for a miner's current assigned work that satisfies the protocol-defined Share Target.

A share qualifies only when all applicable conditions are satisfied:

1. The proof belongs to the miner's current work assignment.
2. The mining session is associated with a valid registered `stn0_` identity.
3. The nonce is part of the canonical proof for that work.
4. The resulting proof-of-work hash is less than or equal to the current Share Target.
5. The work is not stale when submitted.
6. The proof has not previously been accepted for compensation.
7. The proof passes deterministic Stratum verification.
8. The resulting compensation evidence satisfies the Chain protocol rules required for accepted issuance.

A malformed, stale, duplicate, replayed, unverifiable, or otherwise non-qualifying proof creates no STNC.

---

## 6. Share Target

The initial share factor is:

    SHARE_FACTOR = 10

The Share Target is derived from the current Chain target:

    Share Target = min(MAX_TARGET, Chain Target × SHARE_FACTOR)

A larger proof-of-work target is easier to satisfy. Therefore the Share Target is normally ten times easier than the Chain target.

The calculation MUST use deterministic integer arithmetic.

The implementation MUST detect or avoid arithmetic overflow when multiplying the 256-bit Chain target by `SHARE_FACTOR`.

If the mathematical result would exceed `MAX_TARGET`, the Share Target is exactly `MAX_TARGET`.

The Share Target MUST NOT exceed the maximum proof-of-work target merely to manufacture compensable participation.

No floating-point difficulty calculation is permitted in this rule.

---

## 7. Expected Share Frequency

When the Share Target is exactly ten times the Chain target, the expected relationship is approximately:

    10 qualifying shares
    per
    1 accepted block solution

This is an expected-value relationship, not a per-block quota.

A particular block interval may produce fewer or more than ten qualifying shares because proof-of-work is probabilistic.

The protocol MUST NOT manufacture shares, discard otherwise valid shares, or force a block interval to contain exactly ten shares merely to match the expectation.

When the Share Target is capped by `MAX_TARGET`, the expected number of qualifying shares per block may be lower than ten.

---

## 8. Expected Issuance

At the uncapped 10× share target, expected mining issuance per accepted-block interval is approximately:

    10 qualifying shares × 1 unit    = 10 units = 0.10 STNC
    1 accepted block × 100 units     = 100 units = 1.00 STNC
                                       -----------------------
    Expected total                   = 110 units = 1.10 STNC

This is expected-value analysis, not a fixed issuance quota.

Actual issuance is determined only by protocol-recognized qualifying shares and accepted block solutions present in accepted Chain history.

---

## 9. HASH_PROGRESS Is Not Economic Proof

STN-Stratum `HASH_PROGRESS` exists to report mining progress and support operational hashrate measurement.

It does not create STNC.

A miner reporting that it performed a number of hashes is not sufficient evidence for economic issuance.

Economic share evidence requires an independently reproducible proof:

    Current Work
    + Nonce
    -> Proof-of-Work Hash
    -> hash <= Share Target

This prevents self-reported hashrate from becoming a currency-creation mechanism.

---

## 10. Duplicate and Replay Protection

A qualifying proof may create its share reward exactly once.

Share identity must deterministically bind the proof to the relevant work and mining identity. At minimum, the economic evidence must distinguish:

    Work / Job Identity
    Mining Identity
    Nonce

The final canonical share identifier and serialization belong to the Chain protocol implementation and must be defined before issuance activation.

Reconnecting a miner, resubmitting the same proof, submitting it through another Stratum instance, or replaying previously accepted evidence MUST NOT create additional issuance.

Duplicate prevention must survive restart, synchronization, and reorganization through deterministic accepted Chain state rather than relying solely on transient Stratum memory.

### 10.1 Canonical Share Evidence v2

The canonical accepted share record is self-contained:

    version[1]
    work_id[32]
    mining identity identifier[32]
    nonce[8] big-endian
    canonical zero-nonce mining header[168]

Total:

    241 bytes

The retained mining header is consensus evidence, not telemetry. It allows every
compliant node to reproduce the Work ID, derive the Share Target from the
historical Chain target, insert the submitted nonce, reproduce the proof hash,
and independently verify the qualifying share during normal acceptance,
restart reconstruction, synchronization, and reorganization.

The Work ID is derived from the exact retained 168-byte zero-nonce header. The
header commits to candidate body content through its transaction commitment.

A share record whose Work ID does not reproduce from its retained header, whose
header does not describe the applicable Chain work context, or whose reproduced
proof exceeds the Share Target is invalid economic evidence and creates no
issuance.

---

## 11. Stratum Responsibility

STN-Stratum coordinates mining and verifies qualifying participation evidence.

For qualifying shares, Stratum may:

- distribute current canonical mining work;
- bind a mining session to its presented `stn0_` identity;
- receive candidate share proofs;
- reproduce the proof-of-work hash;
- compare the hash against the protocol-defined Share Target;
- reject malformed, stale, duplicate, or non-qualifying submissions at its coordination boundary;
- associate qualifying evidence with the mining identity that produced it;
- submit the required evidence through the defined STNC interface.

Stratum does not determine balances or accepted supply.

A Stratum statement that a share qualifies is evidence presented to Chain. It is not independently authoritative economic state.

---

## 12. Chain Responsibility

STN Chain determines whether economic evidence becomes accepted state.

Chain must ultimately provide deterministic rules for:

- canonical economic evidence;
- qualifying-share acceptance;
- block-reward acceptance;
- duplicate and replay rejection;
- mining-identity-to-wallet compensation relationships;
- integer balance accounting;
- total accepted supply;
- persistence and restart reconstruction;
- synchronization;
- fork choice and reorganization effects on economic state;
- STNC interfaces required by Stratum and wallets.

Accepted economic state must be reconstructable from accepted Chain history.

A peer, miner, Stratum server, wallet, or application cannot dictate a balance or supply value to Chain.

---

## 13. Reorganization Requirement

Economic state follows accepted Chain history.

If accepted history changes through a valid reorganization, economic state must deterministically reflect the newly accepted history.

This includes, where applicable:

- qualifying-share issuance;
- block rewards;
- balances;
- total supply;
- duplicate/replay state.

Cached or persisted balances are evidence of prior computation, not independent authority.

---

## 14. Fees and Gas

Gas:

    None

Initial transfer fee:

    None

The absence of gas and initial transfer fees does not permit non-mining systems to create STNC.

Native issuance remains limited to protocol-recognized mining compensation unless a future consensus rule explicitly changes the issuance model.

---

## 15. Hardware Neutrality

The qualifying-share rule is based on protocol-valid proof, not hardware class.

The same Share Target applies regardless of whether work originates from:

- CPU;
- GPU;
- USB ASIC;
- dedicated ASIC;
- ARM;
- another supported mining platform.

Hardware capability changes the probability and rate of finding qualifying proofs. It does not change the validity rule for an individual proof.

Hashrate does not grant additional consensus authority.

---

## 16. Economic Invariants

The initial economy is governed by these invariants:

    1 economic unit = 0.01 STNC

    Qualifying share
        -> 1 unit

    Accepted block solution
        -> additional 100 units

    Block-solving qualifying share
        -> 101 units total

    SHARE_FACTOR
        -> 10

    Share Target
        -> min(MAX_TARGET, Chain Target × 10)

    Consensus accounting
        -> integer only

    HASH_PROGRESS
        -> telemetry only
        -> zero issuance

    Gas
        -> none

    Initial transfer fee
        -> none

    Accepted Chain history
        -> authoritative economic history

    Peer evidence
        != accepted balance

    Stratum verification
        != consensus authority

    Mining identity
        != wallet

---

## 17. Required Development Before Economic Activation

This handoff intentionally does not invent the remaining protocol formats.

Before STNC issuance is activated, Chain development must define and test at least:

1. Canonical qualifying-share evidence and identifier.
2. The STNC message path by which qualifying evidence reaches Chain.
3. Deterministic 256-bit Share Target derivation with saturation at `MAX_TARGET`.
4. Deterministic proof verification independent of Stratum trust.
5. Replay and duplicate state.
6. The explicit relationship between compensated `stn0_` mining identity and `stnw0_` wallet destination.
7. Canonical economic record representation.
8. Integer balance and total-supply reconstruction.
9. Persistence and restart behavior.
10. P2P synchronization and independent validation of economic history.
11. Reorganization rollback/reconstruction behavior.
12. Cross-platform deterministic tests.

Every Chain implementation addition must preserve the project's development rule that a new Chain component is introduced with its corresponding `.c`, `.h`, and test `.c`.

---

## 18. Development Boundary

This economic model should shape Phase 19 Chain development, but implementation should remain incremental.

The first implementation step should establish deterministic primitives and tests before balances or spendable issuance are activated.

The intended authority relationship is:

    Miner
        -> produces proof

    STN-Stratum
        -> coordinates work
        -> verifies qualifying participation
        -> presents evidence

    STN Chain
        -> independently validates protocol evidence
        -> applies economic consensus rules
        -> accepts or rejects issuance
        -> reconstructs balances and supply

    Peers
        -> exchange evidence

    Wallets
        -> represent ownership/control of accepted economic state

The governing principle remains:

> Evidence does not determine truth. Consensus does.

For the STNC economy:

> Mining produces economic evidence. Consensus determines accepted issuance.
