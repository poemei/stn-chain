#ifndef STN_CHAIN_INTERNAL_H
#define STN_CHAIN_INTERNAL_H
#include "stn_chain.h"
/* Storage-only fast path. Prior MUST be derived from these exact fully validated
 * immutable prefix bytes under this context. Never use peer-supplied metadata.
 * Runs the same history-aware candidate evaluator as full reconstruction. */
stn_chain_report stn_chain_validate_history_candidate(const stn_chain_context *,
    const stn_chain_state *,const stn_block_span *,size_t,
    const uint8_t *,size_t,stn_chain_state *);
#endif
