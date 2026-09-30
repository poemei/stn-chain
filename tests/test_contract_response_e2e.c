/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
/* Reuse the existing real-PoW in-memory storage fixture and signing vectors. */
#define main mining_regression_main
#include "test_mining_mempool.c"
#undef main
#include "stn_contract_response_acceptance.h"
#include "stn_contract_response_query.h"
static const uint8_t bsk[32]={0x4c,0xcd,0x08,0x9b,0x28,0xff,0x96,0xda,0x9d,0xb6,0xc3,0x46,0xec,0x11,0x4e,0x0f,0x5b,0x8a,0x31,0x9f,0x35,0xab,0xa6,0x24,0xda,0x8c,0xf6,0xed,0x4f,0xb8,0xa6,0xfb};
static const uint8_t bpk[32]={0x3d,0x40,0x17,0xc3,0xe8,0x43,0x89,0x5a,0x92,0xb7,0x0a,0xa7,0x4d,0x1b,0x7e,0xbc,0x9c,0x98,0x2c,0xcf,0x2e,0xc4,0x96,0x8c,0xc0,0xcd,0x55,0xf1,0x2a,0xf4,0x66,0x0c};
static const uint8_t csk[32]={0xc5,0xaa,0x8d,0xf4,0x3f,0x9f,0x83,0x7b,0xed,0xb7,0x44,0x2f,0x31,0xdc,0xb7,0xb1,0x66,0xd3,0x85,0x35,0x07,0x6f,0x09,0x4b,0x85,0xce,0x3a,0x2e,0x0b,0x44,0x58,0xf7};
static const uint8_t cpk[32]={0xfc,0x51,0xcd,0x8e,0x62,0x18,0xa1,0xa3,0x8d,0xa4,0x7e,0xd0,0x02,0x30,0xf0,0x58,0x08,0x16,0xed,0x13,0xba,0x33,0x03,0xac,0x5d,0xeb,0x91,0x15,0x48,0x90,0x80,0x25};
static int response_wire(const uint8_t cid[32],const uint8_t pk[32],const uint8_t sk[32],uint8_t *wire,size_t *n)
{
    stn_contract_response r={0};stn_transaction tx={0};
    uint8_t unsigned_bytes[512],statement[600],payload[700];size_t un=0,sn=0,pn=0;
    static const uint8_t text[]="B responds: the original terms have been received.";
    r.version=1;memcpy(r.contract_id,cid,32);memcpy(r.actor,pk,32);r.text=text;r.text_length=sizeof(text)-1;
    CHECK(stn_contract_response_statement(&r,unsigned_bytes,sizeof(unsigned_bytes),&un)==STN_CONTRACT_OK);
    CHECK(stn_identity_statement(unsigned_bytes,un,statement,sizeof(statement),&sn)==STN_IDENTITY_VALID);
    CHECK(stn_ed25519_sign(statement,sn,pk,sk,r.signature)==0);
    CHECK(stn_contract_response_signature_verify(&r)==STN_CONTRACT_OK);
    CHECK(stn_contract_response_encode(&r,payload,sizeof(payload),&pn)==STN_CONTRACT_OK);
    tx.version=1;tx.type=STN_TX_CONTRACT_RESPONSE;tx.record_bytes=payload;tx.record_length=(uint32_t)pn;
    CHECK(stn_transaction_encode(&tx,wire,2048,n)==STN_DATA_OK);return 0;
}
static int rejected_response(stn_mining_service *s,const stn_address *miner,const uint8_t *wire,size_t n)
{
    stn_storage_view v={0};stn_validation_report report;uint8_t id[32],work[8192];size_t wn=0;uint64_t height=s->active.height;
    CHECK(stn_storage_load(s->chain,s->storage,snapshot,sizeof(snapshot),&v)==STN_STORAGE_OK);
    CHECK(stn_pending_admit_transaction(s->pending,wire,n,NULL,&v,&hash,&report,id)==STN_PENDING_INVALID);
    stn_storage_view_release(&v);
    CHECK(s->pending->count==0);
    /* A peer/miner cannot bypass admission by submitting a solved bad block. */
    CHECK(stn_mining_template_local(s,work,sizeof(work),&wn)==STN_RPC_OK);
    stn_wire_write(work+68+160,4,1);stn_wire_write(work+68+164,4,n+4);
    stn_wire_write(work+68+168,4,n);memcpy(work+68+172,wire,n);wn=68+172+n;
    CHECK(stn_block_body_commitment(work+68+168,n+4,1,&hash,work+68+88)==STN_DATA_OK);
    CHECK(work_id(work+68,work+32));CHECK(solve(work+68,wn-68,1));
    CHECK(stn_mining_submit_work_local(s,miner,work,work+32,work+68,wn-68)==STN_RPC_REJECTED);
    CHECK(s->active.height==height);return 0;
}
int main(void)
{
    stn_chain_context context={0};stn_pow_policy policy={0};stn_block block={0};
    stn_mining_service service={0};stn_pending pending={0};stn_storage_view view={0};
    stn_storage_provider storage={NULL,acquire,release,read_store,write_store};
    stn_block_span history[1];stn_address miner={0};
    uint8_t genesis[249],body[81],mapping[77];
    uint8_t template_bytes[16384],pending_body[16384];
    size_t gn=0;


    /* An easy real-PoW genesis supplies an explicit compensation mapping. */
    CHECK(destination(1,2,mapping)==77);
    stn_wire_write(body,4,77);memcpy(body+4,mapping,77);
    block.header.version=3;block.header.network_id[0]=1;block.header.timestamp=1;
    memset(policy.fixed_target,255,32);policy.fixed_target[0]=127;
    memcpy(block.header.reserved_target,policy.fixed_target,32);
    block.header.transaction_count=1;block.header.body_length=81;block.body=body;
    CHECK(stn_block_body_commitment(body,81,1,&hash,block.header.transaction_commitment)==STN_DATA_OK);
    CHECK(stn_block_encode(&block,genesis,sizeof(genesis),&gn)==STN_DATA_OK);
    CHECK(solve(genesis,gn,1));
    context.network_id[0]=1;context.genesis_bytes=genesis;context.genesis_length=gn;
    context.hash_provider=hash;context.pow_policy=&policy;
    history[0].bytes=genesis;history[0].length=gn;
    CHECK(stn_storage_create(&context,&storage,history,1,next,sizeof(next),&service.active)==STN_STORAGE_OK);
    service.chain=&context;service.storage=&storage;service.pending=&pending;
    service.snapshot=snapshot;service.snapshot_capacity=sizeof(snapshot);
    service.workspace.current_bytes=current;service.workspace.current_capacity=sizeof(current);
    service.workspace.next_bytes=next;service.workspace.next_capacity=sizeof(next);
    service.template_bytes=template_bytes;service.template_capacity=sizeof(template_bytes);
    service.pending_body=pending_body;service.pending_body_capacity=sizeof(pending_body);
    miner.type=STN_ADDRESS_IDENTITY;miner.identifier[0]=1;

    {
        uint8_t create[2048],response[2048],bad[2048],id[32],cid[32],other[32],work[8192],query[74],reply[2048];
        uint8_t signbytes[2048],statement[2100];size_t cn=0,rn=0,bn=0,wn=0,sn=0,qn=0;
        stn_contract_transaction a;stn_address address,b_identity;stn_validation_report report;
        stn_contract_response decoded;stn_contract_snapshot_response saved_response;
        stn_rpc_message request={1,STN_RPC_CONTRACT_RESPONSE,STN_RPC_OK,77,query,74};
        CHECK(create_contract(create,&cn,0,0)==0);
        CHECK(stn_contract_transaction_decode(create+12,cn-12,&a)==STN_CONTRACT_OK);
        CHECK(stn_address_derive(STN_ADDRESS_IDENTITY,bpk,32,&b_identity)==STN_DATA_OK);
        memcpy(create+12+116+32+2*34,b_identity.identifier,32);
        memcpy(signbytes,a.canonical_contract,a.canonical_contract_length);
        memcpy(signbytes+a.canonical_contract_length,contract_pk,32);
        stn_wire_write(signbytes+a.canonical_contract_length+32,2,1);stn_wire_write(signbytes+a.canonical_contract_length+34,8,0);
        CHECK(stn_identity_statement(signbytes,a.canonical_contract_length+42,statement,sizeof(statement),&sn)==STN_IDENTITY_VALID);
        CHECK(stn_ed25519_sign(statement,sn,contract_pk,contract_sk,create+12+52)==0);
        CHECK(stn_contract_address(a.canonical_contract,a.canonical_contract_length,&address)==STN_CONTRACT_OK);memcpy(cid,address.identifier,32);
        CHECK(response_wire(cid,bpk,bsk,response,&rn)==0);
        CHECK(rejected_response(&service,&miner,response,rn)==0); /* not yet accepted */
        CHECK(stn_storage_load(&context,&storage,snapshot,sizeof(snapshot),&view)==STN_STORAGE_OK);
        CHECK(stn_pending_admit_transaction(&pending,create,cn,NULL,&view,&hash,&report,id)==STN_PENDING_ACCEPTED);
        CHECK(stn_pending_admit_transaction(&pending,response,rn,NULL,&view,&hash,&report,id)==STN_PENDING_INVALID);
        stn_storage_view_release(&view);
        CHECK(stn_mining_template_local(&service,work,sizeof(work),&wn)==STN_RPC_OK);CHECK(solve(work+68,wn-68,1));
        CHECK(stn_mining_submit_work_local(&service,&miner,work,work+32,work+68,wn-68)==STN_RPC_OK);
        CHECK(service.active.height==1);stn_pending_clear(&pending);
        memcpy(bad,response,rn);bad[12+76]^=1;CHECK(rejected_response(&service,&miner,bad,rn)==0);
        memcpy(bad,response,rn);bad[rn-1]^=1;CHECK(rejected_response(&service,&miner,bad,rn)==0);
        memcpy(bad,response,rn);bad[12+8]^=1;CHECK(rejected_response(&service,&miner,bad,rn)==0);
        memset(other,0x55,32);CHECK(response_wire(other,bpk,bsk,bad,&bn)==0);CHECK(rejected_response(&service,&miner,bad,bn)==0);
        CHECK(response_wire(cid,cpk,csk,bad,&bn)==0);CHECK(rejected_response(&service,&miner,bad,bn)==0);
        CHECK(stn_storage_load(&context,&storage,snapshot,sizeof(snapshot),&view)==STN_STORAGE_OK);
        CHECK(stn_pending_admit_transaction(&pending,response,rn-1,NULL,&view,&hash,&report,id)==STN_PENDING_INVALID);
        CHECK(stn_pending_admit_transaction(&pending,response,rn,NULL,&view,&hash,&report,id)==STN_PENDING_ACCEPTED);
        CHECK(stn_pending_admit_transaction(&pending,response,rn,NULL,&view,&hash,&report,id)==STN_PENDING_DUPLICATE);
        CHECK(stn_contract_snapshot_response_count(view.state.contracts,cid)==0);stn_storage_view_release(&view);
        CHECK(stn_pending_insert(&pending,bad,bn,&hash,id)==STN_PENDING_ACCEPTED); /* assembly must exclude nonparticipant */
        CHECK(stn_mining_template_local(&service,work,sizeof(work),&wn)==STN_RPC_OK);
        CHECK(stn_wire_read(work+68+160,4)==1);CHECK(solve(work+68,wn-68,1));
        fail_write=1;CHECK(stn_mining_submit_work_local(&service,&miner,work,work+32,work+68,wn-68)==STN_RPC_PROVIDER);fail_write=0;
        CHECK(service.active.height==1 && stn_contract_snapshot_response_count(service.active.contracts,cid)==0);
        CHECK(stn_mining_submit_work_local(&service,&miner,work,work+32,work+68,wn-68)==STN_RPC_OK);
        CHECK(service.active.height==2);stn_pending_clear(&pending);
        CHECK(rejected_response(&service,&miner,response,rn)==0); /* accepted replay */
        CHECK(stn_storage_load(&context,&storage,snapshot,sizeof(snapshot),&view)==STN_STORAGE_OK);
        CHECK(stn_contract_snapshot_response_count(view.state.contracts,cid)==1);
        CHECK(stn_contract_snapshot_response_at(view.state.contracts,cid,0,&saved_response)==STN_CONTRACT_OK);
        CHECK(memcmp(saved_response.actor,bpk,32)==0 && memcmp(saved_response.contract_id,cid,32)==0);
        CHECK(stn_contract_snapshot_const_state(view.state.contracts)->entries[0].current.state==STN_CONTRACT_STATE_ISSUED);
        CHECK(stn_contract_snapshot_const_state(view.state.contracts)->entries[0].current.sequence==1);
        /* Discard live state: RPC now uses only the history-reconstructed copy. */
        stn_chain_state_release(&service.active);CHECK(stn_chain_state_share(&view.state,&service.active)==STN_DATA_OK);
        stn_storage_view_release(&view);
        CHECK(stn_address_encode(&address,(char *)query,71,&qn)==STN_DATA_OK && qn==70);memset(query+70,0,4);
        CHECK(stn_mining_handle(&service,&request,reply,sizeof(reply),&qn)==STN_RPC_OK);
        CHECK(stn_contract_response_query_reply_valid(reply,qn));CHECK(stn_wire_read(reply,4)==1 && qn==8+rn-12);
        CHECK(memcmp(reply+8,response+12,rn-12)==0);
        CHECK(stn_contract_response_decode(reply+8,qn-8,&decoded)==STN_CONTRACT_OK);
        CHECK(stn_contract_response_signature_verify(&decoded)==STN_CONTRACT_OK);
        CHECK(decoded.text_length==sizeof("B responds: the original terms have been received.")-1 && memcmp(decoded.text,"B responds: the original terms have been received.",sizeof("B responds: the original terms have been received.")-1)==0);
        query[73]=1;CHECK(stn_mining_handle(&service,&request,reply,sizeof(reply),&qn)==STN_RPC_NOT_FOUND);
    }
    stn_chain_state_release(&service.active);stn_pending_clear(&pending);
    printf("Contract RESPONSE end-to-end: %u checks, %u failures.\n",checks,failures);return failures!=0;
}
