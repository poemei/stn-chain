/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_transfer_envelope_acceptance.h"

stn_data_status stn_transfer_envelope_accept(stn_economic_state *economic,
 stn_transfer_envelope_replay_state *replay,const stn_transfer_envelope *envelope)
{
 stn_transfer_envelope_replay_result r;stn_data_status s;
 if(economic==NULL || replay==NULL || envelope==NULL)return STN_DATA_ARGUMENT;
 s=stn_transfer_envelope_authorization_verify(envelope);if(s!=STN_DATA_OK)return s;
 r=stn_transfer_envelope_replay_check(replay,envelope);
 if(r==STN_TRANSFER_ENVELOPE_REPLAY_DUPLICATE)return STN_DATA_DUPLICATE;
 if(r==STN_TRANSFER_ENVELOPE_REPLAY_ARGUMENT)return STN_DATA_ARGUMENT;
 if(r!=STN_TRANSFER_ENVELOPE_REPLAY_FRESH)return STN_DATA_CONTENT;
 if(replay->consumed_count==replay->consumed_capacity)return STN_DATA_CAPACITY;
 s=stn_economic_state_apply_transfer(economic,&envelope->transfer);if(s!=STN_DATA_OK)return s;
 r=stn_transfer_envelope_replay_consume(replay,envelope);
 return r==STN_TRANSFER_ENVELOPE_REPLAY_FRESH?STN_DATA_OK:STN_DATA_ARGUMENT;
}
