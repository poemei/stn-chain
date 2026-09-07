/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_peer.h"
#include "stn_wire_internal.h"
#include <string.h>
static int type_valid(uint16_t t){return t>=STN_PEER_HELLO && t<=STN_PEER_BLOCK;}
stn_peer_status stn_peer_header(const uint8_t *p,size_t n,uint16_t *t,size_t *length)
{
    uint16_t type;size_t size;
    if(p==NULL || t==NULL || length==NULL){return STN_PEER_ARGUMENT;}
    if(n!=12 || memcmp(p,"STNP",4)!=0 || stn_wire_read(p+4,2)!=1){return STN_PEER_PROTOCOL;}
    type=(uint16_t)stn_wire_read(p+6,2);size=(size_t)stn_wire_read(p+8,4);
    if(!type_valid(type) || size>STN_PEER_MAX_PAYLOAD){return STN_PEER_PROTOCOL;}
    *t=type;*length=size;return STN_PEER_OK;
}
stn_peer_status stn_peer_decode(const uint8_t *p,size_t n,stn_peer_message *out)
{
    stn_peer_message m;stn_peer_status s;
    if(p==NULL || out==NULL){return STN_PEER_ARGUMENT;}
    if(n<12){return STN_PEER_PROTOCOL;}
    s=stn_peer_header(p,12,&m.type,&m.length);if(s!=STN_PEER_OK){return s;}
    if(n-12!=m.length){return STN_PEER_PROTOCOL;}
    m.payload=p+12;*out=m;return STN_PEER_OK;
}
stn_peer_status stn_peer_encode(uint16_t t,const uint8_t *p,size_t n,uint8_t *out,size_t cap,size_t *written)
{
    if(written!=NULL){*written=0;}
    if(out==NULL || written==NULL || (n!=0 && p==NULL)){return STN_PEER_ARGUMENT;}
    if(!type_valid(t) || n>STN_PEER_MAX_PAYLOAD){return STN_PEER_PROTOCOL;}
    if(cap<12 || n>cap-12){return STN_PEER_CAPACITY;}
    if(n!=0){memmove(out+12,p,n);}memcpy(out,"STNP",4);
    stn_wire_write(out+4,2,1);stn_wire_write(out+6,2,t);stn_wire_write(out+8,4,n);*written=n+12;return STN_PEER_OK;
}
static stn_peer_status local_validate(const stn_chain_context *c,const stn_block_span *b,size_t n,stn_chain_state *out)
{
    stn_chain_state empty;
    if(c==NULL || c->pow_policy==NULL || b==NULL || n==0 || n>64 || stn_chain_initialize(c,&empty)!=STN_DATA_OK){return STN_PEER_VALIDATION;}
    return stn_chain_validate_sequence(c,&empty,b,n,out).acceptance==STN_ACCEPTANCE_UNDER_CONTEXT ? STN_PEER_OK : STN_PEER_VALIDATION;
}
static stn_peer_status hello(const stn_chain_context *c,uint8_t p[68])
{
    memcpy(p,c->network_id,32);
    if(stn_chain_block_id(c->genesis_bytes,c->genesis_length,&c->hash_provider,p+32)!=STN_DATA_OK){return STN_PEER_VALIDATION;}
    stn_wire_write(p+64,4,1);return STN_PEER_OK;
}
stn_peer_status stn_peer_serve(const stn_chain_context *c,const stn_block_span *b,size_t count,
    stn_peer_session *session,const uint8_t *request,size_t n,uint8_t *response,size_t cap,size_t *written)
{
    stn_peer_message m;stn_chain_state state;stn_peer_status s;size_t start,num,i,length=0;uint16_t type;uint8_t greeting[68];
    if(written!=NULL){*written=0;}
    if(c==NULL || session==NULL || response==NULL || written==NULL){return STN_PEER_ARGUMENT;}
    if(session->requests>=67 || session->handshake<0){return STN_PEER_PROTOCOL;}
    s=stn_peer_decode(request,n,&m);if(s!=STN_PEER_OK){session->handshake=-1;return s;}
    s=local_validate(c,b,count,&state);if(s!=STN_PEER_OK){return s;}
    if(cap<12+68){return STN_PEER_CAPACITY;}
    type=m.type;
    if(!session->handshake){
        if(m.type!=STN_PEER_HELLO || m.length!=68){goto bad;}
        s=hello(c,greeting);if(s!=STN_PEER_OK){return s;}
        if(memcmp(m.payload,greeting,68)!=0){session->handshake=-1;return STN_PEER_NETWORK;}
        memcpy(response+12,greeting,68);length=68;
    }else if(m.type==STN_PEER_STATE && m.length==0){
        if(cap<88){return STN_PEER_CAPACITY;}
        stn_wire_write(response+12,8,state.height);memcpy(response+20,state.tip_id,32);
        memcpy(response+52,state.cumulative_work.bytes,32);stn_wire_write(response+84,4,count);length=76;
    }else if(m.type==STN_PEER_GET_HEADERS && m.length==8){
        start=(size_t)stn_wire_read(m.payload,4);num=(size_t)stn_wire_read(m.payload+4,4);
        if(start>=count || num==0 || num>64 || num>count-start){goto bad;}
        length=8+num*168;if(cap<12+length){return STN_PEER_CAPACITY;}
        stn_wire_write(response+12,4,start);stn_wire_write(response+16,4,num);
        for(i=0;i<num;++i){memcpy(response+20+i*168,b[start+i].bytes,168);}type=STN_PEER_HEADERS;
    }else if(m.type==STN_PEER_GET_BLOCK && m.length==4){
        start=(size_t)stn_wire_read(m.payload,4);if(start>=count){goto bad;}
        length=4+b[start].length;if(cap<12+length){return STN_PEER_CAPACITY;}
        stn_wire_write(response+12,4,start);memcpy(response+16,b[start].bytes,b[start].length);type=STN_PEER_BLOCK;
    }else{goto bad;}
    s=stn_peer_encode(type,response+12,length,response,cap,written);
    if(s==STN_PEER_OK){session->handshake=1;++session->requests;}return s;
bad:
    session->handshake=-1;return STN_PEER_PROTOCOL;
}
stn_peer_status stn_peer_receive(const stn_peer_transport *t,uint8_t *frame,size_t cap,size_t *length)
{
    stn_peer_status s;uint16_t type;size_t n;
    if(t==NULL || t->receive==NULL || frame==NULL || length==NULL){return STN_PEER_ARGUMENT;}
    if(cap<12){return STN_PEER_CAPACITY;}
    s=t->receive(t->user,frame,12);if(s!=STN_PEER_OK){return s;}
    s=stn_peer_header(frame,12,&type,&n);if(s!=STN_PEER_OK){return s;}
    if(n>cap-12){return STN_PEER_CAPACITY;}
    if(n!=0){s=t->receive(t->user,frame+12,n);if(s!=STN_PEER_OK){return s;}}
    *length=12+n;return STN_PEER_OK;
}
static stn_peer_status exchange(const stn_peer_transport *t,stn_peer_workspace *w,uint16_t type,
    const uint8_t *payload,size_t n,uint16_t expected,stn_peer_message *m)
{
    stn_peer_status s;size_t length;
    s=stn_peer_encode(type,payload,n,w->frame,w->frame_capacity,&length);if(s!=STN_PEER_OK){return s;}
    s=t->send(t->user,w->frame,length);if(s!=STN_PEER_OK){return s;}
    s=stn_peer_receive(t,w->frame,w->frame_capacity,&length);if(s!=STN_PEER_OK){return s;}
    s=stn_peer_decode(w->frame,length,m);if(s!=STN_PEER_OK){return s;}
    return m->type==expected ? STN_PEER_OK : STN_PEER_PROTOCOL;
}
stn_peer_report stn_peer_sync(const stn_chain_context *c,const stn_storage_provider *storage,
    const stn_peer_transport *t,stn_peer_workspace *w,stn_chain_state *active)
{
    stn_peer_report r={0};stn_storage_view local;stn_peer_message m;stn_storage_status ss;
    uint8_t greeting[68],request[8],headers[64][168];stn_block_span blocks[64];
    size_t count,i,reuse=0,offset=0;stn_chain_state checked;
    r.status=STN_PEER_ARGUMENT;
    if(c==NULL || storage==NULL || t==NULL || t->send==NULL || t->receive==NULL || w==NULL || active==NULL || w->candidate==NULL || w->frame==NULL){return r;}
    ss=stn_storage_recovery_read(c,storage,w->storage.current_bytes,w->storage.current_capacity,&local,&r.recovery);
    if(ss!=STN_STORAGE_OK){r.status=STN_PEER_STORAGE;return r;}
    r.status=hello(c,greeting);if(r.status!=STN_PEER_OK){return r;}
    r.status=exchange(t,w,STN_PEER_HELLO,greeting,68,STN_PEER_HELLO,&m);if(r.status!=STN_PEER_OK){return r;}
    if(m.length!=68 || memcmp(m.payload,greeting,68)!=0){r.status=STN_PEER_NETWORK;return r;}
    r.status=exchange(t,w,STN_PEER_STATE,NULL,0,STN_PEER_STATE,&m);if(r.status!=STN_PEER_OK){return r;}
    if(m.length!=76){r.status=STN_PEER_PROTOCOL;return r;}
    count=(size_t)stn_wire_read(m.payload+72,4);
    if(count==0 || count>64 || stn_wire_read(m.payload,8)!=count-1){r.status=STN_PEER_PROTOCOL;return r;}
    /* Tip/work claims intentionally never participate in fork selection. */
    stn_wire_write(request,4,0);stn_wire_write(request+4,4,count);
    r.status=exchange(t,w,STN_PEER_GET_HEADERS,request,8,STN_PEER_HEADERS,&m);if(r.status!=STN_PEER_OK){return r;}
    if(m.length!=8+count*168 || stn_wire_read(m.payload,4)!=0 || stn_wire_read(m.payload+4,4)!=count){r.status=STN_PEER_PROTOCOL;return r;}
    memcpy(headers,m.payload+8,count*168);
    while(reuse<count && reuse<local.count && memcmp(headers[reuse],local.blocks[reuse].bytes,168)==0){++reuse;}
    if(stn_chain_initialize(c,&checked)!=STN_DATA_OK){r.status=STN_PEER_VALIDATION;return r;}
    for(i=0;i<count;++i){
        const uint8_t *bytes;size_t length;stn_chain_report validation;
        if(i<reuse){bytes=local.blocks[i].bytes;length=local.blocks[i].length;++r.reused_blocks;}
        else{
            stn_wire_write(request,4,i);
            r.status=exchange(t,w,STN_PEER_GET_BLOCK,request,4,STN_PEER_BLOCK,&m);if(r.status!=STN_PEER_OK){return r;}
            if(m.length<4 || stn_wire_read(m.payload,4)!=i){r.status=STN_PEER_PROTOCOL;return r;}
            bytes=m.payload+4;length=m.length-4;++r.received_blocks;
        }
        if(length<168 || memcmp(bytes,headers[i],168)!=0){r.status=STN_PEER_PROTOCOL;return r;}
        if(length>w->candidate_capacity-offset){r.status=STN_PEER_CAPACITY;return r;}
        validation=stn_chain_validate_candidate(c,&checked,bytes,length,&checked);
        if(validation.acceptance!=STN_ACCEPTANCE_UNDER_CONTEXT){r.status=STN_PEER_VALIDATION;return r;}
        memcpy(w->candidate+offset,bytes,length);blocks[i].bytes=w->candidate+offset;blocks[i].length=length;offset+=length;
    }
    ss=stn_storage_adopt(c,storage,blocks,count,&w->storage,active);
    if(ss==STN_STORAGE_NOT_PREFERRED){r.status=STN_PEER_RETAINED;r.verified=checked;return r;}
    if(ss!=STN_STORAGE_OK){r.status=STN_PEER_STORAGE;return r;}
    r.status=STN_PEER_OK;r.verified=checked;return r;
}
