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
| D-010 | Explicit staged validation context; missing verification/authority/replay providers remain unresolved | Owner's bounded semantic/context increment; no economics or new blockchain subsystems; Phase 14 Block 6 reuses the existing signer-plus-record-nonce replay discriminator |
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
| O-003 | SHA-256 IDs and PureEd25519, with a replaceable crypto provider | Authorized for Phase 14 Blocks 1–5; canonical 32-byte keys, 64-byte R||S signatures, strict RFC 8032 acceptance, isolated public-domain provider, explicit authority evaluation, genesis-root grant provenance, deterministic grant revocation, and ordinary identity rotation are qualified |
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

## RPC core increment (2026-09-07)

D-035: Adopt RPC-v1 STNC fixed binary framing with deterministic error codes and
method shapes. Dispatch only validated requests after explicit capability checks.
No new protocol transport or public listener is introduced.

D-036: Chain queries use a held immutable snapshot and existing full validation;
never expose persistence paths or treat cached/remote metadata as authority.

D-037: Intelligence checking uses the existing staged validator; submission
remains unavailable without an admission path. Mining exposes tip/target context
and deterministic base-tip freshness only, not templates or accepted solutions.

D-038: RPC is the application/miner integration boundary, separate from P2P.
Read/submission/admin classification is explicit; authentication and deployment
remain deferred. See [RPC.md](RPC.md).

## Mining work and development runtime (2026-09-07)

D-039: Bind work to the entire zero-nonce canonical block with domain-separated SHA-256. Permit only the existing 64-bit big-endian nonce to change. Rebuild deterministically from current validated persisted evidence; no job cache or miner-specific validity.

D-040: Existing blocks require content and no admission queue exists. Require explicit configured canonical content, with an explicit --dev fixture for integration tests; introduce neither empty blocks nor inferred transaction selection. Preserve the existing structural/semantic boundary.

D-041: Implement reserved mining RPC successes within existing v1 bounds. Apply solved blocks through ordinary fork evaluation and atomic storage application. Provide a bounded Windows loopback executable for external integration, reusing existing OS adapters. Public deployment, authentication, automatic P2P orchestration and mining remain deferred.

## Runtime/history reconciliation (2026-09-08)

D-042: Preserve bounded validation/fork APIs and add a complete-history evaluator
for storage adoption. Header-page size is not chain length. Retain STNP/STNC v1
wire shapes; paginate headers with their existing indexed requests.

D-043: Keep one serialized node dispatch boundary with resource-limited client
sessions. Retain partial-frame progress across idle polls; close sockets only
when their worker has stopped. Explicitly distinguish borrowed scratch from
runtime-owned reallocatable memory.

D-044: Finish this increment at runtime/history qualification. Pending admission
and deterministic selection require their own validated signature/authority/replay
foundation; never label repeated structural fixtures accepted intelligence.
Contracts remain addressed, signed, sequenced agreements with deterministic state
transitions, not VM/bytecode/arbitrary execution. Economics remain deferred.

## Phase 11 calculation parameters (2026-09-09)

The owner's Authorized Development Parameters for Phase 11 resolve the timing,
window, clamp, rounding, history and bootstrap portions of O-011 as recorded in
POW.md. They authorize the standalone calculation only. Consensus activation,
mining-template enforcement and deployment choices remain open. No economic
policy or software release version is assigned by these calculation parameters.
## Phase 11 consensus activation (2026-09-09)

The owner authorized Chunk 2 to activate the Chunk 1 rule at candidate height 60
and subsequent 60-block boundaries. Validation and mining share
stn_chain_required_target. Its bounded branch-local calculation history is derived
only from validated blocks and reconstructed on reload, never trusted from disk.
The fixed_target field is bootstrap configuration only. Checked cumulative work
and greater-work fork choice are unchanged. Older fixed-target histories receive
ordinary revalidation; no compatibility bypass or migration is authorized.
See POW.md for qualification, overflow-domain limits and outstanding scope.
## Phase 11 Chunk 3 — 320-bit work and development-history boundary

Authorized consensus rule (2026-09-10): cumulative work is an exact unsigned
320-bit integer encoded as exactly 40 big-endian bytes. Target/hash/work-ID widths
remain 32 bytes. The target domain remains 1 through 2^255-1, and per-block work
remains floor(2^256/(T+1)). At most 2^64 blocks, including height-zero genesis,
each contribute at most 2^255 work, so every supported history fits within 2^319.
No valid history needs saturation, wrapping, truncation or an artificial ceiling.
The generic addition API still rejects out-of-domain 320-bit overflow atomically;
that guard cannot be reached by valid cumulative work over supported heights.

stn_work now holds 40 canonical bytes. Addition visits every byte; target work
is zero-extended into that representation. Fork comparison compares all 40 bytes.
Genesis and target-one successors can accumulate past 2^256 with exact ordering.
Current-window difficulty and the authorized adjustment formula are unchanged.

STNC and STNP use wire version 2, explicitly rejecting version 1 rather than
silently interpreting its narrower work fields. STNC INFO is 184 bytes: work
at 104..143, target at 144..175, status at 176 and count at 180. SUBMIT_WORK success
is 80 bytes: block ID 32, height 8, work 40. STNP STATE is 84 payload bytes:
height 8, tip 32, work 40, count 4. All work fields are unsigned big-endian with
leading zeros required. Wrong lengths are rejected at the relevant message
boundary. Other payload layouts, STNM, target bytes and work-ID semantics remain.

Persistence stores canonical blocks rather than cumulative-work metadata, so its
format is unchanged. Reload and reorganization reconstruct exact 40-byte sums
from accepted history; no serialized difficulty/work cache becomes authoritative.
The Stratum client version and affected response bounds were updated, including
the historical adapter buffer. Mining job/result formats were not redesigned.

Pre-Phase-11 fixed-target chains were explicit development/test evidence, with
no established compatibility guarantee. If their targets violate the activated
rule, revalidation rejects TARGET at the actual boundary. They are not described
as byte-corrupted merely because consensus evolved. No rewriting, bypass,
automatic migration or alternate work ordering is introduced. Development history
that already satisfies current rules remains eligible for normal revalidation.

Qualification uses arithmetic boundaries and scripted block-ID fixtures for
otherwise computationally infeasible target-one blocks. The tests cover full
height range at target one, adjacent minimum targets, high-bit comparison,
addition boundary/failure atomicity, 61 minimum-target blocks, canonical storage
reconstruction, high-work STNC/P2P serialization and invalid lengths/old versions.
Real SHA-256 adjusted branches retain reorg preference after storage reload.
No hardware or production-identity qualification is implied.
## Phase 11 Chunk 4 — final qualification (2026-09-10)

Phase 11 is COMPLETE for Windows Release/x64 bounded development qualification. Chunk 4 required no consensus or production changes. Peers remain evidence providers: each node reconstructs branch-specific targets and exact 320-bit work before existing fork choice. The full target domain, canonical 40-byte big-endian work and strict revalidation of legacy fixed-target development history remain controlling. See ROADMAP.md for the final evidence and limits; Phase 12 is not started.
