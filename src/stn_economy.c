/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_economy.h"
#include <string.h>

stn_data_status stn_economy_share_target(
    const uint8_t chain_target[32],
    uint8_t share_target[32])
{
    uint8_t result[32];
    unsigned carry=0;
    size_t i;
    stn_data_status status;

    if(chain_target==NULL || share_target==NULL) {
        return STN_DATA_ARGUMENT;
    }
    status=stn_target_validate(chain_target,32);
    if(status!=STN_DATA_OK) {
        return status;
    }

    for(i=32;i!=0;) {
        unsigned value;
        --i;
        value=(unsigned)chain_target[i]*STN_SHARE_FACTOR+carry;
        result[i]=(uint8_t)(value&255u);
        carry=value>>8;
    }

    /* Valid Chain targets are <= 2^255-1. Any multiplication carry, or a
     * product with bit 255 set, exceeds MAX_TARGET and therefore saturates. */
    if(carry!=0 || result[0]>=0x80u) {
        memset(result,0xff,sizeof(result));
        result[0]=0x7f;
    }

    memcpy(share_target,result,sizeof(result));
    return STN_DATA_OK;
}
