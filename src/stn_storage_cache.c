/* Copyright (c) 2026 STN-Labz. */
#include "stn_storage_cache.h"
#include <stdlib.h>
#include "stn_wire_internal.h"
#include <string.h>
void stn_storage_cache_release(stn_storage_cache *c)
{
    if(!c)return;
    stn_storage_view_release(&c->view);free(c->bytes);memset(c,0,sizeof(*c));
}
static stn_storage_status copy_view(const stn_storage_view *from,
    const uint8_t *old_bytes,uint8_t *new_bytes,stn_storage_view *out)
{
    stn_storage_view v={0};size_t i;
    v.blocks=calloc(from->count,sizeof(*v.blocks));
    if(!v.blocks)return STN_STORAGE_CAPACITY;
    v.count=from->count;
    for(i=0;i<v.count;i++){
        v.blocks[i].bytes=new_bytes+(from->blocks[i].bytes-old_bytes);
        v.blocks[i].length=from->blocks[i].length;
    }
    if(stn_chain_state_share(&from->state,&v.state)!=STN_DATA_OK){
        free(v.blocks);return STN_STORAGE_CAPACITY;
    }
    *out=v;return STN_STORAGE_OK;
}
void stn_storage_cache_remember(stn_storage_cache *c,
    const stn_chain_context *context,const uint8_t *bytes,size_t n,
    const stn_chain_state *state)
{
    size_t count,i,offset=12,length;
    if(!c)return;
    stn_storage_cache_release(c);
    if(!context||!bytes||!state||n<STN_STORAGE_OVERHEAD)return;
    count=(size_t)stn_wire_read(bytes+8,4);
    if(!count||count>(n-STN_STORAGE_OVERHEAD)/(4u+STN_BLOCK_HEADER_SIZE))return;
    c->bytes=malloc(n);c->view.blocks=calloc(count,sizeof(*c->view.blocks));
    if(!c->bytes||!c->view.blocks)goto fail;
    memcpy(c->bytes,bytes,n);c->view.count=count;
    for(i=0;i<count;i++){
        if(offset>n-32||n-32-offset<4)goto fail;
        length=(size_t)stn_wire_read(bytes+offset,4);offset+=4;
        if(length<STN_BLOCK_HEADER_SIZE||length>STN_BLOCK_MAX_SIZE||length>n-32-offset)goto fail;
        c->view.blocks[i].bytes=c->bytes+offset;c->view.blocks[i].length=length;offset+=length;
    }
    if(offset!=n-32||stn_chain_state_share(state,&c->view.state)!=STN_DATA_OK)goto fail;
    c->length=n;c->context=*context;return;
fail:stn_storage_cache_release(c);
}
stn_storage_status stn_storage_cache_load_locked(stn_storage_cache *c,
    const stn_chain_context *context,const stn_storage_provider *p,
    uint8_t *scratch,size_t capacity,stn_storage_view *out)
{
    stn_storage_status status;size_t n=0;
    if(!context||!p||!p->read||!scratch||!out)return STN_STORAGE_ARGUMENT;
    status=p->read(p->user,scratch,capacity,&n);
    if(status==STN_STORAGE_OK&&n>capacity)status=STN_STORAGE_IO;
    if(status!=STN_STORAGE_OK)return status;
    if(c&&c->bytes&&c->length==n&&!memcmp(&c->context,context,sizeof(*context))&&
       !memcmp(c->bytes,scratch,n))return copy_view(&c->view,c->bytes,scratch,out);
    stn_storage_cache_release(c);
    status=stn_storage_decode(context,scratch,n,out);
    if(status==STN_STORAGE_OK)stn_storage_cache_remember(c,context,scratch,n,&out->state);
    return status;
}
stn_storage_status stn_storage_cache_load(stn_storage_cache *c,
    const stn_chain_context *context,const stn_storage_provider *p,
    uint8_t *scratch,size_t capacity,stn_storage_view *out)
{
    stn_storage_status status;
    if(!c)return stn_storage_load(context,p,scratch,capacity,out);
    if(!context||!p||!p->acquire||!p->release||!p->read||!p->replace||!scratch||!out)
        return STN_STORAGE_ARGUMENT;
    status=p->acquire(p->user);if(status!=STN_STORAGE_OK)return status;
    status=stn_storage_cache_load_locked(c,context,p,scratch,capacity,out);
    p->release(p->user);return status;
}
