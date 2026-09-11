# Delivery Milestones

Status: proposed sequence with evidence gates, not a release schedule.

## Phase 9 — Pending Submissions and Deterministic Block Assembly

**COMPLETE for the authorized Windows Release/x64 integration proof (2026-09-09).**
The separate `stn-chain-phase9-test.exe` uses the same executable runtime,
STNC dispatcher, validators, candidate construction, real SHA-256/PoW, and NTFS
storage, with explicitly scripted identity hooks. This is not production
signature/authority qualification; the production executable remains fail-closed.

The 1,544 executable integration checks prove RPC admission and canonical IDs,
pending accounting, concurrent duplicates/status, ordered multi-item candidates,
stable work, invalid/stale work rejection, nonce-only solutions above 32 bits,
acceptance and exact cleanup, clean restart/content reconstruction, and continued
submissions through height 65 with reconstructed work at height 66. The existing
1,130,094 C checks (including peer adoption, unrelated-entry preservation, failure
atomicity and catch-up/reorg regressions) and 34 build probes also pass.
No Phase 10 work was started.

Reorg re-addition, pending persistence/gossip, production identity providers,
Stratum integration and other future phases remain deferred. The broader
milestones below retain their original evidence requirements.

## Phase 10 — STN-Chain ↔ STN-Stratum Integration

**Chunk 1 COMPLETE on Windows Release/x64 (2026-09-09).** Actual current
C:\poes_projects\stn-stratum server/client sources are qualified for configured
STNC connection, INFO, mining-template retrieval, solved-work request/result
contract, stale/unavailable/errors, reconnect, and concurrent Chain RPC.
81 cross-process integration checks, 27 Stratum parser checks, two existing
session assertions, all 1,130,094 Chain C checks and 34 probes pass.

The work corrected Stratum-side bounds, missing INFO, response-shape validation,
send/receive timeouts and unsafe mutation retry; Chain wire/consensus semantics
are unchanged. The independent driver links the same client and Windows transport
used by the running Stratum server. External miners, jobs/shares and Chunk 2 are
not qualified or started here. Production identity remains fail-closed.

**Phase 10 Chunk 2 COMPLETE (Windows Release/x64, 2026-09-09).** The actual
Stratum server maps real Chain 0x2002 templates to deterministic STNM jobs with
exact candidate/work ID/target/nonce preservation, session-independent bytes,
replacement on changed work and no fabricated/cached current jobs when unavailable.
93 new observation-only checks pass; 81 Chunk 1 checks, 27 Stratum parser checks,
two session assertions, 1,130,094 Chain C checks and 34 build probes remain passing.
No production mapping defect was found. Accepted test content uses the authorized
scripted-identity Chain runtime. No miner execution/shares or Chunk 3 work occurred.

**Phase 10 Chunk 3 COMPLETE (Windows Release/x64, 2026-09-09).** Actual STNM
miner-result messages traverse the current Stratum server and STNC 0x2003 to
Chain acceptance/rejection. 115 new checks prove exact candidate/target/work ID,
full-width nonce, malformed/unknown/replaced job rejection, bounded session
isolation/reconnect and accepted pending cleanup/restart. Two Stratum protocol
handling defects were minimally repaired; Chain consensus remains unchanged.
Prior Chunk 1/2, Phase 9 lifecycle, C suites and build probes pass. The result
generator and identity hooks are explicit test fixtures, not production miner
or identity qualification. Chunk 4 was not started.

**Phase 10 Chunk 4 COMPLETE (Windows Release/x64, 2026-09-09).** 148 actual-
process failure-state checks qualify Chain loss/recovery, submission during outage,
empty-work reconnect, exact same-work restoration, changed work across miner
disconnect, stale results, partial-frame disposal, session isolation and Stratum
restart. One Stratum defect was repaired: failed submission now invalidates cached
current work immediately instead of waiting for polling. Chain consensus is
unchanged. Prior Phase 9 and Phase 10 suites pass. Hardware-miner qualification,
discovery, durable queues and Chunk 5 remain out of scope.

## Phase 10 — Chain ↔ Stratum Integration COMPLETE

Qualified on Windows Release/x64 (2026-09-09). Chain-issued work passes through
actual STNC 0x2002, deterministic STNM jobs, fixture miner results and Stratum's
STNC 0x2003 submission into independent Chain validation and persistence. Work
identity, full target and canonical candidate remain exact except the permitted
64-bit nonce. Invalid/stale results remain rejected; bounded sessions agree.

The final 125-check lifecycle stops Chain and Stratum once, recovers identical
accepted INFO and block bytes with Stratum absent, then starts a fresh coordinator.
Unavailable work yields no placeholder/cached current job. New height-two work
extends the recovered accepted tip with a new Chain work ID. Stratum coordinates
mining; STN Chain remains consensus authority. No production fix was necessary.

All 562 Phase 10 process checks (81/93/115/148/125), 1,544 Phase 9 checks,
1,130,094 Chain C checks, 27 parser checks, two session assertions and 34 build
probes pass. Release/x64 builds have zero warnings/errors. Identity and result
fixtures remain test-only; hardware, production identity, accounting, performance
and other platforms are not qualified. Phase 11 has not started.

## Broader milestone table

| Stage | Deliverable | Evidence required to advance |
| --- | --- | --- |
| 0. Mission baseline | Mission, architecture, decision register, changelog | Scope and unresolved choices are explicit; document links work |
| 1. Protocol foundation | Toolchain decision, canonical encoding, identities, record envelope, test vectors | Equivalent bytes and validation outcomes across target platforms |
| 2. Intelligence path | Signed submission, local validation, durable block/state, second node, test consumer | Valid record delivered; invalid/replayed records rejected; restart and missing-data recovery demonstrated |
| 3. Consensus | If PoW is selected: CPU mining, target validation, adjustment, chain selection, reorganization | Competing branches, invalid work, stale jobs, partitions, and recovery tested |
| 4. Company contracts | Authorized policy lifecycle, acknowledgment, supersession | Unauthorized transitions fail; offline catch-up and branch changes have defined outcomes |
| 5. Economic testnet | If activated: native coin, transfers, fees, rewards, wallet interfaces | Supply/accounting invariants, replay protection, invalid spends/rewards, and reorganization tested |
| 6. Participation release | Hardened node, supported mining backends, signed packages, operating documentation | Parser fuzzing, resource limits, crash recovery, cross-platform checks, and security review |

Stage 2 is isolated development work. Any temporary block-production harness
is explicitly non-consensus and does not qualify for public operation.
Contract design can proceed alongside consensus design; general-purpose
execution requirements must be resolved before freezing the contract engine.

GPU and compatible ASIC work follows algorithm and work-interface decisions.
Coin-market integration is outside the core implementation milestones.

## First end-to-end exercise

Use synthetic intelligence and test identities. Submit a signed report via
an API adapter or explicitly labeled adapter test fixture, validate it on
node A, persist it, synchronize it to node B, and deliver it to a test
consumer shaped for future Sentinel_Daemon integration.

Demonstrate tamper rejection, duplicate handling, interrupted persistence,
restart, and a consumer reconnecting after missing records. A fixture is not
evidence of completed integration with the actual STN-Labz API.

## Working discipline

Update docs/CHANGELOG.md with each meaningful change. Keep protocol decisions
and implementation status accurate. Run checks suited to each increment;
documentation-only changes require link and diff checks, not invented runtime
tests. Do not treat prototypes as evidence of production readiness.

## Phase 11 — Deterministic Difficulty Adjustment — COMPLETE

Windows Release/x64, 2026-09-10. The following chunk checkpoints are historical;
the final Chunk 4 qualification below completes this phase.

**Chunk 1 COMPLETE: calculation foundation only (2026-09-09).** The owner supplied
60-second spacing, 60-block windows, boundary height 60, accepted H-60/H-1
endpoints, 3600-second expectation, 900/14400-second clamps, floor arithmetic and
existing target limits. See POW.md for the exact API, arithmetic and bootstrap rules.
193 targeted checks qualify canonical deterministic next-target calculation;
1,130,287 Chain C checks and 34 probes pass. Phase 9 (1,544) and all Phase 10 (562)
process checks remain passing. Release/x64 builds have zero warnings/errors.
Normal acceptance and templates still use the qualified fixed target. Dynamic
adjustment is not operational. Chunk 2 has not started.
### Phase 11 Chunk 2 — COMPLETE (Windows Release/x64, 2026-09-09)

Required-target consensus is activated. Validation and mining share one
branch-derived rule; target equality remains separate from real SHA-256 PoW.
Adjusted state reconstructs from canonical persistence, branch-specific reorgs
retain ordinary cumulative-work preference, and the actual Stratum path preserves
the changed target through acceptance/restart. See POW.md for exact rules and limits.
1,658 new C checks and 834 adjusted-target process checks pass; total Chain C
checks are 1,131,945. Prior Phase 9 (1,544), Phase 10 (562), and 34 probes pass.
Builds have zero warnings/errors. Phase 11 as a whole is not marked complete;
later qualification remains outside this increment. Chunk 3 has not started.
### Phase 11 Chunk 3 — COMPLETE (Windows Release/x64, 2026-09-10)

The authorized 320-bit cumulative-work representation preserves the full target
and height domains. STNC/STNP v2 transport 40-byte work without truncation;
canonical block storage reconstructs it without a format change. Legacy
fixed-target development evidence receives strict current-rule validation.
See POW.md for the exact mathematical bound and qualification scope.
Chunk 4 has not started; Phase 11 as a whole remains open.
Evidence: 223 new C checks; 1,132,168 total C checks, 34 probes, 1,544 Phase 9,
562 Phase 10 and 834 adjusted-target process checks, 27 Stratum parser checks
and two session assertions. Zero warnings/errors/failures. No Chunk 4 work.

## Phase 11 Chunk 4 — final convergence qualification COMPLETE (2026-09-10)

Deterministic difficulty adjustment is qualified on Windows Release/x64.
Required targets follow validated branch ancestry through validation, mining,
STNC/Stratum, actual P2P, reorganization and reconstruction. Cumulative work
remains exact unsigned 320-bit / 40-byte canonical big-endian. Consensus,
protocol layouts and the strict legacy-history boundary are unchanged.

The existing C peer harness adds 1,109 checks with two independent NTFS-backed
node states and actual loopback Winsock exchange. Both start at shared genesis.
One bounded partition permits independent 60-block branches: 1,800/7,200-second
ancestry spans derive harder/easier targets at height 60. Both nodes validate
received evidence independently and converge on the greater-work branch. One
node replaces 60 blocks from the common genesis through existing atomic storage
adoption. A wrong-target block satisfying its encoded-target PoW, accompanied
by a fabricated maximum advertised work value, is rejected with accepted state
and stored bytes unchanged.

Both node states are discarded and reconstructed by reopening canonical stores;
all 61 block byte sequences, tips, current targets, 40-byte work and next targets
agree. Fresh STNC mining work uses the converged target; the pre-reorg job is
stale. A separate scripted block-hash fixture repeats exchange/reconstruction
above the former 256-bit work limit. The ordinary scenario uses real SHA-256.
Node-state teardown/reload is in-process; the existing 834-check adjusted-target
process lifecycle additionally stops/restarts Chain and Stratum and proves
continued mining. This does not add automatic P2P process orchestration.

Final evidence: 1,133,277 Chain C checks, 34 build probes, 1,544 Phase 9 checks,
562 Phase 10 checks, 834 adjusted-target process checks, 27 Stratum parser checks
and two session assertions. Windows Release/x64: zero warnings/errors/failures.
No production defect or production-code change was required in Chunk 4.
Production identities, hardware, Internet operation, performance and other
platforms remain unqualified. Phase 12 was not started. No commit or push.

## Phase 12 — Automatic P2P Orchestration

### Block 1 — deterministic peer candidate foundation COMPLETE (2026-09-10)

The existing peer core now provides a bounded 64-entry IPv4/port candidate set,
canonical endpoint identity and deterministic sorted enumeration. Malformed
endpoints, duplicates and full capacity have explicit outcomes without eviction
or unrelated mutation. This is local resource/selection policy, not consensus.
See PEER_PROTOCOL.md for the API, eligibility rules and platform boundary.

440 targeted checks pass; total Chain C checks: 1,133,717. Also passing: 34 build
probes, 1,544 Phase 9, 562 Phase 10, 834 adjusted-target process checks, 27 Stratum
parser checks and two session assertions. Windows Release/x64 production, test
and scripted-runtime builds: zero warnings/errors/failures. No prior production
defect required correction; existing consensus and transport behavior is unchanged.

Phase 12 remains open. Automatic connections, retries, reconnect, failover and
discovery await later authorization. No other-platform qualification. Block 2
was not started. This increment was not committed or pushed.
### Block 2 — automatic outbound connection management COMPLETE (2026-09-10)

Repeated configured IPv4 peers feed the qualified candidate set. A portable,
monotonic-paced outbound lane selects deterministically, reuses existing P2P
handshake/synchronization, retains usable sessions and closes/advances after
failure. The Windows executable composes it with private scratch and existing
RPC/pending exclusion. Candidate preference never confers consensus authority.

Passed 165 new C checks (121 policy + 44 real Winsock), 30 executable checks,
1,133,882 total Chain C checks, 34 probes, 1,544 Phase 9, 562 Phase 10 and 834
adjusted-target process checks. Windows Release/x64 builds: zero warnings,
errors and failures. Block 1's 440 checks remain passing. No existing production
defect required correction; session reuse and bounded outbound I/O are additions.
See PEER_PROTOCOL.md and BUILD.md for runtime limits and qualification scope.

Phase 12 remains open. Discovery and later authorized orchestration work remain.
Block 3 was not started. This increment was not committed or pushed.

### Block 3 — peer discovery COMPLETE (2026-09-10)

Established peers may advertise a bounded list of configured IPv4 candidates via
STNP v2 capability bit 2 and one `GET_PEERS`/`PEERS` exchange. The 386-byte maximum
payload carries at most 64 fixed-width endpoints. Receivers validate the complete
batch through the Block 1 store, omit configured self endpoints, and commit only
after deterministic sorting and capacity checks succeed. The existing Block 2
manager then uses admitted candidates; no second store or connection path exists.

Passed 974 new C checks (955 codec/policy and 19 real Winsock failover), 40
executable discovery checks, and 30 outbound/27 pending executable regressions.
The resulting 1,134,856 Chain C checks, 34 build probes and prior Phase 9/10/11
process suites passed with zero failures on Windows Release/x64. Discovery does
not alter accepted state, storage, pending state or consensus. No external
bootstrap, gossip, scoring, reputation, banning or other platform qualification.

Phase 12 remains open. Block 4 was not started. This increment was not committed
or pushed.
