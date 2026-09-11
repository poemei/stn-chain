#include "stn_lifecycle.h"
#include "stn_sha256.h"
#include <stdio.h>
#include <string.h>
#define CHECK(e) do { ++checks; if(!(e)){++failures;fprintf(stderr,"lifecycle line %d: %s\n",__LINE__,#e);} } while(0)
static unsigned checks,failures;
static const uint8_t root[32]={0xd7,0x5a,0x98,0x01,0x82,0xb1,0x0a,0xb7,0xd5,0x4b,0xfe,0xd3,0xc9,0x64,0x07,0x3a,0x0e,0xe1,0x72,0xf3,0xda,0xa6,0x23,0x25,0xaf,0x02,0x1a,0x68,0xf7,0x07,0x51,0x1a};
static const uint8_t grant_sig[64]={0xc5,0xf9,0x93,0x39,0x86,0xf9,0x5e,0x18,0xbf,0x67,0x99,0x49,0x90,0xdc,0x42,0x5c,0x14,0x74,0xa4,0x8b,0xce,0x66,0x0b,0x3f,0x04,0x15,0x1e,0x61,0x15,0xbf,0x36,0x33,0x28,0x0b,0x63,0x63,0xf8,0x93,0x91,0xea,0xd9,0x4f,0xb6,0x7b,0xb5,0x91,0xc9,0x72,0x0b,0x3e,0xd1,0xf0,0xdf,0x62,0xe0,0xa3,0x96,0x3e,0x80,0x66,0x36,0x97,0xb1,0x04};
static const uint8_t revoke_sig[64]={0x39,0xb9,0xe8,0x46,0x88,0x2e,0xe0,0xcf,0x5d,0x4d,0xfb,0x2a,0xa5,0x1d,0x51,0x78,0x25,0x97,0xab,0x22,0x7b,0x88,0x60,0x7c,0xce,0x3e,0x26,0xaf,0x8f,0x45,0x3c,0x80,0xe9,0xee,0x33,0x0e,0x79,0x69,0x81,0x9b,0x14,0xbe,0x41,0x93,0x50,0x50,0xa5,0xf3,0x9b,0x2d,0x1a,0xb9,0x68,0xeb,0x72,0xc8,0x14,0x40,0x36,0x7b,0x29,0x19,0xd1,0x01};
int test_lifecycle(void)
{
    uint8_t action[32]={1,2},context[32]={1,3},evidence[97],grant[194],revoke[129],grant_store[194*4],replay_store[64*8],tx_bytes[256],tx_grant[256],tx_revoke[256],body1[512],body2[512],block1[1024],block2[1024];size_t n=0,ng=0,nr=0,bn1=0,bn2=0;stn_hash_provider p={stn_sha256,NULL};stn_lifecycle_state s;stn_transaction tx;stn_transaction_span span;stn_block block={0};stn_block_span history[2];uint8_t id[32];
    CHECK(stn_authority_evidence_encode(root,action,context,evidence,sizeof(evidence),&n)==STN_AUTHORITY_AUTHORIZED);
    CHECK(stn_authority_grant_encode(root,evidence,grant_sig,grant,sizeof(grant),&n)==STN_AUTHORITY_VALID_GRANT);
    tx.version=1;tx.type=STN_TX_AUTHORITY_GRANT;tx.record_bytes=grant;tx.record_length=sizeof(grant);
    CHECK(stn_transaction_encode(&tx,tx_bytes,sizeof(tx_bytes),&n)==STN_DATA_OK); { stn_transaction decoded={0}; CHECK(stn_transaction_decode(tx_bytes,n,&decoded)==STN_DATA_OK && decoded.type==STN_TX_AUTHORITY_GRANT && decoded.record_length==194); }
    memcpy(tx_grant,tx_bytes,n);ng=n;
    stn_lifecycle_initialize(&s,grant_store,4,replay_store,8,root);
    CHECK(stn_lifecycle_apply_transaction(&s,&tx,root,1,&p)==STN_LIFECYCLE_OK && s.grant_count==1);
    CHECK(stn_lifecycle_apply_transaction(&s,&tx,root,1,&p)==STN_LIFECYCLE_REPLAY && s.grant_count==1);
    CHECK(stn_authority_grant_id(grant,sizeof(grant),&p,id)==STN_DATA_OK);
    CHECK(stn_authority_revocation_encode(root,id,revoke_sig,revoke,sizeof(revoke),&n)==STN_AUTHORITY_VALID_REVOCATION);
    tx.type=STN_TX_AUTHORITY_REVOKE;tx.record_bytes=revoke;tx.record_length=sizeof(revoke);
    CHECK(stn_lifecycle_apply_transaction(&s,&tx,root,1,&p)==STN_LIFECYCLE_OK);
    CHECK(stn_authority_state_is_revoked(&s.authority,id));
    CHECK(stn_transaction_encode(&tx,tx_revoke,sizeof(tx_revoke),&nr)==STN_DATA_OK); { stn_transaction decoded={0}; CHECK(stn_transaction_decode(tx_revoke,nr,&decoded)==STN_DATA_OK && decoded.type==STN_TX_AUTHORITY_REVOKE && decoded.record_length==129); }
    span.bytes=tx_grant;span.length=(uint32_t)ng;CHECK(stn_block_body_encode(&span,1,body1,sizeof(body1),&n)==STN_DATA_OK);
    block.header.version=1;block.header.transaction_count=1;block.header.body_length=(uint32_t)n;block.body=body1;CHECK(stn_block_encode(&block,block1,sizeof(block1),&bn1)==STN_DATA_OK);
    span.bytes=tx_revoke;span.length=(uint32_t)nr;CHECK(stn_block_body_encode(&span,1,body2,sizeof(body2),&n)==STN_DATA_OK);
    block.header.body_length=(uint32_t)n;block.body=body2;CHECK(stn_block_encode(&block,block2,sizeof(block2),&bn2)==STN_DATA_OK);
    history[0].bytes=block1;history[0].length=bn1;history[1].bytes=block2;history[1].length=bn2;
    CHECK(stn_lifecycle_rebuild(&s,history,1,root,1,&p)==STN_LIFECYCLE_OK && s.grant_count==1 && s.authority.revoked_count==0);
    CHECK(stn_lifecycle_rebuild(&s,history,2,root,1,&p)==STN_LIFECYCLE_OK && s.authority.revoked_count==1);
    CHECK(stn_lifecycle_rebuild(&s,history,1,root,1,&p)==STN_LIFECYCLE_OK && s.authority.revoked_count==0);
    CHECK(stn_lifecycle_rebuild(&s,NULL,0,root,1,&p)==STN_LIFECYCLE_OK && s.grant_count==0 && s.authority.revoked_count==0);
    printf("Lifecycle integration: %u checks, %u failures.\n",checks,failures);return failures!=0;
}
