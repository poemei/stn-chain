# Chain mining stalls: investigation, 2026-10-01

Status: investigation baseline. The inbound/append repair is now implemented;
see CHAIN_CONTENTION_REPAIR.md for validation and deployment.

## Live evidence

The running Chain binary remains
`67979d0a4f9ca7bb563dc4ff0bd86759be1a13004b8c94b4eacb3889aaf6cc15`.
Local source is commit `db6c0ae` before this investigation's documentation.

Chain journal on chain01 (UTC):

- 14:30:46: incoming P2P connection accepted.
- 14:31:26: snapshot ready, 1,436 blocks; HELLO frame received.
- 14:32:08: HELLO response completed, 80 bytes.

Preparing the snapshot took 40 seconds; answering HELLO then took another 42
seconds. Stratum repeatedly logged mining-template provider error 8 during the
snapshot interval and obtained work at 14:31:26, exactly when the snapshot became
ready. A later process sample showed a running worker at approximately 100% CPU.
Accepted storage was only about 5.9 MB: byte volume alone does not explain the
repeated delay. Code inspection identifies full consensus reconstruction.

## Causes in source

1. `platforms/linux/stn_app_linux.c`, `inbound_client_thread`, calls
   `stn_storage_load` for every connection. `src/stn_storage.c` holds provider
   exclusion throughout both the disk read and full `stn_storage_decode`.
   Mining cannot acquire that provider while replay runs and returns an error.
2. `src/stn_peer.c`, `stn_peer_serve`, calls `local_validate` before every request,
   including HELLO, STATE, header retrieval and block retrieval. This reconstructs
   the complete immutable session history again. That second replay no longer
   needs the storage lock, but consumes CPU and delays the reply. Client timeouts
   and retries can produce more snapshot/replay work.
3. `src/stn_storage.c`, `stn_storage_extend_cached`, reconstructs the complete
   prefix plus new candidate for each append. The earlier cache repair removed
   duplicate passes around that replay but did not make acceptance incremental.
4. `src/stn_peer.c`, `sync_session`, reconstructs each successive downloaded
   prefix. This is another scaling cost; it is source-confirmed, but this capture
   does not establish that it caused the measured inbound HELLO delay.

## Smallest safe repair sequence

First repair inbound serving, the directly measured starvation source:

- Give sessions owned, immutable accepted-history snapshots taken from the
  existing validated cache. Match actual persisted bytes and the immutable
  validation context before reuse; do not trust disk metadata or peer claims.
- Serialize cache ownership/reference operations with the runtime's state lock.
  Existing state references are not a license for unsynchronized sharing.
  Never hold storage exclusion or the dispatch lock across peer network waits.
- Validate a session snapshot once, then serve requests from that exact snapshot.
  Retain the existing validating API for arbitrary/untrusted inputs. An internal
  validated-snapshot serving API must not accept freely supplied state metadata
  as proof of validation.
- Invalidate/rebuild snapshots when accepted bytes or validation context change.
  Keep session bytes alive until the last reader releases them.

Then repair append latency:

- Validate only the new candidate against the exact previously validated prefix
  and accepted state, retaining access to historical bytes for economic evidence.
- Reuse the same history-aware consensus evaluator used during reconstruction.
  The existing public state-only `stn_chain_validate_candidate` is NOT a drop-in
  replacement: share issuance and compensation can require earlier history.
- Publish accepted state/cache only after successful atomic storage replacement.
  Keep full replay for restart, unknown/changed history, and reference checks.

Do not address this by disabling validation, dropping contracts/history, changing
targets or lengthening timeouts again. No wire, contract, address, authority or
economic rule needs to change.

## Required focused proof before deployment

- Repeated HELLO/STATE/header/block requests must not repeat full replay and must
  return byte-identical responses to the current validating implementation.
- Concurrent peer sessions must not create provider-busy mining failures through
  replay lock ownership; INFO and contract-list reads must remain responsive.
- Changed/corrupted bytes, changed context and untrusted candidate histories must
  still receive full validation and rejection where required.
- Incremental append must match full replay on accepted state, contract responses,
  replay protection, balances, rewards and work; failed append must publish nothing.
- Live verification must cover peer qualification, share acceptance, block
  acceptance, contract reads and continued Stratum telemetry during synchronization.

This investigation made no runtime or consensus-code changes. The Stratum
telemetry fix remains deployed; it does not solve this Chain-side contention.
