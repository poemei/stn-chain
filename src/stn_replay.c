/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_replay.h"
#include "stn_identity.h"
#include <string.h>

static int nonzero(const uint8_t *p,size_t n){size_t i;uint8_t v=0;if(p==NULL){return 0;}for(i=0;i<n;++i){v|=p[i];}return v!=0;}
stn_replay_result stn_replay_id_from_signer_nonce(const uint8_t signer[32],const uint8_t nonce[32],uint8_t id[64])
{
    uint8_t canonical[32];
    if(id==NULL || stn_identity_derive(signer,canonical)!=STN_IDENTITY_VALID || !nonzero(nonce,32)){return STN_REPLAY_MALFORMED;}
    memcpy(id,canonical,32);memcpy(id+32,nonce,32);return STN_REPLAY_FRESH;
}
stn_replay_result stn_replay_id_from_record(const uint8_t *bytes,size_t length,uint8_t id[64])
{
    stn_record record;
    if(bytes==NULL || id==NULL || stn_record_decode(bytes,length,&record)!=STN_RECORD_OK){return STN_REPLAY_MALFORMED;}
    return stn_replay_id_from_signer_nonce(record.signer_public_key,record.nonce,id);
}
void stn_replay_state_initialize(stn_replay_state *state,uint8_t *consumed,size_t capacity){if(state!=NULL){state->consumed=consumed;state->consumed_count=0;state->consumed_capacity=capacity;}}
stn_replay_result stn_replay_state_check(const stn_replay_state *state,const uint8_t id[64])
{
    size_t i;if(state==NULL || id==NULL || state->consumed_count>state->consumed_capacity || (state->consumed_capacity!=0 && state->consumed==NULL)){return STN_REPLAY_MALFORMED;}for(i=0;i<state->consumed_count;++i){if(memcmp(state->consumed+i*64,id,64)==0){return STN_REPLAY_REPLAY;}}return STN_REPLAY_FRESH;
}
stn_replay_result stn_replay_state_consume(stn_replay_state *state,const uint8_t id[64])
{
    stn_replay_result r;if(state==NULL || id==NULL || state->consumed_count>state->consumed_capacity || (state->consumed_capacity!=0 && state->consumed==NULL)){return STN_REPLAY_MALFORMED;}r=stn_replay_state_check(state,id);if(r!=STN_REPLAY_FRESH){return r;}if(state->consumed_count==state->consumed_capacity){return STN_REPLAY_MALFORMED;}memcpy(state->consumed+state->consumed_count*64,id,64);++state->consumed_count;return STN_REPLAY_FRESH;
}
stn_replay_result stn_replay_state_rebuild(stn_replay_state *state,const uint8_t *const *records,const size_t *lengths,size_t count)
{
    size_t i;uint8_t id[64];stn_replay_result r;if(state==NULL || count!=0 && (records==NULL || lengths==NULL)){return STN_REPLAY_MALFORMED;}state->consumed_count=0;for(i=0;i<count;++i){r=stn_replay_id_from_record(records[i],lengths[i],id);if(r!=STN_REPLAY_FRESH){return r;}r=stn_replay_state_consume(state,id);if(r==STN_REPLAY_REPLAY){return STN_REPLAY_REPLAY;}if(r!=STN_REPLAY_FRESH){return r;}}return STN_REPLAY_FRESH;
}
