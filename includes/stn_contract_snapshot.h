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

/* Copy and register an immutable canonical DRAFT into snapshot-owned storage.
 * No Contract state may borrow candidate/block bytes beyond this call. */
stn_contract_status stn_contract_snapshot_register(
    stn_contract_snapshot *snapshot,
    const uint8_t *canonical_draft,
    size_t canonical_draft_length,
    size_t *index);

#endif
