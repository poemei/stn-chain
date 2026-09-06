# Decision Register

Status: initial register. Open choices are not implementation defaults.

## Established direction

| ID | Direction | Basis |
| --- | --- | --- |
| D-001 | Fresh implementation primarily in ISO C; no Go compatibility obligation | Project owner direction |
| D-002 | Intelligence, decisions, policies, doctrine, and contracts are core use cases | Project owner direction |
| D-003 | Broad participation with a proprietary, no-modification license | Project owner direction; LICENSE.md |
| D-004 | Account for possible future PoW, miner rewards, and a native coin | Project owner direction; activation remains open |
| D-005 | Maintain docs/CHANGELOG.md as changes are made | Project owner requirement |
| D-006 | Native Visual Studio solution/projects and IDE-first Windows build/test workflow; no CMake requirement | Project owner preference and acceptance |
| D-007 | ISO C17 development scaffold, Release/x64, v145 and SDK 10.0.26100.0; root project files with includes/ and src/ | Requested project scaffold and discovered local toolchain; not a network release |
| D-008 | Adopt envelope version 1 layout and limits for the structural development codec | Authorized codec increment; fixed bytes and negative tests; not finalized consensus rules |

## Proposed architecture constraints

These proposals are documented in ARCHITECTURE.md and are not final protocol
specifications: deterministic validation; separate company authorization and
consensus; explicit platform interfaces; crash-safe state; bounded untrusted
input; independent validation of work and records; consumer reorganization
awareness. Changes to these constraints should include their rationale here.

## Open decisions

| ID | Decision required | Resolve before |
| --- | --- | --- |
| O-001 | ISO C version, supported toolchains, build system, dependency policy | C scaffold |
| O-002 | Canonical encoding, integer widths, versions, record/block limits | Serialization implementation |
| O-003 | Hash/signature algorithms, libraries, signing domains, key representation | Identity and record implementation |
| O-004 | Network identity, fixed genesis contents, activation/version rules | Multi-node chain testing |
| O-005 | Public network admission, submission rights, anti-spam and resource controls | Exposed network testing |
| O-006 | Threat schema, provenance, evidence, deduplication, correction, replay rules | Intelligence vertical slice |
| O-007 | API adapter ownership, push/pull contract, authentication, delivery retries | STN-Labz integration |
| O-008 | Inline/off-chain/encrypted payloads, availability, privacy, retention | Real company data ingestion |
| O-009 | Company identity bootstrap, roles, delegation, key rotation/revocation | Contract authorization |
| O-010 | Structured workflows versus general-purpose VM, execution limits, upgrades | Contract engine design |
| O-011 | PoW selection and activation, algorithm, target, interval, adjustment | Consensus implementation |
| O-012 | Chain selection, work accounting, tie handling, reorganizations, timestamps | Consensus implementation |
| O-013 | Durable format, commit/recovery model, indexing and rollback | Persistent node implementation |
| O-014 | Peer discovery, framing, synchronization, transport security and limits | Peer networking implementation |
| O-015 | Consumer confirmation, acknowledgment, idempotency and external-action recovery | Acting on chain records |
| O-016 | Native coin activation, ownership/accounting model, supply, rewards, fees | Economic testnet |
| O-017 | Mining work format, backend API, supported GPU/ASIC families | Accelerated mining |
| O-018 | Release signing, upgrade authority, compatibility and rollout | Public releases |

## Decision procedure

For each resolved item, record its status, chosen behavior, rationale,
alternatives considered, compatibility implications, and validation evidence.
Do not silently convert an open item into a protocol constant. Record changes
in CHANGELOG.md. Owner decisions are required for mission or economic policy;
routine engineering proposals can be developed with explicit rationale.

## Supplied policy constraints

The owner supplied these organizational references during baseline design:

- 20260906.0, Version Numbering Doctrine, sections 2-14: project releases
  use MAJOR.MINOR.REVISION; Revision ranges from 0 through 10 and rolls
  over to the next Minor at 0. Major advancement is explicit. Historical
  versions remain unchanged and release identities must agree across
  artifacts. No software release version has been assigned here.
- 20260905.0, Sovereignty Act, especially sections 4, 6, 10-11, 17, 21-23,
  and 31: retain practical source/build/data/recovery control, justify
  dependencies, and establish proportionate continuity and exit paths.
  Mature cryptography remains appropriate when justified.

Authoritative source locations supplied by the owner are
`C:/stn-labz/policies/20260906.0_VERSION_NUMBERING.md` and
`C:/stn-labz/policies/20260905.0_SOVEREIGNTY_ACT.md`.
These summaries do not revise or replace those documents. Their embedded
approval metadata is reported as supplied, not independently authenticated.

Software release numbering does not by itself specify wire-protocol
compatibility or activation. O-004 and O-018 must define that relationship.

## Proposal progress

| Items | Concrete proposal | Status and remaining gate |
| --- | --- | --- |
| O-001 | [C17 with native Visual Studio projects and a separate Unix build path](TOOLCHAIN_PROPOSAL.md) | Windows C17 scaffold implemented; Unix builds and minimum supported tool versions remain open |
| O-002 | [Fixed-order binary envelope](ENCODING_PROPOSAL.md) | Structural development codec implemented/tested; payload semantics, block format and complete budgets remain open |
| O-003 | SHA-256 IDs and PureEd25519, with a replaceable crypto provider | Proposed; exact verification acceptance profile and dependency qualification remain open |
| O-006/O-007 | [Signed intelligence exercise](SIGNED_RECORD_PROPOSAL.md) | Proposed synthetic schema; actual API and source-attestation contract still needed |

## Next design increment

The Windows C17 Release/x64 scaffold and bounded envelope codec now build and
run, with a separate native test project. Platform directories reserve other
targets without asserting support. Next, implement the intelligence payload
schema and explicit validation context, then qualify cryptographic verification.
Do not treat structural decoding as authorization to accept a chain record.
No blockchain implementation or economic activation is claimed here.
