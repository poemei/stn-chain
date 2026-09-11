/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_authority.h"
#include <string.h>

int stn_ed25519_verify(const uint8_t *,size_t,const uint8_t[32],const uint8_t[64]);

static int id_compare(const uint8_t *a,const uint8_t *b){return memcmp(a,b,32);}

static int nonzero(const uint8_t *p,size_t n)
{
    size_t i; uint8_t v=0;
    if(p==NULL){return 0;}
    for(i=0;i<n;++i){v|=p[i];}
    return v!=0;
}
static int token_valid(const uint8_t *p,size_t n)
{
    return p!=NULL && n>=2 && p[0]==STN_AUTHORITY_VERSION && nonzero(p+1,n-1);
}
stn_authority_result stn_authority_action_validate(const uint8_t action[32])
{
    return token_valid(action,32) ? STN_AUTHORITY_AUTHORIZED : STN_AUTHORITY_MALFORMED;
}
stn_authority_result stn_authority_context_validate(const uint8_t context[32])
{
    return token_valid(context,32) ? STN_AUTHORITY_AUTHORIZED : STN_AUTHORITY_MALFORMED;
}
stn_authority_result stn_authority_evidence_validate(const uint8_t *evidence,size_t length)
{
    uint8_t identity[32];
    if(evidence==NULL || length!=STN_AUTHORITY_EVIDENCE_SIZE || evidence[0]!=STN_AUTHORITY_VERSION){return STN_AUTHORITY_MALFORMED;}
    if(stn_identity_derive(evidence+1,identity)!=STN_IDENTITY_VALID ||
       stn_authority_action_validate(evidence+33)!=STN_AUTHORITY_AUTHORIZED ||
       stn_authority_context_validate(evidence+65)!=STN_AUTHORITY_AUTHORIZED){return STN_AUTHORITY_MALFORMED;}
    return STN_AUTHORITY_AUTHORIZED;
}
stn_authority_result stn_authority_root_set_validate(const uint8_t *roots,size_t count)
{
    size_t i;
    if(count>STN_AUTHORITY_MAX_ROOTS || (count!=0 && roots==NULL)){return STN_AUTHORITY_MALFORMED;}
    for(i=0;i<count;++i){uint8_t id[32];if(stn_identity_derive(roots+i*32,id)!=STN_IDENTITY_VALID){return STN_AUTHORITY_MALFORMED;}if(i!=0 && memcmp(roots+(i-1)*32,roots+i*32,32)>=0){return STN_AUTHORITY_MALFORMED;}}
    return STN_AUTHORITY_AUTHORIZED;
}
static int root_contains(const uint8_t *roots,size_t count,const uint8_t issuer[32])
{
    size_t i;for(i=0;i<count;++i){if(memcmp(roots+i*32,issuer,32)==0){return 1;}}return 0;
}
stn_authority_grant_result stn_authority_grant_statement(const uint8_t issuer[32],const uint8_t evidence[97],uint8_t *statement,size_t capacity,size_t *written)
{
    uint8_t id[32];size_t n=STN_AUTHORITY_GRANT_DOMAIN_SIZE+1u+32u+97u;
    if(written!=NULL){*written=0;}
    if(statement==NULL || written==NULL || capacity<n || stn_identity_derive(issuer,id)!=STN_IDENTITY_VALID || stn_authority_evidence_validate(evidence,97)!=STN_AUTHORITY_AUTHORIZED){return STN_AUTHORITY_MALFORMED_GRANT;}
    memcpy(statement,STN_AUTHORITY_GRANT_DOMAIN,STN_AUTHORITY_GRANT_DOMAIN_SIZE-1u);statement[STN_AUTHORITY_GRANT_DOMAIN_SIZE-1u]=0;
    statement[STN_AUTHORITY_GRANT_DOMAIN_SIZE-1u+1u]=STN_AUTHORITY_VERSION;
    memcpy(statement+STN_AUTHORITY_GRANT_DOMAIN_SIZE+1u,id,32);memcpy(statement+STN_AUTHORITY_GRANT_DOMAIN_SIZE+33u,evidence,97);*written=n;return STN_AUTHORITY_VALID_GRANT;
}
stn_authority_grant_result stn_authority_grant_encode(const uint8_t issuer[32],const uint8_t evidence[97],const uint8_t signature[64],uint8_t *grant,size_t capacity,size_t *written)
{
    uint8_t statement[STN_AUTHORITY_GRANT_DOMAIN_SIZE+130u];size_t n=0;
    if(written!=NULL){*written=0;}
    if(grant==NULL || signature==NULL || written==NULL || capacity<STN_AUTHORITY_GRANT_SIZE || stn_authority_grant_statement(issuer,evidence,statement,sizeof(statement),&n)!=STN_AUTHORITY_VALID_GRANT){return STN_AUTHORITY_MALFORMED_GRANT;}
    memcpy(grant,statement+STN_AUTHORITY_GRANT_DOMAIN_SIZE,130);memcpy(grant+130,signature,64);*written=STN_AUTHORITY_GRANT_SIZE;return STN_AUTHORITY_VALID_GRANT;
}
stn_authority_grant_result stn_authority_grant_validate(const uint8_t *grant,size_t length,const uint8_t *roots,size_t count,uint8_t evidence[97])
{
    uint8_t issuer[32],statement[STN_AUTHORITY_GRANT_DOMAIN_SIZE+130u];size_t n=0;
    if(grant==NULL || evidence==NULL || length!=STN_AUTHORITY_GRANT_SIZE || grant[0]!=STN_AUTHORITY_VERSION || stn_authority_root_set_validate(roots,count)!=STN_AUTHORITY_AUTHORIZED){return STN_AUTHORITY_MALFORMED_GRANT;}
    memcpy(issuer,grant+1,32);memcpy(evidence,grant+33,97);
    if(stn_identity_derive(issuer,issuer)!=STN_IDENTITY_VALID || stn_authority_evidence_validate(evidence,97)!=STN_AUTHORITY_AUTHORIZED){return STN_AUTHORITY_MALFORMED_GRANT;}
    if(stn_authority_grant_statement(issuer,evidence,statement,sizeof(statement),&n)!=STN_AUTHORITY_VALID_GRANT){return STN_AUTHORITY_MALFORMED_GRANT;}
    if(!root_contains(roots,count,issuer) || stn_ed25519_verify(statement,n,issuer,grant+130)!=0){return STN_AUTHORITY_INVALID_GRANT;}
    return STN_AUTHORITY_VALID_GRANT;
}
stn_data_status stn_authority_grant_id(const uint8_t *grant,size_t length,const stn_hash_provider *provider,uint8_t id[32])
{
    uint8_t temp[32];stn_data_status s;
    if(id==NULL || grant==NULL || length!=STN_AUTHORITY_GRANT_SIZE){return STN_DATA_ARGUMENT;}
    if(grant[0]!=STN_AUTHORITY_VERSION || stn_identity_derive(grant+1,temp)!=STN_IDENTITY_VALID || stn_authority_evidence_validate(grant+33,STN_AUTHORITY_EVIDENCE_SIZE)!=STN_AUTHORITY_AUTHORIZED){return STN_DATA_CONTENT;}
    if(provider==NULL || provider->hash==NULL){return STN_DATA_UNRESOLVED;}
    s=provider->hash(provider->user,NULL,0,grant,length,temp);
    if(s==STN_DATA_OK){memcpy(id,temp,32);}return s==STN_DATA_OK||s==STN_DATA_UNRESOLVED||s==STN_DATA_ARGUMENT?s:STN_DATA_PROVIDER_ERROR;
}
stn_authority_revocation_result stn_authority_revocation_statement(const uint8_t issuer[32],const uint8_t id[32],uint8_t *statement,size_t capacity,size_t *written)
{
    uint8_t canonical[32];size_t n=STN_AUTHORITY_REVOKE_DOMAIN_SIZE+1u+64u;
    if(written!=NULL){*written=0;}
    if(statement==NULL || written==NULL || capacity<n || id==NULL || stn_identity_derive(issuer,canonical)!=STN_IDENTITY_VALID || !nonzero(id,32)){return STN_AUTHORITY_MALFORMED_REVOCATION;}
    memcpy(statement,STN_AUTHORITY_REVOKE_DOMAIN,STN_AUTHORITY_REVOKE_DOMAIN_SIZE-1u);statement[STN_AUTHORITY_REVOKE_DOMAIN_SIZE-1u]=0;statement[STN_AUTHORITY_REVOKE_DOMAIN_SIZE]=STN_AUTHORITY_VERSION;memcpy(statement+STN_AUTHORITY_REVOKE_DOMAIN_SIZE+1u,canonical,32);memcpy(statement+STN_AUTHORITY_REVOKE_DOMAIN_SIZE+33u,id,32);*written=n;return STN_AUTHORITY_VALID_REVOCATION;
}
stn_authority_revocation_result stn_authority_revocation_encode(const uint8_t issuer[32],const uint8_t id[32],const uint8_t signature[64],uint8_t *revocation,size_t capacity,size_t *written)
{
    uint8_t statement[STN_AUTHORITY_REVOKE_DOMAIN_SIZE+65u];size_t n=0;
    if(written!=NULL){*written=0;}
    if(revocation==NULL || signature==NULL || written==NULL || capacity<STN_AUTHORITY_REVOCATION_SIZE || stn_authority_revocation_statement(issuer,id,statement,sizeof(statement),&n)!=STN_AUTHORITY_VALID_REVOCATION){return STN_AUTHORITY_MALFORMED_REVOCATION;}
    revocation[0]=STN_AUTHORITY_VERSION;memcpy(revocation+1,issuer,32);memcpy(revocation+33,id,32);memcpy(revocation+65,signature,64);*written=STN_AUTHORITY_REVOCATION_SIZE;return STN_AUTHORITY_VALID_REVOCATION;
}
stn_authority_revocation_result stn_authority_revocation_validate(const uint8_t *revocation,size_t revocation_length,const uint8_t *grant,size_t grant_length,const uint8_t *roots,size_t root_count,const stn_hash_provider *provider,uint8_t id[32])
{
    uint8_t evidence[97],issuer[32],grant_issuer[32],statement[STN_AUTHORITY_REVOKE_DOMAIN_SIZE+65u];size_t n=0;stn_authority_grant_result g;
    if(revocation==NULL || grant==NULL || id==NULL || revocation_length!=STN_AUTHORITY_REVOCATION_SIZE || revocation[0]!=STN_AUTHORITY_VERSION){return STN_AUTHORITY_MALFORMED_REVOCATION;}
    g=stn_authority_grant_validate(grant,grant_length,roots,root_count,evidence);if(g==STN_AUTHORITY_MALFORMED_GRANT){return STN_AUTHORITY_MALFORMED_REVOCATION;}if(g!=STN_AUTHORITY_VALID_GRANT){return STN_AUTHORITY_INVALID_REVOCATION;}
    if(stn_authority_grant_id(grant,grant_length,provider,id)!=STN_DATA_OK){return STN_AUTHORITY_INVALID_REVOCATION;}
    memcpy(issuer,revocation+1,32);memcpy(grant_issuer,grant+1,32);if(!nonzero(revocation+33,32) || stn_identity_derive(issuer,issuer)!=STN_IDENTITY_VALID){return STN_AUTHORITY_MALFORMED_REVOCATION;}if(memcmp(issuer,grant_issuer,32)!=0){return STN_AUTHORITY_INVALID_REVOCATION;}
    if(memcmp(revocation+33,id,32)!=0 || stn_authority_revocation_statement(issuer,id,statement,sizeof(statement),&n)!=STN_AUTHORITY_VALID_REVOCATION){return STN_AUTHORITY_INVALID_REVOCATION;}
    { stn_identity_result v=stn_identity_verify(issuer,statement,n,revocation+65,STN_IDENTITY_SIGNATURE_SIZE);return v==STN_IDENTITY_VALID?STN_AUTHORITY_VALID_REVOCATION:v==STN_IDENTITY_MALFORMED?STN_AUTHORITY_MALFORMED_REVOCATION:STN_AUTHORITY_INVALID_REVOCATION; }
}
void stn_authority_state_initialize(stn_authority_state *state){if(state!=NULL){memset(state,0,sizeof(*state));}}
int stn_authority_state_is_revoked(const stn_authority_state *state,const uint8_t id[32])
{
    size_t i;if(state==NULL || id==NULL){return 0;}for(i=0;i<state->revoked_count && i<STN_AUTHORITY_MAX_REVOKED;++i){if(memcmp(state->revoked_ids[i],id,32)==0){return 1;}}return 0;
}
stn_authority_revocation_result stn_authority_state_apply(stn_authority_state *state,const uint8_t *revocation,size_t revocation_length,const uint8_t *grant,size_t grant_length,const uint8_t *roots,size_t root_count,const stn_hash_provider *provider)
{
    uint8_t id[32];size_t i,pos;stn_authority_revocation_result r;
    if(state==NULL || state->revoked_count>STN_AUTHORITY_MAX_REVOKED){return STN_AUTHORITY_MALFORMED_REVOCATION;}
    r=stn_authority_revocation_validate(revocation,revocation_length,grant,grant_length,roots,root_count,provider,id);if(r!=STN_AUTHORITY_VALID_REVOCATION){return r;}
    for(i=0;i<state->revoked_count;++i){if(memcmp(state->revoked_ids[i],id,32)==0){return STN_AUTHORITY_VALID_REVOCATION;}}
    if(state->revoked_count==STN_AUTHORITY_MAX_REVOKED){return STN_AUTHORITY_INVALID_REVOCATION;}
    pos=state->revoked_count;while(pos!=0 && id_compare(state->revoked_ids[pos-1],id)>0){memcpy(state->revoked_ids[pos],state->revoked_ids[pos-1],32);--pos;}memcpy(state->revoked_ids[pos],id,32);++state->revoked_count;return STN_AUTHORITY_VALID_REVOCATION;
}
stn_authority_grant_result stn_authority_grant_active(const uint8_t *grant,size_t length,const uint8_t *roots,size_t count,const stn_hash_provider *provider,const stn_authority_state *state,uint8_t evidence[97])
{
    uint8_t id[32];stn_authority_grant_result r;if(state==NULL || evidence==NULL){return STN_AUTHORITY_MALFORMED_GRANT;}r=stn_authority_grant_validate(grant,length,roots,count,evidence);if(r!=STN_AUTHORITY_VALID_GRANT){return r;}if(stn_authority_grant_id(grant,length,provider,id)!=STN_DATA_OK){return STN_AUTHORITY_INVALID_GRANT;}return stn_authority_state_is_revoked(state,id)?STN_AUTHORITY_INVALID_GRANT:STN_AUTHORITY_VALID_GRANT;
}
stn_authority_result stn_authority_evidence_encode(const uint8_t subject[32],const uint8_t action[32],const uint8_t context[32],uint8_t *evidence,size_t capacity,size_t *written)
{
    uint8_t identity[32];
    if(written!=NULL){*written=0;}
    if(subject==NULL || evidence==NULL || written==NULL || capacity<STN_AUTHORITY_EVIDENCE_SIZE ||
       stn_identity_derive(subject,identity)!=STN_IDENTITY_VALID ||
       stn_authority_action_validate(action)!=STN_AUTHORITY_AUTHORIZED ||
       stn_authority_context_validate(context)!=STN_AUTHORITY_AUTHORIZED){return STN_AUTHORITY_MALFORMED;}
    evidence[0]=STN_AUTHORITY_VERSION;
    memcpy(evidence+1,identity,32); memcpy(evidence+33,action,32); memcpy(evidence+65,context,32);
    *written=STN_AUTHORITY_EVIDENCE_SIZE;
    return STN_AUTHORITY_AUTHORIZED;
}
stn_authority_result stn_authority_evaluate(const uint8_t identity[32],const uint8_t action[32],const uint8_t context[32],const uint8_t *evidence,size_t evidence_length)
{
    uint8_t canonical[32];
    if(stn_identity_derive(identity,canonical)!=STN_IDENTITY_VALID ||
       stn_authority_action_validate(action)!=STN_AUTHORITY_AUTHORIZED ||
       stn_authority_context_validate(context)!=STN_AUTHORITY_AUTHORIZED){return STN_AUTHORITY_MALFORMED;}
    if(evidence==NULL && evidence_length==0){return STN_AUTHORITY_UNAUTHORIZED;}
    if(evidence==NULL || evidence_length!=STN_AUTHORITY_EVIDENCE_SIZE || evidence[0]!=STN_AUTHORITY_VERSION){return STN_AUTHORITY_MALFORMED;}
    if(memcmp(evidence+1,canonical,32)!=0 || memcmp(evidence+33,action,32)!=0 || memcmp(evidence+65,context,32)!=0){return STN_AUTHORITY_UNAUTHORIZED;}
    return STN_AUTHORITY_AUTHORIZED;
}
