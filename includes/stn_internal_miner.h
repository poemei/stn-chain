/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_INTERNAL_MINER_H
#define STN_INTERNAL_MINER_H

#include "stn_share.h"

#define STN_INTERNAL_MINER_DEFAULT_DUTY_PERMILLE 20u
#define STN_INTERNAL_MINER_MAX_DUTY_PERMILLE 20u

typedef enum stn_internal_miner_result {
    STN_INTERNAL_MINER_IDLE = 0,
    STN_INTERNAL_MINER_SHARE = 1,
    STN_INTERNAL_MINER_BLOCK = 2,
    STN_INTERNAL_MINER_EXHAUSTED = 3
} stn_internal_miner_result;

/* Allocation-free, platform-neutral local mining primitive.
 * duty_permille is capped at 20 (2%). The primitive itself does not sleep or
 * sample clocks: platform runtimes enforce the duty cycle by choosing bounded
 * nonce budgets and scheduling calls. This keeps consensus-visible hashing
 * identical on every platform.
 *
 * template_header is the exact canonical 168-byte Chain mining header with
 * reserved_work_nonce == 0. miner must be an stn0_ identity. The function
 * searches at most nonce_budget consecutive nonces beginning at *next_nonce.
 * A block solution takes precedence over a qualifying share. On SHARE/BLOCK,
 * evidence is complete canonical share evidence suitable for the existing
 * acceptance/issuance path. */
stn_data_status stn_internal_miner_search(
    const uint8_t template_header[STN_SHARE_TEMPLATE_HEADER_SIZE],
    const uint8_t work_id[STN_SHARE_WORK_ID_SIZE],
    const stn_address *miner,
    uint64_t *next_nonce,
    uint64_t nonce_budget,
    unsigned duty_permille,
    const stn_hash_provider *provider,
    stn_share_evidence *evidence,
    stn_internal_miner_result *result);

#endif
