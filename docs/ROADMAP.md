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
