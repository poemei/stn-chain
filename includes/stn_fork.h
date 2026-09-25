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
/* Successful evaluate transfers two references to a fresh plan. */
void stn_reorg_plan_release(stn_reorg_plan *plan);

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
/* Complete histories validated incrementally with constant working state.
 * Unlike the bounded batch wrapper, total history is not capped at 64. */
stn_fork_report stn_fork_evaluate_history(const stn_chain_context *context,
    const stn_block_span *current,size_t current_count,
    const stn_block_span *candidate,size_t candidate_count,stn_reorg_plan *out);
/* Evaluate a candidate assembled from an already accepted immutable prefix plus
 * an untrusted suffix without allocating a second complete span table. The
 * prefix is still revalidated from genesis as part of candidate reconstruction;
 * no cached state or caller work claim is authoritative. */
stn_fork_report stn_fork_evaluate_suffix(const stn_chain_context *context,
    const stn_block_span *current,size_t current_count,size_t prefix_count,
    const stn_block_span *suffix,size_t suffix_count,stn_reorg_plan *out);
typedef enum stn_cursor_reorg_result {
    STN_CURSOR_REORG_CURRENT=0, STN_CURSOR_REORG_COMMON_ANCESTOR,
    STN_CURSOR_REORG_NO_COMMON_ANCESTOR, STN_CURSOR_REORG_MALFORMED,
    STN_CURSOR_REORG_UNAVAILABLE, STN_CURSOR_REORG_PROVIDER
} stn_cursor_reorg_result;
typedef struct stn_cursor_ancestor {
    uint64_t height;
    uint8_t block_id[32];
} stn_cursor_ancestor;
/* Current and optional retained full histories use existing immutable span
 * ownership. Revalidate evidence; never accept claimed ancestry. NULL/0 retained
 * history means unavailable evidence, not implicit genesis. Output assigned only
 * for COMMON_ANCESTOR. No retention, fetch, reactivation or recovery is performed.
 * All inputs/output disjoint; caller holds current accepted selection stable. */
stn_cursor_reorg_result stn_chain_resolve_cursor_reorg(const stn_chain_context *context,
    const stn_block_span *current,size_t current_count,
    const stn_block_span *retained,size_t retained_count,
    const stn_chain_cursor *cursor,stn_cursor_ancestor *out);
typedef enum stn_consumer_recovery_result {
    STN_RECOVERY_CURRENT=0, STN_RECOVERY_FROM_START, STN_RECOVERY_AFTER_CURSOR,
    STN_RECOVERY_UNAVAILABLE, STN_RECOVERY_MALFORMED, STN_RECOVERY_PROVIDER
} stn_consumer_recovery_result;
typedef struct stn_consumer_recovery_plan {
    stn_cursor_ancestor rollback;
    stn_chain_cursor resume; /* meaningful only for AFTER_CURSOR */
} stn_consumer_recovery_plan;
/* Observational: assigns output only for FROM_START/AFTER_CURSOR. FROM_START
 * has no resume cursor (zeroed unused storage). Caller owns application rollback.
 * Same retained-evidence, immutable/disjoint input rules as ancestry resolution. */
stn_consumer_recovery_result stn_chain_build_consumer_recovery_plan(
    const stn_chain_context *context,const stn_block_span *current,size_t current_count,
    const stn_block_span *retained,size_t retained_count,
    const stn_chain_cursor *cursor,stn_consumer_recovery_plan *out);
#endif
