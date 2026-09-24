/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_share.h"
#include <stdio.h>
#include <string.h>

static unsigned checks,failures;
#define CHECK(e) do { ++checks; if(!(e)){++failures;fprintf(stderr,"share line %d: %s\n",__LINE__,#e);} } while(0)

static void vectors(void)
{
    stn_share_evidence s={0},changed;
    uint8_t canonical[STN_SHARE_CANONICAL_SIZE],saved[STN_SHARE_CANONICAL_SIZE];
    uint8_t id[32],id2[32],saved_id[32];
    size_t i;

    s.miner.type=STN_ADDRESS_IDENTITY;
    for(i=0;i<32u;++i){
        s.work_id[i]=(uint8_t)i;
        s.miner.identifier[i]=(uint8_t)(0xa0u+i);
    }
    s.nonce=UINT64_C(0x0102030405060708);

    CHECK(stn_share_encode(&s,canonical)==STN_DATA_OK);
    CHECK(canonical[0]==STN_SHARE_VERSION);
    CHECK(memcmp(canonical+1,s.work_id,32)==0);
    CHECK(memcmp(canonical+33,s.miner.identifier,32)==0);
    CHECK(STN_SHARE_CANONICAL_SIZE==73u);
    CHECK(canonical[65]==1 && canonical[66]==2 && canonical[67]==3 &&
          canonical[68]==4 && canonical[69]==5 && canonical[70]==6 &&
          canonical[71]==7 && canonical[72]==8);

    CHECK(stn_share_id(&s,id)==STN_DATA_OK);
    CHECK(stn_share_id(&s,id2)==STN_DATA_OK && memcmp(id,id2,32)==0);

    changed=s;changed.nonce++;
    CHECK(stn_share_id(&changed,id2)==STN_DATA_OK && memcmp(id,id2,32)!=0);
    changed=s;changed.work_id[0]^=1u;
    CHECK(stn_share_id(&changed,id2)==STN_DATA_OK && memcmp(id,id2,32)!=0);
    changed=s;changed.miner.identifier[0]^=1u;
    CHECK(stn_share_id(&changed,id2)==STN_DATA_OK && memcmp(id,id2,32)!=0);

    memset(saved,0xa5,sizeof(saved));memcpy(canonical,saved,sizeof(saved));
    CHECK(stn_share_encode(NULL,canonical)==STN_DATA_ARGUMENT);
    CHECK(memcmp(canonical,saved,sizeof(saved))==0);
    CHECK(stn_share_encode(&s,NULL)==STN_DATA_ARGUMENT);

    changed=s;changed.miner.type=STN_ADDRESS_WALLET;
    CHECK(stn_share_encode(&changed,canonical)==STN_DATA_TYPE);
    CHECK(memcmp(canonical,saved,sizeof(saved))==0);

    memset(saved_id,0x5a,sizeof(saved_id));memcpy(id2,saved_id,sizeof(id2));
    CHECK(stn_share_id(NULL,id2)==STN_DATA_ARGUMENT);
    CHECK(memcmp(id2,saved_id,sizeof(id2))==0);
    CHECK(stn_share_id(&s,NULL)==STN_DATA_ARGUMENT);
    CHECK(stn_share_id(&changed,id2)==STN_DATA_TYPE);
    CHECK(memcmp(id2,saved_id,sizeof(id2))==0);
}

int test_share(void);
int test_share(void)
{
    vectors();
    printf("Economy share evidence: %u checks, %u failures.\n",checks,failures);
    return failures==0 ? 0 : 1;
}

#ifdef STN_SHARE_TEST_MAIN
int main(void){return test_share();}
#endif
