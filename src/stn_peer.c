/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_peer.h"
#include "stn_wire_internal.h"
#include <string.h>
#include <stdlib.h>
static int type_valid(uint16_t t){return t>=STN_PEER_HELLO && t<=STN_PEER_PEERS;}
static int discovery_size(uint16_t type,size_t size)
{
    return type==STN_PEER_GET_PEERS ? size==0 :
        (type!=STN_PEER_PEERS || (size>=2 && size<=STN_PEER_DISCOVERY_MAX && (size-2)%6==0));
}
stn_peer_status stn_peer_header(const uint8_t *p,size_t n,uint16_t *t,size_t *length)
{
    uint16_t type;size_t size;
    if(p==NULL || t==NULL || length==NULL){return STN_PEER_ARGUMENT;}
    if(n!=12 || memcmp(p,"STNP",4)!=0 || stn_wire_read(p+4,2)!=2){return STN_PEER_PROTOCOL;}
    type=(uint16_t)stn_wire_read(p+6,2);size=(size_t)stn_wire_read(p+8,4);
    if(!type_valid(type) || size>STN_PEER_MAX_PAYLOAD || !discovery_size(type,size)){return STN_PEER_PROTOCOL;}
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
    if(!type_valid(t) || n>STN_PEER_MAX_PAYLOAD || !discovery_size(t,n)){return STN_PEER_PROTOCOL;}
    if(cap<12 || n>cap-12){return STN_PEER_CAPACITY;}
    if(n!=0){memmove(out+12,p,n);}memcpy(out,"STNP",4);
    stn_wire_write(out+4,2,2);stn_wire_write(out+6,2,t);stn_wire_write(out+8,4,n);*written=n+12;return STN_PEER_OK;
}
static stn_peer_status local_validate(const stn_chain_context *c,const stn_block_span *b,size_t n,stn_chain_state *out)
{
    stn_chain_state state;size_t i;
    if(c==NULL || c->pow_policy==NULL || b==NULL || n==0 || n>UINT32_MAX || stn_chain_initialize(c,&state)!=STN_DATA_OK){return STN_PEER_VALIDATION;}
    for(i=0;i<n;++i){
        if(stn_chain_validate_candidate(c,&state,b[i].bytes,b[i].length,&state).acceptance!=STN_ACCEPTANCE_UNDER_CONTEXT){return STN_PEER_VALIDATION;}
    }
    *out=state;return STN_PEER_OK;
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
    if(session->handshake<0){return STN_PEER_PROTOCOL;}
    s=stn_peer_decode(request,n,&m);if(s!=STN_PEER_OK){session->handshake=-1;return s;}
    s=local_validate(c,b,count,&state);if(s!=STN_PEER_OK){return s;}
    if(cap<12+68){return STN_PEER_CAPACITY;}
    type=m.type;
    if(!session->handshake){
        if(m.type!=STN_PEER_HELLO || m.length!=68){goto bad;}
        s=hello(c,greeting);if(s!=STN_PEER_OK){return s;}
        if(memcmp(m.payload,greeting,68)!=0){session->handshake=-1;return STN_PEER_NETWORK;}
        if(session->known!=NULL){stn_wire_write(greeting+64,4,3);}
        memcpy(response+12,greeting,68);length=68;
    }else if(m.type==STN_PEER_GET_PEERS && m.length==0 && session->known!=NULL && !session->discovery_sent){
        s=stn_peer_discovery_encode(session->known,session->self,response+12,cap-12,&length);
        if(s!=STN_PEER_OK){return s;}type=STN_PEER_PEERS;session->discovery_sent=1;
    }else if(m.type==STN_PEER_STATE && m.length==0){
        if(cap<96){return STN_PEER_CAPACITY;}
        stn_wire_write(response+12,8,state.height);memcpy(response+20,state.tip_id,32);
        memcpy(response+52,state.cumulative_work.bytes,STN_WORK_SIZE);stn_wire_write(response+92,4,count);length=84;
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
    if(s==STN_PEER_OK){session->handshake=1;if(session->requests<SIZE_MAX){++session->requests;}}return s;
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
static stn_peer_report sync_session(int established,uint32_t *capabilities,const stn_chain_context *c,const stn_storage_provider *storage,
    const stn_peer_transport *t,stn_peer_workspace *w,stn_chain_state *active)
{
    stn_peer_report r={0};stn_storage_view local={0};stn_peer_message m;stn_storage_status ss;
    uint8_t greeting[68],request[8],headers[64][168],remove[STN_PENDING_MAX_ENTRIES];stn_block_span *blocks=NULL;
    size_t count,i,page,num,offset=0;stn_chain_state checked;int matching=1;
    r.status=STN_PEER_ARGUMENT;
    if(c==NULL || storage==NULL || t==NULL || t->send==NULL || t->receive==NULL || w==NULL || active==NULL || w->candidate==NULL || w->frame==NULL){return r;}
    ss=stn_storage_recovery_read(c,storage,w->storage.current_bytes,w->storage.current_capacity,&local,&r.recovery);
    if(ss!=STN_STORAGE_OK){r.status=STN_PEER_STORAGE;goto done;}
    if(!established){
    r.status=hello(c,greeting);if(r.status!=STN_PEER_OK){goto done;}
    r.status=exchange(t,w,STN_PEER_HELLO,greeting,68,STN_PEER_HELLO,&m);if(r.status!=STN_PEER_OK){goto done;}
    if(m.length!=68 || memcmp(m.payload,greeting,64)!=0 ||
        (stn_wire_read(m.payload+64,4)!=1 && stn_wire_read(m.payload+64,4)!=3)){r.status=STN_PEER_NETWORK;goto done;}
    if(capabilities!=NULL){*capabilities=(uint32_t)stn_wire_read(m.payload+64,4);}
    }
    r.status=exchange(t,w,STN_PEER_STATE,NULL,0,STN_PEER_STATE,&m);if(r.status!=STN_PEER_OK){goto done;}
    if(m.length!=84){r.status=STN_PEER_PROTOCOL;goto done;}
    count=(size_t)stn_wire_read(m.payload+80,4);
    if(count==0 || stn_wire_read(m.payload,8)!=count-1){r.status=STN_PEER_PROTOCOL;goto done;}
    /* Bound allocation by caller-owned bytes, never merely a peer count. */
    if(count>w->candidate_capacity/(STN_BLOCK_HEADER_SIZE+STN_BLOCK_MIN_BODY) || count>SIZE_MAX/sizeof(*blocks)){r.status=STN_PEER_CAPACITY;goto done;}
    blocks=(stn_block_span*)calloc(count,sizeof(*blocks));
    if(blocks==NULL){r.status=STN_PEER_CAPACITY;goto done;}
    if(stn_chain_initialize(c,&checked)!=STN_DATA_OK){r.status=STN_PEER_VALIDATION;goto done;}
    for(page=0;page<count;page+=num){
        num=count-page;if(num>64){num=64;}
        stn_wire_write(request,4,page);stn_wire_write(request+4,4,num);
        r.status=exchange(t,w,STN_PEER_GET_HEADERS,request,8,STN_PEER_HEADERS,&m);if(r.status!=STN_PEER_OK){goto done;}
        if(m.length!=8+num*168 || stn_wire_read(m.payload,4)!=page || stn_wire_read(m.payload+4,4)!=num){r.status=STN_PEER_PROTOCOL;goto done;}
        memcpy(headers,m.payload+8,num*168);
        for(i=page;i<page+num;++i){
            const uint8_t *bytes;size_t length;stn_chain_report validation;
            if(matching && i<local.count && memcmp(headers[i-page],local.blocks[i].bytes,168)==0){
                bytes=local.blocks[i].bytes;length=local.blocks[i].length;++r.reused_blocks;
            }else{
                matching=0;stn_wire_write(request,4,i);
                r.status=exchange(t,w,STN_PEER_GET_BLOCK,request,4,STN_PEER_BLOCK,&m);if(r.status!=STN_PEER_OK){goto done;}
                if(m.length<4 || stn_wire_read(m.payload,4)!=i){r.status=STN_PEER_PROTOCOL;goto done;}
                bytes=m.payload+4;length=m.length-4;++r.received_blocks;
            }
            if(length<168 || memcmp(bytes,headers[i-page],168)!=0){r.status=STN_PEER_PROTOCOL;goto done;}
            if(length>w->candidate_capacity-offset){r.status=STN_PEER_CAPACITY;goto done;}
            validation=stn_chain_validate_candidate(c,&checked,bytes,length,&checked);
            if(validation.acceptance!=STN_ACCEPTANCE_UNDER_CONTEXT){r.status=STN_PEER_VALIDATION;goto done;}
            memcpy(w->candidate+offset,bytes,length);blocks[i].bytes=w->candidate+offset;blocks[i].length=length;offset+=length;
        }
    }
    if(w->pending!=NULL){
        stn_storage_view included={0};included.blocks=blocks;included.count=count;
        if(stn_pending_inclusions(w->pending,&included,&c->hash_provider,remove)!=STN_DATA_OK){r.status=STN_PEER_VALIDATION;goto done;}
    }
    ss=stn_storage_adopt(c,storage,blocks,count,&w->storage,active);
    if(ss==STN_STORAGE_NOT_PREFERRED){r.status=STN_PEER_RETAINED;r.verified=checked;goto done;}
    if(ss!=STN_STORAGE_OK){r.status=STN_PEER_STORAGE;goto done;}
    if(w->pending!=NULL){stn_pending_prune(w->pending,remove);}
    r.status=STN_PEER_OK;r.verified=checked;
done:
    free(blocks);stn_storage_view_release(&local);return r;
}

/* Canonical local endpoint order compares fields, never structure padding. */
static int endpoint_order(const stn_peer_endpoint *a,const stn_peer_endpoint *b)
{
    int c=memcmp(a->address,b->address,4);
    if(c!=0){return c;}return a->port<b->port ? -1 : (a->port>b->port ? 1 : 0);
}
static int endpoint_valid(const stn_peer_endpoint *p)
{
    /* Reject unspecified/this-network, multicast, reserved and limited broadcast.
     * Loopback/private/link-local unicast remain eligible for local operation.
     * Subnet-directed broadcast cannot be inferred without interface policy. */
    return p->port!=0 && p->address[0]!=0 && p->address[0]<224;
}
stn_peer_status stn_peer_candidate_add(stn_peer_candidates *set,const stn_peer_endpoint *endpoint)
{
    size_t i,position;stn_peer_endpoint candidate;
    if(set==NULL || endpoint==NULL || set->count>STN_PEER_CANDIDATE_MAX){return STN_PEER_ARGUMENT;}
    candidate=*endpoint;
    if(!endpoint_valid(&candidate)){return STN_PEER_ARGUMENT;}
    for(i=0;i<set->count;++i){
        if(!endpoint_valid(&set->entries[i]) || (i!=0 && endpoint_order(&set->entries[i-1],&set->entries[i])>=0)){
            return STN_PEER_ARGUMENT;
        }
    }
    for(position=0;position<set->count;++position){
        int comparison=endpoint_order(&candidate,&set->entries[position]);
        if(comparison==0){return STN_PEER_RETAINED;}if(comparison<0){break;}
    }
    if(set->count==STN_PEER_CANDIDATE_MAX){return STN_PEER_CAPACITY;}
    for(i=set->count;i>position;--i){set->entries[i]=set->entries[i-1];}
    memcpy(set->entries[position].address,candidate.address,4);
    set->entries[position].port=candidate.port;++set->count;return STN_PEER_OK;
}

stn_peer_report stn_peer_sync(const stn_chain_context *c,const stn_storage_provider *storage,
    const stn_peer_transport *t,stn_peer_workspace *w,stn_chain_state *active)
{return sync_session(0,NULL,c,storage,t,w,active);}

stn_peer_status stn_peer_outbound_init(stn_peer_outbound *out,const stn_peer_candidates *candidates,
    const stn_peer_connector *connector)
{
    stn_peer_outbound value={0};size_t i;
    if(out==NULL || candidates==NULL || connector==NULL || connector->open==NULL || connector->close==NULL || candidates->count>64){return STN_PEER_ARGUMENT;}
    for(i=0;i<candidates->count;++i){
        if(stn_peer_candidate_add(&value.candidates,&candidates->entries[i])!=STN_PEER_OK){return STN_PEER_ARGUMENT;}
    }
    value.connector=*connector;*out=value;return STN_PEER_OK;
}
void stn_peer_outbound_close(stn_peer_outbound *out)
{
    if(out!=NULL && out->connected){out->connector.close(out->connector.user);out->connected=0;memset(&out->transport,0,sizeof(out->transport));}
}
stn_peer_status stn_peer_outbound_step(stn_peer_outbound *out,uint64_t now,
    const stn_chain_context *c,const stn_storage_provider *storage,stn_peer_workspace *w,stn_chain_state *active)
{
    stn_peer_status status;int established;stn_peer_endpoint selected;
    if(out==NULL || out->connector.open==NULL || out->connector.close==NULL || c==NULL || storage==NULL || w==NULL || active==NULL){return STN_PEER_ARGUMENT;}
    if(out->candidates.count==0){return STN_PEER_RETAINED;}
    if(out->started && (now<out->last_step || now-out->last_step<STN_PEER_OUTBOUND_INTERVAL_MS)){return STN_PEER_RETAINED;}
    out->started=1;out->last_step=now;established=out->connected;selected=out->candidates.entries[out->next];
    if(!out->connected){
        status=out->connector.open(out->connector.user,&out->candidates.entries[out->next],&out->transport);
        if(status!=STN_PEER_OK){
            out->connector.close(out->connector.user);memset(&out->transport,0,sizeof(out->transport));
            out->next=(out->next+1)%out->candidates.count;out->last_status=status;return status;
        }
    }
    status=sync_session(out->connected,&out->remote_capabilities,c,storage,&out->transport,w,active).status;
    if((status==STN_PEER_OK || status==STN_PEER_RETAINED) && !established){
        out->last_discovery=STN_PEER_RETAINED;
        if((out->remote_capabilities&2u)!=0){
            stn_peer_message evidence;
            out->last_discovery=exchange(&out->transport,w,STN_PEER_GET_PEERS,NULL,0,STN_PEER_PEERS,&evidence);
            if(out->last_discovery==STN_PEER_OK){
                out->last_discovery=stn_peer_discovery_admit(&out->candidates,out->self,evidence.payload,evidence.length);
                /* Insertion may shift the selected endpoint's index. */
                for(out->next=0;out->next<out->candidates.count;++out->next){
                    if(endpoint_order(&selected,&out->candidates.entries[out->next])==0){break;}
                }
            }
            if(out->last_discovery!=STN_PEER_OK && out->last_discovery!=STN_PEER_CAPACITY){status=out->last_discovery;}
        }
    }
    if(status==STN_PEER_OK || status==STN_PEER_RETAINED){out->connected=1;}
    else{
        out->connector.close(out->connector.user);memset(&out->transport,0,sizeof(out->transport));out->connected=0;
        out->next=(out->next+1)%out->candidates.count;
    }
    out->last_status=status;return status;
}

stn_peer_status stn_peer_discovery_admit(stn_peer_candidates *set,const stn_peer_endpoint *self,const uint8_t *bytes,size_t length)
{
    stn_peer_candidates ordered={0},merged;stn_peer_endpoint endpoint;size_t count,i;stn_peer_status status;
    if(set==NULL || bytes==NULL || (self!=NULL && !endpoint_valid(self))){return STN_PEER_ARGUMENT;}
    if(length<2 || length>STN_PEER_DISCOVERY_MAX){return STN_PEER_PROTOCOL;}
    count=(size_t)stn_wire_read(bytes,2);
    if(count>64 || length!=2+count*6){return STN_PEER_PROTOCOL;}
    for(i=0;i<count;++i){
        memcpy(endpoint.address,bytes+2+i*6,4);endpoint.port=(uint16_t)stn_wire_read(bytes+6+i*6,2);
        status=stn_peer_candidate_add(&ordered,&endpoint);
        if(status!=STN_PEER_OK && status!=STN_PEER_RETAINED){return STN_PEER_PROTOCOL;}
    }
    /* Validate current invariants even for an empty response. */
    if(set->count>64){return STN_PEER_ARGUMENT;}
    for(i=0;i<set->count;++i){
        if(!endpoint_valid(&set->entries[i]) || (i!=0 && endpoint_order(&set->entries[i-1],&set->entries[i])>=0)){return STN_PEER_ARGUMENT;}
    }
    merged=*set;
    for(i=0;i<ordered.count;++i){
        if(self!=NULL && endpoint_order(self,&ordered.entries[i])==0){continue;}
        status=stn_peer_candidate_add(&merged,&ordered.entries[i]);
        if(status!=STN_PEER_OK && status!=STN_PEER_RETAINED){return status;}
    }
    *set=merged;return STN_PEER_OK;
}
stn_peer_status stn_peer_discovery_encode(const stn_peer_candidates *set,const stn_peer_endpoint *self,uint8_t *bytes,size_t capacity,size_t *written)
{
    uint8_t payload[STN_PEER_DISCOVERY_MAX];size_t i,count=0;
    if(written!=NULL){*written=0;}
    if(set==NULL || bytes==NULL || written==NULL || set->count>64 || (self!=NULL && !endpoint_valid(self))){return STN_PEER_ARGUMENT;}
    for(i=0;i<set->count;++i){
        const stn_peer_endpoint *endpoint=&set->entries[i];
        if(!endpoint_valid(endpoint) || (i!=0 && endpoint_order(&set->entries[i-1],endpoint)>=0)){return STN_PEER_ARGUMENT;}
        if(self!=NULL && endpoint_order(self,endpoint)==0){continue;}
        memcpy(payload+2+count*6,endpoint->address,4);stn_wire_write(payload+6+count*6,2,endpoint->port);++count;
    }
    stn_wire_write(payload,2,count);
    if(capacity<2+count*6){return STN_PEER_CAPACITY;}
    memcpy(bytes,payload,2+count*6);*written=2+count*6;return STN_PEER_OK;
}
