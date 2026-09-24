/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_chain.h"
#include "stn_wire_internal.h"
#include "stn_share.h"
#include "stn_issuance.h"
#include "stn_issuance_binding.h"
#include "stn_transfer_envelope_acceptance.h"
#include <string.h>
#include <stdlib.h>
#include "stn_sha256.h"
#include "stn_contract_transaction.h"

/* Frozen checksums of exact qualified legacy genesis fixtures. */
stn_data_status stn_chain_publication_activation(const stn_chain_context *c,uint64_t *height)
{
    static const struct { uint8_t genesis[32]; uint64_t height; } legacy[]={
        {{0x24,0xbc,0x21,0x9f,0xb3,0xf7,0x16,0xa8,0x27,0x92,0xaa,0x47,0xc0,0x08,0xcb,0xe7,0x05,0x80,0xa6,0x80,0x33,0xbd,0xda,0x29,0x57,0x25,0x84,0xb9,0xdf,0xc9,0x04,0x80},64u},
        {{0x28,0xcb,0xfd,0xf9,0xc3,0xdb,0x7a,0xf6,0x93,0x79,0x08,0x21,0x65,0xb0,0xc5,0x0c,0x7d,0xe9,0x30,0x6c,0x04,0x88,0xec,0x7e,0xeb,0x1f,0x17,0xbe,0x34,0xe1,0xf1,0x27},122u},
        {{0x50,0x1d,0xfd,0xe9,0x85,0xd3,0xda,0x8a,0x56,0xc9,0x32,0x05,0x4e,0xc0,0x05,0x25,0x39,0x0c,0x44,0xcf,0xf2,0xd8,0x37,0x01,0x18,0x33,0x9f,0x72,0xc7,0xbc,0xd8,0x0d},130u},
        {{0x5a,0x1c,0x87,0x3c,0x0e,0xe5,0xa1,0x0e,0x92,0xb7,0x3f,0x00,0x85,0x48,0x38,0x4e,0xa7,0xd9,0x2a,0x07,0x9c,0xb1,0x92,0x8d,0xd4,0x8a,0x28,0xfa,0x31,0x7a,0x10,0x5e},61u},
        {{0x79,0xa2,0x73,0x0e,0x6b,0xb2,0x6c,0xbf,0xb1,0xb2,0x7e,0xa5,0xca,0x8e,0xbd,0x99,0x11,0x1a,0xe6,0xd3,0xcf,0xe1,0x33,0x38,0x44,0xa6,0xe9,0xd4,0x08,0x10,0x5e,0xc3},61u},
        {{0x93,0xd7,0xa8,0x97,0xf6,0x14,0x25,0x56,0x25,0x37,0x15,0x90,0x90,0x0c,0x21,0x81,0x46,0xc9,0xd6,0xe1,0xeb,0xb7,0xea,0x24,0x90,0x30,0x2a,0x47,0xd3,0x2f,0x71,0x27},61u},
        {{0xae,0x8c,0x12,0xac,0x05,0xaf,0xa8,0xde,0x04,0x3f,0x9a,0x52,0x99,0x68,0xc6,0x94,0xad,0x53,0x33,0x0d,0x50,0xbb,0x14,0x99,0xed,0xbd,0xb3,0xc8,0xfa,0x4e,0xb0,0x4d},61u},
    };
    uint8_t digest[32];size_t i;
    if(c==NULL || height==NULL || c->genesis_bytes==NULL)return STN_DATA_ARGUMENT;
    if(stn_sha256(NULL,NULL,0,c->genesis_bytes,c->genesis_length,digest)!=STN_DATA_OK)return STN_DATA_PROVIDER_ERROR;
    *height=1;
    for(i=0;i<sizeof(legacy)/sizeof(legacy[0]);++i){if(memcmp(digest,legacy[i].genesis,32)==0){*height=legacy[i].height;break;}}
    return STN_DATA_OK;
}
typedef struct stn_chain_lifecycle_owned {
    stn_lifecycle_state state;
    size_t references;
    uint8_t bytes[];

} stn_chain_lifecycle_owned;

typedef struct stn_chain_share_owned {
    stn_share_replay_state state;
    size_t references;
    uint8_t bytes[];
} stn_chain_share_owned;

static stn_chain_share_owned *share_new(void)
{
    stn_chain_share_owned *o=(stn_chain_share_owned *)calloc(1,sizeof(*o));
    if(o==NULL)return NULL;
    o->references=1;
    stn_share_replay_initialize(&o->state,NULL,0);
    return o;
}
static stn_chain_share_owned *share_clone(const stn_chain_state *s)
{
    stn_chain_share_owned *o;
    size_t capacity,bytes;
    if(s->shares==NULL ||
       s->shares->consumed_count>s->shares->consumed_capacity ||
       s->shares->consumed_count>SIZE_MAX-STN_BLOCK_MAX_TRANSACTIONS)return NULL;
    capacity=s->shares->consumed_count+STN_BLOCK_MAX_TRANSACTIONS;
    if(capacity>SIZE_MAX/STN_SHARE_REPLAY_KEY_SIZE)return NULL;
    bytes=capacity*STN_SHARE_REPLAY_KEY_SIZE;
    if(bytes>SIZE_MAX-sizeof(*o))return NULL;
    o=(stn_chain_share_owned *)calloc(1,sizeof(*o)+bytes);
    if(o==NULL)return NULL;
    o->references=1;
    stn_share_replay_initialize(&o->state,o->bytes,capacity);
    if(s->shares->consumed_count!=0){
        memcpy(o->bytes,s->shares->consumed,
            s->shares->consumed_count*STN_SHARE_REPLAY_KEY_SIZE);
        o->state.consumed_count=s->shares->consumed_count;
    }
    return o;
}
static void share_destroy(stn_chain_share_owned *o){free(o);}

typedef struct stn_chain_compensation_owned {
    stn_compensation_state state;
    size_t references;
    stn_compensation_destination entries[];
} stn_chain_compensation_owned;

static stn_chain_compensation_owned *compensation_new(void)
{
    stn_chain_compensation_owned *o=(stn_chain_compensation_owned *)calloc(1,sizeof(*o));
    if(o==NULL)return NULL;
    o->references=1;
    stn_compensation_state_initialize(&o->state,NULL,0u);
    return o;
}
static stn_chain_compensation_owned *compensation_clone(const stn_chain_state *s)
{
    stn_chain_compensation_owned *o;size_t capacity,bytes;
    if(s->compensation==NULL || s->compensation->count>s->compensation->capacity ||
       s->compensation->count>SIZE_MAX-STN_BLOCK_MAX_TRANSACTIONS)return NULL;
    capacity=s->compensation->count+STN_BLOCK_MAX_TRANSACTIONS;
    if(capacity>SIZE_MAX/sizeof(stn_compensation_destination))return NULL;
    bytes=capacity*sizeof(stn_compensation_destination);
    if(bytes>SIZE_MAX-sizeof(*o))return NULL;
    o=(stn_chain_compensation_owned *)calloc(1,sizeof(*o)+bytes);if(o==NULL)return NULL;
    o->references=1;stn_compensation_state_initialize(&o->state,o->entries,capacity);
    if(s->compensation->count!=0u){memcpy(o->entries,s->compensation->entries,s->compensation->count*sizeof(*o->entries));o->state.count=s->compensation->count;}
    return o;
}
static void compensation_destroy(stn_chain_compensation_owned *o){free(o);}

typedef struct stn_chain_economic_owned {
    stn_economic_state state;
    size_t references;
    size_t replay_count;
    size_t replay_capacity;
    uint8_t *replay_bytes;
    stn_economic_balance balances[];
} stn_chain_economic_owned;
static stn_chain_economic_owned *economic_new(void){stn_chain_economic_owned *o=(stn_chain_economic_owned *)calloc(1,sizeof(*o));if(o==NULL)return NULL;o->references=1;stn_economic_state_initialize(&o->state,NULL,0u);return o;}
static stn_chain_economic_owned *economic_clone(const stn_chain_state *s){
    stn_chain_economic_owned *source,*o;size_t capacity,balance_bytes,replay_capacity,replay_bytes,total;
    if(s->economy==NULL || s->economy->balance_count>s->economy->balance_capacity ||
       s->economy->balance_count>SIZE_MAX-STN_BLOCK_MAX_TRANSACTIONS)return NULL;
    source=(stn_chain_economic_owned *)s->economy;
    if(source->replay_count>source->replay_capacity ||
       source->replay_count>SIZE_MAX-STN_BLOCK_MAX_TRANSACTIONS)return NULL;
    capacity=s->economy->balance_count+STN_BLOCK_MAX_TRANSACTIONS;
    replay_capacity=source->replay_count+STN_BLOCK_MAX_TRANSACTIONS;
    if(capacity>SIZE_MAX/sizeof(stn_economic_balance) ||
       replay_capacity>SIZE_MAX/STN_TRANSFER_ENVELOPE_REPLAY_KEY_SIZE)return NULL;
    balance_bytes=capacity*sizeof(stn_economic_balance);
    replay_bytes=replay_capacity*STN_TRANSFER_ENVELOPE_REPLAY_KEY_SIZE;
    if(balance_bytes>SIZE_MAX-sizeof(*o) ||
       replay_bytes>SIZE_MAX-sizeof(*o)-balance_bytes)return NULL;
    total=sizeof(*o)+balance_bytes+replay_bytes;
    o=(stn_chain_economic_owned *)calloc(1,total);if(o==NULL)return NULL;
    o->references=1;o->replay_capacity=replay_capacity;
    o->replay_bytes=(uint8_t *)(o->balances+capacity);
    stn_economic_state_initialize(&o->state,o->balances,capacity);
    if(s->economy->balance_count!=0u){memcpy(o->balances,s->economy->balances,s->economy->balance_count*sizeof(*o->balances));o->state.balance_count=s->economy->balance_count;}
    if(source->replay_count!=0u){memcpy(o->replay_bytes,source->replay_bytes,source->replay_count*STN_TRANSFER_ENVELOPE_REPLAY_KEY_SIZE);o->replay_count=source->replay_count;}
    o->state.total_supply=s->economy->total_supply;return o;
}
static void economic_destroy(stn_chain_economic_owned *o){free(o);}


#ifdef STN_LIFECYCLE_TEST
#include <stdatomic.h>
static atomic_size_t lifecycle_live;
static atomic_size_t lifecycle_clones;
size_t stn_chain_test_clone_count(void){return atomic_load(&lifecycle_clones);}
static atomic_size_t lifecycle_budget=SIZE_MAX;
void stn_chain_test_fail_after(size_t budget){atomic_store(&lifecycle_budget,budget);}
size_t stn_chain_test_live_snapshots(void){return atomic_load(&lifecycle_live);}
size_t stn_chain_test_references(stn_chain_state *state,size_t references)
{
    stn_chain_lifecycle_owned *o=(stn_chain_lifecycle_owned *)state->lifecycle;
    size_t previous=o->references;o->references=references;return previous;
}
#endif
static void *lifecycle_allocate(size_t bytes)
{
#ifdef STN_LIFECYCLE_TEST
    size_t budget=atomic_load(&lifecycle_budget);
    while(budget!=SIZE_MAX){
        if(budget==0)return NULL;
        if(atomic_compare_exchange_weak(&lifecycle_budget,&budget,budget-1))break;
    }
#endif
    return calloc(1,bytes);
}

static void lifecycle_destroy(stn_chain_lifecycle_owned *o)
{
#ifdef STN_LIFECYCLE_TEST
    atomic_fetch_sub(&lifecycle_live,1);
#endif
    free(o);
}
void stn_chain_state_release(stn_chain_state *state)
{
    if(state!=NULL){
        if(state->lifecycle!=NULL){
            stn_chain_lifecycle_owned *o=(stn_chain_lifecycle_owned *)state->lifecycle;
            if(o->references==0)abort(); /* invalid ownership, never wrap */
            if(--o->references==0)lifecycle_destroy(o);
        }
        stn_contract_snapshot_release(state->contracts);
        if(state->shares!=NULL){
            stn_chain_share_owned *o=(stn_chain_share_owned *)state->shares;
            if(o->references==0)abort();
            if(--o->references==0)share_destroy(o);
        }
        if(state->compensation!=NULL){
            stn_chain_compensation_owned *o=(stn_chain_compensation_owned *)state->compensation;
            if(o->references==0)abort();
            if(--o->references==0)compensation_destroy(o);
        }
        if(state->economy!=NULL){stn_chain_economic_owned *o=(stn_chain_economic_owned *)state->economy;if(o->references==0)abort();if(--o->references==0)economic_destroy(o);}
        memset(state,0,sizeof(*state));
    }
}
stn_data_status stn_chain_state_share(const stn_chain_state *source,stn_chain_state *out)
{
    if(source==NULL || out==NULL)return STN_DATA_ARGUMENT;
    if(source==out)return STN_DATA_OK;
    if(source->lifecycle!=NULL){
        stn_chain_lifecycle_owned *o=(stn_chain_lifecycle_owned *)source->lifecycle;
        if(o->references==0)return STN_DATA_ARGUMENT;
        if(o->references==SIZE_MAX)return STN_DATA_CAPACITY;
        ++o->references;
    }
    if(source->shares!=NULL){
        stn_chain_share_owned *o=(stn_chain_share_owned *)source->shares;
        if(o->references==0 || o->references==SIZE_MAX){
            if(source->lifecycle!=NULL){
                stn_chain_lifecycle_owned *l=(stn_chain_lifecycle_owned *)source->lifecycle;
                if(--l->references==0)lifecycle_destroy(l);
            }
            return o->references==SIZE_MAX ? STN_DATA_CAPACITY : STN_DATA_ARGUMENT;
        }
        ++o->references;
    }
    if(source->compensation!=NULL){
        stn_chain_compensation_owned *o=(stn_chain_compensation_owned *)source->compensation;
        if(o->references==0 || o->references==SIZE_MAX){
            if(source->lifecycle!=NULL){stn_chain_lifecycle_owned *l=(stn_chain_lifecycle_owned *)source->lifecycle;if(--l->references==0)lifecycle_destroy(l);}
            if(source->shares!=NULL){stn_chain_share_owned *q=(stn_chain_share_owned *)source->shares;if(--q->references==0)share_destroy(q);}
            return o->references==SIZE_MAX ? STN_DATA_CAPACITY : STN_DATA_ARGUMENT;
        }
        ++o->references;
    }
    if(source->economy!=NULL){
        stn_chain_economic_owned *o=(stn_chain_economic_owned *)source->economy;
        if(o->references==0 || o->references==SIZE_MAX){
            if(source->lifecycle!=NULL){stn_chain_lifecycle_owned *l=(stn_chain_lifecycle_owned *)source->lifecycle;if(--l->references==0)lifecycle_destroy(l);}
            if(source->shares!=NULL){stn_chain_share_owned *q=(stn_chain_share_owned *)source->shares;if(--q->references==0)share_destroy(q);}
            if(source->compensation!=NULL){stn_chain_compensation_owned *q=(stn_chain_compensation_owned *)source->compensation;if(--q->references==0)compensation_destroy(q);}
            return o->references==SIZE_MAX ? STN_DATA_CAPACITY : STN_DATA_ARGUMENT;
        }
        ++o->references;
    }
    if(source->contracts!=NULL && stn_contract_snapshot_share(source->contracts)==NULL){
        if(source->lifecycle!=NULL){
            stn_chain_lifecycle_owned *o=(stn_chain_lifecycle_owned *)source->lifecycle;
            if(--o->references==0)lifecycle_destroy(o);
        }
        if(source->shares!=NULL){
            stn_chain_share_owned *q=(stn_chain_share_owned *)source->shares;
            if(--q->references==0)share_destroy(q);
        }
        if(source->compensation!=NULL){stn_chain_compensation_owned *q=(stn_chain_compensation_owned *)source->compensation;if(--q->references==0)compensation_destroy(q);}
        if(source->economy!=NULL){stn_chain_economic_owned *q=(stn_chain_economic_owned *)source->economy;if(--q->references==0)economic_destroy(q);}
        return STN_DATA_CAPACITY;
    }
    *out=*source;return STN_DATA_OK;
}
void stn_chain_state_move(stn_chain_state *out,stn_chain_state *source)
{
    if(out==source)return;
    stn_chain_state_release(out);*out=*source;memset(source,0,sizeof(*source));
}
static int zero32(const uint8_t p[32])
{
    size_t i;
    for (i=0;i<32;++i) { if (p[i]!=0) { return 0; } }
    return 1;
}
static stn_data_status initial_set_valid(const uint8_t *ids,size_t count)
{size_t i;if(count>STN_GENESIS_INITIAL_IDENTITY_MAX||(count!=0&&ids==NULL))return STN_DATA_CONTENT;for(i=0;i<count;++i){uint8_t c[32];if(stn_identity_derive(ids+i*32,c)!=STN_IDENTITY_VALID||memcmp(c,ids+i*32,32)!=0)return STN_DATA_CONTENT;if(i&&memcmp(ids+(i-1)*32,ids+i*32,32)>=0)return STN_DATA_CONTENT;}return STN_DATA_OK;}
static stn_chain_lifecycle_owned *lifecycle_new(const stn_chain_context *c)
{
    stn_chain_lifecycle_owned *o=lifecycle_allocate(sizeof(*o));uint8_t initial[32]={0};
    if(o==NULL)return NULL;
    o->references=1;
#ifdef STN_LIFECYCLE_TEST
    atomic_fetch_add(&lifecycle_live,1);
#endif
    if(c->genesis_initial_identity_count)memcpy(initial,c->genesis_initial_identities,32);
    stn_lifecycle_initialize(&o->state,NULL,0,NULL,0,initial);
    stn_lifecycle_set_initial_identities(&o->state,c->genesis_initial_identities,c->genesis_initial_identity_count);
    return o;
}
static stn_chain_lifecycle_owned *lifecycle_clone(const stn_chain_state *s)
{
    stn_chain_lifecycle_owned *o;size_t grants,replays,grant_bytes,replay_bytes;
    if(s->lifecycle==NULL)return NULL;
    if(s->lifecycle->grant_count>SIZE_MAX-STN_BLOCK_MAX_TRANSACTIONS ||
        s->lifecycle->replay.consumed_count>SIZE_MAX-STN_BLOCK_MAX_TRANSACTIONS)return NULL;
    grants=s->lifecycle->grant_count+STN_BLOCK_MAX_TRANSACTIONS;
    replays=s->lifecycle->replay.consumed_count+STN_BLOCK_MAX_TRANSACTIONS;
    if(grants>SIZE_MAX/STN_AUTHORITY_GRANT_SIZE || replays>SIZE_MAX/STN_REPLAY_ID_SIZE)return NULL;
    grant_bytes=grants*STN_AUTHORITY_GRANT_SIZE;replay_bytes=replays*STN_REPLAY_ID_SIZE;
    if(grant_bytes>SIZE_MAX-sizeof(*o) || replay_bytes>SIZE_MAX-sizeof(*o)-grant_bytes)return NULL;
    o=lifecycle_allocate(sizeof(*o)+grant_bytes+replay_bytes);if(o==NULL)return NULL;
    o->references=1;
#ifdef STN_LIFECYCLE_TEST
    atomic_fetch_add(&lifecycle_live,1);
#endif
#ifdef STN_LIFECYCLE_TEST
    atomic_fetch_add(&lifecycle_clones,1);
#endif
    o->state=*s->lifecycle;
    if(s->lifecycle->grant_count)memcpy(o->bytes,s->lifecycle->grant_bytes,s->lifecycle->grant_count*STN_AUTHORITY_GRANT_SIZE);
    if(s->lifecycle->replay.consumed_count)memcpy(o->bytes+grant_bytes,s->lifecycle->replay.consumed,s->lifecycle->replay.consumed_count*STN_REPLAY_ID_SIZE);
    stn_lifecycle_rebind(&o->state,o->bytes,grants,o->bytes+grant_bytes,replays);
    return o;
}
static stn_chain_report initial_report(void)
{
    stn_chain_report r={0};
    r.acceptance=STN_ACCEPTANCE_ERROR; r.failing_index=SIZE_MAX;
    return r;
}

static stn_chain_report fail(stn_chain_report r, stn_chain_reason reason, stn_data_status detail)
{
    r.reason=reason; r.detail=detail;
    if (detail==STN_DATA_UNRESOLVED) { r.acceptance=STN_ACCEPTANCE_UNRESOLVED; }
    else if (detail==STN_DATA_PROVIDER_ERROR || detail==STN_DATA_ARGUMENT) { r.acceptance=STN_ACCEPTANCE_ERROR; }
    else { r.acceptance=STN_ACCEPTANCE_REJECTED; }
    return r;
}

static stn_stage_status stage(stn_data_status status)
{
    if (status==STN_DATA_OK) { return STN_STAGE_PASS; }
    if (status==STN_DATA_UNRESOLVED) { return STN_STAGE_UNRESOLVED; }
    if (status==STN_DATA_PROVIDER_ERROR || status==STN_DATA_ARGUMENT) { return STN_STAGE_ERROR; }
    return STN_STAGE_REJECT;
}

static stn_data_status context_valid(const stn_chain_context *c)
{
    stn_block b;
    if (c==NULL || c->genesis_bytes==NULL) { return STN_DATA_ARGUMENT; }
    if(stn_authority_root_set_validate(c->genesis_authority_roots,c->genesis_authority_root_count)!=STN_AUTHORITY_AUTHORIZED){return STN_DATA_CONTENT;}
    if(initial_set_valid(c->genesis_initial_identities,c->genesis_initial_identity_count)!=STN_DATA_OK)return STN_DATA_CONTENT;
    if (stn_block_decode(c->genesis_bytes,c->genesis_length,&b)!=STN_DATA_OK ||
        b.header.height!=0 || memcmp(b.header.network_id,c->network_id,32)!=0) { return STN_DATA_CONTENT; }
    if(c->pow_policy!=NULL) {
        if(b.header.version!=STN_POW_BLOCK_VERSION ||
           stn_target_validate(c->pow_policy->fixed_target,32)!=STN_DATA_OK ||
           memcmp(b.header.reserved_target,c->pow_policy->fixed_target,32)!=0) { return STN_DATA_TARGET; }
    } else if(b.header.version!=1) { return STN_DATA_VERSION; }
    return STN_DATA_OK;
}

stn_data_status stn_chain_initialize(const stn_chain_context *context, stn_chain_state *out)
{
    stn_chain_state s={0};
    stn_data_status status;
    if (out==NULL) { return STN_DATA_ARGUMENT; }
    status=context_valid(context);
    if (status!=STN_DATA_OK) { return status; }
    status=stn_chain_publication_activation(context,&s.publication_activation_height);
    if(status!=STN_DATA_OK)return status;
    memcpy(s.network_id,context->network_id,32);
    {
        stn_chain_lifecycle_owned *o=lifecycle_new(context);
        if(o==NULL)return STN_DATA_CAPACITY;
        s.lifecycle=&o->state;
        s.contracts=stn_contract_snapshot_create();
        if(s.contracts==NULL){lifecycle_destroy(o);return STN_DATA_CAPACITY;}
        {
            stn_chain_share_owned *q=share_new();
            if(q==NULL){stn_contract_snapshot_release(s.contracts);lifecycle_destroy(o);return STN_DATA_CAPACITY;}
            s.shares=&q->state;
            {
                stn_chain_compensation_owned *m=compensation_new();
                if(m==NULL){share_destroy(q);stn_contract_snapshot_release(s.contracts);lifecycle_destroy(o);return STN_DATA_CAPACITY;}
                s.compensation=&m->state;
                {stn_chain_economic_owned *e=economic_new();if(e==NULL){compensation_destroy(m);share_destroy(q);stn_contract_snapshot_release(s.contracts);lifecycle_destroy(o);return STN_DATA_CAPACITY;}s.economy=&e->state;}
            }
        }
    }
    *out=s;
    return STN_DATA_OK;
}

stn_data_status stn_chain_block_id(const uint8_t *bytes, size_t length,
    const stn_hash_provider *provider, uint8_t digest[32])
{
    static const uint8_t domain[]="STN-CHAIN:BLOCK:ID:1";
    uint8_t temporary[32]={0};
    stn_data_status status;
    if (digest==NULL) { return STN_DATA_ARGUMENT; }
    status=stn_block_validate_structure(bytes,length);
    if (status!=STN_DATA_OK) { return status; }
    if (provider==NULL || provider->hash==NULL) { return STN_DATA_UNRESOLVED; }
    status=provider->hash(provider->user,domain,sizeof(domain),bytes,STN_BLOCK_HEADER_SIZE,temporary);
    if (status==STN_DATA_OK) { memcpy(digest,temporary,32); }
    else if (status!=STN_DATA_UNRESOLVED) { status=STN_DATA_PROVIDER_ERROR; }
    return status;
}

/* State metadata has consistency checks, not a proof of its historical prefix. */
static stn_data_status prior_valid(const stn_chain_context *c,const stn_chain_state *s)
{
    uint64_t activation;
    uint8_t anchor[32];
    stn_block genesis;
    stn_data_status status;
    status=stn_chain_publication_activation(c,&activation);
    if(status!=STN_DATA_OK)return status;
    if(s->publication_activation_height!=activation)return STN_DATA_CONTENT;
    if (memcmp(s->network_id,c->network_id,32)!=0 || (s->has_tip!=0 && s->has_tip!=1)) { return STN_DATA_CONTENT; }
    if(!s->has_tip || c->pow_policy==NULL) {
        if(!zero32(s->current_target) || !(zero32(s->cumulative_work.bytes) && zero32(s->cumulative_work.bytes+8))) { return STN_DATA_CONTENT; }
    } else {
        stn_work expected;
        if(stn_target_validate(s->current_target,32)!=STN_DATA_OK || (zero32(s->cumulative_work.bytes) && zero32(s->cumulative_work.bytes+8))) { return STN_DATA_CONTENT; }
        if(s->height<60) {
            status=stn_work_at_height(c->pow_policy->fixed_target,s->height,&expected);
            if(status!=STN_DATA_OK) { return status; }
            if(memcmp(s->current_target,c->pow_policy->fixed_target,32)!=0 ||
               memcmp(s->cumulative_work.bytes,expected.bytes,STN_WORK_SIZE)!=0) { return STN_DATA_CONTENT; }
        }
    }
    if (!s->has_tip) {
        return s->height==0 && s->timestamp==0 && zero32(s->tip_id) && zero32(s->genesis_id) ?
            STN_DATA_OK : STN_DATA_CONTENT;
    }
    status=stn_chain_block_id(c->genesis_bytes,c->genesis_length,&c->hash_provider,anchor);
    if (status!=STN_DATA_OK) { return status; }
    if (zero32(anchor) || zero32(s->tip_id) || memcmp(anchor,s->genesis_id,32)!=0) { return STN_DATA_CONTENT; }
    status=stn_block_decode(c->genesis_bytes,c->genesis_length,&genesis);
    if (status!=STN_DATA_OK) { return status; }
    if (s->timestamp<genesis.header.timestamp ||
        (s->height==0 && (memcmp(s->tip_id,anchor,32)!=0 || s->timestamp!=genesis.header.timestamp))) {
        return STN_DATA_CONTENT;
    }
    return STN_DATA_OK;
}

stn_data_status stn_chain_required_target(const stn_chain_context *context,
    const stn_chain_state *prior,uint8_t target[32])
{
    uint64_t height;size_t count,i;const stn_block_header *history;
    stn_data_status status;
    if(prior==NULL || target==NULL) { return STN_DATA_ARGUMENT; }
    status=context_valid(context);if(status!=STN_DATA_OK) { return status; }
    if(context->pow_policy==NULL) { return STN_DATA_UNRESOLVED; }
    status=prior_valid(context,prior);if(status!=STN_DATA_OK) { return status; }
    if(prior->has_tip && prior->height==UINT64_MAX) { return STN_DATA_OVERFLOW; }
    height=prior->has_tip ? prior->height+1 : 0;
    count=prior->target_history_count;
    if(count!=(prior->has_tip ? (size_t)(prior->height%60)+1 : 0)) {
        return STN_DATA_CONTENT;
    }
    for(i=0;i<count;++i) {
        const stn_block_header *h=&prior->target_history[i];
        if(h->height!=height-(uint64_t)count+(uint64_t)i || h->version!=3 ||
           memcmp(h->reserved_target,prior->current_target,32)!=0 ||
           (i!=0 && h->timestamp<prior->target_history[i-1].timestamp)) { return STN_DATA_CONTENT; }
    }
    if(count!=0 && prior->target_history[count-1].timestamp!=prior->timestamp) { return STN_DATA_CONTENT; }
    history=count==0 ? NULL : &prior->target_history[count-1];
    if(height!=0 && height%60==0) { history=prior->target_history; }
    else { count=count==0 ? 0 : 1; }
    return stn_target_next(height,context->pow_policy->fixed_target,history,count,target);
}

static stn_data_status contract_authorized(
    const stn_lifecycle_state *lifecycle,
    const stn_contract_transaction *action,
    const uint8_t *canonical_draft,size_t canonical_draft_length,
    const stn_hash_provider *provider)
{
    size_t i;
    uint8_t grant_id[32];
    if(lifecycle==NULL || action==NULL || canonical_draft==NULL || provider==NULL)
        return STN_DATA_ARGUMENT;
    if(stn_contract_signature_verify(action->actor,action->action,
        action->canonical_contract,action->canonical_contract_length,
        action->sequence,action->signature)!=STN_CONTRACT_OK)
        return STN_DATA_CONTENT;
    if(stn_contract_authority_evaluate(action->actor,action->action,
        canonical_draft,canonical_draft_length,action->authority_evidence,
        action->authority_evidence_length)!=STN_CONTRACT_OK)
        return STN_DATA_CONTENT;
    for(i=0;i<lifecycle->grant_count;++i){
        const uint8_t *grant=lifecycle->grant_bytes+i*STN_AUTHORITY_GRANT_SIZE;
        if(memcmp(grant+33,action->authority_evidence,
            STN_AUTHORITY_EVIDENCE_SIZE)!=0)continue;
        if(stn_authority_grant_id(grant,STN_AUTHORITY_GRANT_SIZE,
            provider,grant_id)!=STN_DATA_OK)
            return STN_DATA_PROVIDER_ERROR;
        if(!stn_authority_state_is_revoked(&lifecycle->authority,grant_id))
            return STN_DATA_OK;
    }
    return STN_DATA_CONTENT;
}

static stn_data_status contract_apply_transaction(
    stn_contract_snapshot *snapshot,const stn_lifecycle_state *lifecycle,
    const stn_transaction *transaction,const stn_hash_provider *provider)
{
    stn_contract_transaction action;
    stn_contract_state_store *state;
    stn_contract current,next;
    stn_contract_status status;
    size_t i,index=SIZE_MAX;
    stn_data_status authorized;
    if(snapshot==NULL || lifecycle==NULL || transaction==NULL || provider==NULL)
        return STN_DATA_ARGUMENT;
    status=stn_contract_transaction_decode(transaction->record_bytes,
        transaction->record_length,&action);
    if(status!=STN_CONTRACT_OK)return STN_DATA_CONTENT;
    state=stn_contract_snapshot_state(snapshot);
    if(state==NULL)return STN_DATA_ARGUMENT;

    if(action.action==STN_CONTRACT_ACTION_CREATE){
        status=stn_contract_decode(action.canonical_contract,
            action.canonical_contract_length,&current);
        if(status!=STN_CONTRACT_OK || current.state!=STN_CONTRACT_STATE_DRAFT ||
           current.sequence!=0u)return STN_DATA_CONTENT;
        authorized=contract_authorized(lifecycle,&action,
            action.canonical_contract,action.canonical_contract_length,provider);
        if(authorized!=STN_DATA_OK)return authorized;
        status=stn_contract_snapshot_register(snapshot,action.canonical_contract,
            action.canonical_contract_length,&index);
        if(status!=STN_CONTRACT_OK)
            return status==STN_CONTRACT_CAPACITY ? STN_DATA_CAPACITY : STN_DATA_CONTENT;
        state=stn_contract_snapshot_state(snapshot);
        status=stn_contract_apply_action(&state->entries[index].current,
            action.action,action.sequence,&next);
        if(status!=STN_CONTRACT_OK)return STN_DATA_CONTENT;
        next.participants=state->entries[index].current.participants;
        next.participant_bytes=state->entries[index].current.participant_bytes;
        next.terms=state->entries[index].current.terms;
        state->entries[index].current=next;
        return STN_DATA_OK;
    }

    for(i=0;i<state->entry_count;++i){
        if(stn_contract_lineage_validate(state->entries[i].canonical_draft,
            state->entries[i].canonical_draft_length,action.canonical_contract,
            action.canonical_contract_length)==STN_CONTRACT_OK){
            index=i;break;
        }
    }
    if(index==SIZE_MAX)return STN_DATA_CONTENT;
    status=stn_contract_decode(action.canonical_contract,
        action.canonical_contract_length,&current);
    if(status!=STN_CONTRACT_OK ||
       current.sequence!=state->entries[index].current.sequence ||
       current.state!=state->entries[index].current.state)
        return STN_DATA_CONTENT;
    authorized=contract_authorized(lifecycle,&action,
        state->entries[index].canonical_draft,
        state->entries[index].canonical_draft_length,provider);
    if(authorized!=STN_DATA_OK)return authorized;

    if(action.action==STN_CONTRACT_ACTION_APPROVE){
        status=stn_contract_state_apply_vote(state,index,action.canonical_contract,
            action.canonical_contract_length,action.sequence,action.actor);
        return status==STN_CONTRACT_OK ? STN_DATA_OK :
            (status==STN_CONTRACT_CAPACITY ? STN_DATA_CAPACITY : STN_DATA_CONTENT);
    }
    status=stn_contract_apply_action(&current,action.action,action.sequence,&next);
    if(status!=STN_CONTRACT_OK)return STN_DATA_CONTENT;
    next.participants=state->entries[index].current.participants;
    next.participant_bytes=state->entries[index].current.participant_bytes;
    next.terms=state->entries[index].current.terms;
    state->entries[index].current=next;
    return STN_DATA_OK;
}

static stn_chain_report validate_candidate(const stn_chain_context *context,
    const stn_chain_state *prior,const stn_block_span *history,size_t history_count,
    const uint8_t *bytes,size_t length,stn_chain_state *out,int reconstruct)
{
    stn_chain_report r=initial_report();
    stn_block b;
    stn_chain_state next;stn_chain_lifecycle_owned *candidate_lifecycle=NULL;
    stn_contract_snapshot *candidate_contracts=NULL;
    stn_chain_share_owned *candidate_shares=NULL;
    stn_chain_compensation_owned *candidate_compensation=NULL;
    stn_chain_economic_owned *candidate_economy=NULL;
    int has_lifecycle=0,has_contracts=0,has_shares=0,has_compensation=0,has_issuance=0,has_transfer=0,reused_lifecycle=0;
    stn_data_status status;
    uint8_t id[32];
    uint32_t i;
    size_t offset=0;
    if (prior==NULL || bytes==NULL || out==NULL) { return fail(r,STN_CHAIN_ARGUMENT,STN_DATA_ARGUMENT); }
    status=context_valid(context);
    if (status!=STN_DATA_OK) { return fail(r,STN_CHAIN_CONTEXT,status); }
    status=stn_block_decode(bytes,length,&b);
    r.structure=stage(status); r.failing_index=0;
    if (status!=STN_DATA_OK) {
        stn_block_header header;
        if (length>=STN_BLOCK_HEADER_SIZE &&
            stn_block_header_decode(bytes,STN_BLOCK_HEADER_SIZE,&header)==STN_DATA_OK) {
            r.height_available=1; r.failing_height=header.height;
        }
        return fail(r,STN_CHAIN_STRUCTURE,status);
    }
    r.height_available=1; r.failing_height=b.header.height;
    if(b.header.version!=(context->pow_policy!=NULL ? STN_POW_BLOCK_VERSION : 1u)) {
        r.link=STN_STAGE_REJECT; return fail(r,STN_CHAIN_CONTEXT,STN_DATA_VERSION);
    }
    status=prior_valid(context,prior); r.link=stage(status);
    if (status!=STN_DATA_OK) { return fail(r,STN_CHAIN_STATE,status); }
    r.link=STN_STAGE_REJECT;
    if (memcmp(b.header.network_id,context->network_id,32)!=0) { return fail(r,STN_CHAIN_NETWORK,STN_DATA_CONTENT); }
    if (!prior->has_tip) {
        if (length!=context->genesis_length || memcmp(bytes,context->genesis_bytes,length)!=0) {
            return fail(r,STN_CHAIN_GENESIS,STN_DATA_CONTENT);
        }
    } else {
        if (prior->height==UINT64_MAX || b.header.height!=prior->height+1) { return fail(r,STN_CHAIN_HEIGHT,STN_DATA_CONTENT); }
        if (memcmp(b.header.previous_hash,prior->tip_id,32)!=0) { return fail(r,STN_CHAIN_PARENT,STN_DATA_CONTENT); }
        if (b.header.timestamp<prior->timestamp) { return fail(r,STN_CHAIN_TIMESTAMP,STN_DATA_CONTENT); }
    }
    r.link=STN_STAGE_PASS;
    if(context->pow_policy!=NULL) {
        uint8_t required[32];
        status=stn_chain_required_target(context,prior,required);r.target=stage(status);
        if(status!=STN_DATA_OK) { return fail(r,STN_CHAIN_TARGET,status); }
        r.target=memcmp(b.header.reserved_target,required,32)==0 ? STN_STAGE_PASS : STN_STAGE_REJECT;
        if(r.target!=STN_STAGE_PASS) { return fail(r,STN_CHAIN_TARGET,STN_DATA_TARGET); }
    }
    status=stn_block_check_integrity(bytes,length,&context->hash_provider); r.body=stage(status);
    if (status!=STN_DATA_OK) { return fail(r,STN_CHAIN_BODY,status); }
    /* Publication record network must agree; payload/signature semantics are
     * intentionally delegated to the existing record-validation layer later. */
    for (i=0;i<b.header.transaction_count;++i) {
        uint32_t n=(uint32_t)stn_wire_read(b.body+offset,4);
        stn_transaction tx; stn_record record;
        offset+=4;
        if (stn_transaction_decode(b.body+offset,n,&tx)!=STN_DATA_OK ||
            (tx.type==STN_TX_PUBLICATION && stn_record_decode(tx.record_bytes,tx.record_length,&record)!=STN_RECORD_OK)) {
            r.body=STN_STAGE_REJECT; return fail(r,STN_CHAIN_BODY,STN_DATA_CONTENT);
        }
        if(tx.type==STN_TX_CONTRACT_ACTION){has_contracts=1;}
        else if(tx.type==STN_TX_SHARE_EVIDENCE){has_shares=1;}
        else if(tx.type==STN_TX_COMPENSATION_DESTINATION){has_compensation=1;}
        else if(tx.type==STN_TX_ISSUANCE){has_issuance=1;}
        else if(tx.type==STN_TX_TRANSFER){has_transfer=1;}
        else if(tx.type!=STN_TX_PUBLICATION){has_lifecycle=1;}
        else {
            if (memcmp(record.network_id,context->network_id,32)!=0) {
                r.body=STN_STAGE_REJECT; return fail(r,STN_CHAIN_NETWORK,STN_DATA_CONTENT);
            }
            if (b.header.height>=prior->publication_activation_height) has_lifecycle=1;
        }
        offset+=n;
    }
    status=stn_chain_block_id(bytes,length,&context->hash_provider,id); r.identifier=stage(status);
    if (status!=STN_DATA_OK) { return fail(r,STN_CHAIN_HASH,status); }
    if (zero32(id)) { r.identifier=STN_STAGE_REJECT; return fail(r,STN_CHAIN_ZERO_ID,STN_DATA_CONTENT); }
    next=*prior; /* BORROW until retained or replaced by a deep clone below. */ next.has_tip=1; next.height=b.header.height; next.timestamp=b.header.timestamp;
    if(context->pow_policy!=NULL) {
        stn_work unit;
        status=stn_pow_compare(id,b.header.reserved_target); r.pow=stage(status);
        if(status!=STN_DATA_OK) { return fail(r,STN_CHAIN_POW,status); }
        status=stn_target_work(b.header.reserved_target,&unit);
        if(status==STN_DATA_OK) { status=stn_work_add(&prior->cumulative_work,&unit,&next.cumulative_work); }
        r.work=stage(status);
        if(status!=STN_DATA_OK) { return fail(r,STN_CHAIN_WORK,status); }
        memcpy(next.current_target,b.header.reserved_target,32);
        if(b.header.height%60==0) { memset(next.target_history,0,sizeof(next.target_history));next.target_history_count=0; }
        {
            stn_block_header *h=&next.target_history[next.target_history_count++];
            memset(h,0,sizeof(*h));h->version=3;h->height=b.header.height;h->timestamp=b.header.timestamp;
            memcpy(h->reserved_target,b.header.reserved_target,32);
        }
    }
    memcpy(next.tip_id,id,32);
    if (!prior->has_tip) { memcpy(next.genesis_id,id,32); }
    if(has_lifecycle) {
        candidate_lifecycle=(stn_chain_lifecycle_owned *)prior->lifecycle;
        if(reconstruct && candidate_lifecycle!=NULL && candidate_lifecycle->references==1 &&
           prior->lifecycle->grant_count<=prior->lifecycle->grant_capacity &&
           prior->lifecycle->replay.consumed_count<=prior->lifecycle->replay.consumed_capacity &&
           b.header.transaction_count<=prior->lifecycle->grant_capacity-prior->lifecycle->grant_count &&
           b.header.transaction_count<=prior->lifecycle->replay.consumed_capacity-prior->lifecycle->replay.consumed_count){
            reused_lifecycle=1;
        }else candidate_lifecycle=lifecycle_clone(prior);
        if(candidate_lifecycle==NULL){r.body=STN_STAGE_ERROR;return fail(r,STN_CHAIN_BODY,STN_DATA_PROVIDER_ERROR);}
    }
    if(has_shares){
        candidate_shares=share_clone(prior);
        if(candidate_shares==NULL){
            if(has_lifecycle && !reused_lifecycle)lifecycle_destroy(candidate_lifecycle);
            if(has_shares)share_destroy(candidate_shares);
            r.body=STN_STAGE_ERROR;return fail(r,STN_CHAIN_BODY,STN_DATA_PROVIDER_ERROR);
        }
    }
    if(has_compensation){
        candidate_compensation=compensation_clone(prior);
        if(candidate_compensation==NULL){
            if(has_lifecycle && !reused_lifecycle)lifecycle_destroy(candidate_lifecycle);
            if(has_shares)share_destroy(candidate_shares);
            r.body=STN_STAGE_ERROR;return fail(r,STN_CHAIN_BODY,STN_DATA_PROVIDER_ERROR);
        }
    }
    if(has_issuance || has_transfer){candidate_economy=economic_clone(prior);if(candidate_economy==NULL){if(has_lifecycle && !reused_lifecycle)lifecycle_destroy(candidate_lifecycle);if(has_shares)share_destroy(candidate_shares);if(has_compensation)compensation_destroy(candidate_compensation);r.body=STN_STAGE_ERROR;return fail(r,STN_CHAIN_BODY,STN_DATA_PROVIDER_ERROR);}}
    if(has_contracts){
        candidate_contracts=stn_contract_snapshot_clone(prior->contracts);
        if(candidate_contracts==NULL){
            if(has_lifecycle && !reused_lifecycle)lifecycle_destroy(candidate_lifecycle);
            r.body=STN_STAGE_ERROR;return fail(r,STN_CHAIN_BODY,STN_DATA_PROVIDER_ERROR);
        }
    }
    if(has_lifecycle || has_contracts || has_shares || has_compensation || has_issuance || has_transfer){
        stn_data_status failure=STN_DATA_OK;
        offset=0;
        for(i=0;i<b.header.transaction_count;++i){
            stn_transaction tx;
            uint32_t n=(uint32_t)stn_wire_read(b.body+offset,4);
            offset+=4;
            if(stn_transaction_decode(b.body+offset,n,&tx)!=STN_DATA_OK){
                failure=STN_DATA_CONTENT;break;
            }
            offset+=n;
            if(tx.type==STN_TX_ISSUANCE){
                stn_issuance_record issuance;stn_address mapped;size_t hi,hj;int found=0;
                if(stn_issuance_decode(tx.record_bytes,tx.record_length,&issuance)!=STN_DATA_OK || issuance.reason!=STN_ISSUANCE_REASON_SHARE){failure=STN_DATA_UNRESOLVED;break;}
                if(stn_compensation_state_lookup(has_compensation?&candidate_compensation->state:prior->compensation,&issuance.destination.mining_identity,&mapped)!=STN_DATA_OK || memcmp(mapped.identifier,issuance.destination.wallet.identifier,32u)!=0){failure=STN_DATA_CONTENT;break;}
                if(history==NULL){failure=STN_DATA_UNRESOLVED;break;}
                for(hi=0;hi<history_count && !found;++hi){stn_block hb;size_t ho=0;if(stn_block_decode(history[hi].bytes,history[hi].length,&hb)!=STN_DATA_OK){failure=STN_DATA_CONTENT;break;}for(hj=0;hj<hb.header.transaction_count;++hj){uint32_t hn=(uint32_t)stn_wire_read(hb.body+ho,4);stn_transaction htx;ho+=4;if(stn_transaction_decode(hb.body+ho,hn,&htx)!=STN_DATA_OK){failure=STN_DATA_CONTENT;break;}if(htx.type==STN_TX_SHARE_EVIDENCE){stn_share_evidence share;if(stn_share_decode(htx.record_bytes,htx.record_length,&share)==STN_DATA_OK && stn_issuance_bind_share(&issuance,&share,has_compensation?&candidate_compensation->state:prior->compensation)==STN_DATA_OK){found=1;break;}}ho+=hn;}}
                if(failure!=STN_DATA_OK)break;if(!found){failure=STN_DATA_CONTENT;break;}failure=stn_economic_state_apply(&candidate_economy->state,&issuance);if(failure!=STN_DATA_OK)break;continue;
            }
            if(tx.type==STN_TX_TRANSFER){
                stn_transfer_envelope envelope;
                stn_transfer_envelope_replay_state replay;
                if(stn_transfer_envelope_decode(tx.record_bytes,tx.record_length,&envelope)!=STN_DATA_OK){failure=STN_DATA_CONTENT;break;}
                stn_transfer_envelope_replay_initialize(&replay,candidate_economy->replay_bytes,candidate_economy->replay_capacity);
                replay.consumed_count=candidate_economy->replay_count;
                failure=stn_transfer_envelope_accept(&candidate_economy->state,&replay,&envelope);
                if(failure!=STN_DATA_OK)break;
                candidate_economy->replay_count=replay.consumed_count;
                continue;
            }
            if(tx.type==STN_TX_COMPENSATION_DESTINATION){
                stn_compensation_destination destination;
                if(stn_compensation_destination_decode(tx.record_bytes,tx.record_length,&destination)!=STN_DATA_OK){failure=STN_DATA_CONTENT;break;}
                failure=stn_compensation_state_apply(&candidate_compensation->state,&destination);
                if(failure!=STN_DATA_OK)break;
                continue;
            }
            if(tx.type==STN_TX_CONTRACT_ACTION){
                const stn_lifecycle_state *authority_state=
                    has_lifecycle ? &candidate_lifecycle->state : prior->lifecycle;
                failure=contract_apply_transaction(candidate_contracts,
                    authority_state,&tx,&context->hash_provider);
                if(failure!=STN_DATA_OK)break;
                continue;
            }
            if(tx.type==STN_TX_SHARE_EVIDENCE){
                stn_share_evidence share;
                stn_share_replay_result replay;
                stn_block_header work_header;
                uint8_t proof[32];
                if(stn_share_decode(tx.record_bytes,tx.record_length,&share)!=STN_DATA_OK ||
                   stn_block_header_decode(share.template_header,
                       STN_SHARE_TEMPLATE_HEADER_SIZE,&work_header)!=STN_DATA_OK ||
                   work_header.version!=STN_POW_BLOCK_VERSION ||
                   work_header.reserved_work_nonce!=0u ||
                   memcmp(work_header.transaction_commitment,share.body_commitment,32u)!=0 ||
                   memcmp(work_header.network_id,context->network_id,32)!=0 ||
                   !prior->has_tip ||
                   prior->height==UINT64_MAX ||
                   work_header.height>=b.header.height ||
                   work_header.height==0u ||
                   stn_share_verify_evidence(&share,&context->hash_provider,proof)!=STN_DATA_OK){
                    failure=STN_DATA_CONTENT;break;
                }
                /*
                 * Reconstruction has the immutable accepted prefix available,
                 * so bind deferred evidence to the exact historical parent.
                 * Ordinary one-block validation has only consensus state; its
                 * immediate-parent case remains independently checkable.
                 */
                if(history!=NULL){
                    size_t parent_index=(size_t)(work_header.height-1u);
                    uint8_t parent_id[32];
                    if(parent_index>=history_count ||
                       stn_chain_block_id(history[parent_index].bytes,
                           history[parent_index].length,
                           &context->hash_provider,parent_id)!=STN_DATA_OK ||
                       memcmp(parent_id,work_header.previous_hash,32u)!=0){
                        failure=STN_DATA_CONTENT;break;
                    }
                }else{
                    /*
                     * State-only validation can prove only the immediately
                     * preceding Work ID. Older deferred evidence requires the
                     * immutable accepted prefix and therefore fails closed here.
                     */
                    if(work_header.height+1u!=b.header.height ||
                       memcmp(work_header.previous_hash,prior->tip_id,32u)!=0){
                        failure=STN_DATA_UNRESOLVED;break;
                    }
                }
                replay=stn_share_replay_consume(&candidate_shares->state,&share);
                if(replay!=STN_SHARE_REPLAY_FRESH){
                    failure=replay==STN_SHARE_REPLAY_CAPACITY ?
                        STN_DATA_CAPACITY : STN_DATA_DUPLICATE;
                    break;
                }
                continue;
            }
            if(!has_lifecycle)continue;
            if(tx.type==STN_TX_PUBLICATION){
                stn_lifecycle_result result;
                if(b.header.height<prior->publication_activation_height)continue;
                result=stn_lifecycle_check_publication(&candidate_lifecycle->state,
                    tx.record_bytes,tx.record_length,&context->hash_provider);
                if(result!=STN_LIFECYCLE_OK){
                    failure=result==STN_LIFECYCLE_CAPACITY || result==STN_LIFECYCLE_PROVIDER ?
                        STN_DATA_PROVIDER_ERROR : STN_DATA_CONTENT;
                    break;
                }
            }
            {
                stn_lifecycle_result result=stn_lifecycle_apply_transaction(
                    &candidate_lifecycle->state,&tx,
                    context->genesis_authority_roots,
                    context->genesis_authority_root_count,&context->hash_provider);
                if(result!=STN_LIFECYCLE_OK){
                    failure=result==STN_LIFECYCLE_CAPACITY || result==STN_LIFECYCLE_PROVIDER ?
                        STN_DATA_PROVIDER_ERROR : STN_DATA_CONTENT;
                    break;
                }
            }
        }
        if(failure!=STN_DATA_OK){
            if(has_contracts)stn_contract_snapshot_release(candidate_contracts);
            if(has_shares)share_destroy(candidate_shares);
            if(has_compensation)compensation_destroy(candidate_compensation);
            if(has_issuance || has_transfer)economic_destroy(candidate_economy);
            if(has_lifecycle && !reused_lifecycle)lifecycle_destroy(candidate_lifecycle);
            r.body=stage(failure);return fail(r,STN_CHAIN_BODY,failure);
        }
    }
    {
        stn_chain_state shared;
        if(stn_chain_state_share(prior,&shared)!=STN_DATA_OK){
            if(has_contracts)stn_contract_snapshot_release(candidate_contracts);
            if(has_shares)share_destroy(candidate_shares);
            if(has_compensation)compensation_destroy(candidate_compensation);
            if(has_issuance || has_transfer)economic_destroy(candidate_economy);
            if(has_lifecycle && !reused_lifecycle)lifecycle_destroy(candidate_lifecycle);
            return fail(r,STN_CHAIN_BODY,STN_DATA_CAPACITY);
        }
        if(has_lifecycle){
            stn_chain_lifecycle_owned *old=(stn_chain_lifecycle_owned *)shared.lifecycle;
            if(--old->references==0)lifecycle_destroy(old);
            next.lifecycle=&candidate_lifecycle->state;
        }else next.lifecycle=shared.lifecycle;
        if(has_contracts){
            stn_contract_snapshot_release(shared.contracts);
            next.contracts=candidate_contracts;
        }else next.contracts=shared.contracts;
        if(has_compensation){
            stn_chain_compensation_owned *old=(stn_chain_compensation_owned *)shared.compensation;
            if(--old->references==0)compensation_destroy(old);
            next.compensation=&candidate_compensation->state;
        }else next.compensation=shared.compensation;
        if(has_issuance || has_transfer){stn_chain_economic_owned *old=(stn_chain_economic_owned *)shared.economy;if(--old->references==0)economic_destroy(old);next.economy=&candidate_economy->state;}else next.economy=shared.economy;
        if(has_shares){
            stn_chain_share_owned *old=(stn_chain_share_owned *)shared.shares;
            if(--old->references==0)share_destroy(old);
            next.shares=&candidate_shares->state;
        }else next.shares=shared.shares;
    }
    if(out==prior && !reused_lifecycle)stn_chain_state_release(out);
    *out=next; /* transfer candidate reference to a fresh output */
    r.acceptance=STN_ACCEPTANCE_UNDER_CONTEXT; r.reason=STN_CHAIN_NONE;
    r.failing_index=SIZE_MAX; r.height_available=0; r.failing_height=0;
    return r;
}

stn_chain_report stn_chain_validate_candidate(const stn_chain_context *context,
    const stn_chain_state *prior,const uint8_t *bytes,size_t length,stn_chain_state *out)
{
    return validate_candidate(context,prior,NULL,0,bytes,length,out,0);
}

stn_chain_report stn_chain_reconstruct_history(const stn_chain_context *context,
    const stn_block_span *blocks,size_t count,stn_chain_state *out)
{
    stn_chain_state state={0};stn_chain_report r=initial_report();size_t i;
    stn_data_status status;
    if(blocks==NULL || count==0 || out==NULL)return fail(r,STN_CHAIN_ARGUMENT,STN_DATA_ARGUMENT);
    status=stn_chain_initialize(context,&state);
    if(status!=STN_DATA_OK)return fail(r,STN_CHAIN_CONTEXT,status);
    for(i=0;i<count;++i){
        r=validate_candidate(context,&state,blocks,i,blocks[i].bytes,blocks[i].length,&state,1);
        if(r.acceptance!=STN_ACCEPTANCE_UNDER_CONTEXT){
            r.failing_index=i;stn_chain_state_release(&state);return r;
        }
    }
    *out=state; /* Transfer only a completely validated history. */
    return r;
}

stn_chain_report stn_chain_validate_sequence(const stn_chain_context *context,
    const stn_chain_state *prior, const stn_block_span *blocks, size_t count, stn_chain_state *out)
{
    stn_chain_report r=initial_report();
    stn_chain_state temporary;
    stn_data_status status;
    size_t i;
    if (prior==NULL || out==NULL || (count!=0 && blocks==NULL)) { return fail(r,STN_CHAIN_ARGUMENT,STN_DATA_ARGUMENT); }
    if (count>STN_CHAIN_MAX_BATCH) { return fail(r,STN_CHAIN_BATCH_LIMIT,STN_DATA_LENGTH); }
    status=context_valid(context);
    if (status!=STN_DATA_OK) { return fail(r,STN_CHAIN_CONTEXT,status); }
    status=prior_valid(context,prior);
    if (status!=STN_DATA_OK) { return fail(r,STN_CHAIN_STATE,status); }
    if(stn_chain_state_share(prior,&temporary)!=STN_DATA_OK)return fail(r,STN_CHAIN_STATE,STN_DATA_CAPACITY);
    for (i=0;i<count;++i) {
        r=stn_chain_validate_candidate(context,&temporary,blocks[i].bytes,blocks[i].length,&temporary);
        if (r.acceptance!=STN_ACCEPTANCE_UNDER_CONTEXT) { r.failing_index=i; stn_chain_state_release(&temporary); return r; }
    }
    if(out==prior)stn_chain_state_release(out);
    *out=temporary; /* transfer */
    r.acceptance=STN_ACCEPTANCE_UNDER_CONTEXT;
    return r;
}

/* Revalidate the complete immutable accepted history before lookup. The two
 * replay projections deliberately differ: legacy qualifying publications affect
 * query eligibility, never historical consensus acceptance of later actions. */
static stn_data_status record_search(const stn_chain_context *context,
    const stn_block_span *blocks,size_t count,const uint8_t key[32],const stn_chain_cursor *after,
    stn_chain_record_match *out,stn_chain_cursor *position)
{
    stn_lifecycle_state historical,observed;stn_replay_state eligible;
    stn_block block;stn_transaction tx;size_t i,j,offset,total=0,grants=0,gb,rb;
    uint8_t *memory,initial[32]={0},id[32],block_id[32];stn_data_status code=STN_DATA_OK;
    stn_chain_state current;stn_chain_report report;stn_chain_record_match match={0};
    uint64_t activation;
    if(out==NULL)return STN_DATA_ARGUMENT;
    if(context==NULL || context->pow_policy==NULL || blocks==NULL || count==0)return STN_DATA_UNRESOLVED;
    if(stn_chain_initialize(context,&current)!=STN_DATA_OK)return STN_DATA_PROVIDER_ERROR;
    for(i=0;i<count;++i){
        report=stn_chain_validate_candidate(context,&current,blocks[i].bytes,blocks[i].length,&current);
        if(report.acceptance==STN_ACCEPTANCE_UNRESOLVED){stn_chain_state_release(&current);return STN_DATA_UNRESOLVED;}
        if(report.acceptance!=STN_ACCEPTANCE_UNDER_CONTEXT){stn_chain_state_release(&current);return STN_DATA_PROVIDER_ERROR;}

    }
    activation=current.publication_activation_height;stn_chain_state_release(&current);
    for(i=0;i<count;++i){
        if(stn_block_decode(blocks[i].bytes,blocks[i].length,&block)!=STN_DATA_OK)return STN_DATA_PROVIDER_ERROR;
        if(total>SIZE_MAX-block.header.transaction_count)return STN_DATA_CAPACITY;
        total+=block.header.transaction_count;offset=0;
        for(j=0;j<block.header.transaction_count;++j){
            size_t n=(size_t)stn_wire_read(block.body+offset,4);offset+=4;
            if(stn_transaction_decode(block.body+offset,n,&tx)!=STN_DATA_OK)return STN_DATA_PROVIDER_ERROR;
            if(tx.type==STN_TX_AUTHORITY_GRANT)++grants;
            offset+=n;
        }
    }
    if(grants>SIZE_MAX/STN_AUTHORITY_GRANT_SIZE || total>SIZE_MAX/(2u*STN_REPLAY_ID_SIZE))return STN_DATA_CAPACITY;
    gb=grants*STN_AUTHORITY_GRANT_SIZE;rb=total*STN_REPLAY_ID_SIZE;
    if(gb>SIZE_MAX-2u*rb)return STN_DATA_CAPACITY;
    memory=malloc(gb+2u*rb);if(memory==NULL)return STN_DATA_PROVIDER_ERROR;
    if(context->genesis_initial_identity_count)memcpy(initial,context->genesis_initial_identities,32);
    stn_lifecycle_initialize(&historical,memory,grants,memory+gb,total,initial);
    stn_lifecycle_set_initial_identities(&historical,context->genesis_initial_identities,context->genesis_initial_identity_count);
    stn_replay_state_initialize(&eligible,memory+gb+rb,total);
    for(i=0;i<count;++i){
        if(stn_block_decode(blocks[i].bytes,blocks[i].length,&block)!=STN_DATA_OK){code=STN_DATA_PROVIDER_ERROR;goto done;}
        offset=0;
        for(j=0;j<block.header.transaction_count;++j){
            size_t n=(size_t)stn_wire_read(block.body+offset,4);stn_lifecycle_result result;
            offset+=4;
            if(stn_transaction_decode(block.body+offset,n,&tx)!=STN_DATA_OK){code=STN_DATA_PROVIDER_ERROR;goto done;}
            if(tx.type==STN_TX_PUBLICATION){
                observed=historical;observed.replay=eligible;
                result=stn_lifecycle_check_publication(&observed,tx.record_bytes,tx.record_length,&context->hash_provider);
                if(result==STN_LIFECYCLE_PROVIDER || result==STN_LIFECYCLE_CAPACITY){code=STN_DATA_PROVIDER_ERROR;goto done;}
                if(result==STN_LIFECYCLE_OK){
                    if(stn_record_id(tx.record_bytes,tx.record_length,&context->hash_provider,id)!=STN_DATA_OK){code=STN_DATA_PROVIDER_ERROR;goto done;}
                    if((key==NULL && after==NULL) || (key!=NULL && memcmp(id,key,32)==0) || (after!=NULL &&
                        (block.header.height>after->height || (block.header.height==after->height && j>after->transaction_position)))){
                        if(stn_chain_block_id(blocks[i].bytes,blocks[i].length,&context->hash_provider,block_id)!=STN_DATA_OK){code=STN_DATA_PROVIDER_ERROR;goto done;}
                        match.found=1;memcpy(match.record_id,id,32);match.height=block.header.height;memcpy(match.block_id,block_id,32);
                        match.transaction.bytes=block.body+offset;match.transaction.length=(uint32_t)n;
                        if(position!=NULL){position->version=1;position->height=block.header.height;memcpy(position->block_id,block_id,32);position->transaction_position=(uint32_t)j;}
                        code=STN_DATA_OK;goto done;
                    }
                    if(stn_lifecycle_apply_transaction(&observed,&tx,context->genesis_authority_roots,context->genesis_authority_root_count,&context->hash_provider)!=STN_LIFECYCLE_OK){code=STN_DATA_PROVIDER_ERROR;goto done;}
                    eligible=observed.replay;
                }
                if(block.header.height<activation){offset+=n;continue;}
            }
            result=stn_lifecycle_apply_transaction(&historical,&tx,context->genesis_authority_roots,context->genesis_authority_root_count,&context->hash_provider);
            if(result!=STN_LIFECYCLE_OK){code=STN_DATA_PROVIDER_ERROR;goto done;}
            if(stn_replay_state_consume(&eligible,historical.replay.consumed+(historical.replay.consumed_count-1)*STN_REPLAY_ID_SIZE)==STN_REPLAY_MALFORMED){code=STN_DATA_PROVIDER_ERROR;goto done;}
            offset+=n;
        }
    }
done:
    free(memory);if(code==STN_DATA_OK)*out=match;return code;
}

stn_cursor_result stn_chain_cursor_encode(const stn_chain_cursor *cursor,
    uint8_t *bytes,size_t length)
{
    if(cursor==NULL || bytes==NULL || length!=STN_CHAIN_CURSOR_SIZE ||
        cursor->version!=STN_CHAIN_CURSOR_VERSION)return STN_CURSOR_MALFORMED;
    bytes[0]=cursor->version;stn_wire_write(bytes+1,8,cursor->height);
    memcpy(bytes+9,cursor->block_id,32);stn_wire_write(bytes+41,4,cursor->transaction_position);
    return STN_CURSOR_VALID;
}
stn_cursor_result stn_chain_cursor_decode(const uint8_t *bytes,size_t length,
    stn_chain_cursor *cursor)
{
    stn_chain_cursor decoded={0};
    if(bytes==NULL || cursor==NULL || length!=STN_CHAIN_CURSOR_SIZE ||
        bytes[0]!=STN_CHAIN_CURSOR_VERSION)return STN_CURSOR_MALFORMED;
    decoded.version=bytes[0];decoded.height=stn_wire_read(bytes+1,8);
    memcpy(decoded.block_id,bytes+9,32);decoded.transaction_position=(uint32_t)stn_wire_read(bytes+41,4);
    *cursor=decoded;return STN_CURSOR_VALID;
}
stn_cursor_result stn_chain_cursor_validate(const stn_chain_context *context,
    const stn_block_span *blocks,size_t count,const stn_chain_cursor *cursor)
{
    stn_chain_state state;stn_chain_report report;stn_block block;
    size_t i;stn_cursor_result result=STN_CURSOR_DETACHED;uint8_t id[32];
    if(cursor==NULL || cursor->version!=STN_CHAIN_CURSOR_VERSION ||
        (blocks==NULL && count!=0))return STN_CURSOR_MALFORMED;
    if(context==NULL || context->pow_policy==NULL || count==0)return STN_CURSOR_UNAVAILABLE;
    if(stn_chain_initialize(context,&state)!=STN_DATA_OK)return STN_CURSOR_PROVIDER;
    for(i=0;i<count;++i){
        report=stn_chain_validate_candidate(context,&state,blocks[i].bytes,blocks[i].length,&state);
        if(report.acceptance==STN_ACCEPTANCE_UNRESOLVED){result=STN_CURSOR_UNAVAILABLE;goto cursor_done;}
        if(report.acceptance!=STN_ACCEPTANCE_UNDER_CONTEXT){result=STN_CURSOR_PROVIDER;goto cursor_done;}

        if(stn_block_decode(blocks[i].bytes,blocks[i].length,&block)!=STN_DATA_OK){result=STN_CURSOR_PROVIDER;goto cursor_done;}
        if(block.header.height==cursor->height){
            if(stn_chain_block_id(blocks[i].bytes,blocks[i].length,&context->hash_provider,id)!=STN_DATA_OK){result=STN_CURSOR_PROVIDER;goto cursor_done;}
            if(memcmp(id,cursor->block_id,32)==0 && cursor->transaction_position<block.header.transaction_count)result=STN_CURSOR_VALID;
        }
    }
cursor_done:
    stn_chain_state_release(&state);return result;
}
stn_data_status stn_chain_lookup_record(const stn_chain_context *context,
    const stn_block_span *blocks,size_t count,const uint8_t key[32],stn_chain_record_match *out)
{
    if(key==NULL)return STN_DATA_ARGUMENT;
    return record_search(context,blocks,count,key,NULL,out,NULL);
}
stn_next_result stn_chain_next_record(const stn_chain_context *context,
    const stn_block_span *blocks,size_t count,const stn_chain_cursor *after,
    stn_chain_record_match *out,stn_chain_cursor *position)
{
    stn_cursor_result valid;stn_data_status status;stn_chain_record_match match;
    stn_chain_cursor next={0};
    if(out==NULL || position==NULL)return STN_NEXT_MALFORMED;
    valid=stn_chain_cursor_validate(context,blocks,count,after);
    if(valid==STN_CURSOR_MALFORMED)return STN_NEXT_MALFORMED;
    if(valid==STN_CURSOR_DETACHED)return STN_NEXT_DETACHED;
    if(valid==STN_CURSOR_UNAVAILABLE)return STN_NEXT_UNAVAILABLE;
    if(valid!=STN_CURSOR_VALID)return STN_NEXT_PROVIDER;
    status=record_search(context,blocks,count,NULL,after,&match,&next);
    if(status==STN_DATA_UNRESOLVED)return STN_NEXT_UNAVAILABLE;
    if(status==STN_DATA_CAPACITY)return STN_NEXT_CAPACITY;
    if(status!=STN_DATA_OK)return STN_NEXT_PROVIDER;
    if(!match.found)return STN_NEXT_END;
    *out=match;*position=next;return STN_NEXT_RECORD;
}
stn_first_result stn_chain_first_record(const stn_chain_context *context,
    const stn_block_span *blocks,size_t count,stn_chain_record_match *out,
    stn_chain_cursor *position)
{
    stn_data_status status;stn_chain_record_match match;stn_chain_cursor first={0};
    if(out==NULL || position==NULL)return STN_FIRST_ARGUMENT;
    status=record_search(context,blocks,count,NULL,NULL,&match,&first);
    if(status==STN_DATA_UNRESOLVED)return STN_FIRST_UNAVAILABLE;
    if(status==STN_DATA_CAPACITY)return STN_FIRST_CAPACITY;
    if(status!=STN_DATA_OK)return STN_FIRST_PROVIDER;
    if(!match.found)return STN_FIRST_END;
    *out=match;*position=first;return STN_FIRST_RECORD;
}