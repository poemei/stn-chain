/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_fork.h"
#include <string.h>

stn_data_status stn_work_order(const stn_work *current,const stn_work *candidate,
    stn_fork_result *out)
{
    int order;
    if(current==NULL || candidate==NULL || out==NULL) { return STN_DATA_ARGUMENT; }
    order=memcmp(candidate->bytes,current->bytes,32);
    *out=order>0 ? STN_FORK_CANDIDATE : order<0 ? STN_FORK_CURRENT : STN_FORK_TIE;
    return STN_DATA_OK;
}

static stn_fork_report failure(stn_fork_report r,int side,stn_data_status detail)
{
    r.failed_side=side;
    if(detail==STN_DATA_UNRESOLVED) { r.result=STN_FORK_UNRESOLVED; }
    else if(detail==STN_DATA_PROVIDER_ERROR || detail==STN_DATA_ARGUMENT || side==0) {
        r.result=STN_FORK_ERROR;
    } else { r.result=side==1 ? STN_FORK_INVALID_CURRENT : STN_FORK_INVALID_CANDIDATE; }
    return r;
}

static stn_fork_report history(const stn_chain_context *c,const stn_block_span *blocks,
    size_t count,int side,stn_chain_state *state,uint8_t ids[STN_CHAIN_MAX_BATCH][32])
{
    stn_fork_report r={0};
    stn_data_status status;
    size_t i;
    r.validation.failing_index=SIZE_MAX;
    if(blocks==NULL || count==0 || count>STN_CHAIN_MAX_BATCH) {
        r.validation.reason=STN_CHAIN_BATCH_LIMIT;
        r.validation.detail=STN_DATA_LENGTH;
        return failure(r,side,STN_DATA_LENGTH);
    }
    status=stn_chain_initialize(c,state);
    if(status!=STN_DATA_OK) {
        r.validation.detail=status; r.validation.reason=STN_CHAIN_CONTEXT;
        return failure(r,0,status);
    }
    for(i=0;i<count;++i) {
        r.validation=stn_chain_validate_candidate(c,state,blocks[i].bytes,blocks[i].length,state);
        if(r.validation.acceptance!=STN_ACCEPTANCE_UNDER_CONTEXT) {
            r.validation.failing_index=i;
            return failure(r,side,r.validation.detail);
        }
        memcpy(ids[i],state->tip_id,32);
    }
    r.result=STN_FORK_TIE;
    return r;
}

stn_fork_report stn_fork_evaluate(const stn_chain_context *context,
    const stn_block_span *current,size_t current_count,
    const stn_block_span *candidate,size_t candidate_count,stn_reorg_plan *out)
{
    stn_fork_report r={0};
    stn_reorg_plan plan={0};
    uint8_t current_ids[STN_CHAIN_MAX_BATCH][32],candidate_ids[STN_CHAIN_MAX_BATCH][32];
    size_t common=0,limit;
    r.validation.failing_index=SIZE_MAX;
    if(context==NULL || out==NULL) {
        r.validation.detail=STN_DATA_ARGUMENT;
        return failure(r,0,STN_DATA_ARGUMENT);
    }
    if(context->pow_policy==NULL) { r.result=STN_FORK_UNSUPPORTED; return r; }
    r=history(context,current,current_count,1,&plan.current,current_ids);
    if(r.result!=STN_FORK_TIE) { return r; }
    r=history(context,candidate,candidate_count,2,&plan.candidate,candidate_ids);
    if(r.result!=STN_FORK_TIE) { return r; }
    limit=current_count<candidate_count ? current_count : candidate_count;
    while(common<limit && memcmp(current_ids[common],candidate_ids[common],32)==0) { ++common; }
    if(common==0) {
        r.validation.reason=STN_CHAIN_GENESIS; r.validation.detail=STN_DATA_CONTENT;
        return failure(r,2,STN_DATA_CONTENT);
    }
    plan.ancestor_index=common-1;
    memcpy(plan.ancestor_id,current_ids[common-1],32);
    plan.detach_begin=common; plan.detach_end=current_count;
    plan.attach_begin=common; plan.attach_end=candidate_count;
    plan.detached_count=current_count-common; plan.attached_count=candidate_count-common;
    (void)stn_work_order(&plan.current.cumulative_work,&plan.candidate.cumulative_work,&r.result);
    plan.actionable=r.result==STN_FORK_CANDIDATE;
    plan.resulting_height=plan.actionable ? plan.candidate.height : plan.current.height;
    *out=plan;
    return r;
}
