/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_config.h"
#include <stdio.h>
#include <string.h>
static unsigned checks,failures;
#define CHECK(e) do{++checks;if(!(e)){++failures;fprintf(stderr,"config line %d: %s\n",__LINE__,#e);}}while(0)
static void vectors(void){
 stn_config c={0};stn_address a={0};char id[STN_ADDRESS_TEXT_CAPACITY];size_t n=0;char json[512];
 a.type=STN_ADDRESS_WALLET;memset(a.identifier,0x11,32u);
 CHECK(stn_address_encode(&a,id,sizeof(id),&n)==STN_DATA_OK);
 snprintf(json,sizeof(json),"{\"internal_miner\":{\"enabled\":true,\"wallet\":\"%s\"}}",id);
 CHECK(stn_config_decode((const uint8_t*)json,strlen(json),&c)==STN_DATA_OK);
 CHECK(c.internal_miner_enabled==1&&c.has_miner_wallet==1&&c.miner_wallet.type==STN_ADDRESS_WALLET);
 CHECK(stn_config_decode((const uint8_t*)"{\"internal_miner\":{\"enabled\":false}}",36u,&c)==STN_DATA_OK);
 CHECK(stn_config_decode((const uint8_t*)"{\"bad\":true}",12u,&c)==STN_DATA_CONTENT);
 CHECK(stn_config_decode(NULL,0u,&c)==STN_DATA_ARGUMENT);
}
int test_config(void){vectors();printf("Chain JSON config: %u checks, %u failures.\n",checks,failures);return failures?1:0;}
#ifdef STN_CONFIG_TEST_MAIN
int main(void){return test_config();}
#endif
