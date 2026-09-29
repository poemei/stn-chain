/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_block.h"
#include "stn_economy.h"
#include <assert.h>
#include <stddef.h>

/*
 * Every qualifying share creates two protocol transactions: evidence and
 * deterministic issuance. The block transaction bound must be able to drain
 * the expected share-generated protocol load for one solved-block interval,
 * with at least one slot left for non-share Chain traffic. This is a liveness
 * invariant: the local pending bound must not be guaranteed to grow merely
 * because miners participate at the protocol's intended share factor.
 */
int main(void)
{
    const size_t mining_transactions_per_interval =
        (size_t)STN_SHARE_FACTOR * 2u;

    assert(STN_SHARE_FACTOR > 0u);
    assert(STN_BLOCK_MAX_TRANSACTIONS > mining_transactions_per_interval);
    assert(STN_BLOCK_MAX_TRANSACTIONS == 32u);
    return 0;
}
