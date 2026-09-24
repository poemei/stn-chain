/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_transfer_envelope_authorization.h"
#include "stn_wallet.h"
#include <stdio.h>
#include <string.h>
int stn_ed25519_sign(const uint8_t *,size_t,const uint8_t[32],const uint8_t[32],uint8_t[64]);
static unsigned checks,failures;
#define CHECK(x) do{++checks;if(!(x)){++failures;printf("FAIL line %d: %s\n",__LINE__,#x);}}while(0)
int test_transfer_envelope_authorization(void)
{
 static const uint8_t sk[32]={0x9d,0x61,0xb1,0x9d,0xef,0xfd,0x5a,0x60,0xba,0x84,0x4a,0xf4,0x92,0xec,0x2c,0xc4,0x44,0x49,0xc5,0x69,0x7b,0x32,0x69,0x19,0x70,0x3b,0xac,0x03,0x1c,0xae,0x7f,0x60};
 static const uint8_t pk[32]={0xd7,0x5a,0x98,0x01,0x82,0xb1,0x0a,0xb7,0xd5,0x4b,0xfe,0xd3,0xc9,0x64,0x07,0x3a,0x0e,0xe1,0x72,0xf3,0xda,0xa6,0x23,0x25,0xaf,0x02,0x1a,0x68,0xf7,0x07,0x51,0x1a};
 stn_transfer_envelope e={0},changed;stn_address id={0};uint8_t statement[STN_TRANSFER_ENVELOPE_AUTHORIZATION_STATEMENT_SIZE];
 id.type=STN_ADDRESS_IDENTITY;memcpy(id.identifier,pk,32);memcpy(e.controller,pk,32);e.nonce[31]=1;
 CHECK(stn_wallet_derive(&id,&e.transfer.source)==STN_DATA_OK);
 e.transfer.destination.type=STN_ADDRESS_WALLET;memset(e.transfer.destination.identifier,0x22,32);e.transfer.units=25;
 CHECK(stn_transfer_envelope_authorization_statement(&e,statement)==STN_DATA_OK);
 CHECK(memcmp(statement,STN_TRANSFER_ENVELOPE_AUTHORIZATION_DOMAIN,STN_TRANSFER_ENVELOPE_AUTHORIZATION_DOMAIN_SIZE-1u)==0);
 CHECK(statement[STN_TRANSFER_ENVELOPE_AUTHORIZATION_DOMAIN_SIZE-1u]==0u);
 CHECK(statement[STN_TRANSFER_ENVELOPE_AUTHORIZATION_DOMAIN_SIZE]==STN_TRANSFER_ENVELOPE_AUTHORIZATION_VERSION);
 CHECK(stn_ed25519_sign(statement,sizeof(statement),pk,sk,e.signature)==0);
 CHECK(stn_transfer_envelope_authorization_verify(&e)==STN_DATA_OK);
 changed=e;changed.nonce[31]=2;CHECK(stn_transfer_envelope_authorization_verify(&changed)==STN_DATA_CONTENT);
 changed=e;changed.transfer.units++;CHECK(stn_transfer_envelope_authorization_verify(&changed)==STN_DATA_CONTENT);
 changed=e;changed.transfer.destination.identifier[0]^=1;CHECK(stn_transfer_envelope_authorization_verify(&changed)==STN_DATA_CONTENT);
 changed=e;changed.signature[0]^=1;CHECK(stn_transfer_envelope_authorization_verify(&changed)==STN_DATA_CONTENT);
 changed=e;memset(changed.nonce,0,32);CHECK(stn_transfer_envelope_authorization_statement(&changed,statement)==STN_DATA_CONTENT);
 CHECK(stn_transfer_envelope_authorization_statement(NULL,statement)==STN_DATA_ARGUMENT);
 CHECK(stn_transfer_envelope_authorization_statement(&e,NULL)==STN_DATA_ARGUMENT);
 CHECK(stn_transfer_envelope_authorization_verify(NULL)==STN_DATA_ARGUMENT);
 printf("Transfer envelope authorization: %u checks, %u failures.\n",checks,failures);return failures?1:0;
}
#ifdef STN_TRANSFER_ENVELOPE_AUTHORIZATION_TEST_MAIN
int main(void){return test_transfer_envelope_authorization();}
#endif
