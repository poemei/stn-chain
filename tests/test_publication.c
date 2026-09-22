/* Included by test_lifecycle.c; deterministic test-only signing keys. */
#include "stn_storage.h"
#include "stn_pending.h"
#include "stn_mining.h"
#include "stn_peer.h"
typedef struct publication_store {uint8_t bytes[8192];size_t length;} publication_store;
static stn_storage_status publication_lock(void *u){(void)u;return STN_STORAGE_OK;}
static void publication_unlock(void *u){(void)u;}
static stn_storage_status publication_read(void *u,uint8_t *out,size_t cap,size_t *n){publication_store *s=u;*n=s->length;if(cap<s->length)return STN_STORAGE_CAPACITY;memcpy(out,s->bytes,s->length);return STN_STORAGE_OK;}
static stn_storage_status publication_replace(void *u,const uint8_t *in,size_t n){publication_store *s=u;if(n>sizeof(s->bytes))return STN_STORAGE_CAPACITY;memcpy(s->bytes,in,n);s->length=n;return STN_STORAGE_OK;}
void ed25519_publickey_stn(const unsigned char *,unsigned char *);
void ed25519_sign_stn(const unsigned char *,size_t,const unsigned char *,const unsigned char *,unsigned char *);
static size_t publication_block(uint8_t *out,const uint8_t *payload,size_t length,
    uint16_t type,uint64_t height,const uint8_t *parent,const stn_hash_provider *hash)
{
    uint8_t transaction[512],body[520];size_t n,w;
    stn_transaction tx={1,type,payload,(uint32_t)length};stn_transaction_span span;
    stn_block block={0};
    CHECK(stn_transaction_encode(&tx,transaction,sizeof(transaction),&n)==STN_DATA_OK);
    span.bytes=transaction;span.length=(uint32_t)n;
    CHECK(stn_block_body_encode(&span,1,body,sizeof(body),&n)==STN_DATA_OK);
    block.header.version=3;block.header.height=height;block.header.timestamp=height;
    memset(block.header.reserved_target,255,32);block.header.reserved_target[0]=127;
    if(parent!=NULL)memcpy(block.header.previous_hash,parent,32);
    block.header.transaction_count=1;block.header.body_length=(uint32_t)n;block.body=body;
    CHECK(stn_block_body_commitment(body,n,1,hash,block.header.transaction_commitment)==STN_DATA_OK);
    CHECK(stn_block_encode(&block,out,1024,&w)==STN_DATA_OK);
    {uint8_t digest[32];unsigned nonce;
        for(nonce=0;nonce<65536;++nonce){out[158]=(uint8_t)(nonce>>8);out[159]=(uint8_t)nonce;if(stn_pow_verify(out,w,hash,digest)==STN_DATA_OK)break;}
        CHECK(nonce<65536);
    }
    return w;
}
static void production_publication(void)
{
    uint8_t root_seed[32]={9},producer_seed[32]={10},issuer[32],producer[32];
    uint8_t record[232],payload[52]={0},action[32],context[32],evidence[97],grant[194],signature[64],statement[512];
    uint8_t genesis[1024],child[1024],alternative[1024],snapshot[4096],rid[32],other_id[32],bad[232],transaction[512],assembled[1024];
    size_t n,w,gn,bn,an,stored;uint32_t selected;
    stn_record r={0};stn_hash_provider hash={stn_sha256,NULL};
    stn_chain_context c={0};stn_pow_policy pow={{0}};stn_chain_state empty,parent,accepted,unchanged;
    stn_chain_report report;stn_block_span history[2];stn_storage_view view={0},restored={0};
    stn_pending pool={0};stn_validation_report pending_report;stn_transaction tx;
    unsigned start=checks;
    ed25519_publickey_stn(root_seed,issuer);ed25519_publickey_stn(producer_seed,producer);
    payload[1]=1;payload[9]=5;payload[10]=1;payload[12]=1;payload[13]='a';
    payload[15]=1;payload[16]='b';payload[18]=1;payload[19]='c';payload[20]=1;
    r.version=1;r.type=1;r.nonce[0]=1;r.payload=payload;r.payload_length=52;r.issued_at=UINT64_MAX;
    memcpy(r.signer_public_key,producer,32);
    CHECK(stn_record_encode(&r,record,sizeof(record),&n)==STN_RECORD_OK);
    CHECK(stn_identity_statement(record,168,statement,sizeof(statement),&w)==STN_IDENTITY_VALID);
    ed25519_sign_stn(statement,w,producer_seed,producer,r.signature);
    CHECK(stn_record_encode(&r,record,sizeof(record),&n)==STN_RECORD_OK);
    CHECK(stn_record_publication_tokens(record,n,&hash,action,context)==STN_DATA_OK);
    CHECK(stn_authority_evidence_encode(producer,action,context,evidence,sizeof(evidence),&w)==STN_AUTHORITY_AUTHORIZED);
    CHECK(stn_authority_grant_statement(issuer,evidence,statement,sizeof(statement),&w)==STN_AUTHORITY_VALID_GRANT);
    ed25519_sign_stn(statement,w,root_seed,issuer,signature);
    CHECK(stn_authority_grant_encode(issuer,evidence,signature,grant,sizeof(grant),&w)==STN_AUTHORITY_VALID_GRANT);
    gn=publication_block(genesis,grant,sizeof(grant),STN_TX_AUTHORITY_GRANT,0,NULL,&hash);
    c.genesis_bytes=genesis;c.genesis_length=gn;c.hash_provider=hash;
    memset(pow.fixed_target,255,32);pow.fixed_target[0]=127;c.pow_policy=&pow;
    c.genesis_authority_roots=issuer;c.genesis_authority_root_count=1;c.genesis_initial_identities=producer;c.genesis_initial_identity_count=1;
    CHECK(stn_chain_initialize(&c,&empty)==STN_DATA_OK && empty.publication_activation_height==1);
    CHECK(stn_chain_validate_candidate(&c,&empty,genesis,gn,&parent).acceptance==STN_ACCEPTANCE_UNDER_CONTEXT);
    CHECK(stn_lifecycle_check_publication(parent.lifecycle,record,n,&hash)==STN_LIFECYCLE_OK);
    CHECK(stn_lifecycle_check_publication(empty.lifecycle,record,n,&hash)==STN_LIFECYCLE_INVALID);
    {
        uint8_t grants[388],replays[256],wrong_grant[194],changed_action[32],changed_context[32],revocation[129],rotation[129],replacement[32],replacement_seed[32]={11};
        stn_lifecycle_state isolated;stn_transaction event={1,STN_TX_AUTHORITY_GRANT,wrong_grant,194};unsigned variant;
        for(variant=0;variant<2;++variant){
            memcpy(changed_action,action,32);memcpy(changed_context,context,32);
            if(variant==0)changed_action[31]^=1;else changed_context[31]^=1;
            CHECK(stn_authority_evidence_encode(producer,changed_action,changed_context,evidence,sizeof(evidence),&w)==STN_AUTHORITY_AUTHORIZED);
            CHECK(stn_authority_grant_statement(issuer,evidence,statement,sizeof(statement),&w)==STN_AUTHORITY_VALID_GRANT);
            ed25519_sign_stn(statement,w,root_seed,issuer,signature);
            CHECK(stn_authority_grant_encode(issuer,evidence,signature,wrong_grant,sizeof(wrong_grant),&w)==STN_AUTHORITY_VALID_GRANT);
            stn_lifecycle_initialize(&isolated,grants,2,replays,4,producer);
            CHECK(stn_lifecycle_apply_transaction(&isolated,&event,issuer,1,&hash)==STN_LIFECYCLE_OK);
            CHECK(stn_lifecycle_check_publication(&isolated,record,n,&hash)==STN_LIFECYCLE_INVALID);
        }
        stn_lifecycle_initialize(&isolated,grants,2,replays,4,producer);stn_lifecycle_set_initial_identities(&isolated,producer,1);
        event.record_bytes=grant;
        CHECK(stn_lifecycle_apply_transaction(&isolated,&event,issuer,1,&hash)==STN_LIFECYCLE_OK);
        CHECK(stn_authority_grant_id(grant,sizeof(grant),&hash,other_id)==STN_DATA_OK);
        CHECK(stn_authority_revocation_statement(issuer,other_id,statement,sizeof(statement),&w)==STN_AUTHORITY_VALID_REVOCATION);
        ed25519_sign_stn(statement,w,root_seed,issuer,signature);
        CHECK(stn_authority_revocation_encode(issuer,other_id,signature,revocation,sizeof(revocation),&w)==STN_AUTHORITY_VALID_REVOCATION);
        event.type=STN_TX_AUTHORITY_REVOKE;event.record_bytes=revocation;event.record_length=129;
        CHECK(stn_lifecycle_apply_transaction(&isolated,&event,issuer,1,&hash)==STN_LIFECYCLE_OK);
        CHECK(stn_lifecycle_check_publication(&isolated,record,n,&hash)==STN_LIFECYCLE_INVALID);
        stn_lifecycle_initialize(&isolated,grants,2,replays,4,producer);stn_lifecycle_set_initial_identities(&isolated,producer,1);
        event.type=STN_TX_AUTHORITY_GRANT;event.record_bytes=grant;event.record_length=194;
        CHECK(stn_lifecycle_apply_transaction(&isolated,&event,issuer,1,&hash)==STN_LIFECYCLE_OK);
        ed25519_publickey_stn(replacement_seed,replacement);
        CHECK(stn_identity_rotation_statement(producer,replacement,statement,sizeof(statement),&w)==STN_AUTHORITY_VALID_ROTATION);
        ed25519_sign_stn(statement,w,producer_seed,producer,signature);
        CHECK(stn_identity_rotation_encode(producer,replacement,signature,rotation,sizeof(rotation),&w)==STN_AUTHORITY_VALID_ROTATION);
        event.type=STN_TX_IDENTITY_ROTATE;event.record_bytes=rotation;event.record_length=129;
        CHECK(stn_lifecycle_apply_transaction(&isolated,&event,issuer,1,&hash)==STN_LIFECYCLE_OK);
        CHECK(stn_lifecycle_check_publication(&isolated,record,n,&hash)==STN_LIFECYCLE_INVALID);
    }
    CHECK(stn_record_id(record,n,&hash,rid)==STN_DATA_OK);
    memcpy(bad,record,n);bad[231]^=1;
    CHECK(stn_record_id(bad,n,&hash,other_id)==STN_DATA_OK && memcmp(rid,other_id,32)==0);
    CHECK(stn_lifecycle_check_publication(parent.lifecycle,bad,n,&hash)==STN_LIFECYCLE_INVALID);
    an=publication_block(alternative,bad,n,STN_TX_PUBLICATION,1,parent.tip_id,&hash);
    unchanged=parent;report=stn_chain_validate_candidate(&c,&parent,alternative,an,&unchanged);
    CHECK(report.acceptance==STN_ACCEPTANCE_REJECTED && report.body==STN_STAGE_REJECT && unchanged.lifecycle==parent.lifecycle && parent.lifecycle->replay.consumed_count==1);
    bad[231]^=1;bad[72]^=2;
    CHECK(stn_record_id(bad,n,&hash,other_id)==STN_DATA_OK && memcmp(rid,other_id,32)!=0);
    view.state=parent; /* BORROW: this manual view owns neither spans nor state. */history[0].bytes=genesis;history[0].length=gn;view.blocks=history;view.count=1;
    CHECK(stn_pending_admit(&pool,record,n,NULL,&view,&hash,&pending_report,other_id)==STN_PENDING_ACCEPTED);
    CHECK(parent.lifecycle->replay.consumed_count==1 && pool.count==1);
    CHECK(stn_pending_assemble(&pool,NULL,&view,assembled,sizeof(assembled),&w,&selected)==STN_DATA_OK && selected==1);
    bn=publication_block(child,record,n,STN_TX_PUBLICATION,1,parent.tip_id,&hash);
    CHECK(stn_chain_validate_candidate(&c,&parent,child,bn,&accepted).acceptance==STN_ACCEPTANCE_UNDER_CONTEXT);
    CHECK(accepted.timestamp==1 && accepted.lifecycle->replay.consumed_count==2 && parent.lifecycle->replay.consumed_count==1);
    CHECK(stn_lifecycle_check_publication(accepted.lifecycle,record,n,&hash)==STN_LIFECYCLE_REPLAY);
    an=publication_block(alternative,record,n,STN_TX_PUBLICATION,2,accepted.tip_id,&hash);
    unchanged=accepted;report=stn_chain_validate_candidate(&c,&accepted,alternative,an,&unchanged);
    CHECK(report.acceptance==STN_ACCEPTANCE_REJECTED && unchanged.lifecycle==accepted.lifecycle);
    history[1].bytes=child;history[1].length=bn;
    CHECK(stn_storage_encode(&c,history,2,snapshot,sizeof(snapshot),&stored)==STN_STORAGE_OK);
    CHECK(stn_storage_decode(&c,snapshot,stored,&restored)==STN_STORAGE_OK);
    CHECK(restored.state.lifecycle->replay.consumed_count==accepted.lifecycle->replay.consumed_count);
    CHECK(stn_lifecycle_check_publication(restored.state.lifecycle,record,n,&hash)==STN_LIFECYCLE_REPLAY);
    stn_storage_view_release(&restored);
    CHECK(stn_storage_encode(&c,history,1,snapshot,sizeof(snapshot),&stored)==STN_STORAGE_OK);
    CHECK(stn_storage_decode(&c,snapshot,stored,&restored)==STN_STORAGE_OK);
    CHECK(stn_lifecycle_check_publication(restored.state.lifecycle,record,n,&hash)==STN_LIFECYCLE_OK);
    stn_storage_view_release(&restored);stn_pending_clear(&pool);
    tx.version=1;tx.type=1;tx.record_bytes=record;tx.record_length=(uint32_t)n;
    CHECK(stn_transaction_encode(&tx,transaction,sizeof(transaction),&w)==STN_DATA_OK);
    CHECK(stn_pending_admit_transaction(&pool,transaction,w,NULL,&view,&hash,&pending_report,other_id)==STN_PENDING_ACCEPTED);
    stn_pending_clear(&pool);
    {
        /* A fully signed, greater-work replacement branch has no publication. */
        uint8_t branch1[1024],branch2[1024],second_grant[194],work_a[8192],work_b[8192];
        stn_block_span competing[3];stn_chain_state branch_state={0};stn_reorg_plan plan={0};
        publication_store disk={0};stn_storage_provider storage={&disk,publication_lock,publication_unlock,publication_read,publication_replace};
        stn_storage_workspace workspace={work_a,sizeof(work_a),work_b,sizeof(work_b)};
        action[31]^=1;
        CHECK(stn_authority_evidence_encode(producer,action,context,evidence,sizeof(evidence),&w)==STN_AUTHORITY_AUTHORIZED);
        CHECK(stn_authority_grant_statement(issuer,evidence,statement,sizeof(statement),&w)==STN_AUTHORITY_VALID_GRANT);
        ed25519_sign_stn(statement,w,root_seed,issuer,signature);
        CHECK(stn_authority_grant_encode(issuer,evidence,signature,second_grant,sizeof(second_grant),&w)==STN_AUTHORITY_VALID_GRANT);
        an=publication_block(branch1,second_grant,sizeof(second_grant),STN_TX_AUTHORITY_GRANT,1,parent.tip_id,&hash);
        CHECK(stn_chain_validate_candidate(&c,&parent,branch1,an,&branch_state).acceptance==STN_ACCEPTANCE_UNDER_CONTEXT);
        competing[0]=history[0];competing[1].bytes=branch1;competing[1].length=an;
        context[31]^=1;
        CHECK(stn_authority_evidence_encode(producer,action,context,evidence,sizeof(evidence),&w)==STN_AUTHORITY_AUTHORIZED);
        CHECK(stn_authority_grant_statement(issuer,evidence,statement,sizeof(statement),&w)==STN_AUTHORITY_VALID_GRANT);
        ed25519_sign_stn(statement,w,root_seed,issuer,signature);
        CHECK(stn_authority_grant_encode(issuer,evidence,signature,second_grant,sizeof(second_grant),&w)==STN_AUTHORITY_VALID_GRANT);
        an=publication_block(branch2,second_grant,sizeof(second_grant),STN_TX_AUTHORITY_GRANT,2,branch_state.tip_id,&hash);
        competing[2].bytes=branch2;competing[2].length=an;
        CHECK(stn_fork_evaluate(&c,history,2,competing,3,&plan).result==STN_FORK_CANDIDATE && plan.detached_count==1 && plan.attached_count==2);
        CHECK(stn_storage_encode(&c,history,2,disk.bytes,sizeof(disk.bytes),&disk.length)==STN_STORAGE_OK);
        CHECK(stn_storage_adopt(&c,&storage,competing,3,&workspace,&accepted)==STN_STORAGE_OK);
        CHECK(accepted.height==2 && stn_lifecycle_check_publication(accepted.lifecycle,record,n,&hash)==STN_LIFECYCLE_OK);
        CHECK(stn_storage_decode(&c,disk.bytes,disk.length,&restored)==STN_STORAGE_OK);
        CHECK(stn_lifecycle_check_publication(restored.state.lifecycle,record,n,&hash)==STN_LIFECYCLE_OK);
        stn_storage_view_release(&restored);
        /* STNC encodes and dispatches the same transaction against persisted genesis. */
        {
            uint8_t request[1024],response[4096],template_bytes[2048],current_bytes[8192],next_bytes[8192];size_t request_n,response_n;
            stn_rpc_message q={1,STN_RPC_SUBMIT_TRANSACTION,STN_RPC_OK,17,transaction,0},answer;
            stn_mining_service mining={0};stn_rpc_service service={&mining,stn_mining_handle};
            q.length=12+n;
            CHECK(stn_storage_encode(&c,history,1,disk.bytes,sizeof(disk.bytes),&disk.length)==STN_STORAGE_OK);
            mining.chain=&c;mining.storage=&storage;mining.snapshot=work_a;mining.snapshot_capacity=sizeof(work_a);
            mining.template_bytes=template_bytes;mining.template_capacity=sizeof(template_bytes);mining.pending=&pool;
            CHECK(stn_rpc_encode(&q,request,sizeof(request),&request_n)==STN_RPC_OK);
            CHECK(stn_rpc_dispatch(request,request_n,STN_RPC_SUBMISSION,&service,response,sizeof(response),&response_n)==STN_RPC_OK);
            CHECK(stn_rpc_decode(response,response_n,&answer)==STN_RPC_OK && answer.request_id==17 && answer.code==STN_RPC_OK && answer.length==36 && answer.payload[3]==STN_RPC_ADMITTED);
            CHECK(pool.count==1 && memcmp(pool.entries[0].transaction,transaction,q.length)==0);
            transaction[12+n-1]^=1;
            CHECK(stn_rpc_encode(&q,request,sizeof(request),&request_n)==STN_RPC_OK);
            CHECK(stn_rpc_dispatch(request,request_n,STN_RPC_SUBMISSION,&service,response,sizeof(response),&response_n)==STN_RPC_OK);
            CHECK(stn_rpc_decode(response,response_n,&answer)==STN_RPC_OK && answer.code==STN_RPC_OK && answer.payload[3]==STN_RPC_UNAUTHORIZED && pool.count==1);
            transaction[12+n-1]^=1;
            q.method=STN_RPC_CHECK_INTELLIGENCE;q.payload=record;q.length=n;
            CHECK(stn_rpc_encode(&q,request,sizeof(request),&request_n)==STN_RPC_OK);
            CHECK(stn_rpc_dispatch(request,request_n,STN_RPC_READ,&service,response,sizeof(response),&response_n)==STN_RPC_OK);
            CHECK(stn_rpc_decode(response,response_n,&answer)==STN_RPC_OK && answer.code==STN_RPC_OK && answer.length==20 && answer.payload[15]==STN_ACCEPTANCE_UNDER_CONTEXT);
            mining.pending_body=assembled;mining.pending_body_capacity=sizeof(assembled);
            mining.workspace.current_bytes=current_bytes;mining.workspace.current_capacity=sizeof(current_bytes);
            mining.workspace.next_bytes=next_bytes;mining.workspace.next_capacity=sizeof(next_bytes);
            q.method=STN_RPC_MINING_TEMPLATE;q.payload=NULL;q.length=0;
            CHECK(stn_rpc_encode(&q,request,sizeof(request),&request_n)==STN_RPC_OK);
            CHECK(stn_rpc_dispatch(request,request_n,STN_RPC_READ,&service,response,sizeof(response),&response_n)==STN_RPC_OK);
            CHECK(stn_rpc_decode(response,response_n,&answer)==STN_RPC_OK && answer.code==STN_RPC_OK && answer.length>68);
            if(answer.code==STN_RPC_OK && answer.length>68){
                uint8_t digest[32];unsigned nonce;
                for(nonce=0;nonce<65536;++nonce){response[24+68+158]=(uint8_t)(nonce>>8);response[24+68+159]=(uint8_t)nonce;if(stn_pow_verify(answer.payload+68,answer.length-68,&hash,digest)==STN_DATA_OK)break;}
                CHECK(nonce<65536);
                q.method=STN_RPC_SUBMIT_WORK;q.payload=answer.payload;q.length=answer.length;
                CHECK(stn_rpc_encode(&q,request,sizeof(request),&request_n)==STN_RPC_OK);
                CHECK(stn_rpc_dispatch(request,request_n,STN_RPC_SUBMISSION,&service,response,sizeof(response),&response_n)==STN_RPC_OK);
                CHECK(stn_rpc_decode(response,response_n,&answer)==STN_RPC_OK && answer.code==STN_RPC_OK && answer.length==80);
                CHECK(pool.count==0 && mining.active.height==1 && stn_lifecycle_check_publication(mining.active.lifecycle,record,n,&hash)==STN_LIFECYCLE_REPLAY);
            }
            stn_chain_state_release(&mining.active);stn_pending_clear(&pool);
        }
        /* Peer block framing preserves exactly the candidate consumed locally. */
        {
            uint8_t frame[2048],peer_payload[1028];size_t framed;stn_peer_message message;
            peer_payload[0]=(uint8_t)(bn>>24);peer_payload[1]=(uint8_t)(bn>>16);peer_payload[2]=(uint8_t)(bn>>8);peer_payload[3]=(uint8_t)bn;
            memcpy(peer_payload+4,child,bn);
            CHECK(stn_peer_encode(STN_PEER_BLOCK,peer_payload,bn+4,frame,sizeof(frame),&framed)==STN_PEER_OK);
            CHECK(stn_peer_decode(frame,framed,&message)==STN_PEER_OK && message.length==bn+4);
            stn_chain_state_release(&branch_state);
            CHECK(stn_chain_validate_candidate(&c,&parent,message.payload+4,bn,&branch_state).acceptance==STN_ACCEPTANCE_UNDER_CONTEXT);
            CHECK(stn_lifecycle_check_publication(branch_state.lifecycle,record,n,&hash)==STN_LIFECYCLE_REPLAY);
            stn_chain_state_release(&branch_state);stn_reorg_plan_release(&plan);
        }
    }
    {
        uint8_t next_block[1024],next_record[232];stn_chain_state growing={0};unsigned i;
        CHECK(stn_chain_state_share(&parent,&growing)==STN_DATA_OK);
        for(i=1;i<=20;++i){
            r.nonce[0]=(uint8_t)i;
            CHECK(stn_record_encode(&r,next_record,sizeof(next_record),&w)==STN_RECORD_OK);
            CHECK(stn_identity_statement(next_record,168,statement,sizeof(statement),&w)==STN_IDENTITY_VALID);
            ed25519_sign_stn(statement,w,producer_seed,producer,r.signature);
            CHECK(stn_record_encode(&r,next_record,sizeof(next_record),&w)==STN_RECORD_OK);
            an=publication_block(next_block,next_record,sizeof(next_record),STN_TX_PUBLICATION,i,growing.tip_id,&hash);
            CHECK(stn_chain_validate_candidate(&c,&growing,next_block,an,&growing).acceptance==STN_ACCEPTANCE_UNDER_CONTEXT);
            CHECK(growing.lifecycle->replay.consumed_count==(size_t)i+1);
        }
        CHECK(parent.lifecycle->replay.consumed_count==1);
        CHECK(stn_lifecycle_check_publication(parent.lifecycle,record,n-1,&hash)==STN_LIFECYCLE_MALFORMED);
        memcpy(bad,record,n);bad[7]=2;
        CHECK(stn_lifecycle_check_publication(parent.lifecycle,bad,n,&hash)==STN_LIFECYCLE_MALFORMED);
        CHECK(stn_lifecycle_check_publication(parent.lifecycle,record,n,NULL)==STN_LIFECYCLE_PROVIDER);
        view.state=growing; /* BORROW until growing is released. */
        CHECK(stn_pending_insert(&pool,transaction,12+n,&hash,other_id)==STN_PENDING_ACCEPTED);
        CHECK(stn_pending_assemble(&pool,NULL,&view,assembled,sizeof(assembled),&w,&selected)==STN_DATA_UNRESOLVED && selected==0);
        stn_pending_clear(&pool);
        r.nonce[0]=21;r.network_id[0]=1;
        CHECK(stn_record_encode(&r,next_record,sizeof(next_record),&w)==STN_RECORD_OK);
        CHECK(stn_identity_statement(next_record,168,statement,sizeof(statement),&w)==STN_IDENTITY_VALID);
        ed25519_sign_stn(statement,w,producer_seed,producer,r.signature);
        CHECK(stn_record_encode(&r,next_record,sizeof(next_record),&w)==STN_RECORD_OK);
        CHECK(stn_pending_admit(&pool,next_record,w,NULL,&view,&hash,&pending_report,other_id)==STN_PENDING_NETWORK && pending_report.acceptance==STN_ACCEPTANCE_REJECTED && pool.count==0);
        stn_chain_state_release(&growing);
    }
    stn_chain_state_release(&empty);stn_chain_state_release(&parent);stn_chain_state_release(&accepted);
    printf("Phase 15 production publication: %u targeted checks.\n",checks-start);
}
