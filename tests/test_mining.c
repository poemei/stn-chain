/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_mining.h"
#include "stn_sha256.h"
#include <stdio.h>
#include <string.h>
static unsigned checks,failures;
#define CHECK(e) do {++checks;if(!(e)){++failures;fprintf(stderr,"mining line %d: %s\n",__LINE__,#e);}} while(0)
typedef struct memory_store {uint8_t bytes[65536];size_t n;int fail,locked,acquisitions,race;uint8_t race_bytes[65536];size_t race_n;} memory_store;
static stn_storage_status acquire(void *u)
{memory_store *m=u;++m->acquisitions;if(m->race && m->acquisitions==2){memcpy(m->bytes,m->race_bytes,m->race_n);m->n=m->race_n;}if(m->locked){return STN_STORAGE_BUSY;}m->locked=1;return STN_STORAGE_OK;}
static void release(void *u){((memory_store *)u)->locked=0;}
static stn_storage_status read_store(void *u,uint8_t *p,size_t cap,size_t *n)
{memory_store *m=u;if(m->n==0){return STN_STORAGE_NOT_FOUND;}if(cap<m->n){return STN_STORAGE_CAPACITY;}memcpy(p,m->bytes,m->n);*n=m->n;return STN_STORAGE_OK;}
static stn_storage_status replace(void *u,const uint8_t *p,size_t n)
{memory_store *m=u;if(m->fail){return STN_STORAGE_IO;}if(n>sizeof(m->bytes)){return STN_STORAGE_CAPACITY;}memcpy(m->bytes,p,n);m->n=n;return STN_STORAGE_OK;}
static void fixture(uint8_t p[364])
{
    static const uint8_t commitment[32]={0xec,0xf9,0x1b,0xfa,0x6a,0x4e,0x06,0xd8,0x6a,0x24,0xd6,0x36,0xee,0xd2,0x5f,0x01,0xcf,0x30,0x29,0x49,0xe8,0x53,0x51,0xd4,0x37,0xe0,0x4c,0x4e,0x49,0x21,0x9a,0x76};
    memset(p,0,364);memcpy(p,"STNB",4);p[5]=3;p[8]=1;p[163]=1;p[167]=196;p[171]=192;
    memcpy(p+172,"STNT",4);p[177]=1;p[179]=1;p[183]=180;
    memcpy(p+184,"STNR",4);p[189]=1;p[191]=1;p[192]=1;p[256]=3;
    memcpy(p+88,commitment,32);memset(p+120,255,32);p[120]=127;
}
static stn_rpc_code rpc(stn_mining_service *s,uint16_t method,const uint8_t *p,size_t n,uint8_t *out,size_t *w)
{
    uint8_t request[512],response[512];size_t rn,wn;stn_rpc_message q={1,0,STN_RPC_OK,123,NULL,0},r;
    stn_rpc_service service={s,stn_mining_handle};q.method=method;q.payload=p;q.length=n;
    CHECK(stn_rpc_encode(&q,request,sizeof(request),&rn)==STN_RPC_OK);
    CHECK(stn_rpc_dispatch(request,rn,3,&service,response,sizeof(response),&wn)==STN_RPC_OK);
    CHECK(stn_rpc_decode(response,wn,&r)==STN_RPC_OK && r.request_id==123);
    *w=r.length;if(r.length!=0){memcpy(out,r.payload,r.length);}return r.code;
}
static void put64(uint8_t *p,uint64_t value)
{size_t i;for(i=8;i!=0;){--i;p[i]=(uint8_t)value;value>>=8;}}
static int solve_block(uint8_t *p,size_t n,const stn_hash_provider *hash)
{uint64_t nonce;uint8_t digest[32];for(nonce=UINT64_C(4294967296);nonce<UINT64_C(4295032832);++nonce){put64(p+152,nonce);if(stn_pow_verify(p,n,hash,digest)==STN_DATA_OK){return 1;}}return 0;}

static void activated_difficulty(void)
{
    static const uint64_t spans[]={3600,1800,0,7200,20000,14400};
    static uint8_t blocks[122][364],alternative[61][364],snapshot[65536],current[65536],next[65536];
    static memory_store store;
    stn_block_span history[122],other[61];stn_chain_context c={0};stn_pow_policy policy;
    stn_chain_state state,before,unchanged;stn_chain_report report;stn_storage_view view;
    stn_storage_provider provider={&store,acquire,release,read_store,replace};
    stn_mining_service s={0};stn_reorg_plan plan;stn_fork_report fork;
    uint8_t templ[512],work[512],again[512],out[512],required[32],wrong[364];
    size_t i,j,k,n,w;unsigned count_before=checks;
    for(k=0;k<sizeof(spans)/sizeof(spans[0]);++k) {
        memset(&store,0,sizeof(store));memset(&s,0,sizeof(s));fixture(blocks[0]);
        if(k!=5){blocks[0][120]=31;} /* room for easier adjustments */
        memcpy(policy.fixed_target,blocks[0]+120,32);c.network_id[0]=1;c.genesis_bytes=blocks[0];c.genesis_length=364;
        c.pow_policy=&policy;c.hash_provider.hash=stn_sha256;
        CHECK(solve_block(blocks[0],364,&c.hash_provider));
        CHECK(stn_chain_initialize(&c,&state)==STN_DATA_OK);
        for(i=0;i<60;++i) {
            if(i!=0){memcpy(blocks[i],blocks[i-1],364);memcpy(blocks[i]+40,state.tip_id,32);put64(blocks[i]+72,(uint64_t)i);put64(blocks[i]+80,spans[k]*(uint64_t)i/59);CHECK(solve_block(blocks[i],364,&c.hash_provider));}
            history[i].bytes=blocks[i];history[i].length=364;
            CHECK(stn_chain_validate_candidate(&c,&state,blocks[i],364,&state).acceptance==STN_ACCEPTANCE_UNDER_CONTEXT);
            CHECK(memcmp(state.current_target,policy.fixed_target,32)==0);
        }
        before=state;
        CHECK(stn_target_next(60,policy.fixed_target,state.target_history,60,required)==STN_DATA_OK);
        CHECK(stn_storage_adopt(&c,&provider,history,60,&(stn_storage_workspace){current,sizeof(current),next,sizeof(next)},&state)==STN_STORAGE_OK);
        s.chain=&c;s.storage=&provider;s.body=blocks[0]+168;s.body_length=196;s.transaction_count=1;
        s.snapshot=snapshot;s.snapshot_capacity=sizeof(snapshot);s.template_bytes=templ;s.template_capacity=sizeof(templ);
        s.workspace.current_bytes=current;s.workspace.current_capacity=sizeof(current);s.workspace.next_bytes=next;s.workspace.next_capacity=sizeof(next);
        CHECK(rpc(&s,STN_RPC_MINING_TEMPLATE,NULL,0,work,&n)==STN_RPC_OK);
        CHECK(memcmp(work+188,required,32)==0);
        CHECK(rpc(&s,STN_RPC_MINING_TEMPLATE,NULL,0,again,&w)==STN_RPC_OK && w==n && memcmp(work,again,n)==0);
        /* Required history unavailable/corrupt: no guessed template output. */
        {uint8_t saved_byte=store.bytes[16];store.bytes[16]^=1;
            CHECK(rpc(&s,STN_RPC_MINING_TEMPLATE,NULL,0,again,&w)!=STN_RPC_OK && w==0);
            store.bytes[16]=saved_byte;}
        for(j=0;j<3;++j) {
            memcpy(wrong,work+68,364);
            if(j==0){memcpy(wrong+120,policy.fixed_target,32);if(memcmp(wrong+120,required,32)==0){continue;}}
            else if(j==1){size_t digit=152;if(required[0]==127){continue;}while(digit>120){--digit;++wrong[digit];if(wrong[digit]!=0){break;}}}
            else {--wrong[151];}
            CHECK(solve_block(wrong,364,&c.hash_provider));unchanged=before;
            report=stn_chain_validate_candidate(&c,&before,wrong,364,&unchanged);
            CHECK(report.reason==STN_CHAIN_TARGET && report.detail==STN_DATA_TARGET && memcmp(&unchanged,&before,sizeof(before))==0);
        }
        state=before;state.target_history_count=59;memset(out,0xa5,32);
        CHECK(stn_chain_required_target(&c,&state,out)==STN_DATA_UNRESOLVED && out[0]==0xa5);
        CHECK(stn_chain_validate_candidate(&c,&state,work+68,364,&unchanged).acceptance==STN_ACCEPTANCE_UNRESOLVED);
        CHECK(solve_block(work+68,364,&c.hash_provider));
        CHECK(rpc(&s,STN_RPC_SUBMIT_WORK,work,n,out,&w)==STN_RPC_OK && s.active.height==60);
        CHECK(stn_storage_load(&c,&provider,snapshot,sizeof(snapshot),&view)==STN_STORAGE_OK);
        CHECK(view.state.height==60 && memcmp(view.state.current_target,required,32)==0 && memcmp(view.blocks[60].bytes,work+68,364)==0);
        state=view.state;stn_storage_view_release(&view);
        memset(&s.active,0,sizeof(s.active));
        CHECK(rpc(&s,STN_RPC_MINING_TEMPLATE,NULL,0,again,&w)==STN_RPC_OK && memcmp(again+188,required,32)==0 && again[147]==61);
        if(k==1) {
            /* Competing branch: same genesis, slower accepted history. */
            stn_chain_state alt;
            CHECK(stn_chain_initialize(&c,&alt)==STN_DATA_OK);
            for(i=0;i<61;++i) {
                memcpy(alternative[i],blocks[i==60 ? 59 : i],364);
                if(i!=0){memcpy(alternative[i]+40,alt.tip_id,32);put64(alternative[i]+72,(uint64_t)i);put64(alternative[i]+80,7200*(uint64_t)(i>59 ? 59 : i)/59);}
                CHECK(stn_chain_required_target(&c,&alt,alternative[i]+120)==STN_DATA_OK);
                CHECK(solve_block(alternative[i],364,&c.hash_provider));
                other[i].bytes=alternative[i];other[i].length=364;
                CHECK(stn_chain_validate_candidate(&c,&alt,alternative[i],364,&alt).acceptance==STN_ACCEPTANCE_UNDER_CONTEXT);
            }
            memcpy(blocks[60],work+68,364);history[60].bytes=blocks[60];history[60].length=364;
            fork=stn_fork_evaluate_history(&c,other,61,history,61,&plan);
            CHECK(fork.result==STN_FORK_CANDIDATE && plan.ancestor_index==0 && memcmp(plan.current.current_target,plan.candidate.current_target,32)!=0);
            memset(&store,0,sizeof(store));
            CHECK(stn_storage_adopt(&c,&provider,other,61,&s.workspace,&s.active)==STN_STORAGE_OK);
            CHECK(stn_storage_adopt(&c,&provider,history,61,&s.workspace,&s.active)==STN_STORAGE_OK && memcmp(s.active.current_target,required,32)==0);
            CHECK(stn_storage_load(&c,&provider,snapshot,sizeof(snapshot),&view)==STN_STORAGE_OK);
            CHECK(memcmp(view.state.cumulative_work.bytes,plan.candidate.cumulative_work.bytes,STN_WORK_SIZE)==0 && memcmp(view.state.current_target,required,32)==0);
            fork=stn_fork_evaluate_history(&c,other,61,view.blocks,view.count,&plan);
            CHECK(fork.result==STN_FORK_CANDIDATE && plan.ancestor_index==0);
            stn_storage_view_release(&view);
            /* Continue through the second boundary using the same rule. */
            for(i=61;i<=121;++i) {
                memcpy(blocks[i],blocks[i-1],364);memcpy(blocks[i]+40,state.tip_id,32);put64(blocks[i]+72,(uint64_t)i);
                CHECK(stn_chain_required_target(&c,&state,blocks[i]+120)==STN_DATA_OK);
                CHECK(solve_block(blocks[i],364,&c.hash_provider));
                CHECK(stn_chain_validate_candidate(&c,&state,blocks[i],364,&state).acceptance==STN_ACCEPTANCE_UNDER_CONTEXT);
                history[i].bytes=blocks[i];history[i].length=364;
            }
            CHECK(state.height==121 && state.current_target[0]==3);
            CHECK(stn_storage_adopt(&c,&provider,history,122,&s.workspace,&s.active)==STN_STORAGE_OK);
            CHECK(rpc(&s,STN_RPC_MINING_TEMPLATE,NULL,0,again,&w)==STN_RPC_OK && again[147]==122 && again[188]==3);
        }
    }
    printf("Activated difficulty: %u targeted checks.\n",checks-count_before);
}

int test_mining(void)
{
    static const uint8_t expected[32]={0x59,0xf6,0xd1,0x0b,0x44,0x3d,0xcf,0x2b,0xf0,0xbb,0x37,0xf0,0x21,0xb3,0xc5,0x64,0x4a,0x66,0xe7,0x18,0x39,0xbe,0x83,0x80,0xd9,0x56,0xcb,0xf2,0xca,0x4b,0x76,0xc7};
    memory_store store={0};stn_storage_provider provider={&store,acquire,release,read_store,replace};
    uint8_t genesis[364],child[364],alt[364],grand[364],snapshot[4096],current[4096],next[4096],templ[512];
    uint8_t work[512],again[512],changed[520],out[512],disk_before[4096],body_copy[196],second_work[512];
    stn_chain_context c={0};stn_pow_policy policy;stn_mining_service s={0};stn_block_span history[3];
    stn_chain_state before;stn_storage_view view;stn_rpc_message q={1,STN_RPC_MINING_TEMPLATE,STN_RPC_OK,1,NULL,0};
    size_t n,w,i,disk_n,second_n;int recovery;
    fixture(genesis);memcpy(policy.fixed_target,genesis+120,32);c.network_id[0]=1;
    c.genesis_bytes=genesis;c.genesis_length=364;c.hash_provider.hash=stn_sha256;c.pow_policy=&policy;
    s.chain=&c;s.storage=&provider;s.body=genesis+168;s.body_length=196;s.transaction_count=1;
    s.snapshot=snapshot;s.snapshot_capacity=sizeof(snapshot);s.template_bytes=templ;s.template_capacity=sizeof(templ);
    s.workspace.current_bytes=current;s.workspace.current_capacity=sizeof(current);s.workspace.next_bytes=next;s.workspace.next_capacity=sizeof(next);
    history[0].bytes=genesis;history[0].length=364;
    CHECK(stn_storage_create(&c,&provider,history,1,next,sizeof(next),&s.active)==STN_STORAGE_OK);
    before=s.active;disk_n=store.n;memcpy(disk_before,store.bytes,disk_n);
    CHECK(rpc(&s,STN_RPC_MINING_TEMPLATE,NULL,0,work,&n)==STN_RPC_OK && n==432);
    CHECK(rpc(&s,STN_RPC_MINING_TEMPLATE,NULL,0,again,&w)==STN_RPC_OK && n==w && memcmp(work,again,n)==0);
    CHECK(memcmp(work,before.tip_id,32)==0 && memcmp(work+32,expected,32)==0);
    CHECK(memcmp(work+68+120,policy.fixed_target,32)==0 && work[68+79]==1 && work[67]==108 && work[66]==1);
    CHECK(memcmp(work+68+152,"\0\0\0\0\0\0\0\0",8)==0);
    memcpy(body_copy,s.body,196);s.body=body_copy;
    CHECK(rpc(&s,STN_RPC_MINING_TEMPLATE,NULL,0,again,&w)==STN_RPC_OK && memcmp(work,again,n)==0);
    CHECK(rpc(&s,STN_RPC_MINING_CONTEXT,NULL,0,out,&w)==STN_RPC_OK && w==76 && out[75]==1);
    CHECK(stn_mining_handle(&s,&q,out,431,&w)==STN_RPC_CAPACITY && w==0);
    /* Every immutable byte is bound, even fields ignored by structural decoding. */
    for(i=0;i<n;++i){
        if(i>=68+152 && i<68+160){continue;}
        memcpy(changed,work,n);changed[i]^=1;q.method=STN_RPC_SUBMIT_WORK;q.payload=changed;q.length=n;
        CHECK(stn_mining_handle(&s,&q,out,sizeof(out),&w)!=STN_RPC_OK && w==0);
        CHECK(store.n==disk_n && memcmp(store.bytes,disk_before,disk_n)==0 && memcmp(&s.active,&before,sizeof(before))==0);
    }
    memcpy(changed,work,n);changed[68+159]=1;
    CHECK(rpc(&s,STN_RPC_SUBMIT_WORK,changed,n,out,&w)==STN_RPC_REJECTED); /* real hash > target */
    CHECK(store.n==disk_n && memcmp(store.bytes,disk_before,disk_n)==0);
    q.payload=work;q.length=n-1;
    CHECK(stn_mining_handle(&s,&q,out,sizeof(out),&w)==STN_RPC_INVALID && w==0);
    s.workspace.next_capacity=8;
    CHECK(rpc(&s,STN_RPC_SUBMIT_WORK,work,n,out,&w)==STN_RPC_CAPACITY && w==0);
    CHECK(store.n==disk_n && memcmp(&s.active,&before,sizeof(before))==0);
    s.workspace.next_capacity=sizeof(next);
    store.fail=1;
    CHECK(rpc(&s,STN_RPC_SUBMIT_WORK,work,n,out,&w)==STN_RPC_PROVIDER && w==0);
    CHECK(memcmp(&s.active,&before,sizeof(before))==0 && store.n==disk_n && memcmp(store.bytes,disk_before,disk_n)==0);
    store.fail=0;memcpy(child,work+68,364);history[1].bytes=child;history[1].length=364;
    CHECK(stn_storage_encode(&c,history,2,store.race_bytes,sizeof(store.race_bytes),&store.race_n)==STN_STORAGE_OK);
    store.acquisitions=0;store.race=1;
    CHECK(rpc(&s,STN_RPC_SUBMIT_WORK,work,n,out,&w)==STN_RPC_STALE);
    CHECK(memcmp(&s.active,&before,sizeof(before))==0 && store.n==store.race_n && memcmp(store.bytes,store.race_bytes,store.n)==0);store.race=0;
    memcpy(store.bytes,disk_before,disk_n);store.n=disk_n;
    for(i=0;i<8;++i){memcpy(changed+i,work,n);q.payload=changed+i;q.length=n;store.fail=1;
        CHECK(stn_mining_handle(&s,&q,out,sizeof(out),&w)==STN_RPC_PROVIDER && w==0);
    }store.fail=0;
    CHECK(rpc(&s,STN_RPC_SUBMIT_WORK,work,n,out,&w)==STN_RPC_OK && w==80 && out[39]==1 && out[79]==4);
    CHECK(stn_storage_load(&c,&provider,snapshot,sizeof(snapshot),&view)==STN_STORAGE_OK && view.count==2 && memcmp(view.state.tip_id,s.active.tip_id,32)==0);
    CHECK(rpc(&s,STN_RPC_SUBMIT_WORK,work,n,out,&w)==STN_RPC_STALE);
    memcpy(child,work+68,364);history[1].bytes=child;history[1].length=364;
    CHECK(rpc(&s,STN_RPC_MINING_TEMPLATE,NULL,0,second_work,&second_n)==STN_RPC_OK);
    /* Real-SHA256 alternative branch: independently fixed nonces 1 and 0. */
    memcpy(alt,child,364);alt[87]=1;alt[159]=1;memcpy(grand,alt,364);grand[79]=2;grand[159]=0;
    CHECK(stn_chain_block_id(alt,364,&c.hash_provider,grand+40)==STN_DATA_OK);
    history[1].bytes=alt;history[2].bytes=grand;history[2].length=364;
    CHECK(stn_storage_adopt(&c,&provider,history,3,&s.workspace,&s.active)==STN_STORAGE_OK);
    CHECK(rpc(&s,STN_RPC_SUBMIT_WORK,second_work,second_n,out,&w)==STN_RPC_STALE);
    /* Rebuild activation from damaged genesis snapshot to a new validated tip. */
    CHECK(stn_storage_encode(&c,history,1,store.bytes,sizeof(store.bytes),&store.n)==STN_STORAGE_OK);
    store.bytes[store.n-1]^=1;
    CHECK(stn_storage_recovery_read(&c,&provider,snapshot,sizeof(snapshot),&view,&recovery)==STN_STORAGE_OK && recovery);
    CHECK(stn_storage_adopt(&c,&provider,history,3,&s.workspace,&s.active)==STN_STORAGE_OK);
    CHECK(rpc(&s,STN_RPC_SUBMIT_WORK,work,n,out,&w)==STN_RPC_STALE);
    s.body=NULL;CHECK(rpc(&s,STN_RPC_MINING_TEMPLATE,NULL,0,out,&w)==STN_RPC_UNAVAILABLE);
    CHECK(rpc(&s,STN_RPC_MINING_CONTEXT,NULL,0,out,&w)==STN_RPC_OK && out[75]==0);
    s.body=body_copy;body_copy[24]^=1;
    CHECK(rpc(&s,STN_RPC_MINING_TEMPLATE,NULL,0,out,&w)==STN_RPC_REJECTED);body_copy[24]^=1;
    CHECK(stn_storage_encode(&c,history,1,store.bytes,sizeof(store.bytes),&store.n)==STN_STORAGE_OK);
    memcpy(changed,work,n);changed[68+159]=2;
    CHECK(rpc(&s,STN_RPC_SUBMIT_WORK,changed,n,out,&w)==STN_RPC_OK);
    activated_difficulty();
    printf("Mining work: %u checks, %u failures.\n",checks,failures);return failures!=0;
}
