#include "stn_authority.h"
#include <stdio.h>
#include <string.h>
#define CHECK(e) do { ++checks; if(!(e)){++failures;fprintf(stderr,"authority line %d: %s\n",__LINE__,#e);} } while(0)
static unsigned checks,failures;
static const uint8_t key[32]={0xd7,0x5a,0x98,0x01,0x82,0xb1,0x0a,0xb7,0xd5,0x4b,0xfe,0xd3,0xc9,0x64,0x07,0x3a,0x0e,0xe1,0x72,0xf3,0xda,0xa6,0x23,0x25,0xaf,0x02,0x1a,0x68,0xf7,0x07,0x51,0x1a};
int test_authority(void)
{
    uint8_t action[32]={1,2},context[32]={1,3},evidence[STN_AUTHORITY_EVIDENCE_SIZE],bad[STN_AUTHORITY_EVIDENCE_SIZE+1],other[32];size_t n=0;
    CHECK(stn_authority_action_validate(action)==STN_AUTHORITY_AUTHORIZED);
    CHECK(stn_authority_context_validate(context)==STN_AUTHORITY_AUTHORIZED);
    CHECK(stn_authority_evidence_encode(key,action,context,evidence,sizeof(evidence),&n)==STN_AUTHORITY_AUTHORIZED && n==97);
    CHECK(stn_authority_evaluate(key,action,context,evidence,n)==STN_AUTHORITY_AUTHORIZED);
    CHECK(stn_authority_evaluate(key,action,context,evidence,n)==STN_AUTHORITY_AUTHORIZED);
    CHECK(stn_authority_evaluate(key,action,context,NULL,0)==STN_AUTHORITY_UNAUTHORIZED);
    memcpy(other,key,32);other[0]^=1;CHECK(stn_authority_evaluate(other,action,context,evidence,n)==STN_AUTHORITY_UNAUTHORIZED);
    memcpy(other,action,32);other[1]^=1;CHECK(stn_authority_evaluate(key,other,context,evidence,n)==STN_AUTHORITY_UNAUTHORIZED);
    memcpy(other,context,32);other[1]^=1;CHECK(stn_authority_evaluate(key,action,other,evidence,n)==STN_AUTHORITY_UNAUTHORIZED);
    memset(bad,0,sizeof(bad));CHECK(stn_authority_evaluate(key,action,context,bad,n)==STN_AUTHORITY_MALFORMED);
    CHECK(stn_authority_evaluate(key,action,context,evidence,n-1)==STN_AUTHORITY_MALFORMED);
    CHECK(stn_authority_evaluate(key,action,context,evidence,n+1)==STN_AUTHORITY_MALFORMED);
    memcpy(bad,evidence,n);bad[n]=0;CHECK(stn_authority_evaluate(key,action,context,bad,n+1)==STN_AUTHORITY_MALFORMED);
    memcpy(bad,evidence,n);bad[0]=2;CHECK(stn_authority_evaluate(key,action,context,bad,n)==STN_AUTHORITY_MALFORMED);
    other[0]=2;other[1]=1;memset(other+2,0,30);CHECK(stn_authority_action_validate(other)==STN_AUTHORITY_MALFORMED);
    memset(other,0,32);CHECK(stn_authority_context_validate(other)==STN_AUTHORITY_MALFORMED);
    CHECK(stn_authority_evidence_encode(key,other,context,evidence,sizeof(evidence),&n)==STN_AUTHORITY_MALFORMED);
    CHECK(stn_authority_evidence_encode(key,action,context,evidence,96,&n)==STN_AUTHORITY_MALFORMED && n==0);
    printf("Authority evaluation: %u checks, %u failures.\n",checks,failures);return failures!=0;
}
