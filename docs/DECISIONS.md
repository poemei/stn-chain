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
| D-009 | Adopt intelligence payload schema version 1 for development encoding and syntax validation | Continued implementation; tested field limits and wire examples; production provenance remains open |
| D-010 | Explicit staged validation context; missing verification/authority/replay providers remain unresolved | Owner's bounded semantic/context increment; no economics or new blockchain subsystems |
| D-011 | Development time policy requires observation <= publication and explicit current time/tolerances | Deterministic context checks with boundary tests; consensus time derivation remains open |
| D-012 | Publication transaction wraps exactly one unchanged STNR record; other transaction classes reserved and rejected | Smallest container preserving tested record/signing behavior; no duplicate identity fields |
| D-013 | Development blocks use a 168-byte header and ordered bounded transactions; reserved work fields zero | Explicit structural model; 16 transactions, maximum 1,051,880 bytes; not final consensus |
| D-014 | Transaction IDs hash complete transaction witnesses; linear body commitment and duplicate checks use an explicit provider | No qualified production hash implementation; absent provider unresolved; integrity separate from semantics |
| D-015 | Block ID commits canonical header through the configured hash provider; genesis is exact supplied bytes | Local development linkage, no production hash or final genesis |
| D-016 | Explicit minimal chain state and atomic sequence validation, maximum 64 blocks per call | Owner-authorized chain increment; loaded history uses the same validation path |
| D-017 | Sequential height/parent checks and nondecreasing block timestamps; no ambient time or fork choice | Deterministic development rules, not finalized consensus |
| D-018 | Windows CNG SHA-256 adapter supplies real canonical hashing; other platform providers remain unqualified | Mature locally operated OS primitive, replaceable provider boundary, known-answer tests |
| D-019 | Version 3 activates full 256-bit target and uint64 nonce; version 1 stays explicit legacy and version 2 reserved | Preserve old structural negatives and prevent profile downgrade |
| D-020 | PoW hash equals existing domain-separated single-SHA256 block ID | Smallest compatible construction; no ASIC compatibility claimed |
| D-021 | Fixed development target in [1, 2^255-1]; integer work floor(2^256/(T+1)), checked 256-bit cumulative sum | No adjustment, economic policy, rewards, or supply ceiling |

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
| O-002 | [Fixed-order record envelope](ENCODING_PROPOSAL.md) and [transaction/block containers](TRANSACTION_BLOCK_FORMAT.md) | Development codecs implemented/tested; final protocol budgets and activation remain open |
| O-003 | SHA-256 IDs and PureEd25519, with a replaceable crypto provider | Proposed; exact verification acceptance profile and dependency qualification remain open |
| O-006/O-007 | [Signed intelligence exercise](SIGNED_RECORD_PROPOSAL.md) | Development payload codec implemented/tested; production provenance, corrections, actual API, and source-attestation contract still open |

## Next design increment

The separately authorized SHA-256/PoW/work increment is complete and regression
tested on Windows Release/x64. See [POW.md](POW.md). Stop at this boundary.
Real Windows hashing and local PoW-valid chain accounting exist; distributed
consensus, production signature/authority/replay providers and persistence do
not. Work and coin ownership grant no organizational authority.

## Fork-choice increment (2026-09-07)

D-022: Compare recalculated cumulative PoW work only after both full histories
validate under the same context. Equal work retains current. Never accept
external work claims as evidence of eligibility.

D-023: Preserve fixed-target v3 rules. Unequal-length/work ordering is qualified
with arithmetic vectors only; different-target histories are not eligible
competing branches. No target scheduler or adjustment is introduced.

D-024: Discover the last shared prefix ID and publish a bounded in-memory plan
with ordered detach/attach ranges; no reorganization application or storage
mutation. See [FORK_CHOICE.md](FORK_CHOICE.md). This supersedes the earlier
PoW increment's stopping point only for the explicitly authorized fork scope.

## Persistence increment (2026-09-07)

D-025: Storage v1 holds bounded canonical blocks plus a domain-separated SHA-256
checksum. Recalculate all derived chain state; a checksum is not consensus truth.

D-026: Provider exclusion spans reload, validation and replacement. Extension
and reorganization use the same revalidated-plan, greater-work-only application
path. Commit memory only after complete snapshot publication.

D-027: Qualify Windows local NTFS staging/flush/same-volume replacement only.
Preserve old snapshots on pre-publication failures. Leftover staging blocks
writes until operator handling; no automatic repair or power-loss guarantee.
See [PERSISTENCE.md](PERSISTENCE.md). Scalable storage and other OS adapters remain deferred.

## Peer protocol increment (2026-09-07)

D-028: Use protocol-v1 framed HELLO/STATE/header/indexed-block exchange; explicit
network/genesis/capability agreement, one outstanding request and 64-block bound.
Headers identify reusable prefixes but full blocks establish work/acceptance.

D-029: Treat all advertisements as claims. Recalculate work, recheck current
storage under exclusion, and retain healthy active state on equal/lower work.
Peer counts do not influence preference; fixed-target semantics are unchanged.

D-030: Add explicit validated-prefix recovery separate from strict load. Never
activate a prefix or patch peer bytes in place. Atomic replacement requires
complete valid evidence, with no rollback below valid prefix work.

D-031: Windows Winsock provides bounded nonblocking sessions and loopback test
listening; public listening/discovery and RPC remain deferred. See [PEER_PROTOCOL.md](PEER_PROTOCOL.md).

## Platform isolation increment (2026-09-07)

D-032: Centralize host detection in platforms/stn_build_config.h and runtime
availability in stn_backend.h. Keep host selection out of deterministic core.
Unsupported configurations fail explicitly; simulated detection is not qualification.

D-033: Retain src/ and includes/ as the shared portable core. Isolate Windows
declarations beside their implementations. Existing service contracts cover
hashing, transport/time, persistence and exclusion without unused abstractions.

D-034: Enforce boundary/detection probes during the native test-project build;
retain canonical fixtures and add padding/alignment/endian invariance checks.
Only Windows Release/x64 is qualified. See [PORTABILITY.md](PORTABILITY.md).
