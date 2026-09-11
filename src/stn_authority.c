/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_authority.h"
#include <string.h>

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
