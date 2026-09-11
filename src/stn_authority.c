/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_authority.h"
#include <string.h>

int stn_ed25519_verify(const uint8_t *,size_t,const uint8_t[32],const uint8_t[64]);

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
