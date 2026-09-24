/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_fork.h"
#include <string.h>

stn_data_status stn_work_order(const stn_work *current,const stn_work *candidate,
    stn_fork_result *out)
{
    int order;
    if(current==NULL || candidate==NULL || out==NULL) { return STN_DATA_ARGUMENT; }
    order=memcmp(candidate->bytes,current->bytes,STN_WORK_SIZE);
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
    size_t count,int side,stn_chain_state *state)
{
    stn_fork_report r={0};
    stn_data_status status;
    size_t i;
    r.validation.failing_index=SIZE_MAX;
    if(blocks==NULL || count==0) {
        r.validation.reason=STN_CHAIN_BATCH_LIMIT;
        r.validation.detail=STN_DATA_LENGTH;
        return failure(r,side,STN_DATA_LENGTH);
    }
    /*
     * Fork choice must derive every consensus-visible state from the complete
     * accepted history. This includes Phase 19 balances and transfer replay;
     * no cached economic state from either peer is authoritative.
     */
    r.validation=stn_chain_reconstruct_history(c,blocks,count,state);
    if(r.validation.acceptance!=STN_ACCEPTANCE_UNDER_CONTEXT) {
        i=r.validation.failing_index;
        (void)i;
        return failure(r,side,r.validation.detail);
    }
    r.result=STN_FORK_TIE;
    return r;
}

void stn_reorg_plan_release(stn_reorg_plan *plan)
{
    if(plan!=NULL){stn_chain_state_release(&plan->current);stn_chain_state_release(&plan->candidate);memset(plan,0,sizeof(*plan));}
}

stn_fork_report stn_fork_evaluate_history(const stn_chain_context *context,
    const stn_block_span *current,size_t current_count,
    const stn_block_span *candidate,size_t candidate_count,stn_reorg_plan *out)
{
    stn_fork_report r={0};
    stn_reorg_plan plan={0};

    size_t common=0,limit;
    r.validation.failing_index=SIZE_MAX;
    if(context==NULL || out==NULL) {
        r.validation.detail=STN_DATA_ARGUMENT;
        r=failure(r,0,STN_DATA_ARGUMENT);goto done;
    }
    if(context->pow_policy==NULL) { r.result=STN_FORK_UNSUPPORTED; goto done; }
    r=history(context,current,current_count,1,&plan.current);
    if(r.result!=STN_FORK_TIE) { goto done; }
    r=history(context,candidate,candidate_count,2,&plan.candidate);
    if(r.result!=STN_FORK_TIE) { goto done; }
    limit=current_count<candidate_count ? current_count : candidate_count;
    while(common<limit) {
        uint8_t left[32],right[32];
        if(stn_chain_block_id(current[common].bytes,current[common].length,&context->hash_provider,left)!=STN_DATA_OK ||
           stn_chain_block_id(candidate[common].bytes,candidate[common].length,&context->hash_provider,right)!=STN_DATA_OK){r=failure(r,0,STN_DATA_PROVIDER_ERROR);goto done;}
        if(memcmp(left,right,32)!=0){break;}++common;
    }
    if(common==0) {
        r.validation.reason=STN_CHAIN_GENESIS; r.validation.detail=STN_DATA_CONTENT;
        r=failure(r,2,STN_DATA_CONTENT);goto done;
    }
    plan.ancestor_index=common-1;
    if(stn_chain_block_id(current[common-1].bytes,current[common-1].length,&context->hash_provider,plan.ancestor_id)!=STN_DATA_OK){r=failure(r,0,STN_DATA_PROVIDER_ERROR);goto done;}
    plan.detach_begin=common; plan.detach_end=current_count;
    plan.attach_begin=common; plan.attach_end=candidate_count;
    plan.detached_count=current_count-common; plan.attached_count=candidate_count-common;
    (void)stn_work_order(&plan.current.cumulative_work,&plan.candidate.cumulative_work,&r.result);
    plan.actionable=r.result==STN_FORK_CANDIDATE;
    plan.resulting_height=plan.actionable ? plan.candidate.height : plan.current.height;
    *out=plan;memset(&plan,0,sizeof(plan));
done:
    stn_reorg_plan_release(&plan);return r;
}


stn_fork_report stn_fork_evaluate(const stn_chain_context *context,
    const stn_block_span *current,size_t current_count,
    const stn_block_span *candidate,size_t candidate_count,stn_reorg_plan *out)
{
    if(current_count>STN_CHAIN_MAX_BATCH || candidate_count>STN_CHAIN_MAX_BATCH){
        stn_fork_report r={0};r.validation.reason=STN_CHAIN_BATCH_LIMIT;
        r.validation.detail=STN_DATA_LENGTH;r.validation.failing_index=SIZE_MAX;
        return failure(r,current_count>STN_CHAIN_MAX_BATCH?1:2,STN_DATA_LENGTH);
    }
    return stn_fork_evaluate_history(context,current,current_count,candidate,candidate_count,out);
}

stn_cursor_reorg_result stn_chain_resolve_cursor_reorg(const stn_chain_context *context,
    const stn_block_span *current,size_t current_count,
    const stn_block_span *retained,size_t retained_count,
    const stn_chain_cursor *cursor,stn_cursor_ancestor *out)
{
    stn_cursor_result valid;stn_reorg_plan plan={0};stn_fork_report report;
    if(out==NULL || (retained==NULL && retained_count!=0))return STN_CURSOR_REORG_MALFORMED;
    valid=stn_chain_cursor_validate(context,current,current_count,cursor);
    if(valid==STN_CURSOR_MALFORMED)return STN_CURSOR_REORG_MALFORMED;
    if(valid==STN_CURSOR_UNAVAILABLE)return STN_CURSOR_REORG_UNAVAILABLE;
    if(valid==STN_CURSOR_PROVIDER)return STN_CURSOR_REORG_PROVIDER;
    if(valid==STN_CURSOR_VALID)return STN_CURSOR_REORG_CURRENT;
    if(retained_count==0)return STN_CURSOR_REORG_NO_COMMON_ANCESTOR;
    valid=stn_chain_cursor_validate(context,retained,retained_count,cursor);
    if(valid==STN_CURSOR_PROVIDER)return STN_CURSOR_REORG_PROVIDER;
    if(valid!=STN_CURSOR_VALID)return STN_CURSOR_REORG_NO_COMMON_ANCESTOR;
    report=stn_fork_evaluate_history(context,current,current_count,retained,retained_count,&plan);
    if(report.result==STN_FORK_ERROR)return STN_CURSOR_REORG_PROVIDER;
    if(report.result!=STN_FORK_CURRENT && report.result!=STN_FORK_CANDIDATE && report.result!=STN_FORK_TIE)
        return STN_CURSOR_REORG_NO_COMMON_ANCESTOR;
    out->height=(uint64_t)plan.ancestor_index;memcpy(out->block_id,plan.ancestor_id,32);
    stn_reorg_plan_release(&plan);return STN_CURSOR_REORG_COMMON_ANCESTOR;
}
stn_consumer_recovery_result stn_chain_build_consumer_recovery_plan(
    const stn_chain_context *context,const stn_block_span *current,size_t current_count,
    const stn_block_span *retained,size_t retained_count,
    const stn_chain_cursor *cursor,stn_consumer_recovery_plan *out)
{
    stn_consumer_recovery_plan plan={0};stn_cursor_reorg_result ancestry;
    stn_chain_record_match match;stn_chain_cursor position,next;
    stn_first_result first;stn_next_result following;int found=0;
    if(out==NULL)return STN_RECOVERY_MALFORMED;
    ancestry=stn_chain_resolve_cursor_reorg(context,current,current_count,retained,retained_count,cursor,&plan.rollback);
    if(ancestry==STN_CURSOR_REORG_CURRENT)return STN_RECOVERY_CURRENT;
    if(ancestry==STN_CURSOR_REORG_MALFORMED)return STN_RECOVERY_MALFORMED;
    if(ancestry==STN_CURSOR_REORG_PROVIDER)return STN_RECOVERY_PROVIDER;
    if(ancestry!=STN_CURSOR_REORG_COMMON_ANCESTOR)return STN_RECOVERY_UNAVAILABLE;
    first=stn_chain_first_record(context,current,current_count,&match,&position);
    if(first!=STN_FIRST_RECORD && first!=STN_FIRST_END)return STN_RECOVERY_PROVIDER;
    if(first==STN_FIRST_RECORD){
        while(position.height<=plan.rollback.height){
            plan.resume=position;found=1;
            following=stn_chain_next_record(context,current,current_count,&position,&match,&next);
            if(following==STN_NEXT_END)break;
            if(following!=STN_NEXT_RECORD)return STN_RECOVERY_PROVIDER;
            position=next;
        }
    }
    *out=plan;return found?STN_RECOVERY_AFTER_CURSOR:STN_RECOVERY_FROM_START;
}