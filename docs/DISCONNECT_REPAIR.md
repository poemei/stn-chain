# Core connection repair — September 30, 2026

The lower-left Disconnected indicator was a real loss of Core's main Chain
connection. The POE, stnc-treasurer and STN-Labz profiles all logged RPC transport
failures and repeated Stratum submission timeouts.

## Cause and repair

Mining-service RPCs reconstructed accepted history on every request while
holding the dispatch lock. Block acceptance repeated that validation several
more times. Concurrent client requests accumulated beyond their timeouts.

Added process-owned validated-history reuse in stn_storage_cache.h/.c, with its
own test .c. Every request still reads disk under provider exclusion and compares
all bytes. Only identical history under the immutable Chain context reuses
validated state. Changed history and startup use normal full validation.

The append path still fully validates the entire history including the candidate
block. It reuses the exact validated prefix beforehand and the exact committed
state afterward, avoiding redundant reconstruction. Cache publication follows
successful atomic disk publication; failure leaves authoritative state unchanged.
No consensus serialization, contract address, authority or lifecycle rule changes.
No Windows Core socket changes were made.

Integrated this into mining/storage and Linux/Windows runtime ownership, updated
builds, and exercised existing mining/admission/CREATE tests through the cache.

## Validation

- Linux and Windows production builds passed.
- Focused cache/mining regression: 1,628 checks, zero failures. Covers corruption,
  changed context, failed reads/writes, state ownership, admission and mining.
- Rebuilt a height-945 copy: original contracts, exact terms and signed RESPONSE
  preserved. Isolated block append: 9.663 seconds; next work read: 0.002 seconds.
- Final public persistent-connection check: 24 successful replies, zero error
  replies or transport failures, spanning heights 959–965.
  Maximum INFO/balance/work times: 0.125/7.359/0.204 seconds.
- Existing desktop profiles reconnected and resumed accepted shares and height
  updates. Their native controls showed Connected and populated balances.
- Contract listing for creator and recipient, exact original terms and accepted
  RESPONSE passed again (0.172–0.204 seconds).

The intermediate read-only optimization was insufficient: a longer observation
caught a remaining mining timeout and an application-level PROVIDER reply.
The final deployment includes append-path reuse; the final live results above
refer to that deployment. The desktop logs still recorded Stratum submission timeouts after the final
deployment (all three profiles at 19:42:10 Pacific; treasurer again at 19:43:45).
Main Chain RPC stayed connected in the observed interval, but mining-session
reliability is not fully resolved. P2P peer discovery/synchronization is outside
this repair. Full block validation still has a cost; no indefinite uptime claim.

## Deployment

Deployed on chain01. Final binary SHA256:
67979d0a4f9ca7bb563dc4ff0bd86759be1a13004b8c94b4eacb3889aaf6cc15

Previous binary and accepted history retained under suffix
20261001T024054Z-append-cache (UTC; September 30 Pacific). The earlier restart
was held up by orphaned RPC sockets; only stale sockets on server port 18473 were
cleared after the service stopped. The final restart also cleared those leftovers.
No desktop wallet/identity files were modified. Source changes are uncommitted.
