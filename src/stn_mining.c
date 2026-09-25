/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_mining.h"
#include "stn_sha256.h"
#include "stn_wire_internal.h"
#include "stn_address.h"
#include "stn_share.h"
#include <string.h>
#include <stdlib.h>

static stn_rpc_code storage_code(stn_storage_status s)
{
    if(s==STN_STORAGE_OK){return STN_RPC_OK;}
    if(s==STN_STORAGE_STALE){return STN_RPC_STALE;}
    if(s==STN_STORAGE_CAPACITY){return STN_RPC_CAPACITY;}
    if(s==STN_STORAGE_VALIDATION || s==STN_STORAGE_FORMAT || s==STN_STORAGE_NOT_PREFERRED){return STN_RPC_REJECTED;}
    if(s==STN_STORAGE_NOT_FOUND || s==STN_STORAGE_UNRESOLVED){return STN_RPC_UNAVAILABLE;}
    return STN_RPC_PROVIDER;
}

static stn_rpc_submission_result submission_code(stn_pending_result result)
{
    switch(result){
    case STN_PENDING_ACCEPTED:return STN_RPC_ADMITTED;
    case STN_PENDING_DUPLICATE:return STN_RPC_DUPLICATE;
    case STN_PENDING_CAPACITY:return STN_RPC_POOL_FULL;
    case STN_PENDING_INVALID:case STN_PENDING_NETWORK:case STN_PENDING_TIME:return STN_RPC_BAD_SUBMISSION;
    case STN_PENDING_UNSUPPORTED:return STN_RPC_UNSUPPORTED_SUBMISSION;
    case STN_PENDING_REPLAY:return STN_RPC_REPLAY;
    case STN_PENDING_SIGNATURE:case STN_PENDING_AUTHORITY:return STN_RPC_UNAUTHORIZED;
    case STN_PENDING_UNAVAILABLE:return STN_RPC_ADMISSION_UNAVAILABLE;
    default:return STN_RPC_ADMISSION_INTERNAL;
    }
}

static int storage_bytes_required(const stn_storage_view *v,size_t extra,size_t *required)
{
    size_t total=STN_STORAGE_OVERHEAD,i;
    if(v==NULL || required==NULL){return 0;}
    for(i=0;i<v->count;++i){
        if(total>SIZE_MAX-4u || v->blocks[i].length>SIZE_MAX-total-4u){return 0;}
        total+=4u+v->blocks[i].length;
    }
    if(total>SIZE_MAX-4u || extra>SIZE_MAX-total-4u){return 0;}
    *required=total+4u+extra;return 1;
}

static int grow(uint8_t **p,size_t *capacity,size_t required,int owned)
{
    uint8_t *next;size_t cap;
    if(required<=*capacity){return 1;}
    if(!owned){return 0;}
    cap=*capacity ? *capacity : 4096u;
    while(cap<required){
        size_t doubled=cap<=SIZE_MAX/2u ? cap*2u : SIZE_MAX;
        if(doubled<=cap){cap=required;break;}
        cap=doubled;
    }
    next=(uint8_t*)realloc(*p,cap);
    if(next==NULL){return 0;}
    *p=next;*capacity=cap;return 1;
}

static int ensure_storage_capacity(stn_mining_service *s,size_t required)
{
    return
        grow(&s->workspace.current_bytes,&s->workspace.current_capacity,required,s->owns_buffers) &&
        grow(&s->workspace.next_bytes,&s->workspace.next_capacity,required,s->owns_buffers);
}

static stn_rpc_code template_build(stn_mining_service *s,const stn_storage_view *v,size_t *length,uint8_t id[32])
{
    static const uint8_t domain[]="STN-CHAIN:WORK:ID:1";
    stn_block b={0};size_t offset=0,i;stn_work increment,total;uint8_t required_target[32];
    const uint8_t *body=s->body;size_t body_length=s->body_length;uint32_t transaction_count=s->transaction_count;

    if(s->pending!=NULL){
        stn_data_status assembled=stn_pending_assemble(
            s->pending,
            s->intelligence,
            v,
            s->pending_body,
            s->pending_body_capacity,
            &body_length,
            &transaction_count);

        if(assembled!=STN_DATA_OK){
            return assembled==STN_DATA_UNRESOLVED ? STN_RPC_UNAVAILABLE :
                assembled==STN_DATA_CAPACITY ? STN_RPC_CAPACITY : STN_RPC_PROVIDER;
        }

        body=s->pending_body;
    }

    if(v->state.height==UINT64_MAX){return STN_RPC_CAPACITY;}

    /*
     * Empty mining candidates are canonical as:
     *
     *     transaction_count == 0
     *     body_length == 0
     *
     * A non-empty body is still required to contain at least one
     * transaction. This keeps the mining service aligned with the
     * canonical block structural rules.
     */
    if(transaction_count==0){
        if(body_length!=0){return STN_RPC_REJECTED;}
    } else if(body==NULL){
        return STN_RPC_UNAVAILABLE;
    }

    if(stn_block_body_validate_structure(body,body_length,transaction_count)!=STN_DATA_OK){
        return STN_RPC_REJECTED;
    }

    {
        stn_data_status target_status=stn_chain_required_target(
            s->chain,
            &v->state,
            required_target);

        if(target_status!=STN_DATA_OK){
            if(target_status==STN_DATA_UNRESOLVED){
                return STN_RPC_UNAVAILABLE;
            }
            if(target_status==STN_DATA_CAPACITY || target_status==STN_DATA_OVERFLOW){
                return STN_RPC_CAPACITY;
            }
            if(target_status==STN_DATA_PROVIDER_ERROR || target_status==STN_DATA_ARGUMENT){
                return STN_RPC_PROVIDER;
            }
            return STN_RPC_REJECTED;
        }
    }

    for(i=0;i<transaction_count;++i){
        size_t n=(size_t)stn_wire_read(body+offset,4);
        stn_transaction tx;
        if(stn_transaction_decode(body+offset+4,n,&tx)!=STN_DATA_OK){
            return STN_RPC_REJECTED;
        }
        if(tx.type==STN_TX_SHARE_EVIDENCE){
            stn_share_evidence share;
            stn_block_header work_header;
            uint8_t proof[32];
            if(stn_share_decode(tx.record_bytes,tx.record_length,&share)!=STN_DATA_OK ||
               stn_block_header_decode(share.template_header,
                   STN_SHARE_TEMPLATE_HEADER_SIZE,&work_header)!=STN_DATA_OK ||
               work_header.version!=STN_POW_BLOCK_VERSION ||
               work_header.reserved_work_nonce!=0u ||
               memcmp(work_header.network_id,v->state.network_id,32u)!=0 ||
               !v->state.has_tip ||
               v->state.height==UINT64_MAX ||
               work_header.height>=v->state.height+1u ||
               work_header.height==0u ||
               stn_share_verify_evidence(&share,&s->chain->hash_provider,proof)!=STN_DATA_OK){
                return STN_RPC_REJECTED;
            }
            {
                size_t parent_index=(size_t)(work_header.height-1u);
                uint8_t parent_id[32];
                if(parent_index>=v->count ||
                   stn_chain_block_id(v->blocks[parent_index].bytes,
                       v->blocks[parent_index].length,
                       &s->chain->hash_provider,parent_id)!=STN_DATA_OK ||
                   memcmp(parent_id,work_header.previous_hash,32u)!=0){
                    return STN_RPC_REJECTED;
                }
            }
        }
        /*
         * Publication records carry their network ID at record offset 8.
         * Lifecycle records carry it at their canonical record offset 8.
         * Contract actions carry their own typed canonical structure.
         * Phase 19 share evidence deliberately carries no duplicate network
         * field: its Work ID binds it to the exact Chain mining template.
         * Do not interpret share bytes as a lifecycle/publication record.
         */
        if(tx.type==STN_TX_PUBLICATION ||
           tx.type==STN_TX_AUTHORITY_GRANT ||
           tx.type==STN_TX_AUTHORITY_REVOKE ||
           tx.type==STN_TX_IDENTITY_ROTATE){
            if(n<52u ||
               memcmp(body+offset+4+20,s->chain->network_id,32)!=0){
                return STN_RPC_REJECTED;
            }
        }
        offset+=4+n;
    }

    if(stn_target_work(required_target,&increment)!=STN_DATA_OK ||
       stn_work_add(&v->state.cumulative_work,&increment,&total)!=STN_DATA_OK){
        return STN_RPC_REJECTED;
    }

    b.header.version=3;
    memcpy(b.header.network_id,v->state.network_id,32);
    memcpy(b.header.previous_hash,v->state.tip_id,32);
    b.header.height=v->state.height+1;
    b.header.timestamp=v->state.timestamp;
    if(s->timestamp_now!=NULL){
        uint64_t now=s->timestamp_now(s->timestamp_user);
        if(now==0u){return STN_RPC_UNAVAILABLE;}
        if(now>b.header.timestamp){b.header.timestamp=now;}
    }
    memcpy(b.header.reserved_target,required_target,32);
    b.header.transaction_count=transaction_count;
    b.header.body_length=(uint32_t)body_length;
    b.body=body;

    if(stn_block_body_commitment(
            b.body,
            body_length,
            transaction_count,
            &s->chain->hash_provider,
            b.header.transaction_commitment)!=STN_DATA_OK){
        return STN_RPC_REJECTED;
    }

    if(s->template_capacity<STN_BLOCK_HEADER_SIZE+body_length){
        return STN_RPC_CAPACITY;
    }

    if(stn_block_encode(
            &b,
            s->template_bytes,
            s->template_capacity,
            length)!=STN_DATA_OK ||
       stn_block_check_integrity(
            s->template_bytes,
            *length,
            &s->chain->hash_provider)!=STN_DATA_OK){
        return STN_RPC_REJECTED;
    }

    {
        return stn_sha256(
            NULL,
            domain,
            sizeof(domain),
            s->template_bytes,
            STN_BLOCK_HEADER_SIZE,
            id)==STN_DATA_OK ? STN_RPC_OK : STN_RPC_PROVIDER;
    }
}

static void suffix_stage_clear(stn_suffix_stage *stage)
{
    size_t i;
    if(stage==NULL)return;
    if(stage->owned!=NULL){
        for(i=0u;i<(size_t)stage->received_count;i++)free(stage->owned[i]);
    }
    free(stage->owned);free(stage->blocks);memset(stage,0,sizeof(*stage));
}

void stn_mining_session_init(stn_mining_session *session,stn_mining_service *service)
{
    if(session==NULL)return;
    memset(session,0,sizeof(*session));session->service=service;
}
void stn_mining_session_release(stn_mining_session *session)
{
    if(session==NULL)return;
    suffix_stage_clear(&session->suffix_stage);session->service=NULL;
}
stn_rpc_code stn_mining_session_handle(void *user,const stn_rpc_message *q,
    uint8_t *p,size_t cap,size_t *written)
{
    stn_mining_session *session=(stn_mining_session *)user;
    stn_mining_service *service;stn_rpc_code code;
    if(written!=NULL)*written=0u;
    if(session==NULL||session->service==NULL)return STN_RPC_UNAVAILABLE;
    service=session->service;
    if(service->suffix_stage.active)return STN_RPC_UNAVAILABLE;
    service->suffix_stage=session->suffix_stage;
    memset(&session->suffix_stage,0,sizeof(session->suffix_stage));
    code=stn_mining_handle(service,q,p,cap,written);
    session->suffix_stage=service->suffix_stage;
    memset(&service->suffix_stage,0,sizeof(service->suffix_stage));
    return code;
}

stn_rpc_code stn_mining_handle(void *user,const stn_rpc_message *q,uint8_t *p,size_t cap,size_t *written)
{
    stn_mining_service *s=user;stn_storage_view v={0};stn_rpc_code code;size_t n=0,required=0;stn_chain_state accepted={0};
    uint8_t id[32],remove[STN_PENDING_MAX_ENTRIES]={0};stn_node_service query={0};

    if(written!=NULL){*written=0;}

    if(s==NULL || q==NULL || p==NULL || written==NULL || s->chain==NULL || s->storage==NULL ||
       s->storage->acquire==NULL || s->storage->release==NULL || s->storage->read==NULL || s->storage->replace==NULL ||
       s->chain->hash_provider.hash!=stn_sha256 || s->chain->pow_policy==NULL || s->template_bytes==NULL){
        return STN_RPC_PROVIDER;
    }

    if(s->owns_buffers){
        size_t needed=0;
        stn_storage_status probe=s->storage->acquire(s->storage->user);

        if(probe!=STN_STORAGE_OK){return storage_code(probe);}

        probe=s->storage->read(s->storage->user,s->snapshot,0,&needed);
        s->storage->release(s->storage->user);

        if(probe!=STN_STORAGE_OK && probe!=STN_STORAGE_CAPACITY){
            return storage_code(probe);
        }

        if(!grow(&s->snapshot,&s->snapshot_capacity,needed,1)){
            return STN_RPC_CAPACITY;
        }
    }

    code=storage_code(stn_storage_load(
        s->chain,
        s->storage,
        s->snapshot,
        s->snapshot_capacity,
        &v));

    if(code!=STN_RPC_OK){return code;}

    if(q->method==STN_RPC_GET_ACCEPTED_RECORD ||
       q->method==STN_RPC_GET_FIRST_ACCEPTED_RECORD ||
       q->method==STN_RPC_GET_NEXT_ACCEPTED_RECORD ||
       q->method==STN_RPC_GET_CURSOR_REORG_STATUS ||
       q->method==STN_RPC_GET_CONSUMER_RECOVERY_PLAN){
        query.chain=s->chain;
        query.blocks=v.blocks;
        query.count=v.count;
        query.intelligence=s->intelligence;
        code=stn_node_service_handle(&query,q,p,cap,written);
        goto done;
    }

    if(q->method==STN_RPC_SUFFIX_STAGE_ABORT){
        suffix_stage_clear(&s->suffix_stage);code=STN_RPC_OK;goto done;
    }

    if(q->method==STN_RPC_SUFFIX_STAGE_BEGIN){
        uint32_t prefix,total;
        if(q->payload==NULL||q->length!=8u){code=STN_RPC_INVALID;goto done;}
        prefix=(uint32_t)stn_wire_read(q->payload,4u);
        total=(uint32_t)stn_wire_read(q->payload+4u,4u);
        if(prefix==0u||total==0u||total>STN_SUFFIX_STAGE_MAX_BLOCKS||prefix>v.count){code=STN_RPC_REJECTED;goto done;}
        suffix_stage_clear(&s->suffix_stage);
        s->suffix_stage.blocks=(stn_block_span *)calloc((size_t)total,sizeof(*s->suffix_stage.blocks));
        s->suffix_stage.owned=(uint8_t **)calloc((size_t)total,sizeof(*s->suffix_stage.owned));
        if(s->suffix_stage.blocks==NULL||s->suffix_stage.owned==NULL){
            suffix_stage_clear(&s->suffix_stage);code=STN_RPC_CAPACITY;goto done;
        }
        s->suffix_stage.prefix_count=prefix;s->suffix_stage.expected_count=total;
        s->suffix_stage.active=1;code=STN_RPC_OK;goto done;
    }

    if(q->method==STN_RPC_SUFFIX_STAGE_APPEND){
        size_t at=8u,i,count,length;uint32_t start;
        if(!s->suffix_stage.active||q->payload==NULL||q->length<8u){code=STN_RPC_REJECTED;goto done;}
        start=(uint32_t)stn_wire_read(q->payload,4u);
        count=(size_t)stn_wire_read(q->payload+4u,4u);
        if(start!=s->suffix_stage.received_count||count==0u||
           count>(size_t)s->suffix_stage.expected_count-start){code=STN_RPC_REJECTED;goto done;}
        for(i=0u;i<count;i++){
            uint8_t *copy;
            if(at>q->length||q->length-at<4u){code=STN_RPC_INVALID;goto append_fail;}
            length=(size_t)stn_wire_read(q->payload+at,4u);at+=4u;
            if(length<STN_BLOCK_HEADER_SIZE||length>STN_BLOCK_MAX_SIZE||length>q->length-at){
                code=STN_RPC_INVALID;goto append_fail;
            }
            if(length>STN_SUFFIX_STAGE_MAX_BYTES-s->suffix_stage.received_bytes){
                code=STN_RPC_CAPACITY;goto append_fail;
            }
            copy=(uint8_t *)malloc(length);if(copy==NULL){code=STN_RPC_CAPACITY;goto append_fail;}
            memcpy(copy,q->payload+at,length);at+=length;
            s->suffix_stage.received_bytes+=length;
            s->suffix_stage.owned[start+(uint32_t)i]=copy;
            s->suffix_stage.blocks[start+(uint32_t)i].bytes=copy;
            s->suffix_stage.blocks[start+(uint32_t)i].length=length;
        }
        if(at!=q->length){code=STN_RPC_INVALID;goto append_fail;}
        s->suffix_stage.received_count+=(uint32_t)count;code=STN_RPC_OK;goto done;
append_fail:
        while(i!=0u){
            --i;
            s->suffix_stage.received_bytes-=s->suffix_stage.blocks[start+(uint32_t)i].length;
            free(s->suffix_stage.owned[start+(uint32_t)i]);
            s->suffix_stage.owned[start+(uint32_t)i]=NULL;
            s->suffix_stage.blocks[start+(uint32_t)i].bytes=NULL;
            s->suffix_stage.blocks[start+(uint32_t)i].length=0u;
        }
        goto done;
    }

    if(q->method==STN_RPC_SUFFIX_STAGE_COMMIT){
        stn_block_span *candidate=NULL;stn_reorg_plan plan={0};stn_fork_report fork;
        size_t i,prefix,count,total=STN_STORAGE_OVERHEAD;uint8_t reorg_remove[STN_PENDING_MAX_ENTRIES]={0};
        if(!s->suffix_stage.active||
           s->suffix_stage.received_count!=s->suffix_stage.expected_count){
            code=STN_RPC_REJECTED;goto done;
        }
        prefix=(size_t)s->suffix_stage.prefix_count;count=(size_t)s->suffix_stage.expected_count;
        if(prefix>v.count||prefix>SIZE_MAX-count){code=STN_RPC_REJECTED;goto stage_commit_done;}
        candidate=(stn_block_span *)calloc(prefix+count,sizeof(*candidate));
        if(candidate==NULL){code=STN_RPC_CAPACITY;goto stage_commit_done;}
        for(i=0u;i<prefix;i++)candidate[i]=v.blocks[i];
        for(i=0u;i<count;i++)candidate[prefix+i]=s->suffix_stage.blocks[i];
        fork=stn_fork_evaluate_history(s->chain,v.blocks,v.count,candidate,prefix+count,&plan);
        if(fork.result==STN_FORK_CURRENT||fork.result==STN_FORK_TIE){
            code=STN_RPC_CURRENT;goto stage_commit_done;
        }
        if(fork.result==STN_FORK_UNRESOLVED){code=STN_RPC_UNAVAILABLE;goto stage_commit_done;}
        if(fork.result!=STN_FORK_CANDIDATE){code=STN_RPC_REJECTED;goto stage_commit_done;}
        for(i=0u;i<prefix+count;i++){
            if(total>SIZE_MAX-4u||candidate[i].length>SIZE_MAX-total-4u){
                code=STN_RPC_CAPACITY;goto stage_commit_done;
            }
            total+=4u+candidate[i].length;
        }
        if(total>SIZE_MAX-32u){code=STN_RPC_CAPACITY;goto stage_commit_done;}
        total+=32u;
        if(!ensure_storage_capacity(s,total)){code=STN_RPC_CAPACITY;goto stage_commit_done;}
        if(stn_chain_state_share(&v.state,&accepted)!=STN_DATA_OK){
            code=STN_RPC_CAPACITY;goto stage_commit_done;
        }
        if(s->pending!=NULL){
            stn_storage_view candidate_view={0};
            candidate_view.blocks=candidate;candidate_view.count=prefix+count;
            if(stn_pending_inclusions(s->pending,&candidate_view,&s->chain->hash_provider,reorg_remove)!=STN_DATA_OK){
                code=STN_RPC_PROVIDER;goto stage_commit_done;
            }
        }
        code=storage_code(stn_storage_apply(s->chain,s->storage,candidate,prefix+count,
            &plan,&s->workspace,&accepted));
        if(code!=STN_RPC_OK)goto stage_commit_done;
        stn_chain_state_move(&s->active,&accepted);
        if(s->pending!=NULL)stn_pending_prune(s->pending,reorg_remove);
        if(cap<80u){code=STN_RPC_CAPACITY;goto stage_commit_done;}
        memcpy(p,s->active.tip_id,32u);stn_wire_write(p+32u,8u,s->active.height);
        memcpy(p+40u,s->active.cumulative_work.bytes,STN_WORK_SIZE);
        *written=80u;code=STN_RPC_OK;
stage_commit_done:
        stn_chain_state_release(&accepted);stn_reorg_plan_release(&plan);free(candidate);
        suffix_stage_clear(&s->suffix_stage);goto done;
    }

    if(q->method==STN_RPC_SUBMIT_SUFFIX_EVIDENCE){
        stn_storage_view accepted_view={0};
        stn_block_span *candidate=NULL;
        stn_reorg_plan plan={0};
        stn_fork_report fork;
        size_t at=8u,i,prefix,count,length,total=STN_STORAGE_OVERHEAD;
        uint8_t reorg_remove[STN_PENDING_MAX_ENTRIES]={0};

        if(q->payload==NULL||q->length<8u){code=STN_RPC_INVALID;goto done;}
        prefix=(size_t)stn_wire_read(q->payload,4u);
        count=(size_t)stn_wire_read(q->payload+4u,4u);
        if(prefix==0u||count==0u||prefix>SIZE_MAX-count){code=STN_RPC_INVALID;goto done;}

        if(stn_storage_load(s->chain,s->storage,s->workspace.current_bytes,
                s->workspace.current_capacity,&accepted_view)!=STN_STORAGE_OK){
            code=STN_RPC_UNAVAILABLE;goto suffix_done;
        }
        if(prefix>accepted_view.count){code=STN_RPC_REJECTED;goto suffix_done;}
        candidate=(stn_block_span *)calloc(prefix+count,sizeof(*candidate));
        if(candidate==NULL){code=STN_RPC_CAPACITY;goto suffix_done;}
        for(i=0u;i<prefix;i++)candidate[i]=accepted_view.blocks[i];

        for(i=0u;i<count;i++){
            if(at>q->length||q->length-at<4u){code=STN_RPC_INVALID;goto suffix_done;}
            length=(size_t)stn_wire_read(q->payload+at,4u);at+=4u;
            if(length<STN_BLOCK_HEADER_SIZE||length>STN_BLOCK_MAX_SIZE||length>q->length-at){
                code=STN_RPC_INVALID;goto suffix_done;
            }
            candidate[prefix+i].bytes=q->payload+at;
            candidate[prefix+i].length=length;at+=length;
        }
        if(at!=q->length){code=STN_RPC_INVALID;goto suffix_done;}

        fork=stn_fork_evaluate_history(s->chain,accepted_view.blocks,accepted_view.count,
            candidate,prefix+count,&plan);
        if(fork.result==STN_FORK_CURRENT||fork.result==STN_FORK_TIE){
            /* Valid evidence that does not displace accepted state is not a
             * malformed or hostile peer condition. Report CURRENT distinctly
             * so consumers can retain the peer without granting it authority. */
            code=STN_RPC_CURRENT;goto suffix_done;
        }
        if(fork.result==STN_FORK_UNRESOLVED){code=STN_RPC_UNAVAILABLE;goto suffix_done;}
        if(fork.result!=STN_FORK_CANDIDATE){code=STN_RPC_REJECTED;goto suffix_done;}

        for(i=0u;i<prefix+count;i++){
            if(total>SIZE_MAX-4u||candidate[i].length>SIZE_MAX-total-4u){
                code=STN_RPC_CAPACITY;goto suffix_done;
            }
            total+=4u+candidate[i].length;
        }
        if(total>SIZE_MAX-32u){code=STN_RPC_CAPACITY;goto suffix_done;}
        total+=32u;
        if(!ensure_storage_capacity(s,total)){code=STN_RPC_CAPACITY;goto suffix_done;}
        if(stn_chain_state_share(&accepted_view.state,&accepted)!=STN_DATA_OK){
            code=STN_RPC_CAPACITY;goto suffix_done;
        }
        if(s->pending!=NULL){
            stn_storage_view candidate_view={0};
            candidate_view.blocks=candidate;candidate_view.count=prefix+count;
            if(stn_pending_inclusions(s->pending,&candidate_view,&s->chain->hash_provider,reorg_remove)!=STN_DATA_OK){
                code=STN_RPC_PROVIDER;goto suffix_done;
            }
        }
        code=storage_code(stn_storage_apply(s->chain,s->storage,candidate,prefix+count,
            &plan,&s->workspace,&accepted));
        if(code!=STN_RPC_OK)goto suffix_done;
        stn_chain_state_move(&s->active,&accepted);
        if(s->pending!=NULL)stn_pending_prune(s->pending,reorg_remove);
        if(cap<80u){code=STN_RPC_CAPACITY;goto suffix_done;}
        memcpy(p,s->active.tip_id,32u);stn_wire_write(p+32u,8u,s->active.height);
        memcpy(p+40u,s->active.cumulative_work.bytes,STN_WORK_SIZE);
        *written=80u;code=STN_RPC_OK;

suffix_done:
        stn_chain_state_release(&accepted);
        stn_reorg_plan_release(&plan);
        free(candidate);
        stn_storage_view_release(&accepted_view);
        goto done;
    }

    if(q->method==STN_RPC_SUBMIT_HISTORY_EVIDENCE){
        stn_block_span *candidate=NULL;
        stn_reorg_plan plan={0};
        stn_fork_report fork;
        size_t at=4u,i,count,length,total=STN_STORAGE_OVERHEAD;
        uint8_t reorg_remove[STN_PENDING_MAX_ENTRIES]={0};

        if(q->payload==NULL||q->length<4u){code=STN_RPC_INVALID;goto done;}
        count=(size_t)stn_wire_read(q->payload,4u);
        if(count==0u||count>SIZE_MAX/sizeof(*candidate)){code=STN_RPC_INVALID;goto done;}
        candidate=(stn_block_span *)calloc(count,sizeof(*candidate));
        if(candidate==NULL){code=STN_RPC_CAPACITY;goto done;}

        for(i=0;i<count;i++){
            if(at>q->length||q->length-at<4u){code=STN_RPC_INVALID;goto history_done;}
            length=(size_t)stn_wire_read(q->payload+at,4u);at+=4u;
            if(length<STN_BLOCK_HEADER_SIZE||length>STN_BLOCK_MAX_SIZE||length>q->length-at){
                code=STN_RPC_INVALID;goto history_done;
            }
            candidate[i].bytes=q->payload+at;candidate[i].length=length;at+=length;
            if(total>SIZE_MAX-4u||length>SIZE_MAX-total-4u){code=STN_RPC_CAPACITY;goto history_done;}
            total+=4u+length;
        }
        if(at!=q->length){code=STN_RPC_INVALID;goto history_done;}

        fork=stn_fork_evaluate_history(s->chain,v.blocks,v.count,candidate,count,&plan);
        if(fork.result==STN_FORK_CURRENT||fork.result==STN_FORK_TIE){
            code=STN_RPC_REJECTED;goto history_done;
        }
        if(fork.result==STN_FORK_UNRESOLVED){code=STN_RPC_UNAVAILABLE;goto history_done;}
        if(fork.result!=STN_FORK_CANDIDATE){code=STN_RPC_REJECTED;goto history_done;}

        if(total>SIZE_MAX-32u){code=STN_RPC_CAPACITY;goto history_done;}
        total+=32u;
        if(!ensure_storage_capacity(s,total)){code=STN_RPC_CAPACITY;goto history_done;}
        if(stn_chain_state_share(&v.state,&accepted)!=STN_DATA_OK){code=STN_RPC_CAPACITY;goto history_done;}

        if(s->pending!=NULL){
            stn_storage_view candidate_view={0};
            candidate_view.blocks=candidate;candidate_view.count=count;
            if(stn_pending_inclusions(s->pending,&candidate_view,&s->chain->hash_provider,reorg_remove)!=STN_DATA_OK){
                code=STN_RPC_PROVIDER;goto history_done;
            }
        }

        code=storage_code(stn_storage_apply(
            s->chain,s->storage,candidate,count,&plan,&s->workspace,&accepted));
        if(code!=STN_RPC_OK)goto history_done;

        stn_chain_state_move(&s->active,&accepted);
        if(s->pending!=NULL)stn_pending_prune(s->pending,reorg_remove);

        if(cap<80u){code=STN_RPC_CAPACITY;goto history_done;}
        memcpy(p,s->active.tip_id,32u);
        stn_wire_write(p+32u,8u,s->active.height);
        memcpy(p+40u,s->active.cumulative_work.bytes,STN_WORK_SIZE);
        *written=80u;code=STN_RPC_OK;

history_done:
        stn_reorg_plan_release(&plan);
        free(candidate);
        goto done;
    }

    if(q->method==STN_RPC_SUBMIT_BLOCK_EVIDENCE){
        stn_block_span evidence;
        stn_storage_view inclusion={0};

        if(q->payload==NULL ||
           q->length<STN_BLOCK_HEADER_SIZE ||
           q->length>STN_BLOCK_MAX_SIZE){
            code=STN_RPC_INVALID;
            goto done;
        }

        evidence.bytes=q->payload;
        evidence.length=q->length;
        inclusion.blocks=&evidence;
        inclusion.count=1u;

        if(s->pending!=NULL &&
           stn_pending_inclusions(
                s->pending,
                &inclusion,
                &s->chain->hash_provider,
                remove)!=STN_DATA_OK){
            code=STN_RPC_PROVIDER;
            goto done;
        }

        if(!storage_bytes_required(&v,q->length,&required) ||
           !ensure_storage_capacity(s,required)){
            code=STN_RPC_CAPACITY;
            goto done;
        }

        if(stn_chain_state_share(&v.state,&accepted)!=STN_DATA_OK){
            code=STN_RPC_CAPACITY;
            goto done;
        }

        code=storage_code(stn_storage_extend(
            s->chain,
            s->storage,
            q->payload,
            q->length,
            &s->workspace,
            &accepted));

        if(code!=STN_RPC_OK){
            goto done;
        }

        stn_chain_state_move(&s->active,&accepted);

        if(s->pending!=NULL){
            stn_pending_prune(s->pending,remove);
        }

        if(cap<80u){
            code=STN_RPC_CAPACITY;
            goto done;
        }

        memcpy(p,s->active.tip_id,32u);
        stn_wire_write(p+32u,8u,s->active.height);
        memcpy(p+40u,s->active.cumulative_work.bytes,STN_WORK_SIZE);
        *written=80u;
        code=STN_RPC_OK;
        goto done;
    }

    if(q->method==STN_RPC_SUBMIT_TRANSACTION){
        stn_validation_report report;
        stn_pending_result result;

        if(cap<36){code=STN_RPC_CAPACITY;goto done;}
        if(q->length>STN_TX_MAX_SIZE){code=STN_RPC_INVALID;goto done;}

        result=s->pending==NULL ? STN_PENDING_UNAVAILABLE :
            stn_pending_admit_transaction(
                s->pending,
                q->payload,
                q->length,
                s->intelligence,
                &v,
                &s->chain->hash_provider,
                &report,
                id);

        memset(p,0,36);
        stn_wire_write(p,2,1);
        stn_wire_write(p+2,2,submission_code(result));

        if(result==STN_PENDING_ACCEPTED || result==STN_PENDING_DUPLICATE){
            memcpy(p+4,id,32);
        }

        *written=36;
        code=STN_RPC_OK;
        goto done;
    }

    if(s->pending!=NULL){
        /* Work construction observes pending state; cleanup is a separate
         * operation even if the active history makes an entry ineligible. */
        if(q->method!=STN_RPC_MINING_TEMPLATE &&
           q->method!=STN_RPC_MINING_CONTEXT &&
           q->method!=STN_RPC_CHECK_WORK_BASE &&
           q->method!=STN_RPC_SUBMIT_WORK){
            if(stn_pending_inclusions(
                    s->pending,
                    &v,
                    &s->chain->hash_provider,
                    remove)!=STN_DATA_OK){
                code=STN_RPC_PROVIDER;
                goto done;
            }

            stn_pending_prune(s->pending,remove);
        }

        if(q->method==STN_RPC_SUBMIT_INTELLIGENCE){
            stn_validation_context unavailable={0};
            stn_validation_report r;
            stn_pending_result result;
            uint16_t fields[10];
            size_t i;

            if(cap<56){code=STN_RPC_CAPACITY;goto done;}

            memcpy(unavailable.expected_network,s->chain->network_id,32);

            result=stn_pending_admit(
                s->pending,
                q->payload,
                q->length,
                s->intelligence!=NULL ? s->intelligence : &unavailable,
                &v,
                &s->chain->hash_provider,
                &r,
                id);

            stn_wire_write(p,2,1);
            stn_wire_write(p+2,2,result);

            fields[0]=(uint16_t)r.structure;
            fields[1]=(uint16_t)r.payload;
            fields[2]=(uint16_t)r.network;
            fields[3]=(uint16_t)r.time;
            fields[4]=(uint16_t)r.signature;
            fields[5]=(uint16_t)r.authority;
            fields[6]=(uint16_t)r.replay;
            fields[7]=(uint16_t)r.acceptance;
            fields[8]=(uint16_t)r.envelope_error;
            fields[9]=(uint16_t)r.payload_error;

            for(i=0;i<10;++i){
                stn_wire_write(p+4+2*i,2,fields[i]);
            }

            memcpy(p+24,id,32);
            *written=56;
            code=STN_RPC_OK;
            goto done;
        }

        if(q->method==STN_RPC_PENDING){
            n=16;

            if(cap<n){code=STN_RPC_CAPACITY;goto done;}

            stn_wire_write(p,4,s->pending->count);
            stn_wire_write(p+4,4,STN_PENDING_MAX_ENTRIES);
            stn_wire_write(p+8,4,s->pending->bytes);
            stn_wire_write(p+n-4,4,STN_PENDING_MAX_BYTES);

            *written=n;
            code=STN_RPC_OK;
            goto done;
        }
    }

    if(q->method==STN_RPC_SUBMIT_SHARE){
        stn_address miner;
        stn_share_evidence evidence;
        stn_block_header work_header;
        uint8_t proof[32],required_target[32],computed_work_id[32];
        static const uint8_t work_domain[]="STN-CHAIN:WORK:ID:1";
        stn_data_status verified;

        if(q->payload==NULL || q->length!=STN_RPC_SHARE_SUBMISSION_SIZE ||
           stn_address_decode((const char *)(q->payload+32),STN_MINING_IDENTITY_SIZE,&miner)!=STN_DATA_OK ||
           miner.type!=STN_ADDRESS_IDENTITY){
            code=STN_RPC_INVALID;
            goto done;
        }

        memset(&evidence,0,sizeof(evidence));
        memcpy(evidence.work_id,q->payload,32u);
        evidence.miner=miner;
        evidence.nonce=stn_wire_read(q->payload+101,8u);
        memcpy(evidence.template_header,q->payload+STN_RPC_SHARE_SUBMISSION_PREFIX,STN_BLOCK_HEADER_SIZE);

        if(stn_block_header_decode(evidence.template_header,
                STN_BLOCK_HEADER_SIZE,&work_header)!=STN_DATA_OK ||
           work_header.version!=STN_POW_BLOCK_VERSION ||
           work_header.reserved_work_nonce!=0u ||
           memcmp(work_header.network_id,v.state.network_id,32u)!=0 ||
           !v.state.has_tip ||
           v.state.height==UINT64_MAX ||
           work_header.height!=v.state.height+1u ||
           memcmp(work_header.previous_hash,v.state.tip_id,32u)!=0 ||
           work_header.timestamp<v.state.timestamp){
            code=STN_RPC_STALE;
            goto done;
        }
        memcpy(evidence.body_commitment,work_header.transaction_commitment,32u);

        verified=stn_chain_required_target(s->chain,&v.state,required_target);
        if(verified!=STN_DATA_OK){
            code=verified==STN_DATA_UNRESOLVED ? STN_RPC_UNAVAILABLE :
                verified==STN_DATA_CAPACITY || verified==STN_DATA_OVERFLOW ?
                    STN_RPC_CAPACITY : STN_RPC_PROVIDER;
            goto done;
        }
        if(memcmp(work_header.reserved_target,required_target,32u)!=0){
            code=STN_RPC_REJECTED;
            goto done;
        }

        verified=s->chain->hash_provider.hash(
            s->chain->hash_provider.user,
            work_domain,
            sizeof(work_domain),
            evidence.template_header,
            STN_BLOCK_HEADER_SIZE,
            computed_work_id);
        if(verified!=STN_DATA_OK){
            code=verified==STN_DATA_UNRESOLVED ? STN_RPC_UNAVAILABLE : STN_RPC_PROVIDER;
            goto done;
        }
        if(memcmp(evidence.work_id,computed_work_id,32u)!=0){
            code=STN_RPC_REJECTED;
            goto done;
        }

        verified=stn_share_verify_evidence(
            &evidence,
            &s->chain->hash_provider,
            proof);
        if(verified!=STN_DATA_OK){
            code=STN_RPC_REJECTED;
            goto done;
        }

        {
            stn_transaction tx={0};
            uint8_t canonical[STN_SHARE_CANONICAL_SIZE];
            uint8_t transaction[STN_TX_HEADER_SIZE+STN_SHARE_CANONICAL_SIZE];
            uint8_t transaction_id[32];
            size_t transaction_length=0;
            stn_validation_report report;
            stn_pending_result result;

            if(s->pending==NULL){
                code=STN_RPC_UNAVAILABLE;
                goto done;
            }

            if(stn_share_encode(&evidence,canonical)!=STN_DATA_OK){
                code=STN_RPC_PROVIDER;
                goto done;
            }

            tx.version=1;
            tx.type=STN_TX_SHARE_EVIDENCE;
            tx.record_bytes=canonical;
            tx.record_length=STN_SHARE_CANONICAL_SIZE;

            if(stn_transaction_encode(
                    &tx,
                    transaction,
                    sizeof(transaction),
                    &transaction_length)!=STN_DATA_OK){
                code=STN_RPC_PROVIDER;
                goto done;
            }

            result=stn_pending_admit_transaction(
                s->pending,
                transaction,
                transaction_length,
                s->intelligence,
                &v,
                &s->chain->hash_provider,
                &report,
                transaction_id);

            if(result==STN_PENDING_DUPLICATE || result==STN_PENDING_REPLAY){
                code=STN_RPC_REJECTED;
                goto done;
            }
            if(result!=STN_PENDING_ACCEPTED){
                code=result==STN_PENDING_CAPACITY ? STN_RPC_CAPACITY :
                    result==STN_PENDING_UNAVAILABLE ? STN_RPC_UNAVAILABLE :
                    result==STN_PENDING_PROVIDER ? STN_RPC_PROVIDER :
                    STN_RPC_REJECTED;
                goto done;
            }

            if(stn_share_id(&evidence,p)!=STN_DATA_OK){
                stn_pending_remove(s->pending,transaction_id);
                code=STN_RPC_PROVIDER;
                goto done;
            }
        }

        *written=32u;
        code=STN_RPC_OK;
        goto done;
    }

    if(q->method==STN_RPC_SUBMIT_WORK){
        /*
         * A solved empty block consists of the mining envelope plus the
         * canonical block header. Transaction body bytes are optional and
         * are governed by the block header itself.
         */
        stn_address miner;
        if(q->payload==NULL ||
           q->length<STN_MINING_SUBMISSION_PREFIX+STN_BLOCK_HEADER_SIZE ||
           q->length>STN_MINING_SUBMISSION_PREFIX+STN_BLOCK_MAX_SIZE ||
           stn_wire_read(q->payload+64,4)!=q->length-STN_MINING_SUBMISSION_PREFIX ||
           stn_address_decode((const char *)(q->payload+68),STN_MINING_IDENTITY_SIZE,&miner)!=STN_DATA_OK ||
           miner.type!=STN_ADDRESS_IDENTITY){
            code=STN_RPC_INVALID;
            goto done;
        }

        if(memcmp(q->payload,v.state.tip_id,32)!=0){
            code=STN_RPC_STALE;
            goto done;
        }
    }

    if(q->method!=STN_RPC_MINING_TEMPLATE &&
       q->method!=STN_RPC_SUBMIT_WORK){
        query.chain=s->chain;
        query.blocks=v.blocks;
        query.count=v.count;
        query.intelligence=s->intelligence;

        code=stn_node_service_handle(
            &query,
            q,
            p,
            cap,
            written);

        if(code==STN_RPC_OK &&
           (q->method==STN_RPC_MINING_CONTEXT ||
            q->method==STN_RPC_CHECK_WORK_BASE)){
            stn_wire_write(
                p+72,
                4,
                template_build(s,&v,&n,id)==STN_RPC_OK ? 1 : 0);
        }

        goto done;
    }

    code=template_build(s,&v,&n,id);

    if(code!=STN_RPC_OK){
        if(s->pending!=NULL &&
           q->method==STN_RPC_SUBMIT_WORK &&
           code==STN_RPC_UNAVAILABLE){
            code=STN_RPC_STALE;
        }

        goto done;
    }

    if(q->method==STN_RPC_MINING_TEMPLATE){
        if(cap<68+n){code=STN_RPC_CAPACITY;goto done;}

        memcpy(p,v.state.tip_id,32);
        memcpy(p+32,id,32);
        stn_wire_write(p+64,4,n);
        memcpy(p+68,s->template_bytes,n);

        *written=68+n;
        code=STN_RPC_OK;
        goto done;
    }

    if(cap<80){code=STN_RPC_CAPACITY;goto done;}

    if(s->pending!=NULL &&
       memcmp(q->payload+32,id,32)!=0){
        code=STN_RPC_STALE;
        goto done;
    }

    if(q->length!=STN_MINING_SUBMISSION_PREFIX+n ||
       memcmp(q->payload+32,id,32)!=0 ||
       memcmp(
           q->payload+STN_MINING_SUBMISSION_PREFIX,
           s->template_bytes,
           STN_MINING_NONCE_OFFSET)!=0 ||
       memcmp(
           q->payload+STN_MINING_SUBMISSION_PREFIX+STN_MINING_NONCE_OFFSET+STN_MINING_NONCE_SIZE,
           s->template_bytes+STN_MINING_NONCE_OFFSET+STN_MINING_NONCE_SIZE,
           n-STN_MINING_NONCE_OFFSET-STN_MINING_NONCE_SIZE)!=0){
        code=STN_RPC_REJECTED;
        goto done;
    }

    if(s->pending!=NULL){
        stn_block_span block={q->payload+STN_MINING_SUBMISSION_PREFIX,n};
        stn_storage_view inclusion={0};

        inclusion.blocks=&block;
        inclusion.count=1;

        if(stn_pending_inclusions(
                s->pending,
                &inclusion,
                &s->chain->hash_provider,
                remove)!=STN_DATA_OK){
            code=STN_RPC_PROVIDER;
            goto done;
        }
    }

    if(!storage_bytes_required(&v,n,&required) ||
       !ensure_storage_capacity(s,required)){
        code=STN_RPC_CAPACITY;
        goto done;
    }

    if(stn_chain_state_share(&v.state,&accepted)!=STN_DATA_OK){
        code=STN_RPC_CAPACITY;
        goto done;
    }

    code=storage_code(stn_storage_extend(
        s->chain,
        s->storage,
        q->payload+STN_MINING_SUBMISSION_PREFIX,
        n,
        &s->workspace,
        &accepted));

    if(code!=STN_RPC_OK){
        goto done;
    }

    stn_chain_state_move(&s->active,&accepted);

    if(s->pending!=NULL){
        stn_pending_prune(s->pending,remove);
    }

    memcpy(p,s->active.tip_id,32);
    stn_wire_write(p+32,8,s->active.height);
    memcpy(p+40,s->active.cumulative_work.bytes,STN_WORK_SIZE);
    *written=80;

done:
    stn_chain_state_release(&accepted);
    stn_storage_view_release(&v);
    return code;
}

stn_rpc_code stn_mining_template_local(stn_mining_service *service,
    uint8_t *payload,size_t capacity,size_t *written)
{
    stn_rpc_message request={0};
    request.kind=1u;request.method=STN_RPC_MINING_TEMPLATE;
    return stn_mining_handle(service,&request,payload,capacity,written);
}

stn_rpc_code stn_mining_submit_share_local(stn_mining_service *service,
    const stn_share_evidence *evidence,uint8_t share_id[STN_SHARE_ID_SIZE])
{
    stn_rpc_message request={0};
    uint8_t payload[STN_RPC_SHARE_SUBMISSION_SIZE],response[32];
    char identity[STN_ADDRESS_TEXT_CAPACITY];size_t identity_length=0,written=0;
    stn_rpc_code code;
    if(service==NULL || evidence==NULL || share_id==NULL)return STN_RPC_INVALID;
    if(stn_address_encode(&evidence->miner,identity,sizeof(identity),&identity_length)!=STN_DATA_OK ||
       identity_length!=STN_MINING_IDENTITY_SIZE)return STN_RPC_INVALID;
    memcpy(payload,evidence->work_id,32u);
    memcpy(payload+32u,identity,STN_MINING_IDENTITY_SIZE);
    stn_wire_write(payload+101u,8u,evidence->nonce);
    memcpy(payload+STN_RPC_SHARE_SUBMISSION_PREFIX,evidence->template_header,STN_BLOCK_HEADER_SIZE);
    request.kind=1u;request.method=STN_RPC_SUBMIT_SHARE;request.payload=payload;request.length=sizeof(payload);
    code=stn_mining_handle(service,&request,response,sizeof(response),&written);
    if(code==STN_RPC_OK && written==32u)memcpy(share_id,response,32u);
    return code;
}

stn_rpc_code stn_mining_submit_work_local(stn_mining_service *service,
    const stn_address *miner,const uint8_t parent[32],const uint8_t work_id[32],
    const uint8_t *block,size_t block_length)
{
    stn_rpc_message request={0};uint8_t *payload,*response;char identity[STN_ADDRESS_TEXT_CAPACITY];
    size_t identity_length=0,written=0,total;stn_rpc_code code;
    if(service==NULL || miner==NULL || parent==NULL || work_id==NULL || block==NULL ||
       block_length<STN_BLOCK_HEADER_SIZE || block_length>STN_BLOCK_MAX_SIZE)return STN_RPC_INVALID;
    if(stn_address_encode(miner,identity,sizeof(identity),&identity_length)!=STN_DATA_OK ||
       identity_length!=STN_MINING_IDENTITY_SIZE)return STN_RPC_INVALID;
    total=STN_MINING_SUBMISSION_PREFIX+block_length;
    payload=(uint8_t*)malloc(total);response=(uint8_t*)malloc(80u);
    if(payload==NULL || response==NULL){free(payload);free(response);return STN_RPC_CAPACITY;}
    memcpy(payload,parent,32u);memcpy(payload+32u,work_id,32u);stn_wire_write(payload+64u,4u,block_length);
    memcpy(payload+68u,identity,STN_MINING_IDENTITY_SIZE);memcpy(payload+STN_MINING_SUBMISSION_PREFIX,block,block_length);
    request.kind=1u;request.method=STN_RPC_SUBMIT_WORK;request.payload=payload;request.length=total;
    code=stn_mining_handle(service,&request,response,80u,&written);
    free(payload);free(response);return code;
}
