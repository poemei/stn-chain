/* Included by test_lifecycle.c. Keys and histories are test-only. */
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <process.h>
typedef struct query_socket_test {
    SOCKET socket;unsigned short port;const stn_rpc_service *service;
    const uint8_t *request,*expected;size_t request_length,expected_length;int ok;
} query_socket_test;
static unsigned __stdcall query_socket_client(void *arg)
{
    query_socket_test *t=arg;struct sockaddr_in address={0};uint8_t response[2048];size_t i,n=0;int got;DWORD timeout=10000;
    SOCKET connection=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if(connection==INVALID_SOCKET)return 0;
    setsockopt(connection,SOL_SOCKET,SO_RCVTIMEO,(const char *)&timeout,sizeof(timeout));
    address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);address.sin_port=htons(t->port);
    if(connect(connection,(const struct sockaddr *)&address,sizeof(address))!=0)goto done;
    for(i=0;i<t->request_length;++i)if(send(connection,(const char *)t->request+i,1,0)!=1)goto done;
    while(n<t->expected_length){got=recv(connection,(char *)response+n,(int)(t->expected_length-n),0);if(got<=0)goto done;n+=(size_t)got;}
    t->ok=memcmp(response,t->expected,n)==0;
done:closesocket(connection);return 0;
}
static unsigned __stdcall query_socket_server(void *arg)
{
    query_socket_test *t=arg;uint8_t request[69],response[2048];size_t n=0,w,i;int got;DWORD timeout=10000;
    setsockopt(t->socket,SOL_SOCKET,SO_RCVTIMEO,(const char *)&timeout,sizeof(timeout));
    while(n<t->request_length){got=recv(t->socket,(char *)request+n,(int)(t->request_length-n),0);if(got<=0)goto done;n+=(size_t)got;}
    if(stn_rpc_dispatch(request,n,STN_RPC_READ,t->service,response,sizeof(response),&w)!=STN_RPC_OK)goto done;
    for(i=0;i<w;++i)if(send(t->socket,(const char *)response+i,1,0)!=1)goto done;
    t->ok=1;
done:closesocket(t->socket);return 0;
}
static void query_tcp(const stn_rpc_service *service,const uint8_t *request,size_t rn,const uint8_t *expected,size_t en)
{
    WSADATA ws;SOCKET listener;struct sockaddr_in address={0};int length=sizeof(address);size_t i;
    HANDLE clients[4],servers[4];query_socket_test client[4]={{0}},server[4]={{0}};
    CHECK(WSAStartup(MAKEWORD(2,2),&ws)==0);listener=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);CHECK(listener!=INVALID_SOCKET);
    address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    CHECK(bind(listener,(const struct sockaddr *)&address,sizeof(address))==0);
    CHECK(getsockname(listener,(struct sockaddr *)&address,&length)==0 && listen(listener,4)==0);
    for(i=0;i<4;++i){client[i].port=ntohs(address.sin_port);client[i].request=request;client[i].request_length=rn;client[i].expected=expected;client[i].expected_length=en;clients[i]=(HANDLE)_beginthreadex(NULL,0,query_socket_client,client+i,0,NULL);CHECK(clients[i]!=NULL);}
    for(i=0;i<4;++i){server[i].socket=accept(listener,NULL,NULL);server[i].service=service;server[i].request_length=rn;CHECK(server[i].socket!=INVALID_SOCKET);servers[i]=(HANDLE)_beginthreadex(NULL,0,query_socket_server,server+i,0,NULL);CHECK(servers[i]!=NULL);}
    closesocket(listener);
    for(i=0;i<4;++i){CHECK(WaitForSingleObject(clients[i],20000)==WAIT_OBJECT_0);CHECK(WaitForSingleObject(servers[i],20000)==WAIT_OBJECT_0);CHECK(client[i].ok && server[i].ok);CloseHandle(clients[i]);CloseHandle(servers[i]);}
    CHECK(WSACleanup()==0);
}
static size_t query_block(uint8_t *out,const stn_transaction *tx,size_t count,
    const uint8_t network[32],uint64_t height,const uint8_t parent[32],const stn_hash_provider *hash)
{
    uint8_t transactions[8][1600],body[12832],digest[32];stn_transaction_span spans[8];stn_block b={0};size_t i,n,w;unsigned nonce;
    for(i=0;i<count;++i){CHECK(stn_transaction_encode(tx+i,transactions[i],sizeof(transactions[i]),&n)==STN_DATA_OK);spans[i].bytes=transactions[i];spans[i].length=(uint32_t)n;}
    CHECK(stn_block_body_encode(spans,(uint32_t)count,body,sizeof(body),&n)==STN_DATA_OK);
    b.header.version=3;b.header.height=height;b.header.timestamp=height;memcpy(b.header.network_id,network,32);
    if(parent)memcpy(b.header.previous_hash,parent,32);memset(b.header.reserved_target,255,32);b.header.reserved_target[0]=127;
    b.header.transaction_count=(uint32_t)count;b.header.body_length=(uint32_t)n;b.body=body;
    CHECK(stn_block_body_commitment(body,n,(uint32_t)count,hash,b.header.transaction_commitment)==STN_DATA_OK);
    CHECK(stn_block_encode(&b,out,16000,&w)==STN_DATA_OK);
    for(nonce=0;nonce<65536;++nonce){out[158]=(uint8_t)(nonce>>8);out[159]=(uint8_t)nonce;if(stn_pow_verify(out,w,hash,digest)==STN_DATA_OK)break;}
    CHECK(nonce<65536);return w;
}
static void query_sign(stn_record *r,uint8_t bytes[232],const uint8_t seed[32])
{
    uint8_t statement[256];size_t n;
    CHECK(stn_record_encode(r,bytes,232,&n)==STN_RECORD_OK);
    CHECK(stn_identity_statement(bytes,168,statement,sizeof(statement),&n)==STN_IDENTITY_VALID);
    ed25519_sign_stn(statement,n,seed,r->signer_public_key,r->signature);
    CHECK(stn_record_encode(r,bytes,232,&n)==STN_RECORD_OK);
}
static stn_rpc_code query_call(stn_node_service *node,const uint8_t id[32],uint8_t *answer,size_t capacity,size_t *written)
{
    stn_rpc_message q={1,STN_RPC_GET_ACCEPTED_RECORD,STN_RPC_OK,19,id,32};
    return stn_node_service_handle(node,&q,answer,capacity,written);
}
static void traversal_rpc(const stn_chain_context *c,const stn_block_span *blocks,size_t count,int tcp)
{
    stn_node_service node={0};stn_rpc_service service={&node,stn_node_service_handle};
    stn_rpc_message q={1,5,STN_RPC_OK,700,NULL,0},answer;
    stn_chain_record_match match;stn_chain_cursor cursor,next;
    stn_first_result first;stn_next_result following;
    uint8_t request[128],response[2048],cursor_bytes[45],bad[46]={0};size_t rn,sn;
    node.chain=c;node.blocks=blocks;node.count=count;
    CHECK(STN_RPC_GET_FIRST_ACCEPTED_RECORD==5 && STN_RPC_GET_NEXT_ACCEPTED_RECORD==6 && STN_RPC_END==11 && STN_RPC_DETACHED==12);
    first=stn_chain_first_record(c,blocks,count,&match,&cursor);
    CHECK(stn_rpc_encode(&q,request,sizeof(request),&rn)==STN_RPC_OK && rn==24);
    CHECK(stn_rpc_dispatch(request,rn,STN_RPC_READ,&service,response,sizeof(response),&sn)==STN_RPC_OK);
    CHECK(stn_rpc_decode(response,sn,&answer)==STN_RPC_OK && answer.request_id==700 && answer.code==(first==STN_FIRST_RECORD?STN_RPC_OK:STN_RPC_END));
    if(tcp)query_tcp(&service,request,rn,response,sn);
    if(first==STN_FIRST_RECORD){
        CHECK(stn_chain_cursor_encode(&cursor,cursor_bytes,45)==STN_CURSOR_VALID);
        CHECK(answer.length==125+match.transaction.length && memcmp(answer.payload,match.record_id,32)==0 && memcmp(answer.payload+40,match.block_id,32)==0);
        CHECK(memcmp(answer.payload+32,cursor_bytes+1,8)==0 && memcmp(answer.payload+72,cursor_bytes+41,4)==0 && memcmp(answer.payload+76,cursor_bytes,45)==0);
        CHECK(answer.payload[121]==0 && answer.payload[122]==0 && answer.payload[123]==0 && answer.payload[124]==244 && memcmp(answer.payload+125,match.transaction.bytes,244)==0);
        q.method=6;q.payload=cursor_bytes;q.length=45;q.request_id=701;
        following=stn_chain_next_record(c,blocks,count,&cursor,&match,&next);
        CHECK(stn_rpc_encode(&q,request,sizeof(request),&rn)==STN_RPC_OK);
        CHECK(stn_rpc_dispatch(request,rn,STN_RPC_READ,&service,response,sizeof(response),&sn)==STN_RPC_OK);
        CHECK(stn_rpc_decode(response,sn,&answer)==STN_RPC_OK && answer.code==(following==STN_NEXT_RECORD?STN_RPC_OK:STN_RPC_END));
        if(tcp)query_tcp(&service,request,rn,response,sn);
        if(following==STN_NEXT_RECORD){
            CHECK(memcmp(answer.payload,match.record_id,32)==0 && memcmp(answer.payload+125,match.transaction.bytes,244)==0);
            CHECK(stn_chain_cursor_encode(&next,cursor_bytes,45)==STN_CURSOR_VALID && memcmp(answer.payload+76,cursor_bytes,45)==0);
            CHECK(stn_rpc_encode(&q,request,sizeof(request),&rn)==STN_RPC_OK);
            CHECK(stn_rpc_dispatch(request,rn,STN_RPC_READ,&service,response,sizeof(response),&sn)==STN_RPC_OK);
            CHECK(stn_rpc_decode(response,sn,&answer)==STN_RPC_OK && answer.code==STN_RPC_END);
        }
        cursor_bytes[9]^=1;
        CHECK(stn_rpc_encode(&q,request,sizeof(request),&rn)==STN_RPC_OK);
        CHECK(stn_rpc_dispatch(request,rn,STN_RPC_READ,&service,response,sizeof(response),&sn)==STN_RPC_OK);
        CHECK(stn_rpc_decode(response,sn,&answer)==STN_RPC_OK && answer.code==STN_RPC_DETACHED && answer.length==0);
        cursor_bytes[0]=2;
        CHECK(stn_rpc_encode(&q,request,sizeof(request),&rn)==STN_RPC_OK);
        CHECK(stn_rpc_dispatch(request,rn,STN_RPC_READ,&service,response,sizeof(response),&sn)==STN_RPC_OK);
        CHECK(stn_rpc_decode(response,sn,&answer)==STN_RPC_OK && answer.code==STN_RPC_INVALID);
    }
    q.method=5;q.payload=bad;q.length=1;CHECK(stn_rpc_encode(&q,request,sizeof(request),&rn)==STN_RPC_INVALID);
    q.method=6;q.length=44;CHECK(stn_rpc_encode(&q,request,sizeof(request),&rn)==STN_RPC_INVALID);
    q.length=46;CHECK(stn_rpc_encode(&q,request,sizeof(request),&rn)==STN_RPC_INVALID);
}
static void recovery_rpc(const stn_chain_context *c,const stn_block_span *current,size_t count,
    const stn_block_span *retained,size_t retained_count,const stn_chain_cursor *cursor,int tcp)
{
    stn_node_service node={0};stn_rpc_service service={&node,stn_node_service_handle};
    stn_rpc_message q={1,7,STN_RPC_OK,900,NULL,45},answer;
    uint8_t bytes[45],request[128],response[2048],expected[85];size_t rn,sn,n;unsigned method;
    stn_cursor_ancestor ancestor={0};stn_consumer_recovery_plan plan={0};
    node.chain=c;node.blocks=current;node.count=count;node.retained=retained;node.retained_count=retained_count;
    CHECK(STN_RPC_GET_CURSOR_REORG_STATUS==7 && STN_RPC_GET_CONSUMER_RECOVERY_PLAN==8);
    CHECK(stn_chain_cursor_encode(cursor,bytes,45)==STN_CURSOR_VALID);q.payload=bytes;
    for(method=7;method<=8;++method){
        stn_rpc_code code;n=0;
        if(method==7){
            stn_cursor_reorg_result r=stn_chain_resolve_cursor_reorg(c,current,count,retained,retained_count,cursor,&ancestor);
            code=r==STN_CURSOR_REORG_CURRENT?STN_RPC_CURRENT:r==STN_CURSOR_REORG_COMMON_ANCESTOR?STN_RPC_COMMON_ANCESTOR:STN_RPC_NO_COMMON_ANCESTOR;
            if(code==STN_RPC_COMMON_ANCESTOR)n=40;
        }else{
            stn_consumer_recovery_result r=stn_chain_build_consumer_recovery_plan(c,current,count,retained,retained_count,cursor,&plan);
            code=r==STN_RECOVERY_CURRENT?STN_RPC_CURRENT:r==STN_RECOVERY_FROM_START?STN_RPC_RECOVER_FROM_START:r==STN_RECOVERY_AFTER_CURSOR?STN_RPC_RECOVER_AFTER_CURSOR:STN_RPC_UNAVAILABLE;
            if(code==STN_RPC_RECOVER_FROM_START || code==STN_RPC_RECOVER_AFTER_CURSOR){ancestor=plan.rollback;n=code==STN_RPC_RECOVER_FROM_START?40:85;}
        }
        if(n){
            size_t k;for(k=0;k<8;++k)expected[k]=(uint8_t)(ancestor.height>>(56-8*k));memcpy(expected+8,ancestor.block_id,32);
            if(n==85)CHECK(stn_chain_cursor_encode(&plan.resume,expected+40,45)==STN_CURSOR_VALID);
        }
        q.method=(uint16_t)method;q.length=45;bytes[0]=1;
        CHECK(stn_rpc_encode(&q,request,sizeof(request),&rn)==STN_RPC_OK && rn==69);
        CHECK(stn_rpc_dispatch(request,rn,STN_RPC_READ,&service,response,sizeof(response),&sn)==STN_RPC_OK);
        CHECK(stn_rpc_decode(response,sn,&answer)==STN_RPC_OK && answer.code==code && answer.length==n && answer.request_id==900);
        CHECK(n==0 || memcmp(answer.payload,expected,n)==0);
        if(tcp)query_tcp(&service,request,rn,response,sn);
        if(method==8 && (code==STN_RPC_RECOVER_AFTER_CURSOR || code==STN_RPC_RECOVER_FROM_START)){
            stn_rpc_message continuation={1,5,STN_RPC_OK,901,NULL,0};
            stn_chain_record_match record;stn_chain_cursor resume,following;uint8_t resume_bytes[45];
            if(code==STN_RPC_RECOVER_AFTER_CURSOR){
                memcpy(resume_bytes,answer.payload+40,45);
                CHECK(stn_chain_cursor_decode(resume_bytes,45,&resume)==STN_CURSOR_VALID);
                CHECK(stn_chain_cursor_validate(c,current,count,&resume)==STN_CURSOR_VALID);
                CHECK(stn_chain_next_record(c,current,count,&resume,&record,&following)==STN_NEXT_RECORD);
                continuation.method=6;continuation.payload=resume_bytes;continuation.length=45;
            }else{
                CHECK(answer.length==40);
                CHECK(stn_chain_first_record(c,current,count,&record,&following)==STN_FIRST_RECORD);
            }
            CHECK(stn_rpc_encode(&continuation,request,sizeof(request),&rn)==STN_RPC_OK);
            CHECK(stn_rpc_dispatch(request,rn,STN_RPC_READ,&service,response,sizeof(response),&sn)==STN_RPC_OK);
            CHECK(stn_rpc_decode(response,sn,&answer)==STN_RPC_OK && answer.code==STN_RPC_OK);
            CHECK(memcmp(answer.payload,record.record_id,32)==0 && memcmp(answer.payload+125,record.transaction.bytes,record.transaction.length)==0);
            CHECK(following.height>plan.rollback.height);
            if(tcp)query_tcp(&service,request,rn,response,sn);
        }
        bytes[0]=2;CHECK(stn_rpc_encode(&q,request,sizeof(request),&rn)==STN_RPC_OK);
        CHECK(stn_rpc_dispatch(request,rn,STN_RPC_READ,&service,response,sizeof(response),&sn)==STN_RPC_OK);
        CHECK(stn_rpc_decode(response,sn,&answer)==STN_RPC_OK && answer.code==STN_RPC_INVALID && answer.length==0);
        q.length=44;CHECK(stn_rpc_encode(&q,request,sizeof(request),&rn)==STN_RPC_INVALID);
        q.length=46;CHECK(stn_rpc_encode(&q,request,sizeof(request),&rn)==STN_RPC_INVALID);
    }
}/* 3C: destroy the reconstructed view and rebuild solely from persisted bytes.
 * Copy borrowed transaction bytes before release; no detached pointer is reused. */
static void query_restart(const stn_chain_context *c,publication_store *disk,
    const uint8_t id[32],int found,uint64_t height)
{
    stn_storage_view view={0};stn_chain_record_match before,after,absent;stn_chain_cursor cursor={1,0,{0},0};
    publication_store saved;uint8_t transaction[244],unknown[32]={0};
    stn_next_result next_status;stn_chain_record_match next_before,next_after;
    stn_chain_cursor position_before,position_after;uint8_t next_bytes[244],cursor_bytes[45],after_bytes[45];
    memcpy(&saved,disk,sizeof(saved));
    CHECK(stn_storage_decode(c,disk->bytes,disk->length,&view)==STN_STORAGE_OK);
    traversal_rpc(c,view.blocks,view.count,0);
    CHECK(stn_chain_block_id(view.blocks[0].bytes,view.blocks[0].length,&c->hash_provider,cursor.block_id)==STN_DATA_OK);
    CHECK(stn_chain_cursor_validate(c,view.blocks,view.count,&cursor)==STN_CURSOR_VALID);
    CHECK(stn_chain_lookup_record(c,view.blocks,view.count,id,&before)==STN_DATA_OK && before.found==found);
    CHECK(stn_chain_lookup_record(c,view.blocks,view.count,unknown,&absent)==STN_DATA_OK && !absent.found);
    if(found){
        CHECK(before.height==height && before.transaction.length==sizeof(transaction));
        memcpy(transaction,before.transaction.bytes,sizeof(transaction));
    }
    next_status=stn_chain_next_record(c,view.blocks,view.count,&cursor,&next_before,&position_before);
    {
        stn_chain_record_match first;stn_chain_cursor first_cursor;
        CHECK(stn_chain_first_record(c,view.blocks,view.count,&first,&first_cursor)==STN_FIRST_RECORD);
        CHECK(memcmp(first.record_id,next_before.record_id,32)==0 && first.height==next_before.height && memcmp(first.block_id,next_before.block_id,32)==0);
        CHECK(first.transaction.length==next_before.transaction.length && memcmp(first.transaction.bytes,next_before.transaction.bytes,first.transaction.length)==0 && first_cursor.transaction_position==position_before.transaction_position);
    }
    CHECK(next_status==STN_NEXT_RECORD || next_status==STN_NEXT_END);
    if(next_status==STN_NEXT_RECORD){
        CHECK(next_before.transaction.length==244);
        memcpy(next_bytes,next_before.transaction.bytes,244);
        CHECK(stn_chain_cursor_encode(&position_before,cursor_bytes,45)==STN_CURSOR_VALID);
    }
    stn_storage_view_release(&view);
    CHECK(stn_storage_decode(c,disk->bytes,disk->length,&view)==STN_STORAGE_OK);
    traversal_rpc(c,view.blocks,view.count,0);
    CHECK(stn_chain_cursor_validate(c,view.blocks,view.count,&cursor)==STN_CURSOR_VALID);
    CHECK(stn_chain_lookup_record(c,view.blocks,view.count,id,&after)==STN_DATA_OK && after.found==found);
    CHECK(stn_chain_lookup_record(c,view.blocks,view.count,unknown,&absent)==STN_DATA_OK && !absent.found);
    if(found){
        CHECK(memcmp(before.record_id,after.record_id,32)==0 && before.height==after.height && memcmp(before.block_id,after.block_id,32)==0);
        CHECK(after.transaction.length==sizeof(transaction) && memcmp(transaction,after.transaction.bytes,sizeof(transaction))==0);
    }
    CHECK(stn_chain_next_record(c,view.blocks,view.count,&cursor,&next_after,&position_after)==next_status);
    {
        stn_chain_record_match first;stn_chain_cursor first_cursor;uint8_t first_bytes[45];
        CHECK(stn_chain_first_record(c,view.blocks,view.count,&first,&first_cursor)==STN_FIRST_RECORD);
        CHECK(memcmp(first.record_id,next_before.record_id,32)==0 && first.height==next_before.height && memcmp(first.block_id,next_before.block_id,32)==0);
        CHECK(first.transaction.length==244 && memcmp(first.transaction.bytes,next_bytes,244)==0);
        CHECK(stn_chain_cursor_encode(&first_cursor,first_bytes,45)==STN_CURSOR_VALID && memcmp(first_bytes,cursor_bytes,45)==0);
    }
    if(next_status==STN_NEXT_RECORD){
        CHECK(memcmp(next_before.record_id,next_after.record_id,32)==0 && next_before.height==next_after.height && memcmp(next_before.block_id,next_after.block_id,32)==0);
        CHECK(next_after.transaction.length==244 && memcmp(next_bytes,next_after.transaction.bytes,244)==0);
        CHECK(stn_chain_cursor_encode(&position_after,after_bytes,45)==STN_CURSOR_VALID && memcmp(cursor_bytes,after_bytes,45)==0);
    }
    CHECK(memcmp(&saved,disk,sizeof(saved))==0);
    stn_storage_view_release(&view);
}
static void cursor_codec(void)
{
    stn_chain_cursor c={1,UINT64_C(0x0102030405060708),{0},UINT32_MAX},d;
    uint8_t bytes[45],expected[45]={1,1,2,3,4,5,6,7,8};size_t i;
    for(i=0;i<32;++i)c.block_id[i]=expected[9+i]=(uint8_t)i;
    memset(expected+41,255,4);
    CHECK(stn_chain_cursor_encode(&c,bytes,sizeof(bytes))==STN_CURSOR_VALID && memcmp(bytes,expected,45)==0);
    CHECK(stn_chain_cursor_decode(expected,45,&d)==STN_CURSOR_VALID && d.version==1 && d.height==c.height && d.transaction_position==UINT32_MAX && memcmp(d.block_id,c.block_id,32)==0);
    CHECK(stn_chain_cursor_encode(&d,bytes,45)==STN_CURSOR_VALID && memcmp(bytes,expected,45)==0);
    bytes[0]=2;CHECK(stn_chain_cursor_decode(bytes,45,&d)==STN_CURSOR_MALFORMED);
    CHECK(stn_chain_cursor_decode(expected,44,&d)==STN_CURSOR_MALFORMED);
    CHECK(stn_chain_cursor_decode(expected,46,&d)==STN_CURSOR_MALFORMED);
    CHECK(stn_chain_cursor_decode(NULL,45,&d)==STN_CURSOR_MALFORMED);
    c.version=2;CHECK(stn_chain_cursor_encode(&c,bytes,45)==STN_CURSOR_MALFORMED);
    c.version=1;c.height=0;c.transaction_position=0;
    CHECK(stn_chain_cursor_encode(&c,bytes,45)==STN_CURSOR_VALID && bytes[1]==0 && bytes[8]==0 && bytes[41]==0 && bytes[44]==0);
    CHECK(stn_chain_cursor_decode(bytes,45,&d)==STN_CURSOR_VALID && d.height==0 && d.transaction_position==0);
}
static stn_storage_status ownership_fail_replace(void *user,const uint8_t *bytes,size_t length)
{(void)user;(void)bytes;(void)length;return STN_STORAGE_IO;}
static void reconstruction_equal(const stn_chain_state *a,const stn_chain_state *b)
{
    stn_chain_state left=*a,right=*b;
    stn_lifecycle_state x=*a->lifecycle,y=*b->lifecycle;
    left.lifecycle=NULL;right.lifecycle=NULL;
    CHECK(memcmp(&left,&right,sizeof(left))==0);
    CHECK(x.grant_count==y.grant_count && x.replay.consumed_count==y.replay.consumed_count);
    CHECK(memcmp(x.grant_bytes,y.grant_bytes,x.grant_count*STN_AUTHORITY_GRANT_SIZE)==0);
    CHECK(memcmp(x.replay.consumed,y.replay.consumed,x.replay.consumed_count*STN_REPLAY_ID_SIZE)==0);
    x.grant_bytes=NULL;y.grant_bytes=NULL;x.grant_capacity=0;y.grant_capacity=0;
    x.replay.consumed=NULL;y.replay.consumed=NULL;x.replay.consumed_capacity=0;y.replay.consumed_capacity=0;
    CHECK(memcmp(&x,&y,sizeof(x))==0);
}
static void reconstruction_checks(const stn_chain_context *c,const stn_block_span *base,
    const uint8_t root_seed[32],const uint8_t seed[32],const uint8_t record[232])
{
    static uint8_t bytes[24][16000],encoded[65536],again[65536];
    stn_block_span history[24];stn_chain_state reference={0},reconstructed={0},survivor={0};
    stn_storage_view view={0};stn_record rec;stn_transaction tx[2];stn_chain_report expected,actual;
    uint8_t pub[232],revocation[129],rotation[129],statement[256],sig[64],grant_id[32],rid[32];
    uint8_t replacement_seed[32]={77},replacement[32];size_t i,n,w,prefix,old_clones,new_clones,live=stn_chain_test_live_snapshots();
    unsigned begin=checks;
    history[0]=base[0];history[1]=base[1];
    CHECK(stn_record_decode(record,232,&rec)==STN_RECORD_OK);
    CHECK(stn_chain_initialize(c,&reference)==STN_DATA_OK);
    for(i=0;i<2;++i)CHECK(stn_chain_validate_candidate(c,&reference,history[i].bytes,history[i].length,&reference).acceptance==STN_ACCEPTANCE_UNDER_CONTEXT);
    for(i=2;i<=20;++i){
        rec.nonce[31]=(uint8_t)i;query_sign(&rec,pub,seed);
        tx[0]=(stn_transaction){1,STN_TX_PUBLICATION,pub,232};
        history[i].bytes=bytes[i];history[i].length=query_block(bytes[i],tx,1,c->network_id,i,reference.tip_id,&c->hash_provider);
        CHECK(stn_chain_validate_candidate(c,&reference,history[i].bytes,history[i].length,&reference).acceptance==STN_ACCEPTANCE_UNDER_CONTEXT);
    }
    CHECK(stn_authority_grant_id(reference.lifecycle->grant_bytes,194,&c->hash_provider,grant_id)==STN_DATA_OK);
    CHECK(stn_authority_revocation_statement(c->genesis_authority_roots,grant_id,statement,sizeof(statement),&n)==STN_AUTHORITY_VALID_REVOCATION);
    ed25519_sign_stn(statement,n,root_seed,c->genesis_authority_roots,sig);
    CHECK(stn_authority_revocation_encode(c->genesis_authority_roots,grant_id,sig,revocation,sizeof(revocation),&n)==STN_AUTHORITY_VALID_REVOCATION);
    tx[0]=(stn_transaction){1,STN_TX_AUTHORITY_REVOKE,revocation,129};
    history[21].bytes=bytes[21];history[21].length=query_block(bytes[21],tx,1,c->network_id,21,reference.tip_id,&c->hash_provider);
    CHECK(stn_chain_validate_candidate(c,&reference,history[21].bytes,history[21].length,&reference).acceptance==STN_ACCEPTANCE_UNDER_CONTEXT);
    ed25519_publickey_stn(replacement_seed,replacement);
    CHECK(stn_identity_rotation_statement(rec.signer_public_key,replacement,statement,sizeof(statement),&n)==STN_AUTHORITY_VALID_ROTATION);
    ed25519_sign_stn(statement,n,seed,rec.signer_public_key,sig);
    CHECK(stn_identity_rotation_encode(rec.signer_public_key,replacement,sig,rotation,sizeof(rotation),&n)==STN_AUTHORITY_VALID_ROTATION);
    tx[0]=(stn_transaction){1,STN_TX_IDENTITY_ROTATE,rotation,129};
    history[22].bytes=bytes[22];history[22].length=query_block(bytes[22],tx,1,c->network_id,22,reference.tip_id,&c->hash_provider);
    CHECK(stn_chain_validate_candidate(c,&reference,history[22].bytes,history[22].length,&reference).acceptance==STN_ACCEPTANCE_UNDER_CONTEXT);
    CHECK(stn_chain_state_share(&reference,&survivor)==STN_DATA_OK);
    stn_chain_state_release(&reference);
    for(prefix=1;prefix<=23;++prefix){
        CHECK(stn_chain_initialize(c,&reference)==STN_DATA_OK);
        old_clones=stn_chain_test_clone_count();
        for(i=0;i<prefix;++i)CHECK(stn_chain_validate_candidate(c,&reference,history[i].bytes,history[i].length,&reference).acceptance==STN_ACCEPTANCE_UNDER_CONTEXT);
        old_clones=stn_chain_test_clone_count()-old_clones;
        new_clones=stn_chain_test_clone_count();
        CHECK(stn_chain_reconstruct_history(c,history,prefix,&reconstructed).acceptance==STN_ACCEPTANCE_UNDER_CONTEXT);
        new_clones=stn_chain_test_clone_count()-new_clones;
        reconstruction_equal(&reference,&reconstructed);
        CHECK(new_clones<=old_clones);
        if(prefix==23){CHECK(old_clones==23 && new_clones==2);printf("Reconstruction full clones: %zu -> %zu (23 blocks).\n",old_clones,new_clones);}
        CHECK(stn_lifecycle_check_publication(reference.lifecycle,record,232,&c->hash_provider)==stn_lifecycle_check_publication(reconstructed.lifecycle,record,232,&c->hash_provider));
        CHECK(stn_storage_encode(c,history,prefix,encoded,sizeof(encoded),&n)==STN_STORAGE_OK);
        CHECK(stn_storage_decode(c,encoded,n,&view)==STN_STORAGE_OK);
        reconstruction_equal(&reference,&view.state);
        CHECK(stn_storage_encode(c,view.blocks,view.count,again,sizeof(again),&w)==STN_STORAGE_OK && w==n && memcmp(again,encoded,n)==0);
        stn_storage_view_release(&view);stn_chain_state_release(&reference);stn_chain_state_release(&reconstructed);
        CHECK(stn_chain_test_live_snapshots()==live+1);
    }
    CHECK(survivor.lifecycle->authority.revoked_count==1 && survivor.lifecycle->rotation.rotation_count==1 && survivor.lifecycle->replay.consumed_count==23);
    CHECK(stn_record_id(record,232,&c->hash_provider,rid)==STN_DATA_OK);
    {stn_chain_record_match match;
        CHECK(stn_chain_lookup_record(c,history,23,rid,&match)==STN_DATA_OK && match.found && match.height==1);}
    for(i=0;i<4;++i){
        stn_chain_state unchanged=survivor; /* borrowed sentinel; never released */
        stn_chain_test_fail_after(i);
        actual=stn_chain_reconstruct_history(c,history,23,&unchanged);
        stn_chain_test_fail_after(SIZE_MAX);
        if(i<3){CHECK(actual.acceptance!=STN_ACCEPTANCE_UNDER_CONTEXT && memcmp(&unchanged,&survivor,sizeof(unchanged))==0);}
        else {CHECK(actual.acceptance==STN_ACCEPTANCE_UNDER_CONTEXT);reconstruction_equal(&survivor,&unchanged);stn_chain_state_release(&unchanged);}
        CHECK(stn_chain_test_live_snapshots()==live+1);
    }
    /* A later semantic failure follows a successful transaction in the reused
     * snapshot. Only the private reconstruction is discarded. */
    CHECK(stn_chain_reconstruct_history(c,history,21,&reference).acceptance==STN_ACCEPTANCE_UNDER_CONTEXT);
    rec.nonce[31]=99;query_sign(&rec,pub,seed);rotation[128]^=1;
    tx[0]=(stn_transaction){1,STN_TX_PUBLICATION,pub,232};tx[1]=(stn_transaction){1,STN_TX_IDENTITY_ROTATE,rotation,129};
    history[21].bytes=bytes[23];history[21].length=query_block(bytes[23],tx,2,c->network_id,21,reference.tip_id,&c->hash_provider);
    expected=stn_chain_validate_candidate(c,&reference,history[21].bytes,history[21].length,&reconstructed);
    actual=stn_chain_reconstruct_history(c,history,22,&reconstructed);
    CHECK(expected.acceptance==STN_ACCEPTANCE_REJECTED && actual.acceptance==expected.acceptance && actual.reason==expected.reason && actual.detail==expected.detail && actual.failing_index==21);
    CHECK(reconstructed.lifecycle==NULL && reference.lifecycle->replay.consumed_count==21);
    stn_chain_state_release(&reference);
    encoded[n-1]^=1;CHECK(stn_storage_decode(c,encoded,n,&view)==STN_STORAGE_FORMAT);
    CHECK(view.state.lifecycle==NULL && stn_chain_test_live_snapshots()==live+1);
    stn_chain_state_release(&survivor);
    CHECK(stn_chain_test_live_snapshots()==live);
    printf("Reconstruction copy reduction: %u targeted checks.\n",checks-begin);
}

static void ownership_checks(const stn_chain_context *c,const stn_block_span *blocks)
{
    size_t baseline=stn_chain_test_live_snapshots(),cycle,n;uint8_t saved[8192],encoded[8192],scratch[8192],next[8192],rejected[16000];
    publication_store disk={0};stn_storage_provider provider={&disk,publication_lock,publication_unlock,publication_read,publication_replace};
    stn_storage_workspace workspace={scratch,sizeof(scratch),next,sizeof(next)};
    for(cycle=0;cycle<8;++cycle){
        stn_chain_state empty={0},shared={0},active={0},unchanged;stn_storage_view view={0};
        stn_reorg_plan plan={0};stn_transaction tx;stn_block child;stn_chain_record_match match;stn_chain_cursor position,following;
        CHECK(stn_chain_initialize(c,&empty)==STN_DATA_OK);
        CHECK(stn_chain_test_references(&empty,SIZE_MAX)==1);
        CHECK(stn_chain_state_share(&empty,&shared)==STN_DATA_CAPACITY && shared.lifecycle==NULL);
        CHECK(stn_chain_test_references(&empty,0)==SIZE_MAX);
        CHECK(stn_chain_state_share(&empty,&shared)==STN_DATA_ARGUMENT && shared.lifecycle==NULL);
        CHECK(stn_chain_test_references(&empty,1)==0);
        CHECK(stn_chain_state_share(&empty,&shared)==STN_DATA_OK);
        stn_chain_state_release(&empty);
        CHECK(shared.lifecycle!=NULL && stn_chain_test_live_snapshots()==baseline+1);
        stn_chain_state_release(&shared);stn_chain_state_release(&shared);
        CHECK(stn_chain_test_live_snapshots()==baseline);
        CHECK(stn_storage_encode(c,blocks,1,disk.bytes,sizeof(disk.bytes),&disk.length)==STN_STORAGE_OK);
        memcpy(saved,disk.bytes,disk.length);
        CHECK(stn_storage_load(c,&provider,scratch,sizeof(scratch),&view)==STN_STORAGE_OK);
        CHECK(stn_chain_state_share(&view.state,&active)==STN_DATA_OK);
        stn_storage_view_release(&view);
        CHECK(active.lifecycle!=NULL && active.lifecycle->grant_count==1);
        unchanged=active; /* BORROW for failure comparison only */
        provider.replace=ownership_fail_replace;
        CHECK(stn_storage_extend(c,&provider,blocks[1].bytes,blocks[1].length,&workspace,&active)==STN_STORAGE_IO);
        CHECK(memcmp(&active,&unchanged,sizeof(active))==0 && memcmp(saved,disk.bytes,disk.length)==0);
        CHECK(stn_chain_test_live_snapshots()==baseline+1);
        provider.replace=publication_replace;
        CHECK(stn_storage_extend(c,&provider,blocks[1].bytes,blocks[1].length,&workspace,&active)==STN_STORAGE_OK);
        CHECK(active.height==1 && active.lifecycle->replay.consumed_count==2 && stn_chain_test_live_snapshots()==baseline+1);
        CHECK(stn_storage_load(c,&provider,scratch,sizeof(scratch),&view)==STN_STORAGE_OK);
        CHECK(stn_storage_encode(c,view.blocks,view.count,encoded,sizeof(encoded),&n)==STN_STORAGE_OK && n==disk.length && memcmp(encoded,disk.bytes,n)==0);
        CHECK(stn_chain_first_record(c,view.blocks,view.count,&match,&position)==STN_FIRST_RECORD);
        CHECK(stn_chain_next_record(c,view.blocks,view.count,&position,&match,&following)==STN_NEXT_END);
        CHECK(stn_fork_evaluate_history(c,view.blocks,view.count,blocks,1,&plan).result==STN_FORK_CURRENT);
        stn_reorg_plan_release(&plan);
        CHECK(stn_block_decode(blocks[1].bytes,blocks[1].length,&child)==STN_DATA_OK);
        CHECK(stn_transaction_decode(child.body+4,child.header.body_length-4,&tx)==STN_DATA_OK);
        n=query_block(rejected,&tx,1,c->network_id,2,active.tip_id,&c->hash_provider);
        CHECK(stn_chain_validate_candidate(c,&active,rejected,n,&empty).acceptance==STN_ACCEPTANCE_REJECTED);
        CHECK(empty.lifecycle==NULL);
        stn_storage_view_release(&view);stn_chain_state_release(&active);
        CHECK(stn_chain_test_live_snapshots()==baseline);
        {
            size_t budget;
            for(budget=0;budget<4;++budget){
                stn_storage_status loaded;size_t live=stn_chain_test_live_snapshots();
                stn_chain_test_fail_after(budget);
                loaded=stn_storage_load(c,&provider,scratch,sizeof(scratch),&view);
                stn_chain_test_fail_after(SIZE_MAX);
                CHECK(loaded==STN_STORAGE_OK || loaded==STN_STORAGE_VALIDATION);
                stn_storage_view_release(&view);
                CHECK(stn_chain_test_live_snapshots()==live);
            }
        }
        disk.bytes[disk.length-1]^=1;
        CHECK(stn_storage_load(c,&provider,scratch,sizeof(scratch),&view)==STN_STORAGE_FORMAT);
        CHECK(stn_chain_test_live_snapshots()==baseline);
    }
}

static void record_queries(void)
{
    static uint8_t blocks[12][16000],branch[3][16000];
    uint8_t root_seed[32]={21},seed[32]={22},issuer[32],producer[32],action[32],context[32],evidence[97],grant[194],statement[256],sig[64];
    uint8_t record[232],unsigned_record[232],bad[232],other[232],payload[52]={0},rid[32],other_id[32],txid[32],encoded_tx[244],reply[2048],before[2048];
    stn_hash_provider hash={stn_sha256,NULL};stn_pow_policy pow;stn_chain_context c={0};stn_chain_state state={0},prior={0};
    stn_block_span spans[12],replacement[3];stn_transaction tx[4];stn_record r={0};stn_node_service node={0};
    size_t n,w,len,i;unsigned start=checks;stn_storage_view restored={0};
    size_t ownership_baseline=stn_chain_test_live_snapshots();
    cursor_codec();
    ed25519_publickey_stn(root_seed,issuer);ed25519_publickey_stn(seed,producer);
    payload[1]=1;payload[9]=5;payload[10]=1;payload[12]=1;payload[13]='a';payload[15]=1;payload[16]='b';payload[18]=1;payload[19]='c';payload[20]=1;
    r.version=1;r.type=1;r.network_id[0]=1;memcpy(r.signer_public_key,producer,32);r.nonce[0]=1;r.payload=payload;r.payload_length=52;
    query_sign(&r,record,seed);memcpy(unsigned_record,record,232);memset(unsigned_record+168,0,64);
    CHECK(stn_record_id(record,232,&hash,rid)==STN_DATA_OK);
    CHECK(stn_record_id(unsigned_record,232,&hash,other_id)==STN_DATA_OK && memcmp(rid,other_id,32)==0);
    CHECK(stn_record_publication_tokens(record,232,&hash,action,context)==STN_DATA_OK);
    CHECK(stn_authority_evidence_encode(producer,action,context,evidence,sizeof(evidence),&w)==STN_AUTHORITY_AUTHORIZED);
    CHECK(stn_authority_grant_statement(issuer,evidence,statement,sizeof(statement),&w)==STN_AUTHORITY_VALID_GRANT);
    ed25519_sign_stn(statement,w,root_seed,issuer,sig);
    CHECK(stn_authority_grant_encode(issuer,evidence,sig,grant,sizeof(grant),&w)==STN_AUTHORITY_VALID_GRANT);
    /* Exact qualified legacy genesis; production activation is height 130. */
    {
        /* Use the existing runtime fixture's actual body commitment. */
        uint8_t *g=blocks[0];stn_block b;memset(g,0,364);memcpy(g,"STNB",4);g[5]=3;g[8]=1;g[163]=1;g[167]=196;g[171]=192;
        memcpy(g+172,"STNT",4);g[177]=1;g[179]=1;g[183]=180;memcpy(g+184,"STNR",4);g[189]=1;g[191]=1;g[192]=1;g[256]=3;
        memset(g+120,255,32);g[120]=127;
        CHECK(stn_block_decode(g,364,&b)==STN_DATA_OK);
        CHECK(stn_block_body_commitment(g+168,196,1,&hash,g+88)==STN_DATA_OK);
        spans[0].bytes=g;spans[0].length=364;
    }
    memset(pow.fixed_target,255,32);pow.fixed_target[0]=127;
    c.network_id[0]=1;c.genesis_bytes=blocks[0];c.genesis_length=364;c.hash_provider=hash;c.pow_policy=&pow;
    c.genesis_authority_roots=issuer;c.genesis_authority_root_count=1;c.genesis_initial_identities=producer;c.genesis_initial_identity_count=1;
    node.chain=&c;node.blocks=spans;node.count=1;
    CHECK(stn_chain_initialize(&c,&state)==STN_DATA_OK && state.publication_activation_height==130);
    CHECK(stn_chain_validate_candidate(&c,&state,blocks[0],364,&state).acceptance==STN_ACCEPTANCE_UNDER_CONTEXT);
    CHECK(query_call(&node,rid,reply,sizeof(reply),&w)==STN_RPC_NOT_FOUND && w==0);
    /* Before grant, after grant, unsigned, malformed payload, valid, replay. */
    memcpy(bad,record,232);bad[117]=2;CHECK(stn_record_id(bad,232,&hash,other_id)==STN_DATA_OK);
    for(i=1;i<=6;++i){
        tx[0].version=1;tx[0].type=i==2?STN_TX_AUTHORITY_GRANT:STN_TX_PUBLICATION;
        tx[0].record_bytes=i==2?grant:i==3?unsigned_record:i==4?bad:record;tx[0].record_length=i==2?194:232;
        n=query_block(blocks[i],tx,1,c.network_id,i,state.tip_id,&hash);spans[i].bytes=blocks[i];spans[i].length=n;
        CHECK(stn_chain_validate_candidate(&c,&state,blocks[i],n,&state).acceptance==STN_ACCEPTANCE_UNDER_CONTEXT);
        node.count=i+1;
        if(i<5)CHECK(query_call(&node,rid,reply,sizeof(reply),&w)==STN_RPC_NOT_FOUND && w==0);
        else CHECK(query_call(&node,rid,reply,sizeof(reply),&w)==STN_RPC_OK && w==320 && reply[39]==5);
    }
    /* 3B: direct calls reuse the unauthorized/unsigned/valid/repeated history.
     * No adapter or socket participates in these eligibility assertions. */
    {
        stn_chain_record_match match;
        CHECK(stn_chain_lookup_record(&c,spans,5,rid,&match)==STN_DATA_OK && !match.found);
        CHECK(stn_chain_lookup_record(&c,spans,6,rid,&match)==STN_DATA_OK && match.found && match.height==5);
        CHECK(stn_chain_lookup_record(&c,spans,7,rid,&match)==STN_DATA_OK && match.found && match.height==5);
        CHECK(match.transaction.bytes==blocks[5]+172 && match.transaction.length==244);
    }
    {
        stn_chain_cursor input={1,0,{0},0},next,saved;stn_chain_record_match match;
        CHECK(stn_chain_block_id(spans[0].bytes,spans[0].length,&hash,input.block_id)==STN_DATA_OK);
        CHECK(stn_chain_next_record(&c,spans,7,&input,&match,&next)==STN_NEXT_RECORD);
        CHECK(next.height==5 && next.transaction_position==0 && memcmp(match.record_id,rid,32)==0);
        CHECK(stn_chain_cursor_validate(&c,spans,7,&next)==STN_CURSOR_VALID);
        CHECK(stn_chain_first_record(&c,spans,1,&match,&next)==STN_FIRST_END);
        CHECK(stn_chain_first_record(&c,spans,5,&match,&next)==STN_FIRST_END);
        CHECK(stn_chain_first_record(&c,spans,7,&match,&next)==STN_FIRST_RECORD && next.height==5 && next.transaction_position==0);
        CHECK(memcmp(match.record_id,rid,32)==0 && match.transaction.length==244);
        CHECK(stn_chain_cursor_validate(&c,spans,7,&next)==STN_CURSOR_VALID);
        saved=next;
        /* Same semantic witness at height 6 remains replay-ineligible. */
        CHECK(stn_chain_next_record(&c,spans,7,&saved,&match,&next)==STN_NEXT_END);
        input.version=2;CHECK(stn_chain_next_record(&c,spans,7,&input,&match,&next)==STN_NEXT_MALFORMED);
        input.version=1;input.block_id[0]^=1;
        CHECK(stn_chain_next_record(&c,spans,7,&input,&match,&next)==STN_NEXT_DETACHED);
    }
    CHECK(stn_chain_state_share(&state,&prior)==STN_DATA_OK);
    CHECK(query_call(&node,other_id,reply,sizeof(reply),&w)==STN_RPC_NOT_FOUND);
    tx[0].version=1;tx[0].type=1;tx[0].record_bytes=record;tx[0].record_length=232;
    CHECK(stn_transaction_encode(tx,encoded_tx,sizeof(encoded_tx),&len)==STN_DATA_OK);
    CHECK(stn_transaction_id(encoded_tx,len,&hash,txid)==STN_DATA_OK);
    CHECK(query_call(&node,txid,reply,sizeof(reply),&w)==STN_RPC_NOT_FOUND);
    CHECK(query_call(&node,rid,reply,sizeof(reply),&w)==STN_RPC_OK && memcmp(reply+76,encoded_tx,len)==0);
    CHECK(stn_chain_block_id(blocks[5],spans[5].length,&hash,other_id)==STN_DATA_OK && memcmp(reply+40,other_id,32)==0);
    {
        stn_chain_record_match match,saved;
        CHECK(stn_chain_lookup_record(node.chain,node.blocks,node.count,rid,&match)==STN_DATA_OK && match.found);
        CHECK(memcmp(match.record_id,reply,32)==0 && match.height==5 && memcmp(match.block_id,reply+40,32)==0);
        CHECK(match.transaction.length==len && memcmp(match.transaction.bytes,reply+76,len)==0);
        saved=match;
        CHECK(stn_chain_lookup_record(node.chain,node.blocks,node.count,NULL,&match)==STN_DATA_ARGUMENT && memcmp(&match,&saved,sizeof(match))==0);
        CHECK(stn_chain_lookup_record(node.chain,node.blocks,node.count,other_id,&match)==STN_DATA_OK && !match.found);
    }
    memcpy(before,reply,w);CHECK(query_call(&node,rid,reply,319,&w)==STN_RPC_CAPACITY && w==0 && memcmp(before,reply,320)==0);
    CHECK(state.lifecycle==prior.lifecycle && state.lifecycle->replay.consumed_count==1);
    /* Same replay discriminator with different semantics is not queryable. */
    r.issued_at=1;query_sign(&r,other,seed);CHECK(stn_record_id(other,232,&hash,other_id)==STN_DATA_OK);
    tx[0].record_bytes=other;n=query_block(blocks[7],tx,1,c.network_id,7,state.tip_id,&hash);spans[7].bytes=blocks[7];spans[7].length=n;node.count=8;
    CHECK(query_call(&node,other_id,reply,sizeof(reply),&w)==STN_RPC_NOT_FOUND);
    /* A later accepted revocation excludes new records, never old eligibility. */
    {
        uint8_t revocation[129],rotation[129],new_key[32],new_seed[32]={23};
        CHECK(stn_authority_grant_id(grant,194,&hash,other_id)==STN_DATA_OK);
        CHECK(stn_authority_revocation_statement(issuer,other_id,statement,sizeof(statement),&w)==STN_AUTHORITY_VALID_REVOCATION);
        ed25519_sign_stn(statement,w,root_seed,issuer,sig);
        CHECK(stn_authority_revocation_encode(issuer,other_id,sig,revocation,sizeof(revocation),&w)==STN_AUTHORITY_VALID_REVOCATION);
        for(i=0;i<2;++i){
            stn_chain_state_release(&state);CHECK(stn_chain_state_share(&prior,&state)==STN_DATA_OK);
            if(i){ed25519_publickey_stn(new_seed,new_key);CHECK(stn_identity_rotation_statement(producer,new_key,statement,sizeof(statement),&w)==STN_AUTHORITY_VALID_ROTATION);ed25519_sign_stn(statement,w,seed,producer,sig);CHECK(stn_identity_rotation_encode(producer,new_key,sig,rotation,sizeof(rotation),&w)==STN_AUTHORITY_VALID_ROTATION);}
            tx[0].type=i?STN_TX_IDENTITY_ROTATE:STN_TX_AUTHORITY_REVOKE;tx[0].record_bytes=i?rotation:revocation;tx[0].record_length=129;
            n=query_block(blocks[7],tx,1,c.network_id,7,state.tip_id,&hash);spans[7].length=n;
            CHECK(stn_chain_validate_candidate(&c,&state,blocks[7],n,&state).acceptance==STN_ACCEPTANCE_UNDER_CONTEXT);
            r.nonce[0]=2;query_sign(&r,other,seed);CHECK(stn_record_id(other,232,&hash,other_id)==STN_DATA_OK);
            tx[0].type=1;tx[0].record_bytes=other;tx[0].record_length=232;
            n=query_block(blocks[8],tx,1,c.network_id,8,state.tip_id,&hash);spans[8].bytes=blocks[8];spans[8].length=n;node.count=9;
            CHECK(query_call(&node,other_id,reply,sizeof(reply),&w)==STN_RPC_NOT_FOUND);
            CHECK(query_call(&node,rid,reply,sizeof(reply),&w)==STN_RPC_OK && reply[39]==5);
        }
    }
    {
        /* A legacy qualifying publication deliberately uses the nonce of a
         * later accepted rotation. Query replay must not reject that historical
         * rotation or change its authoritative lineage effect. */
        uint8_t replacement_key[32],rotation_seed[32]={24},rotation[129],collision_record[232],unknown[32]={0};stn_record collision=r;
        ed25519_publickey_stn(rotation_seed,replacement_key);
        CHECK(stn_identity_rotation_statement(producer,replacement_key,statement,sizeof(statement),&w)==STN_AUTHORITY_VALID_ROTATION);
        CHECK(stn_lifecycle_replay_nonce(STN_TX_IDENTITY_ROTATE,statement,w,&hash,collision.nonce)==STN_LIFECYCLE_OK);
        ed25519_sign_stn(statement,w,seed,producer,sig);
        CHECK(stn_identity_rotation_encode(producer,replacement_key,sig,rotation,sizeof(rotation),&w)==STN_AUTHORITY_VALID_ROTATION);
        query_sign(&collision,collision_record,seed);CHECK(stn_record_id(collision_record,232,&hash,other_id)==STN_DATA_OK);
        stn_chain_state_release(&state);CHECK(stn_chain_state_share(&prior,&state)==STN_DATA_OK);tx[0].type=1;tx[0].record_bytes=collision_record;tx[0].record_length=232;
        spans[7].length=query_block(blocks[7],tx,1,c.network_id,7,state.tip_id,&hash);
        CHECK(stn_chain_validate_candidate(&c,&state,blocks[7],spans[7].length,&state).acceptance==STN_ACCEPTANCE_UNDER_CONTEXT);
        tx[0].type=STN_TX_IDENTITY_ROTATE;tx[0].record_bytes=rotation;tx[0].record_length=129;
        spans[8].length=query_block(blocks[8],tx,1,c.network_id,8,state.tip_id,&hash);node.count=9;
        CHECK(query_call(&node,unknown,reply,sizeof(reply),&w)==STN_RPC_NOT_FOUND);
        CHECK(query_call(&node,other_id,reply,sizeof(reply),&w)==STN_RPC_OK && reply[39]==7);
        {
            stn_chain_cursor input={1,5,{0},0},next;stn_chain_record_match match;
            CHECK(stn_chain_block_id(spans[5].bytes,spans[5].length,&hash,input.block_id)==STN_DATA_OK);
            CHECK(stn_chain_next_record(&c,spans,9,&input,&match,&next)==STN_NEXT_RECORD && next.height==7 && memcmp(match.record_id,other_id,32)==0);
            CHECK(stn_chain_next_record(&c,spans,9,&next,&match,&input)==STN_NEXT_END);
        }
    }
    node.count=7;
    {
        publication_store disk={0};uint8_t scratch[8192];
        CHECK(stn_storage_encode(&c,spans,node.count,disk.bytes,sizeof(disk.bytes),&disk.length)==STN_STORAGE_OK);
        CHECK(stn_storage_decode(&c,disk.bytes,disk.length,&restored)==STN_STORAGE_OK);
        node.blocks=restored.blocks;node.count=restored.count;
        CHECK(query_call(&node,rid,reply,sizeof(reply),&w)==STN_RPC_OK && memcmp(reply,before,320)==0);
        stn_storage_view_release(&restored);node.blocks=spans;(void)scratch;
    }
    /* Independent new genesis: grant, bad witness, good witness, duplicate,
     * all at height zero. This also qualifies canonical transaction order. */
    tx[0].version=1;tx[0].type=STN_TX_AUTHORITY_GRANT;tx[0].record_bytes=grant;tx[0].record_length=194;
    tx[1].version=1;tx[1].type=1;tx[1].record_bytes=unsigned_record;tx[1].record_length=232;
    tx[2]=tx[1];tx[2].record_bytes=record;tx[3]=tx[2];memcpy(bad,record,232);bad[231]^=1;tx[3].record_bytes=bad;
    n=query_block(branch[0],tx,4,c.network_id,0,NULL,&hash);replacement[0].bytes=branch[0];replacement[0].length=n;
    c.genesis_bytes=branch[0];c.genesis_length=n;node.blocks=replacement;node.count=1;
    CHECK(query_call(&node,rid,reply,sizeof(reply),&w)==STN_RPC_OK && reply[39]==0 && memcmp(reply+76,encoded_tx,244)==0);
    {
        stn_chain_cursor input={1,0,{0},0},next;stn_chain_record_match match;
        CHECK(stn_chain_block_id(replacement[0].bytes,replacement[0].length,&hash,input.block_id)==STN_DATA_OK);
        CHECK(stn_chain_next_record(&c,replacement,1,&input,&match,&next)==STN_NEXT_RECORD && next.height==0 && next.transaction_position==2);
        CHECK(stn_chain_first_record(&c,replacement,1,&match,&next)==STN_FIRST_RECORD && next.height==0 && next.transaction_position==2);
        CHECK(memcmp(match.transaction.bytes,encoded_tx,244)==0 && match.transaction.length==244);
        CHECK(stn_chain_cursor_validate(&c,replacement,1,&next)==STN_CURSOR_VALID);
        CHECK(stn_chain_next_record(&c,replacement,1,&next,&match,&input)==STN_NEXT_END);
    }
    /* Wire shape and response integrity are independently enforced. */
    {
        uint8_t request[1024],response[2048],key64[64]={0};stn_rpc_message q={1,STN_RPC_GET_ACCEPTED_RECORD,STN_RPC_OK,42,rid,32},answer;
        stn_rpc_service service={&node,stn_node_service_handle};size_t rn,sn;
        CHECK(stn_rpc_encode(&q,request,sizeof(request),&rn)==STN_RPC_OK);
        CHECK(stn_rpc_dispatch(request,rn,STN_RPC_READ,&service,response,sizeof(response),&sn)==STN_RPC_OK);
        CHECK(stn_rpc_decode(response,sn,&answer)==STN_RPC_OK && answer.code==STN_RPC_OK && answer.request_id==42 && answer.length==320);
        {
            stn_chain_record_match match;uint64_t height=0;size_t k;
            CHECK(stn_chain_lookup_record(&c,replacement,1,rid,&match)==STN_DATA_OK && match.found);
            for(k=0;k<8;++k)height=(height<<8)|answer.payload[32+k];
            CHECK(memcmp(match.record_id,answer.payload,32)==0 && match.height==height);
            CHECK(memcmp(match.block_id,answer.payload+40,32)==0);
            CHECK(match.transaction.length==answer.length-76 && memcmp(match.transaction.bytes,answer.payload+76,match.transaction.length)==0);
        }
        query_tcp(&service,request,rn,response,sn);
        response[24]^=1;CHECK(stn_rpc_decode(response,sn,&answer)==STN_RPC_INVALID);response[24]^=1;
        response[24+75]^=1;CHECK(stn_rpc_decode(response,sn,&answer)==STN_RPC_INVALID);response[24+75]^=1;
        CHECK(stn_rpc_decode(response,sn-1,&answer)==STN_RPC_INVALID);
        for(i=0;i<65;++i){if(i==32)continue;q.payload=key64;q.length=i;CHECK(stn_rpc_encode(&q,request,sizeof(request),&rn)==STN_RPC_INVALID);}
        q.payload=rid;q.length=32;CHECK(stn_rpc_encode(&q,request,sizeof(request),&rn)==STN_RPC_OK);
        request[23]=31;CHECK(stn_rpc_dispatch(request,rn-1,STN_RPC_READ,&service,response,sizeof(response),&sn)==STN_RPC_OK);
        CHECK(stn_rpc_decode(response,sn,&answer)==STN_RPC_OK && answer.code==STN_RPC_INVALID && answer.length==0);
        request[23]=32;CHECK(stn_rpc_dispatch(request,rn,STN_RPC_SUBMISSION,&service,response,sizeof(response),&sn)==STN_RPC_OK);
        CHECK(stn_rpc_decode(response,sn,&answer)==STN_RPC_OK && answer.code==STN_RPC_FORBIDDEN);
    }
    /* Production activation: pending exclusion, failed candidate, real storage
     * adoption removing, adding and relocating the same semantic record. */
    {
        static uint8_t competing[4][16000];stn_block_span candidates[4];
        publication_store disk={0};uint8_t current_scratch[8192],next_scratch[8192];
        stn_storage_provider storage={&disk,publication_lock,publication_unlock,publication_read,publication_replace};
        stn_storage_workspace workspace={current_scratch,sizeof(current_scratch),next_scratch,sizeof(next_scratch)};
        stn_chain_state active={0},base={0},branch_state={0};stn_pending pending={0};stn_validation_report report;
        tx[0].version=1;tx[0].type=STN_TX_AUTHORITY_GRANT;tx[0].record_bytes=grant;tx[0].record_length=194;
        n=query_block(branch[0],tx,1,c.network_id,0,NULL,&hash);replacement[0].length=n;
        c.genesis_length=n;node.count=1;
        CHECK(stn_chain_initialize(&c,&active)==STN_DATA_OK && active.publication_activation_height==1);
        CHECK(stn_chain_validate_candidate(&c,&active,branch[0],n,&active).acceptance==STN_ACCEPTANCE_UNDER_CONTEXT);CHECK(stn_chain_state_share(&active,&base)==STN_DATA_OK);
        restored.blocks=replacement;restored.count=1;restored.state=active; /* BORROW until memset; no view release */
        CHECK(stn_pending_admit(&pending,record,232,NULL,&restored,&hash,&report,other_id)==STN_PENDING_ACCEPTED);
        CHECK(query_call(&node,rid,reply,sizeof(reply),&w)==STN_RPC_NOT_FOUND && pending.count==1);
        {
            stn_mining_service service={0};stn_rpc_message q={1,STN_RPC_GET_ACCEPTED_RECORD,STN_RPC_OK,1,rid,32};
            CHECK(stn_storage_encode(&c,replacement,1,disk.bytes,sizeof(disk.bytes),&disk.length)==STN_STORAGE_OK);
            service.chain=&c;service.storage=&storage;service.pending=&pending;service.snapshot=current_scratch;service.snapshot_capacity=sizeof(current_scratch);service.template_bytes=next_scratch;service.template_capacity=sizeof(next_scratch);
            CHECK(stn_mining_handle(&service,&q,reply,sizeof(reply),&w)==STN_RPC_NOT_FOUND && pending.count==1);
        }
        /* Deep snapshots include caller-owned bytes, not just pointer equality. */
        {
            stn_chain_record_match match;stn_chain_state saved_active;
            stn_lifecycle_state saved_lifecycle;stn_pending saved_pending;
            stn_chain_context saved_context;stn_block_span saved_span;
            uint8_t saved_block[16000],saved_grant[194],saved_replay[64],saved_tx[244],unknown[32]={0};
            memcpy(&saved_active,&active,sizeof(active));
            memcpy(&saved_lifecycle,active.lifecycle,sizeof(saved_lifecycle));
            memcpy(&saved_pending,&pending,sizeof(pending));
            memcpy(&saved_context,&c,sizeof(c));memcpy(&saved_span,replacement,sizeof(saved_span));
            CHECK(active.lifecycle->grant_count==1 && active.lifecycle->replay.consumed_count==1 && pending.count==1 && pending.entries[0].length==244);
            memcpy(saved_block,replacement[0].bytes,replacement[0].length);
            memcpy(saved_grant,active.lifecycle->grant_bytes,194);memcpy(saved_replay,active.lifecycle->replay.consumed,64);
            memcpy(saved_tx,pending.entries[0].transaction,244);
            {
                stn_chain_cursor cursor={1,0,{0},0};
                CHECK(stn_chain_block_id(replacement[0].bytes,replacement[0].length,&hash,cursor.block_id)==STN_DATA_OK);
                CHECK(stn_chain_cursor_validate(&c,replacement,1,&cursor)==STN_CURSOR_VALID);
                {
                    stn_chain_record_match pending_match;stn_chain_cursor next;
                    CHECK(stn_chain_next_record(&c,replacement,1,&cursor,&pending_match,&next)==STN_NEXT_END);
                    CHECK(stn_chain_first_record(&c,replacement,1,&pending_match,&next)==STN_FIRST_END);
                }
                {
                    stn_cursor_ancestor ancestor;
                    CHECK(stn_chain_resolve_cursor_reorg(&c,replacement,1,NULL,0,&cursor,&ancestor)==STN_CURSOR_REORG_CURRENT);
                    recovery_rpc(&c,replacement,1,NULL,0,&cursor,0);
                    {
                        stn_consumer_recovery_plan plan;
                        CHECK(stn_chain_build_consumer_recovery_plan(&c,replacement,1,NULL,0,&cursor,&plan)==STN_RECOVERY_CURRENT);
                    }
                }
                cursor.block_id[0]^=1;
                CHECK(stn_chain_cursor_validate(&c,replacement,1,&cursor)==STN_CURSOR_DETACHED);
                cursor.version=2;
                CHECK(stn_chain_cursor_validate(&c,replacement,1,&cursor)==STN_CURSOR_MALFORMED);
            }
            CHECK(stn_chain_lookup_record(&c,replacement,1,rid,&match)==STN_DATA_OK && !match.found);
            CHECK(stn_chain_lookup_record(&c,replacement,1,unknown,&match)==STN_DATA_OK && !match.found);
            CHECK(memcmp(&active,&saved_active,sizeof(active))==0 && memcmp(active.lifecycle,&saved_lifecycle,sizeof(saved_lifecycle))==0);
            CHECK(memcmp(active.lifecycle->grant_bytes,saved_grant,194)==0 && memcmp(active.lifecycle->replay.consumed,saved_replay,64)==0);
            CHECK(memcmp(&pending,&saved_pending,sizeof(pending))==0 && memcmp(pending.entries[0].transaction,saved_tx,244)==0);
            CHECK(memcmp(&c,&saved_context,sizeof(c))==0 && memcmp(replacement,&saved_span,sizeof(saved_span))==0 && memcmp(replacement[0].bytes,saved_block,replacement[0].length)==0);
        }
        traversal_rpc(&c,replacement,1,0);
        stn_pending_clear(&pending);memset(&restored,0,sizeof(restored));
        tx[0].type=1;tx[0].record_bytes=bad;tx[0].record_length=232;
        n=query_block(branch[1],tx,1,c.network_id,1,active.tip_id,&hash);
        CHECK(stn_chain_validate_candidate(&c,&active,branch[1],n,&state).acceptance==STN_ACCEPTANCE_REJECTED);
        CHECK(query_call(&node,rid,reply,sizeof(reply),&w)==STN_RPC_NOT_FOUND);
        tx[0].record_bytes=record;n=query_block(branch[1],tx,1,c.network_id,1,active.tip_id,&hash);replacement[1].bytes=branch[1];replacement[1].length=n;node.count=2;
        CHECK(stn_chain_validate_candidate(&c,&active,branch[1],n,&active).acceptance==STN_ACCEPTANCE_UNDER_CONTEXT);
        CHECK(query_call(&node,rid,reply,sizeof(reply),&w)==STN_RPC_OK && reply[39]==1);
        ownership_checks(&c,replacement);
        reconstruction_checks(&c,replacement,root_seed,seed,record);
        for(i=0;i<2;++i){
            stn_chain_record_match old_match,new_match;uint8_t replacement_id[32],replacement_block_id[32],old_work[STN_WORK_SIZE];
            stn_chain_cursor old_cursor={1,1,{0},0},new_cursor;
            memcpy(old_work,active.cumulative_work.bytes,STN_WORK_SIZE);
            CHECK(stn_chain_lookup_record(&c,replacement,2,rid,&old_match)==STN_DATA_OK && old_match.found && old_match.height==1);
            CHECK(stn_storage_encode(&c,replacement,2,disk.bytes,sizeof(disk.bytes),&disk.length)==STN_STORAGE_OK);
            CHECK(stn_chain_first_record(&c,replacement,2,&new_match,&new_cursor)==STN_FIRST_RECORD && memcmp(new_match.record_id,rid,32)==0);
            memcpy(old_cursor.block_id,old_match.block_id,32);
            CHECK(stn_chain_cursor_validate(&c,replacement,2,&old_cursor)==STN_CURSOR_VALID);
            query_restart(&c,&disk,rid,1,1);
            stn_chain_state_release(&branch_state);CHECK(stn_chain_state_share(&base,&branch_state)==STN_DATA_OK);candidates[0]=replacement[0];
            r.nonce[0]=2;query_sign(&r,other,seed);tx[0].record_bytes=other;
            n=query_block(competing[1],tx,1,c.network_id,1,branch_state.tip_id,&hash);candidates[1].bytes=competing[1];candidates[1].length=n;
            CHECK(stn_chain_validate_candidate(&c,&branch_state,competing[1],n,&branch_state).acceptance==STN_ACCEPTANCE_UNDER_CONTEXT);
            CHECK(stn_record_id(other,232,&hash,replacement_id)==STN_DATA_OK);
            CHECK(stn_chain_lookup_record(&c,replacement,2,replacement_id,&new_match)==STN_DATA_OK && !new_match.found);
            r.nonce[0]=3;query_sign(&r,other,seed);tx[0].record_bytes=i?record:other;
            n=query_block(competing[2],tx,1,c.network_id,2,branch_state.tip_id,&hash);candidates[2].bytes=competing[2];candidates[2].length=n;
            CHECK(stn_storage_adopt(&c,&storage,candidates,3,&workspace,&active)==STN_STORAGE_OK);
            CHECK(memcmp(active.cumulative_work.bytes,old_work,STN_WORK_SIZE)>0);
            CHECK(stn_chain_cursor_validate(&c,candidates,3,&old_cursor)==STN_CURSOR_DETACHED);
            {
                stn_cursor_ancestor ancestor;stn_chain_cursor malformed=old_cursor;
                stn_storage_view replayed={0};publication_store retained_disk={0};
                CHECK(stn_chain_resolve_cursor_reorg(&c,candidates,3,replacement,2,&old_cursor,&ancestor)==STN_CURSOR_REORG_COMMON_ANCESTOR);
                CHECK(ancestor.height==0);
                recovery_rpc(&c,candidates,3,replacement,2,&old_cursor,i==0);
                recovery_rpc(&c,candidates,3,NULL,0,&old_cursor,0);
                {
                    stn_consumer_recovery_plan plan,saved;stn_chain_record_match first;stn_chain_cursor position;
                    memset(&plan,0x5a,sizeof(plan));memcpy(&saved,&plan,sizeof(plan));
                    CHECK(stn_chain_build_consumer_recovery_plan(&c,candidates,3,NULL,0,&old_cursor,&plan)==STN_RECOVERY_UNAVAILABLE && memcmp(&plan,&saved,sizeof(plan))==0);
                    CHECK(stn_chain_build_consumer_recovery_plan(&c,candidates,3,replacement,2,&old_cursor,&plan)==STN_RECOVERY_FROM_START);
                    CHECK(plan.rollback.height==ancestor.height && memcmp(plan.rollback.block_id,ancestor.block_id,32)==0 && plan.resume.version==0);
                    CHECK(stn_chain_first_record(&c,candidates,3,&first,&position)==STN_FIRST_RECORD && position.height==1);
                    malformed.version=2;
                    CHECK(stn_chain_build_consumer_recovery_plan(&c,candidates,3,replacement,2,&malformed,&plan)==STN_RECOVERY_MALFORMED);
                }
                CHECK(stn_chain_block_id(candidates[0].bytes,candidates[0].length,&hash,replacement_block_id)==STN_DATA_OK && memcmp(ancestor.block_id,replacement_block_id,32)==0);
                CHECK(stn_chain_resolve_cursor_reorg(&c,candidates,3,NULL,0,&old_cursor,&ancestor)==STN_CURSOR_REORG_NO_COMMON_ANCESTOR);
                malformed.version=2;
                CHECK(stn_chain_resolve_cursor_reorg(&c,candidates,3,replacement,2,&malformed,&ancestor)==STN_CURSOR_REORG_MALFORMED);
                CHECK(stn_storage_encode(&c,replacement,2,retained_disk.bytes,sizeof(retained_disk.bytes),&retained_disk.length)==STN_STORAGE_OK);
                CHECK(stn_storage_decode(&c,retained_disk.bytes,retained_disk.length,&replayed)==STN_STORAGE_OK);
                CHECK(stn_chain_resolve_cursor_reorg(&c,candidates,3,replayed.blocks,replayed.count,&old_cursor,&ancestor)==STN_CURSOR_REORG_COMMON_ANCESTOR && ancestor.height==0);
                {
                    stn_consumer_recovery_plan before_plan,after_plan;
                    CHECK(stn_chain_build_consumer_recovery_plan(&c,candidates,3,replacement,2,&old_cursor,&before_plan)==STN_RECOVERY_FROM_START);
                    CHECK(stn_chain_build_consumer_recovery_plan(&c,candidates,3,replayed.blocks,replayed.count,&old_cursor,&after_plan)==STN_RECOVERY_FROM_START);
                    CHECK(before_plan.rollback.height==after_plan.rollback.height && memcmp(before_plan.rollback.block_id,after_plan.rollback.block_id,32)==0);
                }
                recovery_rpc(&c,candidates,3,replayed.blocks,replayed.count,&old_cursor,0);
                stn_storage_view_release(&replayed);
                if(i==0){
                    uint8_t fork_block[16000];stn_block_span fork_spans[4];stn_chain_state fork_state;
                    stn_chain_cursor fork_cursor={1,2,{0},0};size_t k;
                    CHECK(stn_chain_initialize(&c,&fork_state)==STN_DATA_OK);
                    for(k=0;k<2;++k){
                        fork_spans[k]=candidates[k];
                        CHECK(stn_chain_validate_candidate(&c,&fork_state,candidates[k].bytes,candidates[k].length,&fork_state).acceptance==STN_ACCEPTANCE_UNDER_CONTEXT);
                    }
                    tx[0].record_bytes=record;
                    fork_spans[2].bytes=fork_block;fork_spans[2].length=query_block(fork_block,tx,1,c.network_id,2,fork_state.tip_id,&hash);
                    CHECK(stn_chain_block_id(fork_block,fork_spans[2].length,&hash,fork_cursor.block_id)==STN_DATA_OK);
                    CHECK(stn_chain_resolve_cursor_reorg(&c,candidates,3,fork_spans,3,&fork_cursor,&ancestor)==STN_CURSOR_REORG_COMMON_ANCESTOR && ancestor.height==1);
                    CHECK(memcmp(ancestor.block_id,fork_state.tip_id,32)==0);
                    recovery_rpc(&c,candidates,3,fork_spans,3,&fork_cursor,1);
                    {
                        stn_consumer_recovery_plan plan;stn_chain_record_match following;stn_chain_cursor position;
                        CHECK(stn_chain_build_consumer_recovery_plan(&c,candidates,3,fork_spans,3,&fork_cursor,&plan)==STN_RECOVERY_AFTER_CURSOR);
                        CHECK(plan.rollback.height==1 && memcmp(plan.rollback.block_id,ancestor.block_id,32)==0 && plan.resume.height==1);
                        CHECK(stn_chain_cursor_validate(&c,candidates,3,&plan.resume)==STN_CURSOR_VALID);
                        CHECK(stn_chain_next_record(&c,candidates,3,&plan.resume,&following,&position)==STN_NEXT_RECORD && position.height==2);
                    }
                    CHECK(stn_chain_resolve_cursor_reorg(&c,replacement,2,fork_spans,3,&fork_cursor,&ancestor)==STN_CURSOR_REORG_COMMON_ANCESTOR && ancestor.height==0);
                    {
                        stn_consumer_recovery_plan plan;
                        fork_spans[2]=candidates[2];
                        CHECK(stn_chain_validate_candidate(&c,&fork_state,candidates[2].bytes,candidates[2].length,&fork_state).acceptance==STN_ACCEPTANCE_UNDER_CONTEXT);
                        r.nonce[0]=4;query_sign(&r,other,seed);tx[0].record_bytes=other;
                        fork_spans[3].bytes=fork_block;fork_spans[3].length=query_block(fork_block,tx,1,c.network_id,3,fork_state.tip_id,&hash);
                        fork_cursor.height=3;
                        CHECK(stn_chain_block_id(fork_block,fork_spans[3].length,&hash,fork_cursor.block_id)==STN_DATA_OK);
                        CHECK(stn_chain_build_consumer_recovery_plan(&c,candidates,3,fork_spans,4,&fork_cursor,&plan)==STN_RECOVERY_AFTER_CURSOR);
                        CHECK(plan.rollback.height==2 && plan.resume.height==2 && plan.resume.transaction_position==0);
                        stn_chain_state_release(&fork_state);
                    }
                }
            }
            {
                stn_chain_record_match match;stn_chain_cursor next;
                CHECK(stn_chain_next_record(&c,candidates,3,&old_cursor,&match,&next)==STN_NEXT_DETACHED);
            }
            new_cursor=old_cursor;
            CHECK(stn_chain_block_id(candidates[1].bytes,candidates[1].length,&hash,new_cursor.block_id)==STN_DATA_OK);
            CHECK(stn_chain_cursor_validate(&c,candidates,3,&new_cursor)==STN_CURSOR_VALID);
            new_cursor.transaction_position=UINT32_MAX;
            CHECK(stn_chain_cursor_validate(&c,candidates,3,&new_cursor)==STN_CURSOR_DETACHED);
            new_cursor.transaction_position=0;new_cursor.height=UINT64_MAX;
            CHECK(stn_chain_cursor_validate(&c,candidates,3,&new_cursor)==STN_CURSOR_DETACHED);
            CHECK(stn_chain_lookup_record(&c,candidates,3,rid,&new_match)==STN_DATA_OK && new_match.found==(i!=0));
            if(i){
                CHECK(new_match.height==2 && memcmp(new_match.block_id,old_match.block_id,32)!=0);
                CHECK(stn_chain_block_id(candidates[2].bytes,candidates[2].length,&hash,replacement_block_id)==STN_DATA_OK && memcmp(new_match.block_id,replacement_block_id,32)==0);
                CHECK(new_match.transaction.length==244 && memcmp(new_match.transaction.bytes,encoded_tx,244)==0);
            }
            CHECK(stn_chain_lookup_record(&c,candidates,3,replacement_id,&new_match)==STN_DATA_OK && new_match.found && new_match.height==1);
            CHECK(stn_chain_first_record(&c,candidates,3,&new_match,&new_cursor)==STN_FIRST_RECORD && memcmp(new_match.record_id,replacement_id,32)==0 && new_cursor.height==1);
            {
                stn_chain_record_match following;stn_chain_cursor following_cursor;
                CHECK(stn_chain_next_record(&c,candidates,3,&new_cursor,&following,&following_cursor)==STN_NEXT_RECORD && following_cursor.height==2 && memcmp(following.record_id,new_match.record_id,32)!=0);
            }
            query_restart(&c,&disk,rid,i!=0,2);
            query_restart(&c,&disk,replacement_id,1,1);
            if(i==0)traversal_rpc(&c,candidates,3,1);
            CHECK(stn_storage_decode(&c,disk.bytes,disk.length,&restored)==STN_STORAGE_OK);
            node.blocks=restored.blocks;node.count=restored.count;
            if(i)CHECK(query_call(&node,rid,reply,sizeof(reply),&w)==STN_RPC_OK && reply[39]==2 && memcmp(reply+76,encoded_tx,244)==0);
            else CHECK(query_call(&node,rid,reply,sizeof(reply),&w)==STN_RPC_NOT_FOUND);
            stn_storage_view_release(&restored);
            stn_chain_state_release(&active);CHECK(stn_chain_state_share(&base,&active)==STN_DATA_OK);CHECK(stn_chain_validate_candidate(&c,&active,branch[1],replacement[1].length,&active).acceptance==STN_ACCEPTANCE_UNDER_CONTEXT);
        }
        stn_chain_state_release(&active);CHECK(stn_chain_state_share(&base,&active)==STN_DATA_OK);CHECK(stn_chain_validate_candidate(&c,&active,competing[1],candidates[1].length,&active).acceptance==STN_ACCEPTANCE_UNDER_CONTEXT);
        r.nonce[0]=3;query_sign(&r,other,seed);tx[0].record_bytes=other;
        candidates[2].length=query_block(competing[2],tx,1,c.network_id,2,active.tip_id,&hash);
        CHECK(stn_chain_validate_candidate(&c,&active,competing[2],candidates[2].length,&active).acceptance==STN_ACCEPTANCE_UNDER_CONTEXT);
        CHECK(stn_storage_encode(&c,candidates,3,disk.bytes,sizeof(disk.bytes),&disk.length)==STN_STORAGE_OK);
        node.blocks=candidates;node.count=3;CHECK(query_call(&node,rid,reply,sizeof(reply),&w)==STN_RPC_NOT_FOUND);
        tx[0].record_bytes=record;candidates[3].bytes=competing[3];candidates[3].length=query_block(competing[3],tx,1,c.network_id,3,active.tip_id,&hash);
        CHECK(stn_storage_adopt(&c,&storage,candidates,4,&workspace,&active)==STN_STORAGE_OK);
        CHECK(stn_storage_decode(&c,disk.bytes,disk.length,&restored)==STN_STORAGE_OK);node.blocks=restored.blocks;node.count=restored.count;
        CHECK(query_call(&node,rid,reply,sizeof(reply),&w)==STN_RPC_OK && reply[39]==3);
        stn_storage_view_release(&restored);
        node.blocks=replacement;node.count=2;
        branch[1][replacement[1].length-1]^=1;
        CHECK(query_call(&node,rid,reply,sizeof(reply),&w)==STN_RPC_PROVIDER && w==0);
        branch[1][replacement[1].length-1]^=1;
        {
            uint8_t large_payload[1390],large_record[1570],signed_bytes[1530],classification[64],source[253],subject[1024];
            stn_intelligence intelligence={0};stn_record maximum=r;stn_rpc_message answer={2,STN_RPC_GET_ACCEPTED_RECORD,STN_RPC_OK,4,reply,0};
            memset(classification,'a',sizeof(classification));memset(source,'a',sizeof(source));source[63]='.';source[127]='.';source[191]='.';memset(subject,'a',sizeof(subject));
            intelligence.version=1;intelligence.severity=1;intelligence.classification=classification;intelligence.classification_length=64;intelligence.source=source;intelligence.source_length=253;intelligence.subject=subject;intelligence.subject_length=1024;intelligence.evidence_digest[0]=1;
            CHECK(stn_intelligence_encode(&intelligence,large_payload,sizeof(large_payload),&n)==STN_INTELLIGENCE_OK && n==1390);
            maximum.payload=large_payload;maximum.payload_length=1390;
            CHECK(stn_record_encode(&maximum,large_record,sizeof(large_record),&n)==STN_RECORD_OK);
            CHECK(stn_identity_statement(large_record,1506,signed_bytes,sizeof(signed_bytes),&n)==STN_IDENTITY_VALID);
            ed25519_sign_stn(signed_bytes,n,seed,producer,maximum.signature);
            CHECK(stn_record_encode(&maximum,large_record,sizeof(large_record),&n)==STN_RECORD_OK);
            CHECK(stn_record_id(large_record,n,&hash,other_id)==STN_DATA_OK);
            tx[0].record_bytes=large_record;tx[0].record_length=1570;
            replacement[1].length=query_block(branch[1],tx,1,c.network_id,1,base.tip_id,&hash);
            CHECK(query_call(&node,other_id,reply,sizeof(reply),&w)==STN_RPC_OK && w==1658 && memcmp(reply+88,large_record,1570)==0);
            answer.length=w;CHECK(stn_rpc_encode(&answer,before,sizeof(before),&n)==STN_RPC_OK);
            CHECK(stn_rpc_decode(before,n,&answer)==STN_RPC_OK && answer.length==1658);
            CHECK(query_call(&node,other_id,reply,1657,&w)==STN_RPC_CAPACITY && w==0);
        }
        stn_chain_state_release(&active);stn_chain_state_release(&base);stn_chain_state_release(&branch_state);
    }
    stn_chain_state_release(&state);stn_chain_state_release(&prior);
    CHECK(stn_chain_test_live_snapshots()==ownership_baseline);
    printf("Phase 15 accepted record query: %u targeted checks.\n",checks-start);
}
