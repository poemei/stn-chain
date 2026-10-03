# Miner pause ticket cfb08b - October 2, 2026

## Observed cause

On chain01, Stratum repeatedly reported Chain RPC code 11 and invalidated work.
The accepted-history file had not advanced since October 2 16:02 UTC.
A live debugger snapshot captured a request executing node-service snapshot ->
full history reconstruction under the shared dispatch mutex. Hundreds of RPC
threads (603 at shutdown) waited on the same mutex, including session cleanup.
The installed binaries matched the prior repair, so this was not a rollback.

The previous peer snapshot repair did not remove the second history replay in
ordinary node RPC reads. Stratum's telemetry remained reachable and correctly
reported chain_connected=false and work_available=false during the outage.

## Narrow repair

The mining service now passes its exact validated storage view to a separate
trusted node-service entry point. Snapshot-based reads reuse that accepted state.
The ordinary entry point still reconstructs history; storage cache byte/context
checks still run before the trusted call. No network-supplied state is trusted.
No consensus, contract, address, transaction, or wire encoding changed.

Files changed for this increment:
- includes/stn_node_service.h
- src/stn_node_service.c
- src/stn_mining.c
- tests/test_peer_snapshot.c (extends the existing uncommitted fixture)
- docs/CHANGELOG.md and this report

## Validation

Linux production build passed. Snapshot/mining tests: 1658 checks, zero failures.
Contract RESPONSE end-to-end: 144 checks, zero failures. Linux INFO helpers:
40053 checks, zero failures. The regression compares full and validated block/INFO
responses and asserts no repeated consensus hashing, with mismatch/corruption
negative cases. Existing compiler warnings remain; no new Windows build was run.

## Deployment and live results

Installed on chain01 October 3 01:53 UTC; normal startup validation completed
before the live probes. Backup suffix: 20261003T015218Z-rpc-replay. Binary SHA256:
492a088e17a066a514dba59a0304afbb6959dff8201dd3f168ceced69ae6fb36.
Build source: /home/stnchain/stn-chain-rpc-replay-20261003.

Eight concurrent peer/read/mining rounds passed. RPC batches (INFO, genesis/tip
block reads, mining work and both participant contract lists) took 1.250-1.797s
from the external client; peer exchanges took 0.468-0.593s. Stratum resumed work.

Follow-up October 3 14:59 UTC: same binary and process, five threads instead of
603. Accepted height advanced 2794 -> 3666. Logs since 01:58:30 UTC recorded
7,887 verified shares, 872 successful block submissions and zero backend
connection-unavailable messages. Telemetry reported two miners, available work,
Chain connected and about 6.0 MH/s. Fresh external tip-block + mining-work reads
completed in 1.063s. These are observed results, not an indefinite guarantee.

## Limits

This addresses the captured snapshot replay path. Historical record/cursor
searches and transaction-status reconstruction still use their existing full
validation paths; those were not the captured stack. A bounded live observation
cannot establish that every cause of miner disconnection is eliminated.
Source changes are uncommitted; no GitHub push was performed for this increment.
