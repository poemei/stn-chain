# Fork Choice and Reorganization Planning

Status: implemented local ISO C17 evaluation and in-memory planning. No
reorganization application, persistent mutation, or distributed node exists.

## Eligibility and comparison

stn_fork_evaluate takes two complete histories, each including genesis and
containing 1 through 64 blocks, under one immutable stn_chain_context.
Both histories are revalidated from EMPTY through the existing candidate
validator. Structural/body/transaction integrity, network, exact genesis,
links, timestamps, fixed target, PoW, and checked cumulative work must pass.
There is no caller-supplied accepted-state or cumulative-work input to forge.
Legacy non-PoW contexts are explicitly unsupported. First failure wins:
validate current first, then candidate; diagnostics identify side and index.

Compare calculated 256-bit work as unsigned big-endian bytes. Greater work
wins. Equal work returns TIE and retains current, including identical chains.
Height, timestamp, arrival order, publisher identity, transaction count and
organizational authority do not influence comparison. Missing/error providers
fail closed; a plan is never published on validation failure.

## Fixed-target scope resolution

The existing v3 policy requires every block, including genesis, to use the
same context target. Consequently valid competing histories sharing genesis
have equal per-block work. A shorter valid history cannot have greater work
under this policy. Changing that fact requires a separately defined target
policy; this increment preserves all existing target rules and regressions.

stn_work_order is a pure arithmetic helper, not a chain acceptance API. Tests
show three easiest-target contributions (6) losing to one harder-target
contribution (16), and the reverse ordering. These are different-policy work
vectors, NOT competing chains accepted by the fork evaluator. No metadata-only
comparison can create a plan. Future varying-target rules can use this same
ordering, but such rules and automatic adjustment are deferred.

## Common ancestor and ranges

After both histories validate, compare their calculated block identifiers
from genesis until the first difference. The last matching prefix block is
the common ancestor; full-history index equals height. Network or genesis
mismatches are ineligible, not independent networks eligible for reorganization.
With a stable deterministic provider and one exact genesis context, two valid
histories always share at least genesis. The no-ancestor case fails closed.

The complete plan contains ancestor index/identifier, calculated current and
candidate states (tip IDs, heights and cumulative work), resulting height,
and half-open detach and attach ranges into the supplied histories. Counts
are range lengths. Detach in descending index order from the old tip; attach
in ascending index order from the ancestor's successor. No payloads are lost
or copied: callers retain their immutable histories to resolve those ranges.
Direct extension detaches nothing; identical chains have empty ranges.

Only strictly greater candidate work sets actionable. Equal/lower-work plans
are informational and resulting_height stays at current height. Their ranges
still describe branch differences, not instructions to execute a change.

## Atomicity and trust boundary

Evaluation uses local temporary state and bounded identifier arrays. It writes
the complete plan once after every check succeeds. On invalid, unsupported,
unresolved or failed input, the plan remains byte-for-byte unchanged. Inputs
are borrowed and never mutated. Provider/context and bytes must stay stable;
input and output objects must not overlap. No ambient time or allocation is
used by the evaluator. A 64-block bound applies per supplied full history;
suffix-only and larger-history evaluation are currently unsupported.

There is no application interface. A returned plan is neither authenticated
nor a durable authorization token. Future application must revalidate against
the then-current active tip and atomically update all affected state. Passing
local rules still does not establish record signatures, organizational
permission, cross-block replay safety, intelligence truth, or full consensus.

## Transaction implications and qualification

Future node coordination must account for detached confirmations, replay
indexes and any mempool state before atomically switching branches. This
implementation exposes block ranges; it neither discards nor resubmits
detached transactions, builds a mempool, or changes wallet state.

Tests cover extension, prefix preference, equal and identical histories,
genesis/deeper divergence, ordered ranges, all candidate block truncations,
network/genesis/link/body/target/PoW failures, both-side provider failures,
overflow, untouched outputs/inputs, forged output metadata, and arithmetic
work ordering. Synthetic hashes are explicit test fixtures; a real Windows
SHA-256 genesis/child fixture also qualifies the production path. All previous
checks remain unchanged. Only Windows Release/x64 has been executed.

Deferred: varying/automatic target rules, longer-history storage/indexing,
reorganization application, persistence, networking/discovery/RPC, mining,
Stratum, wallet/mempool behavior, contracts, fees/gas/rewards and coin economics.
