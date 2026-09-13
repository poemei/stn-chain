/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_chain.h"
#include "stn_wire_internal.h"
#include <string.h>
#include <stdlib.h>
#include "stn_sha256.h"

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
    uint8_t bytes[];

} stn_chain_lifecycle_owned;

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
    stn_chain_lifecycle_owned *o=calloc(1,sizeof(*o));uint8_t initial[32]={0};
    if(o==NULL)return NULL;
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
    o=calloc(1,sizeof(*o)+grant_bytes+replay_bytes);if(o==NULL)return NULL;
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
    memcpy(s.network_id,context->network_id,32); {stn_chain_lifecycle_owned *o=lifecycle_new(context);if(o==NULL)return STN_DATA_CAPACITY;s.lifecycle=&o->state;} *out=s;
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
    if(count!=(prior->has_tip ? (size_t)(prior->height%60)+1 : 0)) { return STN_DATA_UNRESOLVED; }
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

stn_chain_report stn_chain_validate_candidate(const stn_chain_context *context,
    const stn_chain_state *prior, const uint8_t *bytes, size_t length, stn_chain_state *out)
{
    stn_chain_report r=initial_report();
    stn_block b;
    stn_chain_state next;stn_chain_lifecycle_owned *candidate_lifecycle;int has_lifecycle=0;
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
        if(tx.type!=STN_TX_PUBLICATION){has_lifecycle=1;} else {
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
    next=*prior; next.has_tip=1; next.height=b.header.height; next.timestamp=b.header.timestamp;
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
        stn_lifecycle_result result=STN_LIFECYCLE_OK;
        candidate_lifecycle=lifecycle_clone(prior);
        if(candidate_lifecycle==NULL){r.body=STN_STAGE_ERROR;return fail(r,STN_CHAIN_BODY,STN_DATA_PROVIDER_ERROR);}
        offset=0;
        for(i=0;i<b.header.transaction_count;++i) {
            stn_transaction tx;uint32_t n=(uint32_t)stn_wire_read(b.body+offset,4);offset+=4;
            if(stn_transaction_decode(b.body+offset,n,&tx)!=STN_DATA_OK){result=STN_LIFECYCLE_MALFORMED;break;}
            offset+=n;
            if(tx.type==STN_TX_PUBLICATION) {
                if(b.header.height<prior->publication_activation_height)continue;
                result=stn_lifecycle_check_publication(&candidate_lifecycle->state,
                    tx.record_bytes,tx.record_length,&context->hash_provider);
                if(result!=STN_LIFECYCLE_OK)break;
            }
            result=stn_lifecycle_apply_transaction(&candidate_lifecycle->state,&tx,
                context->genesis_authority_roots,context->genesis_authority_root_count,&context->hash_provider);
            if(result!=STN_LIFECYCLE_OK)break;
        }
        if(result!=STN_LIFECYCLE_OK){
            stn_data_status failure=result==STN_LIFECYCLE_CAPACITY || result==STN_LIFECYCLE_PROVIDER ? STN_DATA_PROVIDER_ERROR : STN_DATA_CONTENT;
            free(candidate_lifecycle);r.body=stage(failure);return fail(r,STN_CHAIN_BODY,failure);
        }
        next.lifecycle=&candidate_lifecycle->state;
    }
    *out=next;
    r.acceptance=STN_ACCEPTANCE_UNDER_CONTEXT; r.reason=STN_CHAIN_NONE;
    r.failing_index=SIZE_MAX; r.height_available=0; r.failing_height=0;
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
    temporary=*prior;
    for (i=0;i<count;++i) {
        r=stn_chain_validate_candidate(context,&temporary,blocks[i].bytes,blocks[i].length,&temporary);
        if (r.acceptance!=STN_ACCEPTANCE_UNDER_CONTEXT) { r.failing_index=i; return r; }
    }
    *out=temporary;
    r.acceptance=STN_ACCEPTANCE_UNDER_CONTEXT;
    return r;
}

