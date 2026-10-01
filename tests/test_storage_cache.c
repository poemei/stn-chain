/* Copyright (c) 2026 STN-Labz. */
/* Reuse the existing real-PoW in-memory storage fixture. */
#define main mining_fixture_main
#include "test_mining_mempool.c"
#undef main
static unsigned hash_calls;
static stn_data_status counting_hash(void *u,const uint8_t *d,size_t dn,
    const uint8_t *p,size_t n,uint8_t out[32])
{ ++hash_calls;return stn_sha256(u,d,dn,p,n,out); }
int main(void)
{
    stn_chain_context c={0},other;stn_pow_policy policy={0};stn_block b={0};
    stn_storage_cache cache={0};stn_storage_view v={0};
    stn_storage_provider provider={NULL,acquire,release,read_store,write_store};
    stn_block_span block;uint8_t genesis[249],body[81],mapping[77],saved[1024];
    size_t gn=0;unsigned calls;
    CHECK(destination(1,2,mapping)==77);stn_wire_write(body,4,77);memcpy(body+4,mapping,77);
    b.header.version=3;b.header.network_id[0]=1;b.header.timestamp=1;
    memset(policy.fixed_target,255,32);policy.fixed_target[0]=127;
    memcpy(b.header.reserved_target,policy.fixed_target,32);
    b.header.transaction_count=1;b.header.body_length=81;b.body=body;
    CHECK(stn_block_body_commitment(body,81,1,&hash,b.header.transaction_commitment)==STN_DATA_OK);
    CHECK(stn_block_encode(&b,genesis,sizeof(genesis),&gn)==STN_DATA_OK);
    CHECK(solve(genesis,gn,1));c.network_id[0]=1;c.genesis_bytes=genesis;c.genesis_length=gn;
    c.pow_policy=&policy;c.hash_provider.hash=counting_hash;
    block.bytes=genesis;block.length=gn;
    CHECK(stn_storage_encode(&c,&block,1,disk,sizeof(disk),&disk_length)==STN_STORAGE_OK);
    memcpy(saved,disk,disk_length);
    CHECK(stn_storage_cache_load(&cache,&c,&provider,snapshot,sizeof(snapshot),&v)==STN_STORAGE_OK);
    calls=hash_calls;CHECK(v.count==1&&v.state.has_tip);stn_storage_view_release(&v);
    CHECK(stn_storage_cache_load(&cache,&c,&provider,current,sizeof(current),&v)==STN_STORAGE_OK);
    CHECK(hash_calls==calls&&v.blocks[0].bytes==current+16);
    stn_storage_cache_release(&cache);CHECK(v.state.has_tip&&v.blocks[0].bytes[0]=='S');
    stn_storage_view_release(&v);
    CHECK(stn_storage_cache_load(&cache,&c,&provider,snapshot,sizeof(snapshot),&v)==STN_STORAGE_OK);
    stn_storage_view_release(&v);
    locked=1;CHECK(stn_storage_cache_load(&cache,&c,&provider,snapshot,sizeof(snapshot),&v)==STN_STORAGE_BUSY);locked=0;
    CHECK(stn_storage_cache_load(&cache,&c,&provider,snapshot,1,&v)==STN_STORAGE_CAPACITY);
    disk[20]^=1;
    CHECK(stn_storage_cache_load(&cache,&c,&provider,snapshot,sizeof(snapshot),&v)!=STN_STORAGE_OK);
    CHECK(!cache.bytes&&!v.blocks);memcpy(disk,saved,disk_length);
    CHECK(stn_storage_cache_load(&cache,&c,&provider,snapshot,sizeof(snapshot),&v)==STN_STORAGE_OK);
    stn_storage_view_release(&v);other=c;other.network_id[0]=2;
    CHECK(stn_storage_cache_load(&cache,&other,&provider,snapshot,sizeof(snapshot),&v)!=STN_STORAGE_OK);
    CHECK(!cache.bytes&&!v.blocks);stn_storage_cache_release(&cache);
    disk_length=0;locked=0;CHECK(mining_fixture_main()==0);
    printf("Storage cache: identical history reuse, corruption/context rejection, ownership and mining passed.\n");
    return failures!=0;
}

