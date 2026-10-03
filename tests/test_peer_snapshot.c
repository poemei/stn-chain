#define main mining_fixture_main
#include "test_mining_mempool.c"
#undef main
#include "stn_peer_snapshot.h"
#include "stn_node_service.h"
static unsigned hash_calls;
static stn_data_status counting_hash(void *u,const uint8_t *d,size_t dn,const uint8_t *p,size_t n,uint8_t out[32])
{++hash_calls;return stn_sha256(u,d,dn,p,n,out);}
int main(void)
{
    stn_chain_context c={0},other;stn_pow_policy policy={0};stn_block b={0};
    stn_storage_cache cache={0};stn_storage_view v={0};stn_peer_snapshot *peer=NULL,*sentinel;
    stn_storage_provider provider={NULL,acquire,release,read_store,write_store};
    stn_block_span block;stn_peer_session normal={0},fast={0};
    uint8_t genesis[249],body[81],mapping[77],greeting[68],request[128],a[1024],z[1024],payload[8];
    size_t gn=0,n=0,an=0,zn=0;unsigned calls,i;
    CHECK(destination(1,2,mapping)==77);stn_wire_write(body,4,77);memcpy(body+4,mapping,77);
    b.header.version=3;b.header.network_id[0]=1;b.header.timestamp=1;
    memset(policy.fixed_target,255,32);policy.fixed_target[0]=127;
    memcpy(b.header.reserved_target,policy.fixed_target,32);
    b.header.transaction_count=1;b.header.body_length=81;b.body=body;
    CHECK(stn_block_body_commitment(body,81,1,&hash,b.header.transaction_commitment)==STN_DATA_OK);
    CHECK(stn_block_encode(&b,genesis,sizeof(genesis),&gn)==STN_DATA_OK&&solve(genesis,gn,1));
    c.network_id[0]=1;c.genesis_bytes=genesis;c.genesis_length=gn;c.pow_policy=&policy;c.hash_provider.hash=counting_hash;
    block.bytes=genesis;block.length=gn;
    CHECK(stn_storage_encode(&c,&block,1,disk,sizeof(disk),&disk_length)==STN_STORAGE_OK);
    CHECK(stn_storage_cache_load(&cache,&c,&provider,current,sizeof(current),&v)==STN_STORAGE_OK);
    {
        stn_node_service service={0};stn_rpc_message q={0};unsigned before;
        service.chain=&c;service.blocks=v.blocks;service.count=v.count;
        q.kind=1; q.method=STN_RPC_BLOCK_HEIGHT;q.payload=payload;q.length=8;
        memset(payload,0,8);
        CHECK(stn_node_service_handle(&service,&q,a,sizeof(a),&an)==STN_RPC_OK);
        before=hash_calls;
        CHECK(stn_node_service_handle_validated(&service,&v,&q,z,sizeof(z),&zn)==STN_RPC_OK);
        CHECK(hash_calls==before&&an==zn&&!memcmp(a,z,an));
        q.method=STN_RPC_INFO;q.length=0;
        CHECK(stn_node_service_handle(&service,&q,a,sizeof(a),&an)==STN_RPC_OK);
        before=hash_calls;
        CHECK(stn_node_service_handle_validated(&service,&v,&q,z,sizeof(z),&zn)==STN_RPC_OK);
        CHECK(hash_calls==before&&an==zn&&!memcmp(a,z,an));
        q.method=STN_RPC_BLOCK_HEIGHT;q.length=8;payload[7]=1;
        CHECK(stn_node_service_handle_validated(&service,&v,&q,z,sizeof(z),&zn)==STN_RPC_NOT_FOUND);
        service.count=0;
        CHECK(stn_node_service_handle_validated(&service,&v,&q,z,sizeof(z),&zn)==STN_RPC_PROVIDER);
        service.count=v.count;current[16]^=1;
        CHECK(stn_node_service_handle(&service,&q,z,sizeof(z),&zn)!=STN_RPC_OK);
        current[16]^=1;
    }
    stn_storage_view_release(&v);calls=hash_calls;
    CHECK(stn_peer_snapshot_load(&cache,&c,&provider,&peer)==STN_STORAGE_OK);
    CHECK(hash_calls==calls&&stn_peer_snapshot_count(peer)==1&&!locked);
    memcpy(greeting,c.network_id,32);CHECK(stn_chain_block_id(genesis,gn,&hash,greeting+32)==STN_DATA_OK);
    stn_wire_write(greeting+64,4,1);
    for(i=0;i<4;i++){
        uint16_t type=i==0?STN_PEER_HELLO:i==1?STN_PEER_STATE:i==2?STN_PEER_GET_HEADERS:STN_PEER_GET_BLOCK;
        memset(payload,0,8);stn_wire_write(payload+4,4,1);
        CHECK(stn_peer_encode(type,i==0?greeting:payload,i==0?68:i==1?0:i==2?8:4,request,sizeof(request),&n)==STN_PEER_OK);
        CHECK(stn_peer_serve(&c,&block,1,&normal,request,n,a,sizeof(a),&an)==STN_PEER_OK);
        calls=hash_calls;
        CHECK(stn_peer_snapshot_serve(peer,&fast,request,n,z,sizeof(z),&zn)==STN_PEER_OK);
        CHECK(an==zn&&!memcmp(a,z,an)&&hash_calls-calls==(i==0?1u:0u));
    }
    /* Snapshot owns bytes/scalars, even after cache release and disk corruption. */
    stn_storage_cache_release(&cache);disk[20]^=1;sentinel=peer;
    CHECK(stn_peer_snapshot_load(&cache,&c,&provider,&sentinel)!=STN_STORAGE_OK&&sentinel==peer);
    CHECK(stn_peer_snapshot_serve(peer,&fast,request,n,z,sizeof(z),&zn)==STN_PEER_OK&&!memcmp(a,z,an));
    disk[20]^=1;other=c;other.network_id[0]=2;
    CHECK(stn_peer_snapshot_load(&cache,&other,&provider,&sentinel)!=STN_STORAGE_OK&&sentinel==peer);
    request[0]^=1;
    CHECK(stn_peer_snapshot_serve(peer,&fast,request,n,z,sizeof(z),&zn)==STN_PEER_PROTOCOL);
    stn_peer_snapshot_release(peer);stn_storage_cache_release(&cache);
    disk_length=0;locked=0;
    puts("Peer snapshot: reply equivalence, no replay, owned bytes, corruption/context rejection PASS.");
    return mining_fixture_main();
}
