#include "stn_authority.h"
#include "stn_sha256.h"
#include <stdio.h>
#include <string.h>
#define CHECK(e) do { ++checks; if(!(e)){++failures;fprintf(stderr,"authority line %d: %s\n",__LINE__,#e);} } while(0)
static unsigned checks,failures;
static const uint8_t key[32]={0xd7,0x5a,0x98,0x01,0x82,0xb1,0x0a,0xb7,0xd5,0x4b,0xfe,0xd3,0xc9,0x64,0x07,0x3a,0x0e,0xe1,0x72,0xf3,0xda,0xa6,0x23,0x25,0xaf,0x02,0x1a,0x68,0xf7,0x07,0x51,0x1a};
static const uint8_t second_root[32]={0xfc,0x51,0xcd,0x8e,0x62,0x18,0xa1,0xa3,0x8d,0xa4,0x7e,0xd0,0x02,0x30,0xf0,0x58,0x08,0x16,0xed,0x13,0xba,0x33,0x03,0xac,0x5d,0xeb,0x91,0x15,0x48,0x90,0x80,0x25};
int test_authority(void)
{
    static const uint8_t grant_signature[64]={0xc5,0xf9,0x93,0x39,0x86,0xf9,0x5e,0x18,0xbf,0x67,0x99,0x49,0x90,0xdc,0x42,0x5c,0x14,0x74,0xa4,0x8b,0xce,0x66,0x0b,0x3f,0x04,0x15,0x1e,0x61,0x15,0xbf,0x36,0x33,0x28,0x0b,0x63,0x63,0xf8,0x93,0x91,0xea,0xd9,0x4f,0xb6,0x7b,0xb5,0x91,0xc9,0x72,0x0b,0x3e,0xd1,0xf0,0xdf,0x62,0xe0,0xa3,0x96,0x3e,0x80,0x66,0x36,0x97,0xb1,0x04};
    static const uint8_t revoke_signature[64]={0x39,0xb9,0xe8,0x46,0x88,0x2e,0xe0,0xcf,0x5d,0x4d,0xfb,0x2a,0xa5,0x1d,0x51,0x78,0x25,0x97,0xab,0x22,0x7b,0x88,0x60,0x7c,0xce,0x3e,0x26,0xaf,0x8f,0x45,0x3c,0x80,0xe9,0xee,0x33,0x0e,0x79,0x69,0x81,0x9b,0x14,0xbe,0x41,0x93,0x50,0x50,0xa5,0xf3,0x9b,0x2d,0x1a,0xb9,0x68,0xeb,0x72,0xc8,0x14,0x40,0x36,0x7b,0x29,0x19,0xd1,0x01};
    static const uint8_t expected_id[32]={0x75,0xdd,0x45,0x4a,0x06,0x3d,0x59,0x40,0x8c,0x02,0x7d,0xb8,0xdf,0x85,0x6a,0x7d,0xd8,0xac,0xa4,0x4d,0xc4,0xdb,0xdc,0xee,0xd4,0xae,0x52,0x99,0x1d,0xd2,0xc0,0xdb};
    uint8_t action[32]={1,2},context[32]={1,3},evidence[STN_AUTHORITY_EVIDENCE_SIZE],bad[STN_AUTHORITY_REVOCATION_SIZE+1],grant[STN_AUTHORITY_GRANT_SIZE],revocation[STN_AUTHORITY_REVOCATION_SIZE],roots[64],other[32],id[32];size_t n=0;stn_hash_provider provider={stn_sha256,NULL};stn_authority_state state;
    CHECK(stn_authority_action_validate(action)==STN_AUTHORITY_AUTHORIZED);
    CHECK(stn_authority_context_validate(context)==STN_AUTHORITY_AUTHORIZED);
    CHECK(stn_authority_evidence_encode(key,action,context,evidence,sizeof(evidence),&n)==STN_AUTHORITY_AUTHORIZED && n==97);
    CHECK(stn_authority_root_set_validate(key,1)==STN_AUTHORITY_AUTHORIZED);
    memcpy(roots,key,32);memcpy(roots+32,second_root,32);CHECK(stn_authority_root_set_validate(roots,2)==STN_AUTHORITY_AUTHORIZED);
    memcpy(roots+32,key,32);CHECK(stn_authority_root_set_validate(roots,2)==STN_AUTHORITY_MALFORMED);
    memset(roots,0,64);CHECK(stn_authority_root_set_validate(roots,1)==STN_AUTHORITY_MALFORMED);
    CHECK(stn_authority_grant_encode(key,evidence,grant_signature,grant,sizeof(grant),&n)==STN_AUTHORITY_VALID_GRANT && n==STN_AUTHORITY_GRANT_SIZE);
    CHECK(stn_authority_grant_validate(grant,n,key,1,evidence)==STN_AUTHORITY_VALID_GRANT);
    CHECK(stn_authority_grant_id(grant,n,&provider,id)==STN_DATA_OK && memcmp(id,expected_id,32)==0);
    CHECK(stn_authority_grant_id(grant,n,&provider,id)==STN_DATA_OK && memcmp(id,expected_id,32)==0);
    CHECK(stn_authority_revocation_encode(key,id,revoke_signature,revocation,sizeof(revocation),&n)==STN_AUTHORITY_VALID_REVOCATION && n==STN_AUTHORITY_REVOCATION_SIZE);
    CHECK(stn_authority_revocation_validate(revocation,n,grant,STN_AUTHORITY_GRANT_SIZE,key,1,&provider,id)==STN_AUTHORITY_VALID_REVOCATION);
    stn_authority_state_initialize(&state);
    CHECK(stn_authority_grant_active(grant,STN_AUTHORITY_GRANT_SIZE,key,1,&provider,&state,evidence)==STN_AUTHORITY_VALID_GRANT);
    CHECK(stn_authority_state_apply(&state,revocation,n,grant,STN_AUTHORITY_GRANT_SIZE,key,1,&provider)==STN_AUTHORITY_VALID_REVOCATION);
    CHECK(stn_authority_state_is_revoked(&state,id));
    CHECK(stn_authority_grant_active(grant,STN_AUTHORITY_GRANT_SIZE,key,1,&provider,&state,evidence)==STN_AUTHORITY_INVALID_GRANT);
    CHECK(stn_authority_state_apply(&state,revocation,n,grant,STN_AUTHORITY_GRANT_SIZE,key,1,&provider)==STN_AUTHORITY_VALID_REVOCATION && state.revoked_count==1);
    CHECK(stn_authority_revocation_validate(revocation,n-1,grant,STN_AUTHORITY_GRANT_SIZE,key,1,&provider,id)==STN_AUTHORITY_MALFORMED_REVOCATION);
    memcpy(bad,revocation,STN_AUTHORITY_REVOCATION_SIZE);bad[0]=2;CHECK(stn_authority_revocation_validate(bad,STN_AUTHORITY_REVOCATION_SIZE,grant,STN_AUTHORITY_GRANT_SIZE,key,1,&provider,id)==STN_AUTHORITY_MALFORMED_REVOCATION);
    memcpy(bad,revocation,STN_AUTHORITY_REVOCATION_SIZE);memset(bad+1,0,32);CHECK(stn_authority_revocation_validate(bad,STN_AUTHORITY_REVOCATION_SIZE,grant,STN_AUTHORITY_GRANT_SIZE,key,1,&provider,id)==STN_AUTHORITY_MALFORMED_REVOCATION);
    memcpy(bad,revocation,STN_AUTHORITY_REVOCATION_SIZE);memset(bad+33,0,32);CHECK(stn_authority_revocation_validate(bad,STN_AUTHORITY_REVOCATION_SIZE,grant,STN_AUTHORITY_GRANT_SIZE,key,1,&provider,id)==STN_AUTHORITY_MALFORMED_REVOCATION);
    memcpy(bad,revocation,STN_AUTHORITY_REVOCATION_SIZE);bad[65]^=1;CHECK(stn_authority_revocation_validate(bad,STN_AUTHORITY_REVOCATION_SIZE,grant,STN_AUTHORITY_GRANT_SIZE,key,1,&provider,id)==STN_AUTHORITY_INVALID_REVOCATION);
    memcpy(other,key,32);other[0]^=1;CHECK(stn_authority_grant_validate(grant,STN_AUTHORITY_GRANT_SIZE,other,1,evidence)==STN_AUTHORITY_INVALID_GRANT);
    grant[130]^=1;CHECK(stn_authority_grant_validate(grant,STN_AUTHORITY_GRANT_SIZE,key,1,evidence)==STN_AUTHORITY_INVALID_GRANT);grant[130]^=1;n=STN_AUTHORITY_EVIDENCE_SIZE;
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
