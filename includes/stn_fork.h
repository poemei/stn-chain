/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_FORK_H
#define STN_FORK_H
#include "stn_chain.h"

typedef enum stn_fork_result {
    STN_FORK_CURRENT = 0, STN_FORK_CANDIDATE, STN_FORK_TIE,
    STN_FORK_INVALID_CURRENT, STN_FORK_INVALID_CANDIDATE,
    STN_FORK_ERROR, STN_FORK_UNRESOLVED, STN_FORK_UNSUPPORTED
} stn_fork_result;

typedef struct stn_fork_report {
    stn_fork_result result;
    int failed_side; /* 0: shared arguments/context, 1: current, 2: candidate. */
    stn_chain_report validation;
} stn_fork_report;

typedef struct stn_reorg_plan {
    int actionable; /* Only strictly greater validated candidate work. */
    size_t ancestor_index; /* Full histories start at genesis: index == height. */
    uint8_t ancestor_id[32];
    /* Half-open ranges in supplied histories. Detach in descending index
     * order; attach in ascending order. Informational when !actionable. */
    size_t detach_begin, detach_end, attach_begin, attach_end;
    size_t detached_count, attached_count;
    stn_chain_state current, candidate; /* Calculated, never caller claims. */
    uint64_t resulting_height; /* Current height if no change is actionable. */
} stn_reorg_plan;

/* Arithmetic only; NOT chain eligibility or authorization. Ignores height,
 * timestamps and identity. Output unchanged for invalid arguments. */
stn_data_status stn_work_order(const stn_work *current, const stn_work *candidate,
    stn_fork_result *out);

/* Both complete immutable histories include exact context genesis, 1..64
 * blocks each. Revalidate from EMPTY under ONE explicit PoW context; legacy
 * non-PoW is unsupported. No supplied state/work metadata is accepted.
 * All outputs/inputs must be disjoint and remain stable for the call.
 * Provider must implement deterministic SHA-256 (test doubles only in tests).
 * On invalid/unresolved/error input, plan is byte-for-byte unchanged.
 * Success writes a complete informational plan, actionable only for CANDIDATE.
 * Equal work retains current. No accepted state or block is mutated, no
 * storage/application/mempool side effects. Plan is not a reusable credential;
 * future application must revalidate against its then-current active tip. */
stn_fork_report stn_fork_evaluate(const stn_chain_context *context,
    const stn_block_span *current, size_t current_count,
    const stn_block_span *candidate, size_t candidate_count,
    stn_reorg_plan *out);
#endif
