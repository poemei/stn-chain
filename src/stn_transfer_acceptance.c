/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_transfer_acceptance.h"

stn_data_status stn_transfer_accept(
    stn_economic_state *economic,
    stn_transfer_replay_state *replay,
    const stn_transfer *transfer,
    const uint8_t controller[STN_IDENTITY_PUBLIC_KEY_SIZE],
    const uint8_t signature[STN_IDENTITY_SIGNATURE_SIZE])
{
    stn_data_status status;
    if(economic==NULL || replay==NULL || transfer==NULL ||
       controller==NULL || signature==NULL)return STN_DATA_ARGUMENT;
    status=stn_transfer_authorization_verify(transfer,controller,signature);
    if(status!=STN_DATA_OK)return status;
    return stn_transfer_binding_apply(economic,replay,transfer);
}
