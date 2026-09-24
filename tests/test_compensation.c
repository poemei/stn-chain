/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_compensation.h"
#include <stdio.h>
#include <string.h>

static unsigned checks,failures;
#define CHECK(e) do { ++checks; if(!(e)){++failures;fprintf(stderr,"compensation line %d: %s\n",__LINE__,#e);} } while(0)

static void destination_vectors(void)
{
    stn_compensation_destination in={0},out={0},saved;
    uint8_t canonical[STN_COMPENSATION_DESTINATION_SIZE],before[STN_COMPENSATION_DESTINATION_SIZE];
    size_t i;

    in.mining_identity.type=STN_ADDRESS_IDENTITY;
    in.wallet.type=STN_ADDRESS_WALLET;
    for(i=0;i<STN_ADDRESS_ID_SIZE;++i){
        in.mining_identity.identifier[i]=(uint8_t)i;
        in.wallet.identifier[i]=(uint8_t)(0xffu-i);
    }

    CHECK(stn_compensation_destination_encode(&in,canonical)==STN_DATA_OK);
    CHECK(canonical[0]==STN_COMPENSATION_DESTINATION_VERSION);
    CHECK(memcmp(canonical+1u,in.mining_identity.identifier,STN_ADDRESS_ID_SIZE)==0);
    CHECK(memcmp(canonical+33u,in.wallet.identifier,STN_ADDRESS_ID_SIZE)==0);
    CHECK(stn_compensation_destination_decode(canonical,sizeof(canonical),&out)==STN_DATA_OK);
    CHECK(out.mining_identity.type==STN_ADDRESS_IDENTITY);
    CHECK(out.wallet.type==STN_ADDRESS_WALLET);
    CHECK(memcmp(out.mining_identity.identifier,in.mining_identity.identifier,STN_ADDRESS_ID_SIZE)==0);
    CHECK(memcmp(out.wallet.identifier,in.wallet.identifier,STN_ADDRESS_ID_SIZE)==0);

    memset(before,0xa5,sizeof(before));
    memcpy(canonical,before,sizeof(canonical));
    in.mining_identity.type=STN_ADDRESS_WALLET;
    CHECK(stn_compensation_destination_encode(&in,canonical)==STN_DATA_TYPE);
    CHECK(memcmp(canonical,before,sizeof(canonical))==0);
    in.mining_identity.type=STN_ADDRESS_IDENTITY;
    in.wallet.type=STN_ADDRESS_IDENTITY;
    CHECK(stn_compensation_destination_encode(&in,canonical)==STN_DATA_TYPE);
    CHECK(memcmp(canonical,before,sizeof(canonical))==0);
    in.wallet.type=STN_ADDRESS_WALLET;
    CHECK(stn_compensation_destination_encode(NULL,canonical)==STN_DATA_ARGUMENT);
    CHECK(stn_compensation_destination_encode(&in,NULL)==STN_DATA_ARGUMENT);

    CHECK(stn_compensation_destination_encode(&in,canonical)==STN_DATA_OK);
    saved=out;
    CHECK(stn_compensation_destination_decode(canonical,sizeof(canonical)-1u,&out)==STN_DATA_LENGTH);
    CHECK(memcmp(&out,&saved,sizeof(out))==0);
    canonical[0]=2u;
    CHECK(stn_compensation_destination_decode(canonical,sizeof(canonical),&out)==STN_DATA_VERSION);
    CHECK(memcmp(&out,&saved,sizeof(out))==0);
    CHECK(stn_compensation_destination_decode(NULL,sizeof(canonical),&out)==STN_DATA_ARGUMENT);
    CHECK(stn_compensation_destination_decode(canonical,sizeof(canonical),NULL)==STN_DATA_ARGUMENT);
}

int test_compensation(void);
int test_compensation(void)
{
    destination_vectors();
    printf("Compensation destination: %u checks, %u failures.\n",checks,failures);
    return failures==0 ? 0 : 1;
}

#ifdef STN_COMPENSATION_TEST_MAIN
int main(void)
{
    return test_compensation();
}
#endif
