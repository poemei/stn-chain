/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_transfer.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>

static unsigned checks,failures;
#define CHECK(e) do { ++checks; if(!(e)){++failures;fprintf(stderr,"transfer line %d: %s\n",__LINE__,#e);} } while(0)

static void vectors(void)
{
    stn_transfer transfer={0},decoded={0},snapshot={0};
    uint8_t canonical[STN_TRANSFER_CANONICAL_SIZE],bad[STN_TRANSFER_CANONICAL_SIZE];
    static const uint8_t amount[8]={0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08};

    transfer.source.type=STN_ADDRESS_WALLET;
    transfer.destination.type=STN_ADDRESS_WALLET;
    memset(transfer.source.identifier,0x11,STN_ADDRESS_ID_SIZE);
    memset(transfer.destination.identifier,0x22,STN_ADDRESS_ID_SIZE);
    transfer.units=UINT64_C(0x0102030405060708);

    CHECK(stn_transfer_encode(&transfer,canonical)==STN_DATA_OK);
    CHECK(canonical[0]==STN_TRANSFER_VERSION);
    CHECK(memcmp(canonical+1u,transfer.source.identifier,STN_ADDRESS_ID_SIZE)==0);
    CHECK(memcmp(canonical+33u,transfer.destination.identifier,STN_ADDRESS_ID_SIZE)==0);
    CHECK(memcmp(canonical+65u,amount,sizeof(amount))==0);
    CHECK(stn_transfer_decode(canonical,sizeof(canonical),&decoded)==STN_DATA_OK);
    CHECK(decoded.source.type==STN_ADDRESS_WALLET);
    CHECK(decoded.destination.type==STN_ADDRESS_WALLET);
    CHECK(decoded.units==transfer.units);
    CHECK(memcmp(decoded.source.identifier,transfer.source.identifier,STN_ADDRESS_ID_SIZE)==0);
    CHECK(memcmp(decoded.destination.identifier,transfer.destination.identifier,STN_ADDRESS_ID_SIZE)==0);

    snapshot=decoded;
    transfer.units=0u;
    CHECK(stn_transfer_encode(&transfer,canonical)==STN_DATA_CONTENT);
    transfer.units=1u;
    transfer.destination=transfer.source;
    CHECK(stn_transfer_encode(&transfer,canonical)==STN_DATA_CONTENT);
    transfer.destination.type=STN_ADDRESS_WALLET;
    memset(transfer.destination.identifier,0x22,STN_ADDRESS_ID_SIZE);
    transfer.source.type=STN_ADDRESS_IDENTITY;
    CHECK(stn_transfer_encode(&transfer,canonical)==STN_DATA_TYPE);
    transfer.source.type=STN_ADDRESS_WALLET;
    CHECK(stn_transfer_encode(NULL,canonical)==STN_DATA_ARGUMENT);
    CHECK(stn_transfer_encode(&transfer,NULL)==STN_DATA_ARGUMENT);

    transfer.units=UINT64_C(0x0102030405060708);
    CHECK(stn_transfer_encode(&transfer,canonical)==STN_DATA_OK);
    memcpy(bad,canonical,sizeof(bad));bad[0]=2u;
    CHECK(stn_transfer_decode(bad,sizeof(bad),&decoded)==STN_DATA_CONTENT);
    CHECK(memcmp(&decoded,&snapshot,sizeof(decoded))==0);
    memcpy(bad,canonical,sizeof(bad));memset(bad+65u,0,8u);
    CHECK(stn_transfer_decode(bad,sizeof(bad),&decoded)==STN_DATA_CONTENT);
    CHECK(memcmp(&decoded,&snapshot,sizeof(decoded))==0);
    memcpy(bad,canonical,sizeof(bad));memcpy(bad+33u,bad+1u,STN_ADDRESS_ID_SIZE);
    CHECK(stn_transfer_decode(bad,sizeof(bad),&decoded)==STN_DATA_CONTENT);
    CHECK(memcmp(&decoded,&snapshot,sizeof(decoded))==0);
    CHECK(stn_transfer_decode(canonical,sizeof(canonical)-1u,&decoded)==STN_DATA_LENGTH);
    CHECK(stn_transfer_decode(NULL,sizeof(canonical),&decoded)==STN_DATA_ARGUMENT);
    CHECK(stn_transfer_decode(canonical,sizeof(canonical),NULL)==STN_DATA_ARGUMENT);
}

int test_transfer(void);
int test_transfer(void)
{
    vectors();
    printf("Transfer primitive: %u checks, %u failures.\n",checks,failures);
    return failures==0 ? 0 : 1;
}

#ifdef STN_TRANSFER_TEST_MAIN
int main(void){return test_transfer();}
#endif
