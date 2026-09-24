/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_transfer_envelope.h"
#include <stdio.h>
#include <string.h>
static unsigned checks,failures;
#define CHECK(x) do{++checks;if(!(x)){++failures;printf("FAIL line %d: %s\n",__LINE__,#x);}}while(0)
int test_transfer_envelope(void)
{
 stn_transfer_envelope e={0},d={0},before;uint8_t bytes[STN_TRANSFER_ENVELOPE_CANONICAL_SIZE],bad[STN_TRANSFER_ENVELOPE_CANONICAL_SIZE];
 memset(e.controller,0x11,32);e.nonce[31]=1;e.transfer.source.type=STN_ADDRESS_WALLET;
 e.transfer.destination.type=STN_ADDRESS_WALLET;memset(e.transfer.source.identifier,0x22,32);
 memset(e.transfer.destination.identifier,0x33,32);e.transfer.units=25;memset(e.signature,0x44,64);
 CHECK(stn_transfer_envelope_encode(&e,bytes)==STN_DATA_OK);
 CHECK(sizeof(bytes)==202u);
 CHECK(bytes[0]==STN_TRANSFER_ENVELOPE_VERSION);
 CHECK(memcmp(bytes+1,e.controller,32)==0);
 CHECK(memcmp(bytes+33,e.nonce,32)==0);
 CHECK(stn_transfer_envelope_decode(bytes,sizeof(bytes),&d)==STN_DATA_OK);
 CHECK(memcmp(d.controller,e.controller,32)==0);
 CHECK(memcmp(d.nonce,e.nonce,32)==0);
 CHECK(d.transfer.units==25u);
 CHECK(memcmp(d.signature,e.signature,64)==0);
 before=d;memcpy(bad,bytes,sizeof(bad));bad[0]=2;
 CHECK(stn_transfer_envelope_decode(bad,sizeof(bad),&d)==STN_DATA_VERSION);
 CHECK(memcmp(&d,&before,sizeof(d))==0);
 CHECK(stn_transfer_envelope_decode(bytes,sizeof(bytes)-1,&d)==STN_DATA_LENGTH);
 CHECK(memcmp(&d,&before,sizeof(d))==0);
 memcpy(bad,bytes,sizeof(bad));memset(bad+33,0,32);
 CHECK(stn_transfer_envelope_decode(bad,sizeof(bad),&d)==STN_DATA_CONTENT);
 CHECK(memcmp(&d,&before,sizeof(d))==0);
 CHECK(stn_transfer_envelope_encode(NULL,bytes)==STN_DATA_ARGUMENT);
 CHECK(stn_transfer_envelope_encode(&e,NULL)==STN_DATA_ARGUMENT);
 CHECK(stn_transfer_envelope_decode(NULL,sizeof(bytes),&d)==STN_DATA_ARGUMENT);
 CHECK(stn_transfer_envelope_decode(bytes,sizeof(bytes),NULL)==STN_DATA_ARGUMENT);
 memset(e.nonce,0,32);CHECK(stn_transfer_envelope_encode(&e,bytes)==STN_DATA_CONTENT);
 printf("Transfer envelope: %u checks, %u failures.\n",checks,failures);return failures?1:0;
}
#ifdef STN_TRANSFER_ENVELOPE_TEST_MAIN
int main(void){return test_transfer_envelope();}
#endif
