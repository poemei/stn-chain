/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_chain.h"
#include "stn_wire_internal.h"
#include <string.h>

static int zero32(const uint8_t p[32])
{
    size_t i;
    for (i=0;i<32;++i) { if (p[i]!=0) { return 0; } }
    return 1;
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
    memcpy(s.network_id,context->network_id,32); *out=s;
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
    uint8_t anchor[32];
    stn_block genesis;
    stn_data_status status;
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
    stn_chain_state next;
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
            stn_record_decode(tx.record_bytes,tx.record_length,&record)!=STN_RECORD_OK) {
            r.body=STN_STAGE_REJECT; return fail(r,STN_CHAIN_BODY,STN_DATA_CONTENT);
        }
        if (memcmp(record.network_id,context->network_id,32)!=0) {
            r.body=STN_STAGE_REJECT; return fail(r,STN_CHAIN_NETWORK,STN_DATA_CONTENT);
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
