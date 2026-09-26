/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_ISSUANCE_BINDING_H
#define STN_ISSUANCE_BINDING_H

#include "stn_issuance.h"
#include "stn_share.h"
#include "stn_block_compensation.h"
#include "stn_compensation_state.h"

/* Validate one share-reward issuance against the exact accepted share evidence
 * and the accepted compensation mapping. This does not mutate economic state. */
stn_data_status stn_issuance_bind_share(
    const stn_issuance_record *issuance,
    const stn_share_evidence *share,
    const stn_compensation_state *compensation);

/* Validate one block-reward issuance against canonical solved-block evidence
 * and the accepted compensation mapping. This does not mutate economic state. */
stn_data_status stn_issuance_bind_block(
    const stn_issuance_record *issuance,
    const stn_block_compensation_evidence *evidence,
    const stn_compensation_state *compensation);

#endif
