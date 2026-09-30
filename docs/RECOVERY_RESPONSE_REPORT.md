# STN Chain rollback recovery and RESPONSE repair

Date: 2026-09-30. Repair prepared for GitHub; this is not a numbered release.

## Cause and recovery

Rollback commit `59d8417` removed the verified contract-creation, discovery and
mining fixes from `ae0edb5`, not just RESPONSE work. The older node could not
reconstruct accepted contract block 674. Production subsequently restarted from
height zero. The saved pre-reset history contained 697 blocks through height 696
and four accepted contracts; their bytes had survived in the backup.

The original accepted history was validated before restoration, including its
common genesis and greater accumulated work than the reset branch. The displaced
binary and reset history were preserved. The recovered node resumed mining.
Before deploying the RESPONSE build, the actual production history through
height 859 was reconstructed in an isolated process and all four original
contracts were confirmed as ISSUED, sequence one.

Restored mining fixes preserve room for share/reward pairs and accept valid work
against its own template on the same parent. Restored contract fixes validate
signed CREATE with the established typed issuer identity, perform semantic
pending/candidate checks, and publish independent snapshots for responsive reads.

## RESPONSE behavior

Transaction type 11 records signed participant text against an existing accepted
contract. It is not a lifecycle action and does not change the original address,
terms, state or sequence. The contract ID, responding public key and exact text
are signed together. The actor's typed identity must occur in the immutable
accepted draft. A contract first created in the current candidate block is not
yet eligible. Admission, candidate assembly and block validation use the same
response acceptance component. Block history is authoritative after restart.

Read RPC 14 retrieves exact signed records by original contract address and
zero-based response index, with total count. Duplicate contract/actor/text is
rejected as replay. The inherited limits are 65,536 text bytes and 256 accepted
responses across the snapshot. See CONTRACTS.md for the complete wire layout.
STNC Core was not modified; rendering and submitting responses in Core requires
client support for these protocol messages.

## Validation performed

- Windows production build passed using build.cmd.
- Windows and Linux RESPONSE end-to-end: 144 checks, zero failures. Signed A/B
  CREATE, admission, real proof-of-work acceptance, B's response, durable
  acceptance, history reconstruction and exact signed retrieval passed.
- Negative cases covered unknown/unaccepted contract, wrong contract binding,
  nonparticipant, malformed/truncated payload, invalid/tampered signature,
  pending duplicate, accepted replay, invalid candidate filtering and failed
  storage writes without accepted-state mutation.
- Existing lifecycle/snapshot test: 389 checks, zero failures on both platforms.
  Three old contract fixtures were updated to the established CREATE rules;
  signature verification and lifecycle assertions were preserved.
- All 17 focused Linux Chain, contract, mining and snapshot-read tests passed.
  These include 1,609 mining/pending checks, 1,243 Chain context checks and
  40,053 Linux read-helper checks.
- Isolated socket test held outbound peer synchronization stalled: contract
  list/state reads stayed below one second, including concurrent connections.
- A prior broad exploratory run found unrelated old fixture/build-harness
  failures. No claim is made that the entire repository test collection passes.
  Further unrelated test cleanup was excluded per the user's narrowed request.

## Deployment

Installed on chain01: `/usr/local/bin/stn-chain`.
Build source: `/home/stnchain/stn-chain-response-20260930`.
SHA-256: `75772e6fce4ff20ebd94a523b746624556dd3a1a3579286c3fa1f52ba0c8cebc`.
Previous binary and history backup suffix: `20260930T141441Z-response`.
Deployment replaced only the executable, preserving the recovered chain history.
The existing Linux listener briefly failed to bind during restart until prior
connections expired; the service then started successfully at height 859.

## Files changed

- `Makefile`
- `build.cmd`
- `docs/CHANGELOG.md`
- `docs/CONTRACTS.md`
- `docs/MINING_WORK.md`
- `includes/stn_chain.h`
- `includes/stn_contract_query.h`
- `includes/stn_contract_response_acceptance.h`
- `includes/stn_contract_snapshot.h`
- `includes/stn_contract_transaction.h`
- `includes/stn_pending.h`
- `includes/stn_rpc.h`
- `platforms/linux/stn_app_linux.c`
- `src/stn_block_compensation_acceptance.c`
- `src/stn_chain.c`
- `src/stn_contract_query.c`
- `src/stn_contract_response.c`
- `src/stn_contract_response_acceptance.c`
- `src/stn_contract_snapshot.c`
- `src/stn_mining.c`
- `src/stn_node_service.c`
- `src/stn_pending.c`
- `src/stn_rpc.c`
- `src/stn_share_pending.c`
- `src/stn_transaction.c`
- `tests/test_contract_pending.c`
- `tests/test_contract_response_acceptance.c`
- `tests/test_contract_snapshot.c`
- `tests/test_contract_transaction.c`
- `tests/test_linux_info.c`
- `tools/test-linux-info.ps1`
- `includes/stn_contract_response_query.h`
- `src/stn_contract_response_query.c`
- `tests/test_contract_response_e2e.c`
- `tests/test_contract_response_query.c`
- `tests/test_mining_mempool.c`


## Final live results

- CREATE admitted and accepted in block **860**.
- B's signed RESPONSE admitted and accepted in block **861**.
- Contract: `stnc0_53c19c9410b4e4783a13f030421c68507b0e7e5d257561a64a315ab736313b4a`.
- Both creator and recipient list the original contract automatically through
  the Chain list API. It remains ISSUED, sequence one after the response.
- Response API returns the exact submitted signed record and text:
  "B responds: the original terms have been received."
- A fresh node process reconstructed a copy of the actual accepted production
  file through height 861. The exact response, both participant listings and all
  four recovered original contracts survived that rebuild.
- Final live creator/recipient list reads took 0.188 / 0.297 seconds; response
  retrieval took 0.281 seconds. Submission remains on the serialized processing
  path (the response admission took about 36 seconds); read responsiveness is
  verified separately and is not a claim of instant submission processing.
- Live Stratum logs again show verified qualifying shares after deployment.
- No Core or GUI changes. The new diagnostic uses public test identities and
  explicitly states that it creates no obligations or payments.

During final verification, 16 local files were externally replaced with older
copies. Those displaced copies were preserved under the workspace's
`work/local-before-response-restoration`, then the local files were restored from
the exact tested server build source. An unrelated `config/chan_config.json`
was preserved. The deployed binary was unaffected. The tested local repair is included in this commit. A complete source recovery archive was saved in workspace outputs.
