/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_COMPENSATION_STATE_H
#define STN_COMPENSATION_STATE_H

#include "stn_compensation.h"

typedef struct stn_compensation_state {
    stn_compensation_destination *entries;
    size_t count;
    size_t capacity;
} stn_compensation_state;

/* Caller-owned accepted mapping state, sorted by mining identity identifier.
 * A mining identity has exactly one accepted wallet destination. Repeating the
 * same mapping is idempotent; attempting to remap it is rejected. */
stn_data_status stn_compensation_state_initialize(stn_compensation_state *state,
    stn_compensation_destination *storage,size_t capacity);
stn_data_status stn_compensation_state_apply(stn_compensation_state *state,
    const stn_compensation_destination *destination);
stn_data_status stn_compensation_state_lookup(const stn_compensation_state *state,
    const stn_address *mining_identity,stn_address *wallet);

#endif
