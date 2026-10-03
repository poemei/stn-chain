# Chain contention repair - October 1, 2026

Implemented and deployed to chain01. Changes remain uncommitted.

## Repair

Peer sessions now obtain an owned immutable snapshot through the validated storage
cache. Disk bytes/context still have to match; unknown or changed history is fully
validated. Cache/state ownership is serialized under the runtime lock, and all
shared state references are released before the peer session uses its private
bytes and scalar summary. Network requests hold no storage/cache lock and do not
repeat full replay. The original untrusted-input peer API still validates fully.

Block append uses the existing history-aware consensus evaluator for the new
candidate and its exact validated prefix. Historical share/issuance checks remain
available. Atomic disk replacement still precedes state/cache publication. Startup
does full replay and retains its validated result in the cache.

Files: new `includes/stn_peer_snapshot.h`, `src/stn_peer_snapshot.c`, its internal
header and `tests/test_peer_snapshot.c`; updates to Chain/storage/peer sources,
Linux/Windows startup, both build files, mining tests and documentation. Windows
peer test dependencies and two incorrect wide-path cleanup calls were repaired.
The private Chain helper declaration is in `src/stn_chain_internal.h`.

## Checks

- Linux production build, peer snapshot regression, storage-cache/mining suite,
  RESPONSE end-to-end (144 checks), INFO/read service (40,053 checks): passed.
- Windows production build; accepted-state (1,243 checks), P2P/sync/recovery
  (2,867 checks), snapshot/mining (1,649 checks): passed.
- Snapshot replies match the full-validation API; tests cover no repeated replay,
  private byte ownership, cache release, corruption, changed context and malformed
  requests. Mining tests compare full replay's tip/work/supply/balances and retain
  failed-write and invalid-candidate coverage.
- Isolated copy of live height 1443: four peer exchanges per connection took
  5-7 ms; block append took 34 ms. Restart fully validated the appended history
  at height 1444 and retained exact contract terms, the exact signed response and
  creator/recipient discovery. Post-startup work requests took about 1 ms.

## Deployment

- Installed SHA256:
  `e697eccd3a903920108b992c04b65f99d883570b0d5849896238e8b20bd65dd5`.
- Source/build: `/home/stnchain/stn-chain-contention-20261001`.
- Binary/history backups have suffix `20261001T195730Z-contention` in their
  original `/usr/local/bin` and `/var/lib/stn-chain` locations.
- Initial restart was delayed by old LAST-ACK P2P sockets. Only stale connections
  on the Chain service ports were cleared; normal full startup replay then finished.
- Live contract reads verified both participants' listing, exact original terms
  and exact signed response. Live mining and peer measurements are recorded below.

Live observation after startup: 185 Chain-verified shares, 12 accepted block
submissions, zero provider-busy log entries and zero backend transport failures.
Ten concurrent peer/RPC/telemetry rounds succeeded as sampled height advanced
1571 to 1581. Four-message public peer sessions took 0.453-3.719 seconds, mostly
under 0.71 seconds; combined INFO/work/two participant-list requests took
0.734-3.094 seconds. All telemetry reads succeeded. These external timings include
network and concurrent mining load and are distinct from isolated local benchmarks.

## Limits

Startup and changed/untrusted history still require full replay. Outbound sync's
per-prefix reconstruction is unchanged; this repair targets the measured inbound
lock starvation and append costs. Benchmarks are observations, not latency or
long-duration availability guarantees. No protocol, contract-address, signature,
authority, target or economic-rule changes were made.
