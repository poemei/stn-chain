/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_lifecycle.h"
#include "stn_wire_internal.h"
#include <string.h>

static stn_lifecycle_result map_grant(stn_authority_grant_result r)
{ return r==STN_AUTHORITY_VALID_GRANT?STN_LIFECYCLE_OK:r==STN_AUTHORITY_MALFORMED_GRANT?STN_LIFECYCLE_MALFORMED:STN_LIFECYCLE_INVALID; }
static stn_lifecycle_result map_rev(stn_authority_revocation_result r)
{ return r==STN_AUTHORITY_VALID_REVOCATION?STN_LIFECYCLE_OK:r==STN_AUTHORITY_MALFORMED_REVOCATION?STN_LIFECYCLE_MALFORMED:STN_LIFECYCLE_INVALID; }
static stn_lifecycle_result map_rot(stn_identity_rotation_result r)
{ return r==STN_AUTHORITY_VALID_ROTATION?STN_LIFECYCLE_OK:r==STN_AUTHORITY_MALFORMED_ROTATION?STN_LIFECYCLE_MALFORMED:STN_LIFECYCLE_INVALID; }

void stn_lifecycle_initialize(stn_lifecycle_state *s,uint8_t *grant_bytes,size_t grant_capacity,uint8_t *replay_bytes,size_t replay_capacity,const uint8_t initial[32])
{
    if(s==NULL)return; memset(s,0,sizeof(*s)); s->grant_bytes=grant_bytes; s->grant_capacity=grant_capacity;
    memcpy(s->initial_identity,initial,32); stn_authority_state_initialize(&s->authority);
    stn_identity_rotation_initialize(&s->rotation,initial); stn_replay_state_initialize(&s->replay,replay_bytes,replay_capacity);
}
void stn_lifecycle_set_initial_identities(stn_lifecycle_state *s,const uint8_t *ids,size_t count)
{ if(s==NULL || count>STN_GENESIS_INITIAL_IDENTITY_MAX || (count!=0 && ids==NULL))return; s->initial_identity_count=count; if(count!=0)memcpy(s->initial_identities,ids,count*32); }
void stn_lifecycle_rebind(stn_lifecycle_state *s,uint8_t *grant_bytes,size_t grant_capacity,uint8_t *replay_bytes,size_t replay_capacity)
{ if(s!=NULL){s->grant_bytes=grant_bytes;s->grant_capacity=grant_capacity;s->replay.consumed=replay_bytes;s->replay.consumed_capacity=replay_capacity;} }

stn_lifecycle_result stn_lifecycle_replay_nonce(uint16_t type,const uint8_t *statement,size_t length,const stn_hash_provider *p,uint8_t nonce[32])
{
    static const uint8_t domain[] = STN_LIFECYCLE_REPLAY_DOMAIN;
    uint8_t input[sizeof(domain)+2u+STN_AUTHORITY_GRANT_DOMAIN_SIZE+STN_AUTHORITY_GRANT_SIZE]; size_t n=sizeof(domain)+2u;
    stn_data_status r;
    if(statement==NULL || nonce==NULL || p==NULL || p->hash==NULL || length>sizeof(input)-n)return STN_LIFECYCLE_ARGUMENT;
    memcpy(input,domain,sizeof(domain)); input[sizeof(domain)]=(uint8_t)(type>>8); input[sizeof(domain)+1]=(uint8_t)type; memcpy(input+n,statement,length); n+=length;
    r=p->hash(p->user,NULL,0,input,n,nonce); if(r!=STN_DATA_OK)return r==STN_DATA_UNRESOLVED?STN_LIFECYCLE_PROVIDER:STN_LIFECYCLE_PROVIDER;
    { size_t i;uint8_t v=0;for(i=0;i<32;++i)v|=nonce[i];if(v==0)return STN_LIFECYCLE_MALFORMED; }
    return STN_LIFECYCLE_OK;
}

static stn_lifecycle_result consume(stn_lifecycle_state *s,const uint8_t signer[32],uint16_t type,const uint8_t *statement,size_t length,const stn_hash_provider *p)
{
    uint8_t nonce[32],id[64];stn_lifecycle_result r;
    r=stn_lifecycle_replay_nonce(type,statement,length,p,nonce);if(r!=STN_LIFECYCLE_OK)return r;
    if(stn_replay_id_from_signer_nonce(signer,nonce,id)!=STN_REPLAY_FRESH)return STN_LIFECYCLE_MALFORMED;
    if(stn_replay_state_check(&s->replay,id)==STN_REPLAY_REPLAY)return STN_LIFECYCLE_REPLAY;
    if(stn_replay_state_consume(&s->replay,id)!=STN_REPLAY_FRESH)return STN_LIFECYCLE_CAPACITY;
    return STN_LIFECYCLE_OK;
}

static int find_grant(const stn_lifecycle_state *s,const uint8_t id[32],const stn_hash_provider *p,const uint8_t **grant)
{
    size_t i;uint8_t digest[32];
    for(i=0;i<s->grant_count;++i){const uint8_t *g=s->grant_bytes+i*STN_LIFECYCLE_MAX_GRANT_SIZE;if(stn_authority_grant_id(g,STN_AUTHORITY_GRANT_SIZE,p,digest)==STN_DATA_OK && memcmp(digest,id,32)==0){*grant=g;return 1;}}
    return 0;
}
static int initial_contains(const stn_lifecycle_state *s,const uint8_t id[32])
{ size_t i;for(i=0;i<s->initial_identity_count;++i)if(memcmp(s->initial_identities[i],id,32)==0)return 1;return 0; }

static stn_lifecycle_result consume_publication(stn_lifecycle_state *s,const stn_record *record,const stn_hash_provider *p)
{
    uint8_t id[64];
    if (stn_replay_id_from_signer_nonce(record->signer_public_key,record->nonce,id)!=STN_REPLAY_FRESH) return STN_LIFECYCLE_MALFORMED;
    if (stn_replay_state_check(&s->replay,id)==STN_REPLAY_REPLAY) return STN_LIFECYCLE_REPLAY;
    if (stn_replay_state_consume(&s->replay,id)!=STN_REPLAY_FRESH) return STN_LIFECYCLE_CAPACITY;
    (void)p;
    return STN_LIFECYCLE_OK;
}

stn_lifecycle_result stn_lifecycle_apply_transaction(stn_lifecycle_state *s,const stn_transaction *tx,const uint8_t *roots,size_t root_count,const stn_hash_provider *p)
{
    const uint8_t *b;size_t n;uint8_t statement[256],evidence[STN_AUTHORITY_EVIDENCE_SIZE],id[32];size_t written=0;stn_lifecycle_result r;
    if(s==NULL || tx==NULL || tx->record_bytes==NULL || p==NULL)return STN_LIFECYCLE_ARGUMENT;
    b=tx->record_bytes;n=tx->record_length;
    if(tx->type==STN_TX_AUTHORITY_GRANT){
        if(n!=STN_AUTHORITY_GRANT_SIZE || stn_authority_grant_validate(b,n,roots,root_count,evidence)!=STN_AUTHORITY_VALID_GRANT)return STN_LIFECYCLE_INVALID;
        if(stn_authority_grant_statement(b+1,evidence,statement,sizeof(statement),&written)!=STN_AUTHORITY_VALID_GRANT)return STN_LIFECYCLE_INVALID;
        if(s->grant_count>=s->grant_capacity || s->grant_bytes==NULL)return STN_LIFECYCLE_CAPACITY;
        r=consume(s,b+1,tx->type,statement,written,p);if(r!=STN_LIFECYCLE_OK)return r;
        memcpy(s->grant_bytes+s->grant_count*STN_LIFECYCLE_MAX_GRANT_SIZE,b,n);++s->grant_count;return STN_LIFECYCLE_OK;
    }
    if(tx->type==STN_TX_AUTHORITY_REVOKE){
        const uint8_t *grant=NULL;if(n!=STN_AUTHORITY_REVOCATION_SIZE || !find_grant(s,b+33,p,&grant))return STN_LIFECYCLE_INVALID;
        if(s->authority.revoked_count>=STN_AUTHORITY_MAX_REVOKED)return STN_LIFECYCLE_CAPACITY;
        if(stn_authority_revocation_validate(b,n,grant,STN_AUTHORITY_GRANT_SIZE,roots,root_count,p,id)!=STN_AUTHORITY_VALID_REVOCATION)return STN_LIFECYCLE_INVALID;
        if(stn_authority_revocation_statement(b+1,b+33,statement,sizeof(statement),&written)!=STN_AUTHORITY_VALID_REVOCATION)return STN_LIFECYCLE_INVALID;
        r=consume(s,b+1,tx->type,statement,written,p);if(r!=STN_LIFECYCLE_OK)return r;
        return map_rev(stn_authority_state_apply(&s->authority,b,n,grant,STN_AUTHORITY_GRANT_SIZE,roots,root_count,p));
    }
    if(tx->type==STN_TX_IDENTITY_ROTATE){
        uint8_t saved_identity[32];int seeded=0;
        if(n!=STN_AUTHORITY_ROTATION_SIZE)return STN_LIFECYCLE_INVALID;
        if(s->rotation.rotation_count==0){if(!initial_contains(s,b+1))return STN_LIFECYCLE_INVALID;memcpy(saved_identity,s->rotation.current_identity,32);memcpy(s->rotation.current_identity,b+1,32);seeded=1;}
        if(n!=STN_AUTHORITY_ROTATION_SIZE || stn_identity_rotation_validate(b,n,&s->rotation,roots,root_count)!=STN_AUTHORITY_VALID_ROTATION){if(seeded)memcpy(s->rotation.current_identity,saved_identity,32);return STN_LIFECYCLE_INVALID;}
        if(s->rotation.rotation_count>=STN_AUTHORITY_MAX_ROTATIONS)return STN_LIFECYCLE_CAPACITY;
        if(stn_identity_rotation_statement(b+1,b+33,statement,sizeof(statement),&written)!=STN_AUTHORITY_VALID_ROTATION)return STN_LIFECYCLE_INVALID;
        r=consume(s,b+1,tx->type,statement,written,p);if(r!=STN_LIFECYCLE_OK){if(seeded)memcpy(s->rotation.current_identity,saved_identity,32);return r;}
        return map_rot(stn_identity_rotation_apply(&s->rotation,b,n,roots,root_count));
    }
    if(tx->type==STN_TX_PUBLICATION){stn_record record;if(stn_record_decode(tx->record_bytes,tx->record_length,&record)!=STN_RECORD_OK)return STN_LIFECYCLE_MALFORMED;return consume_publication(s,&record,p);}
    return STN_LIFECYCLE_MALFORMED;
}

stn_lifecycle_result stn_lifecycle_apply_block(stn_lifecycle_state *s,const uint8_t *bytes,size_t length,const uint8_t *roots,size_t root_count,const stn_hash_provider *p)
{ stn_block b;size_t i,off=0;stn_transaction tx;stn_lifecycle_result r;if(s==NULL||bytes==NULL||stn_block_decode(bytes,length,&b)!=STN_DATA_OK)return STN_LIFECYCLE_MALFORMED;for(i=0;i<b.header.transaction_count;++i){size_t n=(size_t)stn_wire_read(b.body+off,4);off+=4;if(stn_transaction_decode(b.body+off,n,&tx)!=STN_DATA_OK)return STN_LIFECYCLE_MALFORMED;r=stn_lifecycle_apply_transaction(s,&tx,roots,root_count,p);if(r!=STN_LIFECYCLE_OK)return r;off+=n;}return STN_LIFECYCLE_OK; }

stn_lifecycle_result stn_lifecycle_rebuild(stn_lifecycle_state *s,const uint8_t *const *blocks,const size_t *lengths,size_t count,const uint8_t *roots,size_t root_count,const stn_hash_provider *p)
{
    size_t i,j,off;stn_block b;stn_transaction tx;stn_lifecycle_result r;
    if(s==NULL || (count!=0 && (blocks==NULL || lengths==NULL)))return STN_LIFECYCLE_ARGUMENT;
    stn_authority_state_initialize(&s->authority);stn_identity_rotation_initialize(&s->rotation,s->initial_identity);s->grant_count=0;s->replay.consumed_count=0;
    for(i=0;i<count;++i){if(stn_block_decode(blocks[i],lengths[i],&b)!=STN_DATA_OK)return STN_LIFECYCLE_MALFORMED;off=0;for(j=0;j<b.header.transaction_count;++j){size_t n=(size_t)stn_wire_read(b.body+off,4);off+=4;if(stn_transaction_decode(b.body+off,n,&tx)!=STN_DATA_OK)return STN_LIFECYCLE_MALFORMED;r=stn_lifecycle_apply_transaction(s,&tx,roots,root_count,p);if(r!=STN_LIFECYCLE_OK)return r;off+=n;}}
    return STN_LIFECYCLE_OK;
}

