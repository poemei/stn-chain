/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_compensation_state.h"
#include <string.h>

stn_data_status stn_compensation_state_initialize(stn_compensation_state *s,
    stn_compensation_destination *storage,size_t capacity)
{
    stn_compensation_state r={0};
    if(s==NULL || (capacity!=0u && storage==NULL))return STN_DATA_ARGUMENT;
    r.entries=storage;r.capacity=capacity;*s=r;return STN_DATA_OK;
}
stn_data_status stn_compensation_state_apply(stn_compensation_state *s,
    const stn_compensation_destination *d)
{
    size_t at=0u;int cmp=0;
    if(s==NULL || d==NULL)return STN_DATA_ARGUMENT;
    if(d->mining_identity.type!=STN_ADDRESS_IDENTITY || d->wallet.type!=STN_ADDRESS_WALLET)return STN_DATA_TYPE;
    if(s->count>s->capacity || (s->capacity!=0u && s->entries==NULL))return STN_DATA_ARGUMENT;
    while(at<s->count){
        cmp=memcmp(s->entries[at].mining_identity.identifier,d->mining_identity.identifier,STN_ADDRESS_ID_SIZE);
        if(cmp>=0)break;++at;
    }
    if(at<s->count && cmp==0){
        return memcmp(s->entries[at].wallet.identifier,d->wallet.identifier,STN_ADDRESS_ID_SIZE)==0 ?
            STN_DATA_OK : STN_DATA_DUPLICATE;
    }
    if(s->count==s->capacity)return STN_DATA_CAPACITY;
    if(at<s->count)memmove(s->entries+at+1u,s->entries+at,(s->count-at)*sizeof(*s->entries));
    s->entries[at]=*d;++s->count;return STN_DATA_OK;
}
stn_data_status stn_compensation_state_lookup(const stn_compensation_state *s,
    const stn_address *identity,stn_address *wallet)
{
    size_t i;
    if(s==NULL || identity==NULL || wallet==NULL)return STN_DATA_ARGUMENT;
    if(identity->type!=STN_ADDRESS_IDENTITY)return STN_DATA_TYPE;
    if(s->count>s->capacity || (s->capacity!=0u && s->entries==NULL))return STN_DATA_ARGUMENT;
    for(i=0u;i<s->count;++i){
        int cmp=memcmp(s->entries[i].mining_identity.identifier,identity->identifier,STN_ADDRESS_ID_SIZE);
        if(cmp==0){*wallet=s->entries[i].wallet;return STN_DATA_OK;}
        if(cmp>0)break;
    }
    return STN_DATA_UNRESOLVED;
}
