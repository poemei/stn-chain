# Contributing to STN Chain

Thank you for your interest in contributing to STN Chain.

STN Chain is developed around a simple engineering principle:

> **Small. Deterministic. Easy to use.**

Contributions are welcome when they preserve the deterministic behavior, portability, security boundaries, and protocol rules of the Chain.

---

## Development Principles

STN Chain is consensus software.

A change that appears harmless on one platform can become a consensus failure if another platform interprets the same data differently.

The primary rule is:

> **Probable ≠ Determinate.**

Consensus-visible behavior must be established by exact rules and evidence rather than assumptions.

Contributions should favor:

- deterministic behavior;
- explicit validation;
- bounded data structures;
- canonical serialization;
- simple interfaces;
- small and reviewable changes;
- platform-independent protocol behavior;
- reproducible tests;
- failure without partial state mutation.

Avoid unnecessary abstraction or complexity.

If an existing primitive already provides the required behavior, use it rather than creating another interface.

---

## Language

STN Chain is written in **ISO C17**.

New Chain code should remain valid ISO C17 unless a platform-specific implementation requires an operating-system interface behind an established abstraction.

Consensus-visible code must not depend on compiler-specific behavior.

---

## Project Structure

The primary source layout is:

```text
stn-chain/
├── includes/
├── src/
├── platforms/
├── tests/
├── build/
├── docs/
├── build.cmd
└── Makefile
```

Public and shared declarations belong under `includes/`.

Portable implementation belongs under `src/`.

Operating-system-specific implementations belong under `platforms/`.

Tests belong under `tests/`.

---

## New Chain Components

A new Chain component is expected to arrive as a complete, testable unit.

For a new component named `stn_example`, contribute:

```text
includes/stn_example.h
src/stn_example.c
tests/test_stn_example.c
```

A new consensus or Chain primitive should not be introduced without its corresponding test coverage.

Do not add speculative APIs for possible future requirements.

First establish that the existing interfaces cannot satisfy the requirement.

---

## Deterministic Serialization

Consensus-visible structures require canonical serialization.

Do not serialize native C structures directly.

Do not depend on:

- compiler padding;
- pointer size;
- host byte order;
- ABI layout;
- locale;
- floating-point interpretation;
- operating-system-specific structure layout.

Multibyte protocol fields use the byte order defined by the applicable STN Chain format.

Variable-length data must be explicitly bounded.

Decoders must validate bounds before accessing variable data.

Trailing or malformed data must be rejected where the canonical format prohibits it.

The same canonical input must produce the same consensus-visible result on every compliant implementation.

---

## Platform Independence

STN Chain is platform agnostic.

Platform-specific implementations may exist behind abstractions, but they must not change:

- canonical serialization;
- identifiers;
- hashes;
- validation results;
- consensus rules;
- transaction interpretation;
- Contract behavior;
- Proof-of-Work interpretation;
- fork choice;
- accepted-state derivation.

Windows, Linux, macOS, ARM, and other supported environments must agree on consensus-visible behavior.

A feature tested on one platform should not automatically be described as qualified on another.

Qualification claims require evidence from the platform being claimed.

---

## Windows Development

Windows uses the command-line build system.

Run:

```text
build.cmd
```

The supported workflow uses the MSVC command-line compiler and Windows SDK.

Visual Studio solution or project files are not required and should not be introduced as a dependency.

When applicable, focused tests can be run through their existing `build.cmd` targets.

A successful contribution should compile cleanly under the warning/error policy defined by the current build.

---

## Linux Development

Linux uses the repository Makefile.

Build with:

```text
make
```

Use the applicable test targets defined by the current Makefile.

Platform-specific Linux implementation belongs under the established Linux platform boundary rather than being mixed into consensus-visible portable code.

---

## Tests

Tests are part of the implementation.

New behavior should include:

- valid cases;
- malformed input;
- boundary conditions;
- deterministic vectors where appropriate;
- rejection cases;
- state-preservation checks after failure;
- reconstruction/recovery checks when accepted state is affected.

Consensus changes should demonstrate that identical canonical inputs produce identical expected outputs.

When a change affects multiple supported platforms, qualification should compare results across those platforms.

Do not describe a test as passing unless it was actually executed.

Do not describe a platform as qualified based solely on another platform's result.

---

## Consensus Changes

Consensus changes require particular care.

Peers provide evidence.

They do not provide authority.

Each compliant Chain node independently validates evidence according to the protocol and derives accepted state through the established consensus rules.

A contribution must not create a shortcut that allows:

- a peer;
- RPC client;
- Stratum server;
- miner;
- website;
- application;
- administrator;
- external service

to become consensus authority.

Consensus-visible changes require explicit protocol definition and deterministic qualification.

---

## Identity and Authority

STN Chain keeps identity, authentication, authority, and consensus separate.

The governing distinction is:

```text
Address identifies.
Signature authenticates.
Authority permits.
Consensus accepts.
```

Do not collapse these responsibilities into a single mechanism.

Possession of an identity does not automatically grant authority.

A valid signature does not automatically make an action acceptable to consensus.

---

## Typed Addresses

STN Chain uses typed address namespaces.

Current namespaces include:

```text
stn0_   Identity/person
stnc0_  Contract
stnw0_  Wallet
```

Address derivation must follow the canonical Chain rules.

Do not introduce hidden normalization, salts, namespaces, case conversion, whitespace modification, or other transformations unless explicitly defined by the protocol.

---

## Contracts

STN Chain Contracts are deterministic protocol objects.

They are not executable programs.

The Contract Engine does not introduce:

- an EVM;
- a general-purpose VM;
- arbitrary bytecode;
- arbitrary scripts;
- gas.

The canonical DRAFT is the stable Contract origin.

Its canonical identity remains associated with subsequent accepted Contract state.

Contract lifecycle, signatures, authority, sequencing, participant roles, approval voting, and accepted state must remain deterministic.

Contract approval uses the protocol-defined majority rules rather than external organizational assumptions.

Application presentation and human interaction belong outside Chain consensus.

---

## Mining

Mining proves work for Chain consensus.

Stratum coordinates miners but does not become Chain authority.

Miner acceptance should depend on protocol correctness rather than hardware class or minimum hashing power.

CPU, GPU, USB-ASIC, ASIC, ARM, and other implementations must ultimately produce protocol-compatible results.

Mining implementation must not silently change consensus-visible work representation or validation.

---

## Economics

Economic behavior is protocol behavior.

Do not infer economic rules from Bitcoin, Ethereum, or another blockchain.

In particular, do not assume:

- a supply ceiling;
- a halving schedule;
- a block reward;
- a balance representation;
- a transfer model;
- a fee model;
- a gas model;
- a payout model.

Economic behavior must be explicitly defined by STN Chain protocol development before implementation.

---

## STNC

STNC is the application-facing Chain interface.

Applications should use defined Chain interfaces rather than treating persistence files as an authoritative API.

STNC clients do not gain consensus authority.

Changes to STNC framing, methods, payloads, result codes, or canonical representations must preserve the deterministic protocol boundary and include appropriate tests.

Do not invent or reuse method identifiers without checking the current protocol definitions.

---

## P2P

P2P is the node-to-node evidence and synchronization interface.

Remote peers provide evidence that the local Chain independently validates.

A peer's reported state must never become accepted merely because the peer supplied it.

Corrupt, incomplete, stale, or competing history must be handled through the established validation, cumulative-work, fork-choice, synchronization, and recovery rules.

---

## Persistence

Persistence stores Chain evidence.

Persistence is not an alternative source of consensus rules.

Recovery must validate persisted evidence and reconstruct accepted state according to the same protocol rules used during normal operation.

Changes affecting persistence require particular attention to:

- corruption;
- truncation;
- restart behavior;
- deterministic reconstruction;
- bounded reads and writes;
- failure atomicity.

---

## Scope Discipline

Keep contributions focused.

Avoid unrelated cleanup while implementing a specific change.

Do not introduce a new subsystem because it might become useful later.

A preferred contribution:

1. identifies a concrete missing requirement;
2. traces the existing implementation path;
3. determines whether an existing primitive already satisfies it;
4. adds the smallest necessary behavior;
5. tests that behavior;
6. documents the resulting protocol or implementation boundary.

This keeps review manageable and reduces consensus risk.

---

## Documentation

Protocol changes must be reflected in the applicable project documentation.

Depending on the change, this may include:

```text
README.md
docs/ROADMAP.md
docs/CHANGELOG.md
docs/CONTRACTS.md
docs/RPC.md
docs/PROTOCOL.md
```

Update only the documentation affected by the change.

Historical changelog entries should remain historical rather than being rewritten to describe the current state.

New status entries can supersede earlier development status.

---

## Commit and Pull Request Scope

Keep commits understandable and bounded.

A pull request should explain:

- what changed;
- why the change is necessary;
- whether consensus-visible behavior changes;
- whether wire formats change;
- whether persistence changes;
- which platforms were built;
- which tests were executed;
- the observed test results;
- any remaining unqualified platforms or behavior.

Do not claim testing that was not performed.

If a contribution has only been tested on Windows, say Windows.

If it has only been tested on Linux, say Linux.

If it has been tested on both and the results match, include that evidence.

---

## Security

Security-sensitive issues should not be intentionally weakened for convenience or compatibility.

Contributions affecting parsing, cryptography, signatures, authority, networking, persistence, mining, Contracts, or consensus should assume hostile or malformed input.

Validate before trusting.

Bound before allocating or copying.

Reject ambiguity.

Preserve accepted state when candidate validation fails.

---

## What STN Chain Does Not Want

STN Chain intentionally avoids unnecessary blockchain complexity.

Contributions should not introduce the following without an explicit project-level protocol decision:

- EVM compatibility;
- Ethereum-style gas;
- arbitrary smart-contract execution;
- general-purpose virtual machines;
- arbitrary scripting;
- native-structure serialization;
- platform-dependent consensus behavior;
- unbounded consensus state;
- hidden normalization;
- trusted-peer consensus shortcuts;
- undocumented economic assumptions.

Compatibility with another blockchain is not itself a design requirement.

---

## Before Submitting

Before opening a pull request:

- build the affected target;
- run the applicable tests;
- confirm existing tests still pass;
- verify deterministic vectors when applicable;
- verify failure cases preserve prior accepted state;
- test each platform you intend to claim as qualified;
- update affected documentation;
- review the diff for unrelated changes.

A contribution does not need to solve every future problem.

It needs to solve its stated problem correctly.

---

## Final Rule

When choosing between a clever implementation and an obvious deterministic implementation, prefer the obvious one.

STN Chain is infrastructure for distributed trust.

Its behavior should be explainable, reproducible, testable, and independently verifiable.

> **Small. Deterministic. Easy to use.**