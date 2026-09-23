/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_CONTRACT_SNAPSHOT_H
#define STN_CONTRACT_SNAPSHOT_H

#include "stn_contract_state.h"

typedef struct stn_contract_snapshot stn_contract_snapshot;

stn_contract_snapshot *stn_contract_snapshot_create(void);
stn_contract_snapshot *stn_contract_snapshot_clone(
    const stn_contract_snapshot *source);
stn_contract_snapshot *stn_contract_snapshot_share(
    stn_contract_snapshot *snapshot);
void stn_contract_snapshot_release(stn_contract_snapshot *snapshot);

stn_contract_state_store *stn_contract_snapshot_state(
    stn_contract_snapshot *snapshot);
const stn_contract_state_store *stn_contract_snapshot_const_state(
    const stn_contract_snapshot *snapshot);

#endif
