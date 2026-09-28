/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_TRANSACTION_STATUS_H
#define STN_TRANSACTION_STATUS_H

#include "stn_chain.h"

typedef enum stn_transaction_status_code {
    STN_TRANSACTION_STATUS_NOT_FOUND = 0,
    STN_TRANSACTION_STATUS_ACCEPTED = 1,
    STN_TRANSACTION_STATUS_ARGUMENT,
    STN_TRANSACTION_STATUS_UNAVAILABLE,
    STN_TRANSACTION_STATUS_PROVIDER
} stn_transaction_status_code;

typedef struct stn_transaction_status {
    uint64_t height;
    uint8_t block_id[32];
    uint32_t transaction_position;
} stn_transaction_status;

/* Revalidates the supplied immutable accepted history and searches every
 * canonical transaction by its protocol transaction identifier. NOT_FOUND is
 * evidence only about the accepted snapshot supplied by the caller; it makes
 * no claim about pending admission. Output is assigned only for ACCEPTED. */
stn_transaction_status_code stn_transaction_status_find(
    const stn_chain_context *context,
    const stn_block_span *blocks,
    size_t count,
    const uint8_t transaction_id[32],
    stn_transaction_status *out);

#endif
