/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_CONTRACT_RESPONSE_ACCEPTANCE_H
#define STN_CONTRACT_RESPONSE_ACCEPTANCE_H

#include "stn_contract_snapshot.h"
#include "stn_transaction.h"

/* Validate an accepted RESPONSE against the immutable contract draft already
 * present in the accepted parent snapshot, then register it in snapshot-owned state.
 * The response actor must be a participant and the signature must verify. */
stn_data_status stn_contract_response_accept(
    stn_contract_snapshot *snapshot,
    const stn_contract_snapshot *accepted,
    const stn_transaction *transaction);

#endif
