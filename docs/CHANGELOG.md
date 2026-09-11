# Changelog

All meaningful project changes are recorded here. Entries under Unreleased
are not claims of a published or deployed release.

## Unreleased
### Phase 12 Block 2 — automatic outbound connections — 2026-09-10

- Added a portable candidate-consuming outbound lane with deterministic rotation,
  five-second monotonic pacing, partial-resource cleanup and session reuse through
  existing P2P validation/synchronization. Consensus and wire rules are unchanged.
- Added repeated --peer IPv4:PORT startup options and an optional Windows worker,
  private bounded scratch, existing RPC/pending exclusion and joined shutdown.
  Optional outbound I/O deadlines bound partial streams without changing inbound
  idle semantics. No global connection ceiling, discovery, scoring or Block 3.
- Passed 165 targeted C checks (121 policy, 44 Winsock) and 30 executable checks:
  unavailable fallback, loss/reconnect, invalid evidence, bounded retries and
  independent inbound P2P/RPC/pending behavior. Total C: 1,133,882; probes: 34.
  Phase 9: 1,544; Phase 10: 562; adjusted-target processes: 834. Affected Windows
  Release/x64 builds have zero warnings/errors; all checks have zero failures.
- Updated roadmap, peer/network and build/runtime documentation. No prior
  production defect required correction. Other platforms/Internet operation
  remain unqualified. No commit/push. Block 3 not started.

### Phase 12 Block 3 — peer discovery — 2026-09-10

- Added bounded STNP v2 `GET_PEERS`/`PEERS` discovery. Capability bit 2 enables
  one demand-driven exchange per established session; payloads use canonical
  big-endian count and fixed IPv4/port fields, capped at 64 entries / 386 bytes.
- Discovered evidence is parsed as a complete temporary set, validated through the
  existing candidate store, sorted deterministically, self-filtered where configured,
  and committed atomically. Capacity, malformed, duplicate and invalid entries do
  not evict or partially alter existing candidates. Block 2 consumes the same store.
- Passed 974 focused C checks (955 policy/codec, 19 real Winsock) and 40 executable
  checks, including failover to a discovered endpoint, malformed frame isolation,
  unchanged accepted state/storage/pending state, and existing inbound/RPC safety.
  Total Chain C: 1,134,856; build probes: 34; Windows Release/x64 has zero
  warnings/errors/failures. Existing Phase 9/10/11 suites remain passing.
- Discovery does not confer authority or affect consensus. No external bootstrap,
  DNS, gossip, scoring, reputation, banning, or learned-peer database was added.
  Updated peer protocol, roadmap, build/runtime documentation. No commit/push;
  Block 4 not started.

### Phase 12 Block 1 — deterministic peer candidates — 2026-09-10

- Added local IPv4-octet/numeric-port endpoints and a 64-entry candidate set in
  the existing peer core. Canonical field ordering is insertion-independent;
  duplicate/full/malformed outcomes are explicit and preserve existing entries.
  No allocation, eviction, connection management, wire changes or consensus change.
- Added 440 focused checks covering three insertion orders, identity/port bounds,
  invalid endpoints/store invariants, duplicate/full failure atomicity and unchanged
  block-validation output. All 1,133,717 Chain C checks and 34 probes pass.
- Passed 1,544 Phase 9, 562 Phase 10, 834 Phase 11 adjusted-target process checks,
  27 Stratum parser checks and two session assertions. Affected Windows Release/x64
  targets build with zero warnings/errors; all checks have zero failures.
- Updated roadmap and peer API/network documentation. No existing production defect
  required correction. IPv4 only, matching current transport; other platforms are
  not qualified. Phase 12 remains open; no Block 2, commit or push.


### Phase 11 Chunk 4 — network convergence / Phase 11 COMPLETE — 2026-09-10

- Added 1,109 existing-harness checks: two NTFS-backed node states, actual
  Winsock exchange, one bounded partition, distinct height-60 adjusted branches,
  independent validation, deterministic greater-work choice and 60-block reorg.
- Rejected wrong-target evidence with valid encoded-target PoW and fabricated
  maximum advertised work without changing accepted state or stored bytes.
  Reopened both stores and compared all accepted block bytes, tip, current/next
  targets and exact 320-bit work. Fresh STNC work follows the converged target;
  pre-reorg work is stale. A separate scripted-hash fixture covers work beyond
  256 bits; the ordinary scenario uses real SHA-256.
- Passed 1,133,277 Chain C checks, 34 probes, 1,544 Phase 9, 562 Phase 10, 834
  adjusted-target process checks, 27 parser checks and two session assertions.
  The process lifecycle separately proves actual Chain/Stratum restart and
  continued mining. Windows Release/x64: zero warnings/errors/failures.
- No production defect or production-code change was required. Corrected an
  uninitialized test-local length caught by the warning-as-error build.
  Updated roadmap, decisions, PoW, fork/P2P, architecture and build documentation.
  Preserved strict legacy-history revalidation and all authorized consensus.
- Phase 11 COMPLETE for bounded Windows qualification. No hardware, production
  identity, Internet, performance or other-platform qualification; no automatic
  P2P orchestration. No commit/push. Phase 12 not started.

### Phase 11 Chunk 3 — 320-bit work/domain consistency — 2026-09-10

- Implemented the owner's authorized unsigned 320-bit cumulative-work rule:
  exact 40-byte big-endian addition, reconstruction and comparison. Targets remain
  1..2^255-1. Maximum work for 2^64 blocks is 2^319, which fits without an artificial
  ceiling. Target-one successors no longer fail at the former 256-bit boundary.
- Versioned affected transports explicitly: STNC v2 INFO=184 bytes, accepted
  solved-work=80 bytes; STNP v2 STATE=84 payload bytes. Widened only cumulative
  work and shifted following fields. Old versions/wrong lengths fail closed.
  Canonical block storage contains no work cache and needs no format change.
- Added 220 domain/history checks and three reorg/reload checks: 223 new C checks.
  Cover minimum/adjacent targets, maximum height, exact carry/order, 320-bit API
  overflow atomicity, 61 minimum-target blocks, storage reconstruction, high-work
  STNC/P2P transport, invalid lengths and strict legacy-history TARGET failure.
- All 1,132,168 Chain C checks, 34 probes, 1,544 Phase 9, 562 Phase 10 and 834
  adjusted-target process checks pass. Stratum's 27 parser checks and two session
  assertions pass. Windows Release/x64 builds have zero warnings/errors/failures.
- Corrected transport capacities/offsets, old-width assertions and handcrafted
  v1 fixture requests. Updated the Stratum client, response buffers and its docs;
  STNM, target, nonce and work-ID semantics are unchanged. Legacy fixed-target
  development evidence receives current-rule validation, with no rewriting,
  migration, exception or claim that historical incompatibility is corruption.
- Updated roadmap, decisions, consensus/protocol architecture, build instructions
  and both changelogs. Scripted minimum-target hashes are qualification fixtures,
  not hardware/production identity evidence. No commit or push. Chunk 4 not started.


### Phase 11 Chunk 2 — consensus difficulty activation — 2026-09-09

- Activated one branch-derived required-target boundary in ordinary validation
  and mining templates. Targets must match exactly independently of raw PoW.
  Bootstrap, 60-block boundaries and all authorized calculation constants remain.
- Added bounded validated-window state, rebuilt by storage/fork replay. Removed
  the fixed-target cumulative-work formula beyond bootstrap; retained exact
  checked per-block work, fork ordering, canonical storage and work identity.
- 1,658 new C checks prove real-PoW wrong-target rejection, span/target bounds,
  STNC bytes, missing/corrupt history failure, branch-specific reorg, persistence
  and second-boundary continuity. Existing minimum-target arithmetic/overflow
  regressions pass; no impossible minimum-target 60-block history is claimed.
- Added -DifficultyOnly to the existing harness: 834 checks pass through actual
  Stratum at height 60 and independent persistence/restart to new work at 61.
  Updated old fixed-target long-history fixtures and full-target fixture solving.
- All 1,131,945 Chain C, 34 probes, 1,544 Phase 9 and 562 prior Phase 10 checks
  pass. Windows Release/x64 builds: zero warnings/errors/failures. No additional
  platform, hardware or production identity qualification; Stratum unchanged.
- Updated roadmap, PoW, architecture, decision, state/fork/mining documentation
  and build instructions. Earlier fixed-target histories may fail revalidation
  beyond adjustment boundaries; no reset/migration or bypass was introduced.
  No commit/push performed. Phase 11 remains open; Chunk 3 not started.

### Phase 11 Chunk 1 — difficulty calculation foundation — 2026-09-09

- Added standalone stn_target_next using owner-authorized 60-second/60-block
  timing, H-60/H-1 endpoints, 3600-second expectation, 900/14400-second clamps,
  integer floor and existing 1..2^255-1 target limits. No unresolved calculation
  parameters remain. Bootstrap/insufficient/malformed history handling is explicit.
- Uses bounded 34-byte multiplication/division with uint32_t steps, canonical
  big-endian bytes, no allocation/dependency/float, unchanged output on failure.
- Added 193 focused checks in the existing PoW harness. All 1,130,287 Chain C
  checks, 34 build probes, 1,544 Phase 9 and 562 Phase 10 process checks pass.
  Windows Release/x64 builds: zero warnings/errors, zero test failures.
- Updated POW, decision register, roadmap and build instructions. Calculation
  is qualified but not activated: ordinary validation, mining, STNC, cumulative
  work and fork choice retain their fixed-target behavior. Stratum unchanged.
  No other platform qualification, commit/push or Chunk 2 work.


### Phase 10 Chunk 5 — full integration COMPLETE — 2026-09-09

- Added 125 actual-process checks (-FullLifecycleOnly) covering Chain template,
  deterministic Stratum jobs, fixture results, STNC solved-work, independent
  Chain acceptance, exact persistence and new work from recovered accepted state.
  Work ID, full target and candidate remain exact except the 64-bit nonce.
- One bounded interruption stops both processes. Chain recovers identical INFO
  and accepted block bytes before Stratum restarts. No unavailable placeholder
  or accepted old job becomes current; invalid/stale results remain rejected.
- No production defect found or production code changed in this chunk. Phase 10
  COMPLETE is documented in roadmap, architecture and protocol documentation.
- Windows Release/x64 rebuilds: zero warnings/errors. All 125 new and 437 prior
  Phase 10 checks, 1,544 Phase 9 checks, 1,130,094 Chain C checks, 27 parser checks,
  two session assertions and 34 build probes pass with zero failures.
- Scripted identity/result fixtures remain test-only. Hardware, production
  identity, accounting, performance and other platforms remain unqualified.
  No commit or push performed. Phase 11 not started.


### Phase 10 Chunk 4 — failure/reconnect state — 2026-09-09

- Extended the existing integration script with -FailureStateOnly: 148 checks
  prove real Chain loss, an attempted miner result returning provider rather than
  accepted, no fabricated/current cached work, empty reconnect, identical and
  changed authoritative work recovery, partial miner-frame disposal, stale results,
  bounded session isolation and actual Stratum process restart.
- Fixed Stratum's delayed invalidation after failed/uncertain solved-work calls;
  shared current-work and session flags now clear immediately. Chain consensus,
  wire formats and stale identities remain unchanged. An initial test expected
  only EOF after process termination; it now correctly also accepts TCP reset.
- Release/x64 builds: zero warnings/errors. New 148 and prior 115/93/81 Phase 10
  checks pass, alongside 1,544 Phase 9 runtime checks, 1,130,094 Chain C checks,
  27 Stratum parser checks, two session assertions and 34 build probes. Zero
  failures; no new Chain C checks or additional platform qualification.
- Updated roadmap/RPC and Stratum protocol/changelog. Scripted identity fixtures
  remain explicitly test-only. No hardware-miner, performance, discovery, queue,
  economic or Chunk 5 work. Temporary files/processes cleaned; current work and
  independent Stratum changes preserved. No commit or push performed.

### Phase 10 Chunk 3 — miner result return path — 2026-09-09

- Added -MinerResultOnly to the existing actual-Stratum integration script;
  shared existing fixed-target result/digest helpers rather than copying fixtures.
  Results travel through actual STNM sessions, Stratum's current cached-work
  reconstruction and STNC 0x2003; no direct internal acceptance path is used.
- 115 new checks prove correct work association, nonce-only reconstruction,
  big-endian nonce above 2^32, unchanged full target/candidate read back from
  accepted Chain storage, rejection of forwarded invalid PoW, unknown/stale/
  malformed/reserved-bit handling, two-session isolation and session reconnect,
  accepted two-item cleanup and Chain restart. Stratum fixes enforce reserved
  bytes and its existing protocol-error mapping; Chain consensus is unchanged.
- Windows Release/x64 builds have zero warnings/errors. All new checks plus
  prior 93 mapping, 81 interface, 1,544 Phase 9 executable lifecycle checks,
  1,130,094 Chain C checks, 27 Stratum parser checks, two session assertions and
  34 build probes pass, zero failures. No other platform qualification.
- Updated roadmap and RPC documentation and Stratum protocol/architecture/
  changelog. Production identity remains fail-closed; the test-only identity
  hooks and bounded result generator do not qualify hardware miners or accounting.
  No Chunk 4, discovery, economics or unrelated architecture. Temporary test
  state/processes removed; existing work preserved. No commit or push performed.

### Phase 10 Chunk 2 — deterministic Stratum job mapping — 2026-09-09

- Extended the existing actual-Stratum integration script with -JobMappingOnly.
  Shared the existing canonical transaction fixture helper with the Phase 9
  script instead of duplicating fixtures. Current working trees are preserved.
- Added 93 checks proving actual 0x2002-to-STNM mapping: exact candidate/target/
  work ID/nonce and embedded metadata, identical jobs across three observation
  sessions, replacement from a second pending submission, and initial/subsequent
  unavailable without placeholder or cached current jobs. No miner computation,
  share, or solved-work submission occurs in the new mapping proof.
- No production mapping defect was discovered; no consensus, protocol or live
  Stratum code changes were needed. The authorized test runtime supplies scripted
  identity validation only; its real RPC/template/runtime paths are exercised.
- Windows Release/x64 builds: zero warnings/errors. New 93 checks and prior 81
  Chunk 1 integration checks pass; Stratum's 27 parser checks and two existing
  session assertions pass. All 1,130,094 Chain C checks and 34 build probes pass,
  zero failures. No new C checks or other platform qualification.
- Updated roadmap/RPC documentation and Stratum protocol/architecture/changelog.
  Chunk 2 complete; no Chunk 3, external miners or share work. Temporary test
  processes/files removed. No commit or push performed.

### Phase 10 Chunk 1 — actual STN-Stratum interface — 2026-09-09

- Qualified current C:\poes_projects\stn-stratum production server/client through
  STNC without Chain protocol or consensus changes. Added one bounded integration
  script and a configurable test-helper port. Existing Phase 9 work is preserved.
- Fixed Stratum-side undersized maximum, missing INFO API, missing successful
  response shape/bounds checks, unbounded socket send/receive, and unsafe mutation
  retransmission. Its driver links the actual production client/Windows transport;
  the actual server runs concurrently and proves endpoint recovery/work polling.
- 81 cross-process checks pass: INFO endian/length, complete work/target/nonce
  identity, accepted/rejected/stale solved-work contracts, unsupported opcode,
  no-work, concurrent clients, outage/reconnect and lost mutation reply isolation.
  Fixed test-only log observation to use redirected stdout rather than a log file
  held exclusively by Stratum. Temporary processes/state/logs are removed.
- Both affected optimized Windows x64 builds: zero warnings/errors. Stratum's
  27 new parser/status checks and two existing session assertions pass. All
  1,130,094 Chain C checks and 34 build probes pass; zero remaining failures.
  No new Chain C checks or other platform qualification.
- Updated RPC.md, MINING_WORK.md and ROADMAP.md; Stratum protocol/architecture
  and changelog also updated. Chunk 1 marked complete, not the full miner lifecycle.
  No Chunk 2, production identity, share/economic/difficulty work, commit or push.

### Phase 9 lifecycle integration proof — 2026-09-09

- Added an explicitly authorized test-only runtime build of the existing Windows
  application with scripted signature/authority/replay hooks. Separate executable
  and intermediate paths preserve the production binary, which still fails closed.
  A private test-only event requests the existing orderly shutdown path; no RPC
  shutdown method, production provider, or protocol feature was added.
- Added tools/test-phase9.ps1 using the existing executable-test helpers. All
  submissions use STNC 0x1005. The real runtime proves IDs/status, duplicates,
  malformed/unresolved/replay rejection, concurrent submission/status clients,
  canonical ordering of 17 pending items with 16 selected, stable candidate/work
  bytes, changed-candidate staleness, invalid PoW, and valid 64-bit nonce solutions.
- Proven local atomic acceptance removes exactly included entries and preserves
  the remaining item's ID. Clean shutdown/restart reconstructs the exact accepted
  block; volatile pending does not return. Subsequent RPC admissions/mining proceed
  through height 65, restart restores that block, and another admission builds
  candidate 66 from the restored tip. No production integration defect was found.
- Windows Release/x64: production and test runtime builds have zero warnings/errors.
  1,544 new executable/TCP/CNG/NTFS checks pass. Existing 1,130,094 C checks and
  34 build probes pass, zero failures. Peer cleanup/unrelated preservation,
  persistence/activation failures, capacity, P2P catch-up/reorg and other prior
  regressions remain passing. No new C checks or other platform qualification.
- Phase 9 marked COMPLETE in ROADMAP.md for this explicitly scripted-identity
  integration proof. BUILD.md documents reproducible commands and limitations.
  Test files/processes are cleaned up; no standalone report or scratch artifacts.
  Production identity qualification, reorg re-addition, pending persistence/gossip,
  Stratum and Phase 10 remain deferred. No commit or push performed.

### Accepted-block pending cleanup — 2026-09-08

- Inclusion preparation now derives protocol transaction IDs through the existing
  hash provider and matches pending IDs explicitly. Preparation is read-only;
  failures leave the removal mask and store unchanged. Applying the mask releases
  only matching owned bytes and preserves unrelated ordering/count accounting.
- Local solved-work acceptance keeps its prepare/commit/prune sequence, but no
  longer prunes old inclusions before validation. Failed work, persistence, or
  stale activation therefore cannot consume pending entries.
- Added an optional pending-store attachment to peer synchronization. Fully
  validated candidate history prepares removals; only successful atomic adoption
  applies them. Retained/failed candidates preserve pending. Caller serialization
  spans synchronization and pending/RPC mutations; no new threads or peer runtime.
- Added 18 targeted checks, including peer success, unrelated same-nonce/different-ID
  preservation, failed peer validation/persistence, retained activation, no-match
  acceptance, missing removal, hash-failure atomicity, local rejection with an
  already-included pending entry, and byte accounting after multiple local removals.
  Existing single/multiple acceptance, persistence-failure, template-read-only,
  pending/admission/RPC/candidate, local mining and P2P regressions still pass.
- Release/x64, Visual Studio 2026: zero warnings/errors; 1,130,094 C checks and
  34 compile/boundary probes pass, zero failures. Other platforms unqualified.
- Updated ARCHITECTURE.md and MINING_WORK.md. Reorg re-addition, pending persistence
  and gossip remain deferred. No commit or push performed.

### Deterministic pending candidates — 2026-09-08

- Reconciled current pending assembly to consume the store's ascending canonical
  ID enumeration and reject mismatched active-network validation contexts.
  Preserved existing revalidation, replay exclusion, canonical body encoding,
  16-transaction/body limits, deterministic stop behavior, and empty no-work result.
- Template and work-context queries no longer perform pending pruning. Ineligible
  entries stay owned by the store; candidate construction is read-only. Existing
  accepted/peer cleanup behavior is preserved without extension.
- Normal pending-based mining already bypassed selected.stnt in the current
  source; preserved that path and explicit --dev fixtures. No CLI redesign,
  nonce/work-identity change, economic priority, persistence, or gossip added.
- Added and ran 132 targeted candidate checks: single/multiple inclusion, ID
  order, reverse arrivals, repeated identical work, count limit, exact/short
  buffer boundary, empty/ineligible behavior, rejection exclusion, changed-content
  stale work, zero template nonce, and no removal even for included entries.
- Release/x64, Visual Studio 2026: zero warnings/errors. The initial test used
  an incorrect header member name; corrected before the successful build.
  All 1,130,076 C runtime checks and 34 compile/boundary probes pass, zero failures.
  Existing pending/admission/RPC/mining regressions remain passing. No new block
  acceptance, peer/reorg, or long-chain scenarios; no other platform qualification.
- Updated MINING_WORK.md and ARCHITECTURE.md. Signature/authority success remains
  scripted in positive tests; production providers are still required. Current
  working-tree changes preserved. No commit or push performed.

### Canonical pending admission over STNC — 2026-09-08

- Added SUBMIT_TRANSACTION (0x1005), carrying only canonical STNT bytes through
  the existing validated admission API. Explicit protocol codes distinguish
  acceptance, duplicate, capacity, invalid, unsupported, replay, unauthorized,
  unresolved providers, and internal errors. The 36-byte response contains only
  format/result and an accepted/duplicate ID; all other IDs are zero.
- Reused PENDING (0x1004) as a fixed 16-byte count, entry-capacity, byte-usage,
  byte-capacity summary. Replaced its earlier uncommitted ID-list shape; no new
  browsing or pagination. Preserved legacy record submission and all existing
  mining/assembly behavior without extending it. No pending persistence/gossip.
- Preserved listener serialization and per-client transport. Generic submission
  checks response capacity before admission and does not prune on rejection.
  Method-oversized payloads reject before admission; global framing is unchanged.
- Release/x64, Visual Studio 2026: zero warnings/errors. Added 763 RPC checks,
  including 12 concurrent dispatcher clients (one malformed), exactly one
  acceptance and ten duplicates, capacity, replay, failure mapping, identity,
  permissions, bounds, and status accounting. Existing C regressions pass:
  1,129,944 total C checks, zero failures; 34 build/boundary probes pass.
- Added and ran the bounded test-node.ps1 -PendingRpcOnly mode: 27 actual
  executable/TCP checks passed. Concurrent client requests verify missing-provider
  rejection, bad-client isolation, surviving sessions, oversize rejection, status
  capacities, and unchanged chain height. Temporary test state was removed.
  Positive admission uses scripted signature/authority hooks in the C harness;
  the production node still fails closed without identity providers.
- Updated RPC.md and ARCHITECTURE.md. No other platform qualification, new mining
  or P2P scenarios, commit, or push. Existing working-tree changes are preserved.

### Validated pending admission — 2026-09-08

- Added canonical STNT admission through the existing record admission and
  validation context. Reconciled record admission to use the shared pending-store
  insertion primitive instead of duplicating allocation and sorted insertion.
  Preserved the current working tree and existing integration work.
- Reused canonical decoding, intelligence payload validation, network/time,
  signature/authority/replay hooks, and protocol transaction-ID derivation.
  Missing context/providers fail closed. Unsupported outer, record, and payload
  versions return UNSUPPORTED; malformed data returns INVALID. Existing detailed
  validation and admission results remain deterministic without another framework.
- Exact pending IDs take precedence over pending nonce conflicts and capacity
  after validation. Existing active-history signer/nonce checks supplement the
  required replay hook. Duplicate/new-full requests preserve count and ownership;
  accepted bytes are copied by the same bounded store. No replay index added.
- Release/x64, Visual Studio 2026: zero warnings/errors. Added and ran 311 direct
  admission checks, zero failures. Prior 1,064 store checks and C regressions pass:
  1,129,181 total C checks and 34 compile/boundary probes, zero failures. No new
  RPC, mining, P2P, or long-chain scenarios; no other platform qualification.
- Positive signature/authority outcomes are explicitly scripted test hooks;
  production SHA-256 supplies canonical IDs. No production Ed25519/authority
  provider is introduced. Existing missing-provider behavior remains closed.
- Updated ARCHITECTURE.md and the public header. No new RPC, template, assembly,
  gossip, or pending-persistence integration. No commit or push performed.

### In-memory pending store foundation — 2026-09-08

- Added explicit initialization, structural canonical-transaction insertion,
  copy-out lookup, removal, count, and bounded ID enumeration to the current
  pending store. Clear supplies reset/destruction. Store insertion is not
  authenticated admission and does not establish chain eligibility.
- Independent limits: 128 entries and 256 KiB of owned transaction bytes,
  plus fixed metadata. Entry/byte exhaustion and allocation failure explicitly
  reject without eviction or mutation. Duplicate canonical IDs return DUPLICATE
  even at capacity. Enumeration uses ascending unsigned canonical ID bytes.
- Inputs are copied; lookup/enumeration return no internal pointers. Removal
  and clear free owned memory. Initialization allocates nothing. External
  serialization is required; live stores must not be shallow-copied.
- Release/x64 (Visual Studio 2026): zero warnings/errors. Added and ran 1,064
  targeted store checks covering empty/init, insertion, identity, independent
  input lifetime, lookup buffer capacity, duplicate/full rejection, preservation,
  arrival-order invariance, bounded pages, removal, reset, and reuse. Zero failures.
  Existing C regressions also pass: 1,128,870 C checks total, including the
  preserved 224 pending/assembly checks; 34 compile/boundary probes pass.
  No additional platform qualification or long-running executable scenarios.
- Documented the store boundary in ARCHITECTURE.md and its header. Preserved
  the existing working tree, including earlier uncommitted integration work;
  this bounded increment adds no RPC, mining-template, assembly, P2P, or pending
  persistence integration. No commit or push performed.

### Long-running RPC and longer-history reconciliation — 2026-09-08

- Continued from 548ea36 without resetting the source. Corrected stale cached
  mining-state rejection, unsafe implicit realloc of borrowed buffers, missing
  resizing after external history growth, and unchecked size arithmetic.
- Ordinary extension validates one candidate and reuses the validated prefix
  under exclusion, avoiding a second history allocation/revalidation and fork
  choice. Atomic replacement still precedes active-state publication.
- Added complete-history fork evaluation for storage adoption while preserving
  the bounded fork API. P2P pages 64 headers instead of capping total history;
  all exit paths release dynamic views/spans. Truncated-tail recovery now retains
  the available validated prefix even when the advertised full count cannot fit.
- Reconciled concurrent RPC resource handling: failed accepts do not kill the
  server, completed workers are reaped under connection churn, and sockets close
  only after worker completion. Preserved partial-frame progress across body and
  chunk idle polls. STNC v1 and the 64-bit nonce region remain unchanged.
- Release/x64: zero warnings/errors. 1,127,582 C runtime checks plus 872 actual
  executable/TCP/NTFS checks = 1,128,454 runtime checks; 34 build/boundary probes;
  1,128,488 total, zero failures. Prior regression checks remain passing.
- Verified real SHA-256 solutions through height 70, dynamic buffer growth,
  persisted restart/work at height 71, 21 simultaneous clients, connection churn,
  more than 64 requests, idle sessions and partial payloads spanning 60 seconds.
  Actual STN Core/stn-stratumd binaries and deliberate OS resource exhaustion
  were not tested; synthetic clients exercise their shared RPC boundary.
- P2P harness verifies 130-block catch-up, incremental reuse, competing branches,
  reorganization, disconnect/reconnect, truncated-prefix recovery and full rebuild.
  Existing real localhost/CNG/NTFS peer tests and atomic-failure tests still pass.
- Updated authoritative runtime/mining/RPC/storage/peer/fork/decision/build docs.
  Temporary executable test storage remains private and is removed afterward.
- Intentionally bounded to runtime/history reconciliation. Whole-snapshot reads
  and rewriting remain; streaming storage, pending authenticated admission/block
  selection, production identity providers, automatic P2P orchestration, difficulty
  adjustment, contracts, economics, wallets/UI and Stratum remain deferred.
  Explicit structural development transactions are not authenticated intelligence.


### Runtime chain-server cleanup — 2026-09-07

- Removed the accidental coupling between `STN_CHAIN_MAX_BATCH` (bounded
  validation/fork work) and total persisted chain history. Storage decode and
  active-state reconstruction now validate history incrementally instead of
  rejecting block 65 with RPC CAPACITY.
- Added a dedicated one-block storage extension path for mined solutions:
  validate against the current accepted tip, recheck persisted state under
  exclusion, atomically publish, then update active state. Ordinary extension
  no longer builds two complete histories merely to invoke fork choice.
- Changed storage views to dynamically sized block-span tables and removed the
  Windows storage-size ceiling derived from the 64-block batch constant. The
  runnable node sizes work buffers from the existing chain and grows them as
  accepted history requires.
- Removed the development RPC fixture limits of 64 requests and a 60-second
  total connection lifetime. Listener polling is now separate from accepted
  client I/O timeouts, avoiding accidental disconnects caused by the accept poll.
- Converted the Windows loopback RPC runtime from a blocking single-client loop
  to bounded concurrent client sessions (16). Stratum and STN Core can remain
  connected simultaneously. Each client owns its socket and frame buffers; RPC
  dispatch into the shared mining/storage service remains serialized so state
  mutation stays deterministic. The Winsock listen backlog now uses SOMAXCONN,
  and shutdown interrupts active client sockets before joining their threads.
- Hardened exact socket transfers so an idle timeout is surfaced only before any
  bytes of that transfer have moved. Partial frame progress is retained until the
  exact transfer completes, disconnects, or is interrupted; timeout polling can
  therefore keep long-lived clients alive without desynchronizing STNC framing.
- Preserved canonical mining semantics: published templates start with nonce 0;
  miners may change only block bytes 152..159; submitted work is fully validated
  by the node. A zero template nonce is not itself an error.
- Added/updated storage regression coverage for histories beyond 64 blocks and
  updated persistence, RPC and mining documentation. Portable core sources pass
  strict C17 syntax checking with `-Wall -Wextra -Werror` in this environment.
  Windows Release/x64 build/test qualification remains to be run on Windows.


### Mining work and runnable development node — 2026-09-07

- Added deterministic v3 templates from fully validated persisted tip, fixed
  target, inherited timestamp and explicit canonical content. Work IDs hash the
  complete zero-nonce block; only the existing big-endian nonce may change.
- Implemented mining RPC retrieval and solved submission using real SHA-256,
  ordinary full fork validation and atomic persistence. Stale tips, changed
  templates, insufficient work and failed replacement never activate solutions.
- Added the actual Windows loopback RPC application, explicit --dev fixture and
  operator-provided genesis/transaction mode, strict startup/reload, bounded
  sessions, run-dev.cmd and Visual Studio development launch arguments. Runtime
  uses existing CNG/NTFS/Winsock adapters; no platform APIs enter core semantics.
- Preserved all 1,126,622 prior runtime checks. Added 937 mining checks and 42
  actual executable/TCP/NTFS checks, including fragmented framing and persisted
  restart: 1,127,601 runtime checks plus 34 build probes = 1,127,635 total,
  zero failures. Release/x64 built with zero warnings/errors.
- Verified immutable-byte binding, independent work ID, nonce variants, alignment,
  reorganization/recovered activation staleness, intervening accepted writes,
  storage failure atomicity and existing regression coverage.
- Updated mining/RPC/build/architecture/chain/PoW/portability/decision documents
  and README. No release version assigned; this is an Unreleased increment.
- Limits remain 64 blocks and 4 MiB runtime storage buffers, single-client
  loopback RPC. Development fixture is structural test content, not accepted
  signed intelligence or coin. No existing stratumd compatibility was tested.
- Deferred miners/hardware, Stratum/shares/payouts, wallets/coins/economics,
  difficulty adjustment, mempool/admission, contracts/explorer, public RPC/auth,
  automatic P2P runtime orchestration and additional platform qualification.

### RPC core and deterministic node interface — 2026-09-07

- Added versioned bounded binary RPC request/response codecs, deterministic
  errors, validated dispatch and explicit read/submission/admin capabilities.
  No socket or native platform types enter RPC semantics.
- Added immutable node snapshot services for validated chain identity/status,
  block lookup by height/calculated ID, and current mining base/target context.
- Intelligence checking/submission uses the existing staged validator. Actual
  admission remains unavailable; no queue, persistence or acceptance is invented.
  Mining templates/solution acceptance and intelligence indexing remain explicitly
  unavailable. Stale base tips return a deterministic error.
- Release/x64 clean build: added 237 RPC checks. Runtime total 1,126,622 plus
  34 build/probe checks = 1,126,656 checks, zero failures. All prior 1,126,385
  runtime checks preserved. RPC in-process integration and existing localhost
  P2P real-hash/NTFS integration pass.
- Added RPC.md and updated architecture, integration, portability, mining and
  build documentation. RPC is the application/miner boundary; local persistence
  files and P2P internals are not application interfaces.
- Deferred: RPC network exposure/authentication, Explorer/Wallet/Contract runtime,
  accepted intelligence indexing/admission, templates/mining/Stratum/stn-stratumd,
  difficulty adjustment, issuance/rewards/treasury/economics, gas/fees, mempool,
  Linux/macOS runtimes and x86/ARM qualification. Windows Release/x64 only qualified.

### Platform isolation and consensus invariance — 2026-09-07

- Centralized OS/CPU detection and explicit runtime-backend selection. Unknown,
  conflicting and unavailable targets fail with clear diagnostics; no Windows
  fallback for Linux/macOS or unqualified Windows architectures.
- Moved Windows-only declaration headers into platforms/windows and updated
  native project include paths. CNG/Winsock/NTFS stay behind existing portable
  contracts; no wire, hash, consensus, persistence or fork rule changed.
- Added Visual Studio pre-build selection/boundary enforcement (34 passing
  compile/audit probes) and 93 runtime invariance checks for padding, unaligned
  inputs, pointer-independent bytes/IDs, endian fields and bounded integers.
- Release/x64: 1,126,385 runtime checks plus 34 build probes = 1,126,419 checks,
  zero failures; all prior 1,126,292 runtime checks remain passing. Localhost
  real SHA-256/NTFS synchronization passed. Build has no warnings/errors.
- Added PORTABILITY.md, explicit Linux/macOS boundary docs and updated architecture,
  decisions/build/platform matrix. Detection probes simulate macros only.
- Only Windows Release/x64 remains qualified. Linux/macOS runtimes and x86/ARM
  qualification remain deferred, along with RPC, explorer, wallets, contracts,
  mining/Stratum, difficulty adjustment, economics and mempool.

### Bounded P2P synchronization and recovery — 2026-09-07

- Added versioned peer framing, strict network/genesis handshake, chain
  advertisements, bounded headers/indexed full-block exchange, and an explicit
  synchronous peer protocol over a replaceable exact-transfer boundary.
- Added Windows nonblocking Winsock transport with finite total deadlines,
  explicit IPv4 connectivity and a loopback-only development listener.
- Added prefix-reusing sync with complete local validation and cumulative-work
  fork choice. Advertisements and peer majority never establish authority.
- Added separate recovery evidence scanning and atomic adoption: preserve strict
  startup rejection, validate prefixes from genesis, rebuild only from fully
  verified blocks, recheck disk under exclusion and commit after replacement.
- Release/x64 build passes without warnings/errors. Added 145 passing checks;
  total 1,126,292 checks, zero failures. All prior 1,126,147 checks preserved.
  Localhost two-endpoint sync passed with real SHA-256 and NTFS save/reload.
- Updated protocol/recovery, architecture, decision, build and platform docs.
  No software release assigned; previous changelog history retained.
- Limited to 64 blocks, one outstanding request, explicit endpoints and fixed
  targets. No Internet/non-Windows qualification, public service/discovery,
  authentication/privacy, scalable storage/sync or power-loss guarantee.
  RPC, wallets, contracts, mining/Stratum, difficulty adjustment, economics,
  treasury, gas/fees and mempool remain deferred.

### Persistence and atomic application — 2026-09-07

- Added a versioned, bounded canonical-block snapshot format with SHA-256
  corruption checks and full startup revalidation; derived state is rebuilt.
- Added portable storage-provider coordination and Windows local NTFS staging,
  flushing, exclusive writer coordination and same-volume snapshot replacement.
- Added one application path for extensions/reorganizations: reload active
  history, recalculate and verify plan/state, require greater work, persist
  before committing memory. Corruption, stale plans and failed writes reject.
- Added 2,486 checks, including real Windows file operations and injected
  staging/write/flush/promotion failures. Final Release/x64 build passes with
  no warnings/errors; 1,126,147 checks, zero failures. All earlier 1,123,661
  regression checks remain unchanged and passing.
- Updated architecture, decision register, chain/fork/build documentation and
  platform status; added PERSISTENCE.md with format, startup, application,
  recovery and actual durability limits. Existing changelog history preserved.
- Limited to 64 complete blocks and whole-snapshot replacement on local NTFS.
  No non-Windows qualification, power-cut guarantee, automatic staging recovery,
  scalable database, node networking/RPC, mining, wallets, contracts, difficulty
  adjustment, economic policy or mempool/confirmation/replay coordination.

### Fork choice and reorganization planning — 2026-09-07

- Added full-history fork evaluation using revalidated cumulative PoW work;
  invalid/unresolved histories cannot win. Equal work retains current.
- Added common-ancestor discovery and atomic in-memory plans with calculated
  old/candidate tips, work, resulting height, and ordered detach/attach ranges.
  Evaluation changes no accepted state; failed calls preserve plan output.
- Preserved fixed-target validation. Shorter/higher-work and longer/lower-work
  comparisons are arithmetic tests only, not eligible different-policy forks.
- Added 785 Release-enabled checks. Windows Release/x64 build passes without
  warnings/errors; 1,123,661 total checks, zero failures. All previous
  1,122,876 checks remain passing and unchanged.
- Updated native projects, architecture, decision register, chain/PoW/build
  documentation, and added FORK_CHOICE.md with replay/confirmation implications.
- Limited to complete histories of 1..64 blocks under one PoW context. No
  reorganization application, persistent mutation, networking/RPC, mining,
  wallets/mempool, contracts, difficulty adjustment or economics implemented.
  Non-Windows builds remain unqualified. Changes are not a published release.

### Validation

- SHA-256/PoW/work increment: final Windows Release/x64 build passed without
  warnings/errors. Added 323 passing checks, including SHA-256 known answers,
  independent IDs, target arithmetic, PoW and atomic cumulative work. Total:
  1,122,876 checks, zero failures; all earlier 1,122,553 checks preserved.

- Windows Release/x64 solution build succeeded with no warnings or errors.
- Record codec tests passed: 230 checks, zero failures; intelligence payload
  tests passed: 2,027 checks, zero failures; validation-context tests passed:
  462 checks, zero failures (2,719 total). Provider hooks use test doubles;
  these results do not establish cryptographic validity.
- Transaction/block suite passed 1,118,656 checks, including exhaustive
  byte-truncation loops for maximum-size containers. Combined suite:
  1,121,375 checks, zero failures; hash-provider tests use a noncryptographic stub.
- Chain-context suite passed 1,178 checks; combined Release/x64 suite passed
  1,122,553 checks with zero failures. All prior 1,121,375 checks retained.
- Application scaffold smoke check passed. Signature verification, payload
  truth/evidence verification, authorization, and consensus are not covered
  by this validation. Payload syntax and field limits are covered.
- Original envelope wire behavior and all 230 existing envelope checks
  preserved; this increment stops at payload and validation-context support.

### Added

- Real SHA-256 Windows CNG adapter behind the existing provider boundary;
  no remote service or bundled third-party dependency introduced.
- Version-3 PoW block profile with full big-endian targets, single-SHA256
  block-ID/work hash, fixed context target, and explicit verification stages.
- Bounded integer work calculation and checked cumulative-work state,
  integrated into atomic candidate/sequence validation including genesis.
- Exact PoW/work documentation and independent fixed real-hash fixtures.
- Legacy version-1 development mode retained explicitly; no mining loops,
  fork choice, persistence, networking, wallets, contracts, or economics added.

- Minimal explicit chain state, exact development-genesis context, and
  provider-bound canonical header IDs without production hashing.
- Candidate validation for structure, contextual links, body integrity,
  expected networks, and nondecreasing development timestamps.
- Bounded sequential validation with atomic output assignment, first-failure
  index/height diagnostics, and fail-closed hash-provider handling.
- Independent genesis fixture and local-chain regressions covering failure
  paths, unchanged state, loaded inputs, and a 64-block development batch.

- Versioned publication transaction wrapping one unchanged record, with
  bounded canonical encoding and reserved future transaction classes.
- Versioned 168-byte block header and ordered length-prefixed transaction
  body; development limits of 16 transactions and 1,051,880 block bytes.
- Separate transaction/header/body/block structural checks, provider-bound
  transaction IDs and body commitments, and duplicate-ID integrity checks.
- Exact wire-layout documentation, fixed independent fixtures, failure-path
  tests, and integration through intelligence, record, transaction, and block.

- Explicit staged validation context for expected network, supplied time
  policy, signature verification, authority lookup, and replay checks.
- Separate stage/final statuses with fail-closed unresolved/error behavior;
  missing providers never yield acceptance and later stages do not run.
- Deterministic observation/publication time rules with overflow-safe bounds,
  hook-contract and stage-order regression tests, and implementation limits.

- Allocation-free intelligence payload encoder/decoder for the development
  schema, with borrowed ASCII spans and exact field-length validation.
- Severity, classification alphabet, source-label, printable subject, and
  nonzero evidence-digest checks; no DNS or external-service calls.
- Intelligence tests covering independent wire bytes, all fixture truncations,
  alphabet/length boundaries, malformed fields, output failure behavior,
  and integration with the outer record envelope.

- Bounded ISO C17 record encoder/decoder with explicit errors, borrowed
  payload views, big-endian fields, length limits, and zero-nonce rejection.
- Native Release/x64 stn-chain-tests project covering independent byte
  fixtures, truncations, invalid inputs, and boundary sizes; checks run in Release.
- Documented the structural codec's separation from cryptographic, payload,
  authorization, network, and replay validation.

- .gitattributes rules for LF C/documentation files, CRLF Visual Studio and
  Windows scripts, and binary asset handling.
- Expanded .gitignore coverage for Windows build products and Visual Studio
  caches while preserving platforms/ architecture directories.

- Root Visual Studio solution, native C project, and filters configured for
  Release/x64 using v145 and Windows SDK 10.0.26100.0.
- includes/ and src/ with ISO C17 compile-time platform checks and a minimal
  console entry point identifying itself as a development scaffold.
- IDE build instructions and ignore rules for generated output/local state.
- platforms/ target directories for Linux and Windows x86/x64/ARM32/ARM64
  and macOS x64/ARM64, with implementation boundaries and support status.
- Successful Windows Release/x64 scaffold build and executable smoke check.

- Concrete C17/local-build and cryptographic-provider proposals, with
  dependency evaluation and cross-platform qualification gates.
- Proposed binary record envelope with explicit byte offsets, signing
  domains, bounded lengths, and network-bound identities.
- Proposed synthetic intelligence payload, authorization/replay behavior,
  and positive/negative qualification cases.

- Mission and scope for intelligence distribution, company decisions,
  policies, doctrine, contracts, and possible future mining economics.
- Proposed component architecture separating deterministic validation,
  node coordination, storage, networking, integrations, and mining.
- Decision register separating established direction from unresolved
  protocol and economic choices.
- Delivery milestones with verification gates.
- Repository instructions requiring maintenance of this changelog.
- Supplied version-numbering and sovereignty policy references, with
  release rollover rules and dependency continuity requirements.

### Changed

- Adopted the owner's Visual Studio IDE-first Windows workflow with native
  solution/project files planned; removed CMake/CTest as the recommended
  build/test path and retained a separate portable Unix build proposal.

- Reframed README around the broader STN Chain mission and current
  documentation-only status.
- Clarified that PoW activation and native-coin economics remain undecided.

## 2026-09-06

### Changed

- Replaced MIT for current material with the STN Chain Proprietary
  Participation License; documented permitted unchanged participation
  and the restriction on modifications (0020931).
- Moved the license into docs/LICENSE.md and linked it from README (815d183).
- Renamed project documentation to STN Chain and described the ISO C
  redesign direction (8351c16).

### Removed

- Historical Go implementation to begin a fresh redesign (0a7ae52).
