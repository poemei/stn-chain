#include "stn_peer_snapshot_internal.h"
#include <stdlib.h>
#include <string.h>
void stn_peer_snapshot_release(stn_peer_snapshot *s)
{
    if(s){free(s->blocks);free(s->bytes);free(s);}
}
size_t stn_peer_snapshot_count(const stn_peer_snapshot *s){return s?s->count:0;}
stn_storage_status stn_peer_snapshot_load(stn_storage_cache *cache,
    const stn_chain_context *context,const stn_storage_provider *provider,
    stn_peer_snapshot **out)
{
    stn_storage_status status;stn_storage_view view={0};
    stn_peer_snapshot *s;uint8_t probe;size_t needed=0;
    if(!context||!provider||!provider->acquire||!provider->release||
       !provider->read||!provider->replace||!out)return STN_STORAGE_ARGUMENT;
    status=provider->acquire(provider->user);if(status!=STN_STORAGE_OK)return status;
    status=provider->read(provider->user,&probe,0,&needed);
    /* Keep exclusion through size/read to avoid growth races. Runtime serializes
     * cache state ownership in addition to the provider's disk exclusion. */
    if((status!=STN_STORAGE_OK&&status!=STN_STORAGE_CAPACITY)||!needed){
        provider->release(provider->user);return status==STN_STORAGE_OK?STN_STORAGE_FORMAT:status;
    }
    s=calloc(1,sizeof(*s));
    if(!s){provider->release(provider->user);return STN_STORAGE_CAPACITY;}
    s->bytes=malloc(needed);
    if(!s->bytes){provider->release(provider->user);free(s);return STN_STORAGE_CAPACITY;}
    status=stn_storage_cache_load_locked(cache,context,provider,s->bytes,needed,&view);
    provider->release(provider->user);
    if(status!=STN_STORAGE_OK){stn_peer_snapshot_release(s);return status;}
    s->context=*context;s->count=view.count;s->blocks=view.blocks;view.blocks=NULL;
    s->summary.height=view.state.height;
    memcpy(s->summary.tip_id,view.state.tip_id,32);
    s->summary.cumulative_work=view.state.cumulative_work;
    stn_storage_view_release(&view);
    *out=s;return STN_STORAGE_OK;
}
