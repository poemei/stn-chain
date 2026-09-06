# Delivery Milestones

Status: proposed sequence with evidence gates, not a release schedule.

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
