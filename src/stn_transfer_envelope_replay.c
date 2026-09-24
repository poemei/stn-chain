/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_transfer_envelope_replay.h"
#include "stn_replay.h"
#include <string.h>

static int valid(const stn_transfer_envelope_replay_state *s)
{return s!=NULL && s->consumed_count<=s->consumed_capacity &&
 (s->consumed_capacity==0u || s->consumed!=NULL);}

stn_transfer_envelope_replay_result stn_transfer_envelope_replay_key(
 const stn_transfer_envelope *e,uint8_t key[STN_TRANSFER_ENVELOPE_REPLAY_KEY_SIZE])
{
 if(e==NULL || key==NULL)return STN_TRANSFER_ENVELOPE_REPLAY_ARGUMENT;
 return stn_replay_id_from_signer_nonce(e->controller,e->nonce,key)==STN_REPLAY_FRESH
  ? STN_TRANSFER_ENVELOPE_REPLAY_FRESH : STN_TRANSFER_ENVELOPE_REPLAY_ARGUMENT;
}
void stn_transfer_envelope_replay_initialize(stn_transfer_envelope_replay_state *s,
 uint8_t *consumed,size_t capacity)
{if(s!=NULL){s->consumed=consumed;s->consumed_count=0u;s->consumed_capacity=capacity;}}
stn_transfer_envelope_replay_result stn_transfer_envelope_replay_check(
 const stn_transfer_envelope_replay_state *s,const stn_transfer_envelope *e)
{
 uint8_t key[STN_TRANSFER_ENVELOPE_REPLAY_KEY_SIZE];size_t i;
 if(!valid(s) || stn_transfer_envelope_replay_key(e,key)!=STN_TRANSFER_ENVELOPE_REPLAY_FRESH)
  return STN_TRANSFER_ENVELOPE_REPLAY_ARGUMENT;
 for(i=0;i<s->consumed_count;++i)
  if(memcmp(s->consumed+i*STN_TRANSFER_ENVELOPE_REPLAY_KEY_SIZE,key,sizeof(key))==0)
   return STN_TRANSFER_ENVELOPE_REPLAY_DUPLICATE;
 return STN_TRANSFER_ENVELOPE_REPLAY_FRESH;
}
stn_transfer_envelope_replay_result stn_transfer_envelope_replay_consume(
 stn_transfer_envelope_replay_state *s,const stn_transfer_envelope *e)
{
 uint8_t key[STN_TRANSFER_ENVELOPE_REPLAY_KEY_SIZE];stn_transfer_envelope_replay_result r;
 if(!valid(s) || stn_transfer_envelope_replay_key(e,key)!=STN_TRANSFER_ENVELOPE_REPLAY_FRESH)
  return STN_TRANSFER_ENVELOPE_REPLAY_ARGUMENT;
 r=stn_transfer_envelope_replay_check(s,e);if(r!=STN_TRANSFER_ENVELOPE_REPLAY_FRESH)return r;
 if(s->consumed_count==s->consumed_capacity)return STN_TRANSFER_ENVELOPE_REPLAY_CAPACITY;
 memcpy(s->consumed+s->consumed_count*STN_TRANSFER_ENVELOPE_REPLAY_KEY_SIZE,key,sizeof(key));
 ++s->consumed_count;return STN_TRANSFER_ENVELOPE_REPLAY_FRESH;
}
