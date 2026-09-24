/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_ECONOMIC_PERSISTENCE_H
#define STN_ECONOMIC_PERSISTENCE_H

#include "stn_economic_state.h"

#define STN_ECONOMIC_PERSISTENCE_VERSION 1u
#define STN_ECONOMIC_PERSISTENCE_HEADER_SIZE 17u
#define STN_ECONOMIC_PERSISTENCE_BALANCE_SIZE 40u

/* Deterministic cache format for accepted economic state:
 * version[1] || total_supply[8] big-endian || balance_count[8] big-endian ||
 * repeated sorted (wallet_id[32] || units[8] big-endian).
 * Persisted state is a restart cache only; accepted Chain history remains
 * authoritative and may replace it through reconstruction/reorganization. */
stn_data_status stn_economic_persistence_size(
    const stn_economic_state *state,size_t *size);

stn_data_status stn_economic_persistence_encode(
    const stn_economic_state *state,uint8_t *bytes,size_t capacity,size_t *written);

stn_data_status stn_economic_persistence_decode(
    const uint8_t *bytes,size_t length,stn_economic_state *state,
    stn_economic_balance *storage,size_t capacity);

#endif
