/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_storage.h"
#include "stn_wire_internal.h"
#include <string.h>
#include <stdlib.h>

static stn_storage_status digest(const stn_chain_context *c,const uint8_t *p,size_t n,uint8_t out[32])
{
    static const uint8_t domain[]="STN-CHAIN:STORAGE:1";
    stn_data_status s;
    if(c->hash_provider.hash==NULL) { return STN_STORAGE_UNRESOLVED; }
    s=c->hash_provider.hash(c->hash_provider.user,domain,sizeof(domain),p,n,out);
    return s==STN_DATA_OK ? STN_STORAGE_OK : s==STN_DATA_UNRESOLVED ? STN_STORAGE_UNRESOLVED : STN_STORAGE_IO;
}
static stn_storage_status validate(const stn_chain_context *c,const stn_block_span *b,size_t n,stn_chain_state *out)
{
    stn_chain_state state,next;size_t i;stn_chain_report r;
    if(c==NULL || b==NULL || n==0 || c->pow_policy==NULL || out==NULL) { return STN_STORAGE_ARGUMENT; }
    if(stn_chain_initialize(c,&state)!=STN_DATA_OK) { return STN_STORAGE_VALIDATION; }
    for(i=0;i<n;++i) {
        r=stn_chain_validate_candidate(c,&state,b[i].bytes,b[i].length,&next);
        if(r.acceptance==STN_ACCEPTANCE_UNRESOLVED) { return STN_STORAGE_UNRESOLVED; }
        if(r.acceptance!=STN_ACCEPTANCE_UNDER_CONTEXT) { return STN_STORAGE_VALIDATION; }
        state=next;
    }
    *out=state;return STN_STORAGE_OK;
}
void stn_storage_view_release(stn_storage_view *v)
{
    if(v!=NULL){free(v->blocks);memset(v,0,sizeof(*v));}
}
stn_storage_status stn_storage_decode(const stn_chain_context *c,const uint8_t *p,size_t n,stn_storage_view *out)
{
    stn_storage_view v={0};size_t i,offset=12,length;uint8_t hash[32];stn_storage_status s;uint64_t count64;
    if(c==NULL || p==NULL || out==NULL) { return STN_STORAGE_ARGUMENT; }
    if(n<STN_STORAGE_OVERHEAD || memcmp(p,"STNS",4)!=0 ||
        stn_wire_read(p+4,2)!=1 || stn_wire_read(p+6,2)!=0) { return STN_STORAGE_FORMAT; }
    count64=stn_wire_read(p+8,4);
    if(count64==0 || count64>SIZE_MAX/sizeof(stn_block_span)) { return STN_STORAGE_FORMAT; }
    v.count=(size_t)count64;
    /* Every record needs at least a length word plus the minimum legal block. */
    if(v.count>(n-STN_STORAGE_OVERHEAD)/(4u+STN_BLOCK_HEADER_SIZE+STN_BLOCK_MIN_BODY)) { return STN_STORAGE_FORMAT; }
    v.blocks=(stn_block_span*)calloc(v.count,sizeof(*v.blocks));
    if(v.blocks==NULL) { return STN_STORAGE_CAPACITY; }
    for(i=0;i<v.count;++i) {
        if(offset>n-32 || n-32-offset<4) { s=STN_STORAGE_FORMAT;goto fail; }
        length=(size_t)stn_wire_read(p+offset,4);offset+=4;
        if(length<STN_BLOCK_HEADER_SIZE+STN_BLOCK_MIN_BODY || length>STN_BLOCK_MAX_SIZE ||
           offset>n-32 || length>n-32-offset) { s=STN_STORAGE_FORMAT;goto fail; }
        v.blocks[i].bytes=p+offset;v.blocks[i].length=length;offset+=length;
    }
    if(offset!=n-32) { s=STN_STORAGE_FORMAT;goto fail; }
    s=digest(c,p,offset,hash);if(s!=STN_STORAGE_OK) { goto fail; }
    if(memcmp(hash,p+offset,32)!=0) { s=STN_STORAGE_FORMAT;goto fail; }
    s=validate(c,v.blocks,v.count,&v.state);if(s!=STN_STORAGE_OK) { goto fail; }
    *out=v;return STN_STORAGE_OK;
fail:
    stn_storage_view_release(&v);return s;
}
stn_storage_status stn_storage_encode(const stn_chain_context *c,const stn_block_span *b,size_t n,
    uint8_t *p,size_t capacity,size_t *written)
{
    size_t i,total=STN_STORAGE_OVERHEAD,offset=12;stn_chain_state state;stn_storage_status s;
    /* Caller scratch is allowed to change after successful validation if the
     * hash provider fails; encoded bytes are not published without written. */
    uint8_t hash[32];
    if(written!=NULL) { *written=0; }
    if(p==NULL || written==NULL) { return STN_STORAGE_ARGUMENT; }
    if(n>UINT32_MAX){return STN_STORAGE_CAPACITY;}
    s=validate(c,b,n,&state);if(s!=STN_STORAGE_OK) { return s; }
    for(i=0;i<n;++i) {
        if(total>SIZE_MAX-4u || b[i].length>SIZE_MAX-total-4u){return STN_STORAGE_CAPACITY;}
        total+=4u+b[i].length;
    }
    if(total>capacity) { return STN_STORAGE_CAPACITY; }
    memcpy(p,"STNS",4);stn_wire_write(p+4,2,1);stn_wire_write(p+6,2,0);stn_wire_write(p+8,4,n);
    for(i=0;i<n;++i) { stn_wire_write(p+offset,4,b[i].length);offset+=4;memcpy(p+offset,b[i].bytes,b[i].length);offset+=b[i].length; }
    s=digest(c,p,offset,hash);if(s!=STN_STORAGE_OK) { return s; }
    memcpy(p+offset,hash,32);*written=total;return STN_STORAGE_OK;
}
static int provider_valid(const stn_storage_provider *p)
{
    return p!=NULL && p->acquire!=NULL && p->release!=NULL && p->read!=NULL && p->replace!=NULL;
}
static stn_storage_status io_status(stn_storage_status s)
{
    switch(s) {
    case STN_STORAGE_OK:case STN_STORAGE_IO:case STN_STORAGE_NOT_FOUND:
    case STN_STORAGE_BUSY:case STN_STORAGE_CAPACITY: return s;
    default:return STN_STORAGE_IO;
    }
}
static stn_storage_status read_locked(const stn_chain_context *c,const stn_storage_provider *p,
    uint8_t *scratch,size_t capacity,stn_storage_view *out)
{
    size_t n=0;stn_storage_status s;
    s=io_status(p->read(p->user,scratch,capacity,&n));
    if(s!=STN_STORAGE_OK) { return s; }
    if(n>capacity) { return STN_STORAGE_IO; }
    return stn_storage_decode(c,scratch,n,out);
}
stn_storage_status stn_storage_load(const stn_chain_context *c,const stn_storage_provider *p,
    uint8_t *scratch,size_t capacity,stn_storage_view *out)
{
    stn_storage_status s;
    if(c==NULL || !provider_valid(p) || scratch==NULL || out==NULL) { return STN_STORAGE_ARGUMENT; }
    s=io_status(p->acquire(p->user));if(s!=STN_STORAGE_OK) { return s; }
    s=read_locked(c,p,scratch,capacity,out);p->release(p->user);return s;
}
static int state_equal(const stn_chain_state *a,const stn_chain_state *b)
{
    return a->height==b->height && a->timestamp==b->timestamp && a->has_tip==b->has_tip &&
        memcmp(a->network_id,b->network_id,32)==0 && memcmp(a->genesis_id,b->genesis_id,32)==0 &&
        memcmp(a->tip_id,b->tip_id,32)==0 && memcmp(a->current_target,b->current_target,32)==0 &&
        memcmp(a->cumulative_work.bytes,b->cumulative_work.bytes,32)==0;
}
static int plan_equal(const stn_reorg_plan *a,const stn_reorg_plan *b)
{
    return a->actionable==b->actionable && a->ancestor_index==b->ancestor_index &&
        memcmp(a->ancestor_id,b->ancestor_id,32)==0 && a->detach_begin==b->detach_begin &&
        a->detach_end==b->detach_end && a->attach_begin==b->attach_begin && a->attach_end==b->attach_end &&
        a->detached_count==b->detached_count && a->attached_count==b->attached_count &&
        a->resulting_height==b->resulting_height && state_equal(&a->current,&b->current) && state_equal(&a->candidate,&b->candidate);
}
stn_storage_status stn_storage_create(const stn_chain_context *c,const stn_storage_provider *p,
    const stn_block_span *b,size_t n,uint8_t *scratch,size_t capacity,stn_chain_state *active)
{
    stn_storage_status s;stn_chain_state state;size_t length=0;
    if(c==NULL || !provider_valid(p) || scratch==NULL || active==NULL) { return STN_STORAGE_ARGUMENT; }
    s=validate(c,b,n,&state);if(s!=STN_STORAGE_OK) { return s; }
    s=io_status(p->acquire(p->user));if(s!=STN_STORAGE_OK) { return s; }
    s=io_status(p->read(p->user,scratch,0,&length));
    if(s!=STN_STORAGE_NOT_FOUND) { p->release(p->user);return s==STN_STORAGE_OK || s==STN_STORAGE_CAPACITY ? STN_STORAGE_EXISTS : s; }
    s=stn_storage_encode(c,b,n,scratch,capacity,&length);
    if(s==STN_STORAGE_OK) { s=io_status(p->replace(p->user,scratch,length)); }
    if(s==STN_STORAGE_OK) { *active=state; }
    p->release(p->user);return s;
}
stn_storage_status stn_storage_apply(const stn_chain_context *c,const stn_storage_provider *p,
    const stn_block_span *b,size_t n,const stn_reorg_plan *plan,stn_storage_workspace *w,stn_chain_state *active)
{
    stn_storage_status s;stn_storage_view current={0};stn_reorg_plan fresh;stn_fork_report r;size_t length;
    if(c==NULL || !provider_valid(p) || b==NULL || plan==NULL || w==NULL || active==NULL ||
        w->current_bytes==NULL || w->next_bytes==NULL) { return STN_STORAGE_ARGUMENT; }
    s=io_status(p->acquire(p->user));if(s!=STN_STORAGE_OK) { return s; }
    s=read_locked(c,p,w->current_bytes,w->current_capacity,&current);
    if(s!=STN_STORAGE_OK) { goto done; }
    if(!state_equal(active,&current.state)) { s=STN_STORAGE_STALE;goto done; }
    r=stn_fork_evaluate_history(c,current.blocks,current.count,b,n,&fresh);
    if(r.result!=STN_FORK_CANDIDATE) {
        s=r.result==STN_FORK_CURRENT || r.result==STN_FORK_TIE ? STN_STORAGE_NOT_PREFERRED :
            r.result==STN_FORK_UNRESOLVED ? STN_STORAGE_UNRESOLVED : STN_STORAGE_VALIDATION;
        goto done;
    }
    if(!plan_equal(plan,&fresh)) { s=STN_STORAGE_STALE;goto done; }
    s=stn_storage_encode(c,b,n,w->next_bytes,w->next_capacity,&length);
    if(s==STN_STORAGE_OK) { s=io_status(p->replace(p->user,w->next_bytes,length)); }
    if(s==STN_STORAGE_OK) { *active=fresh.candidate; }
done:
    stn_storage_view_release(&current);p->release(p->user);return s;
}

stn_storage_status stn_storage_extend(const stn_chain_context *c,const stn_storage_provider *p,
    const uint8_t *block,size_t block_length,stn_storage_workspace *w,stn_chain_state *active)
{
    stn_storage_status s;stn_storage_view current={0};stn_chain_state next;stn_chain_report r;
    size_t encoded=0,offset,i;uint8_t hash[32];
    if(c==NULL || !provider_valid(p) || block==NULL || w==NULL || active==NULL ||
       w->current_bytes==NULL || w->next_bytes==NULL){return STN_STORAGE_ARGUMENT;}
    s=io_status(p->acquire(p->user));if(s!=STN_STORAGE_OK){return s;}
    s=read_locked(c,p,w->current_bytes,w->current_capacity,&current);
    if(s!=STN_STORAGE_OK){goto done;}
    if(!state_equal(active,&current.state)){s=STN_STORAGE_STALE;goto done;}
    r=stn_chain_validate_candidate(c,&current.state,block,block_length,&next);
    if(r.acceptance==STN_ACCEPTANCE_UNRESOLVED){s=STN_STORAGE_UNRESOLVED;goto done;}
    if(r.acceptance!=STN_ACCEPTANCE_UNDER_CONTEXT){s=STN_STORAGE_VALIDATION;goto done;}
    if(current.count>=UINT32_MAX){s=STN_STORAGE_CAPACITY;goto done;}
    offset=12;
    for(i=0;i<current.count;++i){
        if(offset>SIZE_MAX-4 || current.blocks[i].length>SIZE_MAX-offset-4){s=STN_STORAGE_CAPACITY;goto done;}
        offset+=4+current.blocks[i].length;
    }
    if(offset>SIZE_MAX-36 || block_length>SIZE_MAX-offset-36){s=STN_STORAGE_CAPACITY;goto done;}
    encoded=offset+4+block_length+32;
    if(encoded>w->next_capacity){s=STN_STORAGE_CAPACITY;goto done;}
    /* The prefix was validated under this lock. Do not reconstruct and
     * revalidate a second full history to append one validated candidate. */
    memcpy(w->next_bytes,w->current_bytes,offset);
    stn_wire_write(w->next_bytes+8,4,current.count+1);
    stn_wire_write(w->next_bytes+offset,4,block_length);
    memcpy(w->next_bytes+offset+4,block,block_length);
    s=digest(c,w->next_bytes,encoded-32,hash);
    if(s==STN_STORAGE_OK){memcpy(w->next_bytes+encoded-32,hash,32);}
    if(s==STN_STORAGE_OK){s=io_status(p->replace(p->user,w->next_bytes,encoded));}
    if(s==STN_STORAGE_OK){*active=next;}
done:
    stn_storage_view_release(&current);p->release(p->user);return s;
}

static stn_storage_status recovery_locked(const stn_chain_context *c,const stn_storage_provider *p,
    uint8_t *scratch,size_t capacity,stn_storage_view *out,int *recovery)
{
    stn_storage_view v={0};size_t n=0,count=0,offset=12,i;stn_storage_status s;
    if(c->pow_policy==NULL || stn_chain_initialize(c,&v.state)!=STN_DATA_OK) { return STN_STORAGE_VALIDATION; }
    s=io_status(p->read(p->user,scratch,capacity,&n));
    if(s==STN_STORAGE_NOT_FOUND) { *out=v;*recovery=1;return STN_STORAGE_OK; }
    if(s!=STN_STORAGE_OK) { return s; }
    if(n>capacity) { return STN_STORAGE_IO; }
    s=stn_storage_decode(c,scratch,n,&v);
    if(s==STN_STORAGE_OK) { *out=v;*recovery=0;return STN_STORAGE_OK; }
    if(s!=STN_STORAGE_FORMAT && s!=STN_STORAGE_VALIDATION) { return s; }
    /* Wrong/unknown framing is not guessed. Do not interpret unsupported versions. */
    if(n>=12 && memcmp(scratch,"STNS",4)==0 && stn_wire_read(scratch+4,2)==1 && stn_wire_read(scratch+6,2)==0) {
        count=(size_t)stn_wire_read(scratch+8,4);
        if(count!=0 && count<=SIZE_MAX/sizeof(stn_block_span) &&
           n>=12) {
            /* A truncated tail must not discard a valid prefix. Allocate only
             * what the available bytes could possibly contain. */
            if(count>(n-12)/(4u+STN_BLOCK_HEADER_SIZE+STN_BLOCK_MIN_BODY)){count=(n-12)/(4u+STN_BLOCK_HEADER_SIZE+STN_BLOCK_MIN_BODY);}
            v.blocks=count ? (stn_block_span*)calloc(count,sizeof(*v.blocks)) : NULL;
            if(count!=0 && v.blocks==NULL){return STN_STORAGE_CAPACITY;}
        } else { count=0; }
    }
    for(i=0;i<count;++i) {
        size_t length;stn_chain_report r;
        if(offset>n || n-offset<4) { break; }
        length=(size_t)stn_wire_read(scratch+offset,4);offset+=4;
        if(length>STN_BLOCK_MAX_SIZE || length>n-offset) { break; }
        r=stn_chain_validate_candidate(c,&v.state,scratch+offset,length,&v.state);
        if(r.acceptance==STN_ACCEPTANCE_UNRESOLVED) { stn_storage_view_release(&v);return STN_STORAGE_UNRESOLVED; }
        if(r.acceptance==STN_ACCEPTANCE_ERROR) { stn_storage_view_release(&v);return STN_STORAGE_IO; }
        if(r.acceptance!=STN_ACCEPTANCE_UNDER_CONTEXT) { break; }
        v.blocks[v.count].bytes=scratch+offset;v.blocks[v.count].length=length;++v.count;offset+=length;
    }
    *out=v;*recovery=1;return STN_STORAGE_OK;
}
stn_storage_status stn_storage_recovery_read(const stn_chain_context *c,const stn_storage_provider *p,
    uint8_t *scratch,size_t capacity,stn_storage_view *out,int *recovery)
{
    stn_storage_status s;
    if(c==NULL || !provider_valid(p) || scratch==NULL || out==NULL || recovery==NULL) { return STN_STORAGE_ARGUMENT; }
    s=io_status(p->acquire(p->user));if(s!=STN_STORAGE_OK) { return s; }
    s=recovery_locked(c,p,scratch,capacity,out,recovery);p->release(p->user);return s;
}
stn_storage_status stn_storage_adopt(const stn_chain_context *c,const stn_storage_provider *p,
    const stn_block_span *b,size_t n,stn_storage_workspace *w,stn_chain_state *active)
{
    stn_storage_status s;stn_storage_view local={0};stn_chain_state candidate;int recovery;
    stn_reorg_plan plan;stn_fork_report r;size_t length;
    if(c==NULL || !provider_valid(p) || w==NULL || active==NULL || w->current_bytes==NULL || w->next_bytes==NULL) { return STN_STORAGE_ARGUMENT; }
    s=validate(c,b,n,&candidate);if(s!=STN_STORAGE_OK) { return s; }
    s=io_status(p->acquire(p->user));if(s!=STN_STORAGE_OK) { return s; }
    s=recovery_locked(c,p,w->current_bytes,w->current_capacity,&local,&recovery);
    if(s!=STN_STORAGE_OK) { goto finished; }
    if(local.count!=0) {
        r=stn_fork_evaluate_history(c,local.blocks,local.count,b,n,&plan);
        if(r.result!=STN_FORK_CANDIDATE && !(recovery && r.result==STN_FORK_TIE &&
            memcmp(plan.current.tip_id,plan.candidate.tip_id,32)==0)) {
            s=r.result==STN_FORK_CURRENT || r.result==STN_FORK_TIE ? STN_STORAGE_NOT_PREFERRED :
                r.result==STN_FORK_UNRESOLVED ? STN_STORAGE_UNRESOLVED : STN_STORAGE_VALIDATION;
            goto finished;
        }
    }
    s=stn_storage_encode(c,b,n,w->next_bytes,w->next_capacity,&length);
    if(s==STN_STORAGE_OK) { s=io_status(p->replace(p->user,w->next_bytes,length)); }
    if(s==STN_STORAGE_OK) { *active=candidate; }
finished:
    stn_storage_view_release(&local);p->release(p->user);return s;
}
