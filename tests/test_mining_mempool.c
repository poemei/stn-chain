/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_mining.h"
#include "stn_compensation.h"
#include "stn_sha256.h"
#include "stn_wire_internal.h"
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
int main(void)
{
    stn_chain_context context={0};stn_pow_policy policy={0};stn_block block={0};
    stn_mining_service service={0};stn_pending pending={0};stn_storage_view view={0};
    stn_storage_provider storage={NULL,acquire,release,read_store,write_store};
    stn_block_span history[1];stn_address miner={0};
    uint8_t genesis[249],body[81],mapping[77],queued[77],queued_id[32];
    uint8_t template_bytes[1024],pending_body[1024],old[1024],fresh[1024],bad[1024],id[32];
    uint8_t saved[65536];size_t gn=0,on=0,fn=0,saved_length=0,found=0;
    uint64_t timestamp=0;

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

    /* Persistence failure neither advances history nor consumes pending. */
    fail_write=1;
    CHECK(stn_mining_submit_work_local(&service,&miner,old,old+32,old+68,on-68)==STN_RPC_PROVIDER);
    fail_write=0;
    CHECK(service.active.height==0 && pending.count==1 && disk_length==saved_length && memcmp(saved,disk,disk_length)==0);
    CHECK(stn_mining_submit_work_local(&service,&miner,old,old+32,old+68,on-68)==STN_RPC_OK);
    CHECK(service.active.height==1 && pending.count==3);
    CHECK(stn_pending_lookup(&pending,queued_id,queued,sizeof(queued),&found)==STN_PENDING_ACCEPTED && found==77);
    CHECK(stn_mining_submit_work_local(&service,&miner,fresh,fresh+32,fresh+68,fn-68)==STN_RPC_STALE);
    CHECK(stn_storage_load(&context,&storage,snapshot,sizeof(snapshot),&view)==STN_STORAGE_OK);
    CHECK(view.count==2 && view.blocks[1].length==168 && memcmp(view.blocks[1].bytes,old+68,168)==0);
    timestamp=view.state.timestamp;CHECK(timestamp==1);
    stn_storage_view_release(&view);stn_chain_state_release(&service.active);stn_pending_clear(&pending);
    printf("Mining mempool changes: %u checks, %u failures.\n",checks,failures);
    return failures!=0;
}
