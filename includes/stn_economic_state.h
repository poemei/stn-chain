/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_ECONOMIC_STATE_H
#define STN_ECONOMIC_STATE_H

#include "stn_issuance.h"
#include "stn_transfer.h"

typedef struct stn_economic_balance {
    uint8_t wallet_id[STN_ADDRESS_ID_SIZE];
    uint64_t units;
} stn_economic_balance;

typedef struct stn_economic_state {
    stn_economic_balance *balances;
    size_t balance_count;
    size_t balance_capacity;
    uint64_t total_supply;
} stn_economic_state;

/* Caller-owned deterministic economic state. No allocation and no persistence.
 * Entries are maintained in ascending wallet identifier order so identical
 * accepted issuance sequences produce identical state on every platform. */
stn_data_status stn_economic_state_initialize(
    stn_economic_state *state,
    stn_economic_balance *storage,
    size_t capacity);

/* Apply one already-accepted canonical mining issuance record.
 * This primitive does not decide whether evidence is accepted and does not
 * provide replay protection; those remain Chain consensus responsibilities.
 * On failure state is unchanged. Integer overflow and capacity fail closed. */
stn_data_status stn_economic_state_apply(
    stn_economic_state *state,
    const stn_issuance_record *record);

/* Apply one already-accepted canonical wallet transfer atomically.
 * The source must already exist with sufficient funds. The destination may be
 * created when capacity permits. Transfers never change total_supply.
 * On every failure state is unchanged. */
stn_data_status stn_economic_state_apply_transfer(
    stn_economic_state *state,
    const stn_transfer *transfer);

/* Exact wallet lookup. OK with *units=0 means no accepted balance. */
stn_data_status stn_economic_state_balance(
    const stn_economic_state *state,
    const stn_address *wallet,
    uint64_t *units);

#endif
