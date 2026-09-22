#include "stn_replay.h"
#include <stdio.h>
#include <string.h>
#define CHECK(e) do { ++checks; if(!(e)){++failures;fprintf(stderr,"replay line %d: %s\n",__LINE__,#e);} } while(0)
static unsigned checks,failures;
int test_replay(void)
{
    uint8_t signer[32]={0xd7,0x5a,0x98,0x01,0x82,0xb1,0x0a,0xb7,0xd5,0x4b,0xfe,0xd3,0xc9,0x64,0x07,0x3a,0x0e,0xe1,0x72,0xf3,0xda,0xa6,0x23,0x25,0xaf,0x02,0x1a,0x68,0xf7,0x07,0x51,0x1a},nonce[32]={1},zero[32]={0},id[64],id2[64],record[STN_RECORD_MAX_SIZE],record2[STN_RECORD_MAX_SIZE],consumed[3][64],reconsumed[3][64],one[64];size_t n=0,n2=0;stn_replay_state state,restart,limited;const uint8_t *records[2]={record,record2};size_t lengths[2];
    static const uint8_t payload[1]={'x'}; stn_record r={0};
    CHECK(stn_replay_id_from_signer_nonce(signer,nonce,id)==STN_REPLAY_FRESH);
    CHECK(stn_replay_id_from_signer_nonce(signer,nonce,id2)==STN_REPLAY_FRESH && memcmp(id,id2,64)==0);
    CHECK(stn_replay_id_from_signer_nonce(signer,zero,id2)==STN_REPLAY_MALFORMED);
    CHECK(stn_replay_id_from_signer_nonce(zero,nonce,id2)==STN_REPLAY_MALFORMED);
    stn_replay_state_initialize(&state,(uint8_t *)consumed,3);CHECK(stn_replay_state_check(&state,id)==STN_REPLAY_FRESH);
    CHECK(stn_replay_state_consume(&state,id)==STN_REPLAY_FRESH);
    CHECK(stn_replay_state_check(&state,id)==STN_REPLAY_REPLAY);
    CHECK(stn_replay_state_consume(&state,id)==STN_REPLAY_REPLAY && state.consumed_count==1);
    nonce[31]=2;CHECK(stn_replay_id_from_signer_nonce(signer,nonce,id2)==STN_REPLAY_FRESH && memcmp(id,id2,64)!=0);
    CHECK(stn_replay_state_check(&state,id2)==STN_REPLAY_FRESH);
    r.version=STN_RECORD_VERSION; r.type=STN_RECORD_INTELLIGENCE; memcpy(r.signer_public_key,signer,32); r.nonce[31]=1; r.payload=payload; r.payload_length=1;
    CHECK(stn_record_encode(&r,record,sizeof(record),&n)==STN_RECORD_OK);
    CHECK(stn_replay_id_from_record(record,n,id)==STN_REPLAY_FRESH && memcmp(id,signer,32)==0 && id[63]==1);
    r.nonce[31]=2; CHECK(stn_record_encode(&r,record2,sizeof(record2),&n2)==STN_RECORD_OK);
    CHECK(stn_replay_id_from_record(record2,n2,id2)==STN_REPLAY_FRESH && memcmp(id,id2,64)!=0);
    CHECK(stn_replay_id_from_record(NULL,0,id2)==STN_REPLAY_MALFORMED);
    stn_replay_state_initialize(&restart,(uint8_t *)reconsumed,3);lengths[0]=n;lengths[1]=n2;CHECK(stn_replay_state_rebuild(&restart,records,lengths,0)==STN_REPLAY_FRESH && restart.consumed_count==0);
    CHECK(stn_replay_state_rebuild(&restart,records,lengths,2)==STN_REPLAY_FRESH && restart.consumed_count==2);
    CHECK(stn_replay_state_check(&restart,id)==STN_REPLAY_REPLAY && stn_replay_state_check(&restart,id2)==STN_REPLAY_REPLAY);
    stn_replay_state_initialize(&limited,one,1); CHECK(stn_replay_state_consume(&limited,id)==STN_REPLAY_FRESH); CHECK(stn_replay_state_consume(&limited,id2)==STN_REPLAY_MALFORMED);
    stn_replay_state_initialize(&limited,NULL,1); CHECK(stn_replay_state_check(&limited,id)==STN_REPLAY_MALFORMED);
    CHECK(stn_replay_state_rebuild(NULL,records,lengths,0)==STN_REPLAY_MALFORMED);
    CHECK(stn_replay_state_rebuild(&restart,NULL,NULL,1)==STN_REPLAY_MALFORMED);
    printf("Replay protection: %u checks, %u failures.\n",checks,failures);return failures!=0;
}
