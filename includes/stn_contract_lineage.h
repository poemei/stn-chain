/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_CONTRACT_LINEAGE_H
#define STN_CONTRACT_LINEAGE_H

#include "stn_contract.h"

/*
 * Contract identity is established once by the exact canonical DRAFT.
 * Mutable sequence/state never create a new Contract identity.
 *
 * Lineage is deterministic from consensus-visible Contract fields:
 * version, type, created_at, participant bytes and terms must remain identical
 * to the origin DRAFT. Only sequence and state may differ.
 */
stn_contract_status stn_contract_lineage_validate(
    const uint8_t *canonical_draft,
    size_t canonical_draft_length,
    const uint8_t *canonical_current,
    size_t canonical_current_length);

stn_contract_status stn_contract_lineage_id(
    const uint8_t *canonical_draft,
    size_t canonical_draft_length,
    const uint8_t *canonical_current,
    size_t canonical_current_length,
    uint8_t contract_id[STN_ADDRESS_ID_SIZE]);

#endif
