/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_mining.h"
#include "stn_compensation.h"
#include "stn_sha256.h"
#include "stn_wire_internal.h"
#include "stn_contract_transaction.h"
#include "stn_contract_query.h"
#include <stdio.h>
#include <string.h>

static unsigned checks,failures;
#define CHECK(e) do { ++checks; if(!(e)){++failures;fprintf(stderr,"mempool line %d: %s\n",__LINE__,#e);return 1;} } while(0)
static uint8_t disk[65536],snapshot[65536],current[65536],next[65536];
static size_t disk_length;
static int locked,fail_write;
static stn_hash_provider hash={stn_sha256,NULL};
static stn_storage_status acquire(void *u){(void)u;if(locked)return STN_STORAGE_BUSY;locked=1;return STN_STORAGE_OK;}
static void release(void *u){(void)u;locked=0;}
static stn_storage_status read_store(void *u,uint8_t *out,size_t cap,size_t *n){(void)u;*n=disk_length;if(disk_length==0)return STN_STORAGE_NOT_FOUND;if(cap<disk_length)return STN_STORAGE_CAPACITY;memcpy(out,disk,disk_length);return STN_STORAGE_OK;}
static stn_storage_status write_store(void *u,const uint8_t *p,size_t n){(void)u;if(fail_write)return STN_STORAGE_IO;if(n>sizeof(disk))return STN_STORAGE_CAPACITY;memcpy(disk,p,n);disk_length=n;return STN_STORAGE_OK;}
static int solve(uint8_t *block,size_t n,int valid)
{
    uint64_t nonce;uint8_t digest[32];
    for(nonce=0;nonce<10000u;++nonce){
        stn_wire_write(block+152,8,nonce);
        if((stn_pow_verify(block,n,&hash,digest)==STN_DATA_OK)==valid)return 1;
    }
    return 0;
}
static size_t destination(uint8_t miner,uint8_t wallet,uint8_t out[77])
{
    stn_compensation_destination d={0};stn_transaction t={0};uint8_t record[65];size_t n=0;
    d.mining_identity.type=STN_ADDRESS_IDENTITY;d.mining_identity.identifier[0]=miner;
    d.wallet.type=STN_ADDRESS_WALLET;d.wallet.identifier[0]=wallet;
    if(stn_compensation_destination_encode(&d,record)!=STN_DATA_OK)return 0;
    t.version=1;t.type=STN_TX_COMPENSATION_DESTINATION;t.record_bytes=record;t.record_length=65;
    if(stn_transaction_encode(&t,out,77,&n)!=STN_DATA_OK)return 0;
    return n;
}
static int work_id(uint8_t *block,uint8_t id[32])
{
    static const uint8_t domain[]="STN-CHAIN:WORK:ID:1";
    uint8_t header[168];memcpy(header,block,168);memset(header+152,0,8);
    return stn_sha256(NULL,domain,sizeof(domain),header,168,id)==STN_DATA_OK;
}
/* RFC 8032 public test vector, never a user's signing key. */
static const uint8_t contract_sk[32]={0x9d,0x61,0xb1,0x9d,0xef,0xfd,0x5a,0x60,0xba,0x84,0x4a,0xf4,0x92,0xec,0x2c,0xc4,0x44,0x49,0xc5,0x69,0x7b,0x32,0x69,0x19,0x70,0x3b,0xac,0x03,0x1c,0xae,0x7f,0x60};
static const uint8_t contract_pk[32]={0xd7,0x5a,0x98,0x01,0x82,0xb1,0x0a,0xb7,0xd5,0x4b,0xfe,0xd3,0xc9,0x64,0x07,0x3a,0x0e,0xe1,0x72,0xf3,0xda,0xa6,0x23,0x25,0xaf,0x02,0x1a,0x68,0xf7,0x07,0x51,0x1a};
int stn_ed25519_sign(const uint8_t *,size_t,const uint8_t [32],const uint8_t [32],uint8_t [64]);
static int create_contract(uint8_t *out,size_t *length,int wrong_issuer,uint64_t sequence)
{
    stn_contract draft={0};stn_contract_participant participants[3]={{0}};
    stn_contract_transaction action={0};stn_transaction tx={0};stn_address issuer;
    uint8_t canonical[1024],payload[2048],unsigned_bytes[2048],statement[2048];
    size_t cn=0,pn=0,sn=0;
    CHECK(stn_address_derive(STN_ADDRESS_IDENTITY,contract_pk,32,&issuer)==STN_DATA_OK);
    memcpy(participants[0].identity,issuer.identifier,32);
    if(wrong_issuer)participants[0].identity[0]^=1;
    participants[0].role=STN_CONTRACT_ROLE_ISSUER;
    memcpy(participants[1].identity,issuer.identifier,32);
    participants[1].role=STN_CONTRACT_ROLE_APPROVER;
    memset(participants[2].identity,0x42,32);participants[2].role=STN_CONTRACT_ROLE_APPROVER;
    draft.version=1;draft.type=STN_CONTRACT_GENERIC;draft.state=STN_CONTRACT_STATE_DRAFT;
    draft.created_at=123;draft.participants=participants;draft.participant_count=3;
    draft.terms=(const uint8_t *)"contract delivery regression";draft.terms_length=28;
    CHECK(stn_contract_encode(&draft,canonical,sizeof(canonical),&cn)==STN_CONTRACT_OK);
    action.version=1;action.action=STN_CONTRACT_ACTION_CREATE;action.sequence=sequence;
    action.canonical_contract=canonical;action.canonical_contract_length=(uint32_t)cn;
    memcpy(action.actor,contract_pk,32);
    memcpy(unsigned_bytes,canonical,cn);memcpy(unsigned_bytes+cn,contract_pk,32);
    stn_wire_write(unsigned_bytes+cn+32,2,action.action);stn_wire_write(unsigned_bytes+cn+34,8,sequence);
    CHECK(stn_identity_statement(unsigned_bytes,cn+42,statement,sizeof(statement),&sn)==STN_IDENTITY_VALID);
    CHECK(stn_ed25519_sign(statement,sn,contract_pk,contract_sk,action.signature)==0);
    CHECK(stn_contract_transaction_encode(&action,payload,sizeof(payload),&pn)==STN_CONTRACT_OK);
    tx.version=1;tx.type=STN_TX_CONTRACT_ACTION;tx.record_bytes=payload;tx.record_length=(uint32_t)pn;
    CHECK(stn_transaction_encode(&tx,out,2048,length)==STN_DATA_OK);
    return 0;
}
static int contract_delivery(stn_mining_service *service,const stn_address *miner)
{
    stn_storage_view view={0};stn_validation_report report;
    stn_contract_query_result listed[16];stn_address issuer;
    uint8_t tx[2048],bad[2048],id[32],work[16384],bad_work[16384],recipient[32];
    size_t tn=0,bn=0,wn=0,count=0;uint64_t before=service->active.height;
    stn_pending_clear(service->pending);
    CHECK(stn_storage_load(service->chain,service->storage,snapshot,sizeof(snapshot),&view)==STN_STORAGE_OK);
    CHECK(create_contract(tx,&tn,0,0)==0);
    memcpy(bad,tx,tn);bad[STN_TX_HEADER_SIZE+52]^=1; /* signature */
    CHECK(stn_pending_admit_transaction(service->pending,bad,tn,NULL,&view,&hash,&report,id)==STN_PENDING_INVALID);
    CHECK(create_contract(bad,&bn,1,0)==0);
    CHECK(stn_pending_admit_transaction(service->pending,bad,bn,NULL,&view,&hash,&report,id)==STN_PENDING_INVALID);
    CHECK(create_contract(bad,&bn,0,1)==0);
    CHECK(stn_pending_admit_transaction(service->pending,bad,bn,NULL,&view,&hash,&report,id)==STN_PENDING_INVALID);
    CHECK(service->pending->count==0);
    CHECK(stn_pending_admit_transaction(service->pending,tx,tn,NULL,&view,&hash,&report,id)==STN_PENDING_ACCEPTED);
    CHECK(stn_contract_snapshot_const_state(view.state.contracts)->entry_count==0);
    /* Even invalid entries received through a raw/internal insertion are
     * filtered from work. Their presence must not invalidate the good CREATE. */
    CHECK(stn_pending_insert(service->pending,bad,bn,&hash,id)==STN_PENDING_ACCEPTED);
    stn_storage_view_release(&view);
    CHECK(stn_mining_template_local(service,work,sizeof(work),&wn)==STN_RPC_OK);
    CHECK(stn_wire_read(work+68+160,4)==1);
    memcpy(bad_work,work,wn);bad_work[68+168+4+12+52]^=1;
    CHECK(stn_block_body_commitment(bad_work+68+168,wn-68-168,1,&hash,bad_work+68+88)==STN_DATA_OK);
    CHECK(work_id(bad_work+68,bad_work+32));CHECK(solve(bad_work+68,wn-68,1));
    CHECK(stn_mining_submit_work_local(service,miner,bad_work,bad_work+32,bad_work+68,wn-68)==STN_RPC_REJECTED);
    CHECK(service->active.height==before);
    CHECK(solve(work+68,wn-68,1));
    CHECK(stn_mining_submit_work_local(service,miner,work,work+32,work+68,wn-68)==STN_RPC_OK);
    CHECK(service->active.height==before+1);
    /* Full reconstruction from persisted bytes, as on node restart. */
    CHECK(stn_storage_load(service->chain,service->storage,snapshot,sizeof(snapshot),&view)==STN_STORAGE_OK);
    CHECK(stn_address_derive(STN_ADDRESS_IDENTITY,contract_pk,32,&issuer)==STN_DATA_OK);
    CHECK(stn_contract_query_identity(stn_contract_snapshot_const_state(view.state.contracts),issuer.identifier,listed,16,&count)==STN_CONTRACT_OK);
    CHECK(count==1 && listed[0].state==STN_CONTRACT_STATE_ISSUED && listed[0].sequence==1);
    memcpy(id,listed[0].contract_id,32);memset(recipient,0x42,32);
    CHECK(stn_contract_query_identity(stn_contract_snapshot_const_state(view.state.contracts),recipient,listed,16,&count)==STN_CONTRACT_OK);
    CHECK(count==1 && memcmp(id,listed[0].contract_id,32)==0);
    {
        stn_rpc_message request={1,STN_RPC_CONTRACT_LIST,STN_RPC_OK,1,NULL,0};
        stn_address address={0};char text[71];uint8_t response[2048];size_t text_length=0,written=0;
        address.type=STN_ADDRESS_IDENTITY;memcpy(address.identifier,recipient,32);
        CHECK(stn_address_encode(&address,text,sizeof(text),&text_length)==STN_DATA_OK);
        request.payload=(const uint8_t *)text;request.length=(uint32_t)text_length;
        CHECK(stn_mining_handle(service,&request,response,sizeof(response),&written)==STN_RPC_OK);
        CHECK(written==92 && stn_wire_read(response,2)==1 && stn_wire_read(response+72,2)==STN_CONTRACT_STATE_ISSUED);
    }

    CHECK(stn_pending_admit_transaction(service->pending,tx,tn,NULL,&view,&hash,&report,id)==STN_PENDING_INVALID);
    stn_storage_view_release(&view);
    return 0;
}
int main(void)
{
    stn_chain_context context={0};stn_pow_policy policy={0};stn_block block={0};
    stn_mining_service service={0};stn_pending pending={0};stn_storage_view view={0};
    stn_storage_provider storage={NULL,acquire,release,read_store,write_store};
    stn_block_span history[1];stn_address miner={0};
    uint8_t genesis[249],body[81],mapping[77],queued[77],queued_id[32];
    uint8_t template_bytes[16384],pending_body[16384],drain[16384],old[1024],fresh[1024],bad[1024],id[32];
    uint8_t saved[65536];size_t gn=0,on=0,fn=0,saved_length=0,found=0;
    uint64_t timestamp=0;
    size_t queued_count;

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
    CHECK(stn_mining_template_local(&service,old,sizeof(old),&on)==STN_RPC_OK);
    CHECK(on==68+168);

    /* A new pending transaction changes the next Work ID on the same parent. */
    CHECK(destination(3,4,queued)==77);
    CHECK(stn_pending_insert(&pending,queued,77,&hash,queued_id)==STN_PENDING_ACCEPTED);
    CHECK(stn_mining_template_local(&service,fresh,sizeof(fresh),&fn)==STN_RPC_OK);
    CHECK(fn>on && memcmp(old,fresh,32)==0 && memcmp(old+32,fresh+32,32)!=0);
    saved_length=disk_length;memcpy(saved,disk,disk_length);

    /* ID, proof, target, body commitment and economic checks still fail closed. */
    CHECK(solve(old+68,on-68,1));
    memcpy(id,old+32,32);id[0]^=1;
    CHECK(stn_mining_submit_work_local(&service,&miner,old,id,old+68,on-68)==STN_RPC_REJECTED);
    memcpy(bad,old,on);CHECK(solve(bad+68,on-68,0));
    CHECK(stn_mining_submit_work_local(&service,&miner,bad,bad+32,bad+68,on-68)==STN_RPC_REJECTED);
    memcpy(bad,fresh,fn);bad[fn-1]^=1;
    CHECK(stn_mining_submit_work_local(&service,&miner,bad,bad+32,bad+68,fn-68)==STN_RPC_REJECTED);
    memcpy(bad,old,on);bad[68+120]=63;CHECK(work_id(bad+68,bad+32));CHECK(solve(bad+68,on-68,1));
    CHECK(stn_mining_submit_work_local(&service,&miner,bad,bad+32,bad+68,on-68)==STN_RPC_REJECTED);
    memcpy(bad,fresh,fn);CHECK(destination(1,9,bad+68+168+4)==77);
    CHECK(stn_block_body_commitment(bad+68+168,81,1,&hash,bad+68+88)==STN_DATA_OK);
    CHECK(work_id(bad+68,bad+32));CHECK(solve(bad+68,fn-68,1));
    CHECK(stn_mining_submit_work_local(&service,&miner,bad,bad+32,bad+68,fn-68)==STN_RPC_REJECTED);
    CHECK(service.active.height==0 && pending.count==1 && disk_length==saved_length && memcmp(saved,disk,disk_length)==0);

    /* A long block interval exceeds the old 128-entry queue. At the new
     * admission limit the complete share pair is refused, but a block can
     * still commit and reserve its reward without consuming later arrivals. */
    {
        stn_share_evidence share={0};uint8_t proof[32],share_id[32];size_t admitted=0;
        share.miner=miner;memcpy(share.work_id,old+32,32);
        memcpy(share.template_header,old+68,168);memset(share.template_header+152,0,8);
        memcpy(share.body_commitment,old+68+88,32);
        while(pending.count<STN_PENDING_MAX_ENTRIES-3u){
            ++share.nonce;
            CHECK(share.nonce<10000u);
            if(stn_share_verify_evidence(&share,&hash,proof)!=STN_DATA_OK)continue;
            CHECK(stn_mining_submit_share_local(&service,&share,share_id)==STN_RPC_OK);
            ++admitted;
        }
        CHECK(admitted>64u && pending.bytes<STN_PENDING_MAX_BYTES);
        do {++share.nonce;} while(stn_share_verify_evidence(&share,&hash,proof)!=STN_DATA_OK);
        queued_count=pending.count;
        CHECK(stn_mining_submit_share_local(&service,&share,share_id)==STN_RPC_CAPACITY);
        CHECK(pending.count==queued_count);
    }

    /* Persistence failure neither advances history nor consumes pending. */
    fail_write=1;
    CHECK(stn_mining_submit_work_local(&service,&miner,old,old+32,old+68,on-68)==STN_RPC_PROVIDER);
    fail_write=0;
    CHECK(service.active.height==0 && pending.count==queued_count && disk_length==saved_length && memcmp(saved,disk,disk_length)==0);
    CHECK(stn_mining_submit_work_local(&service,&miner,old,old+32,old+68,on-68)==STN_RPC_OK);
    CHECK(service.active.height==1 && pending.count==queued_count+2u);
    CHECK(stn_pending_lookup(&pending,queued_id,queued,sizeof(queued),&found)==STN_PENDING_ACCEPTED && found==77);
    CHECK(stn_mining_submit_work_local(&service,&miner,fresh,fresh+32,fresh+68,fn-68)==STN_RPC_STALE);
    CHECK(stn_storage_load(&context,&storage,snapshot,sizeof(snapshot),&view)==STN_STORAGE_OK);
    CHECK(view.count==2 && view.blocks[1].length==168 && memcmp(view.blocks[1].bytes,old+68,168)==0);
    timestamp=view.state.timestamp;CHECK(timestamp==1);
    stn_storage_view_release(&view);
    CHECK(stn_mining_template_local(&service,drain,sizeof(drain),&fn)==STN_RPC_OK);
    CHECK(stn_wire_read(drain+68+160,4)==STN_BLOCK_MAX_TRANSACTIONS);
    CHECK(solve(drain+68,fn-68,1));
    CHECK(stn_mining_submit_work_local(&service,&miner,drain,drain+32,drain+68,fn-68)==STN_RPC_OK);
    CHECK(service.active.height==2 && pending.count==queued_count+4u-STN_BLOCK_MAX_TRANSACTIONS);
    CHECK(contract_delivery(&service,&miner)==0);
    stn_chain_state_release(&service.active);stn_pending_clear(&pending);
    printf("Mining mempool changes: %u checks, %u failures.\n",checks,failures);
    return failures!=0;
}
