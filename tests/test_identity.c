#include "stn_identity.h"
#include <stdio.h>
#include <string.h>
#define CHECK(e) do { ++checks; if(!(e)){++failures;fprintf(stderr,"identity line %d: %s\n",__LINE__,#e);} } while(0)
static unsigned checks,failures;
static const uint8_t key[32]={0xd7,0x5a,0x98,0x01,0x82,0xb1,0x0a,0xb7,0xd5,0x4b,0xfe,0xd3,0xc9,0x64,0x07,0x3a,0x0e,0xe1,0x72,0xf3,0xda,0xa6,0x23,0x25,0xaf,0x02,0x1a,0x68,0xf7,0x07,0x51,0x1a};
static const uint8_t signature[64]={0xe5,0x56,0x43,0x00,0xc3,0x60,0xac,0x72,0x90,0x86,0xe2,0xcc,0x80,0x6e,0x82,0x8a,0x84,0x87,0x7f,0x1e,0xb8,0xe5,0xd9,0x74,0xd8,0x73,0xe0,0x65,0x22,0x49,0x01,0x55,0x5f,0xb8,0x82,0x15,0x90,0xa3,0x3b,0xac,0xc6,0x1e,0x39,0x70,0x1c,0xf9,0xb4,0x6b,0xd2,0x5b,0xf5,0xf0,0x59,0x5b,0xbe,0x24,0x65,0x51,0x41,0x43,0x8e,0x7a,0x10,0x0b};
int test_identity(void)
{
    uint8_t id[32],statement[64],mutated[64],bad[64];size_t n=0;uint8_t zero[32]={0};
    CHECK(stn_identity_derive(key,id)==STN_IDENTITY_VALID && memcmp(id,key,32)==0);
    CHECK(stn_identity_statement(NULL,0,statement,sizeof(statement),&n)==STN_IDENTITY_VALID && n==24 && statement[23]==0);
    CHECK(stn_identity_verify(key,NULL,0,signature,64)==STN_IDENTITY_MALFORMED);
    CHECK(stn_identity_verify(key,(const uint8_t *)"",0,signature,64)==STN_IDENTITY_VALID);
    memcpy(mutated,signature,64);mutated[0]^=1;CHECK(stn_identity_verify(key,(const uint8_t *)"",0,mutated,64)==STN_IDENTITY_INVALID);
    memcpy(mutated,key,32);mutated[0]^=1;CHECK(stn_identity_verify(mutated,(const uint8_t *)"",0,signature,64)==STN_IDENTITY_INVALID);
    CHECK(stn_identity_verify(key,(const uint8_t *)"x",1,signature,64)==STN_IDENTITY_INVALID);
    memset(bad,0xff,64);CHECK(stn_identity_verify(key,(const uint8_t *)"",0,bad,64)==STN_IDENTITY_MALFORMED);
    CHECK(stn_identity_verify(key,(const uint8_t *)"",0,signature,63)==STN_IDENTITY_MALFORMED);
    CHECK(stn_identity_verify(key,(const uint8_t *)"",0,signature,65)==STN_IDENTITY_MALFORMED);
    CHECK(stn_identity_derive(zero,id)==STN_IDENTITY_MALFORMED);
    memset(mutated,0xff,32);CHECK(stn_identity_derive(mutated,id)==STN_IDENTITY_MALFORMED);
    CHECK(stn_identity_statement(NULL,1,statement,sizeof(statement),&n)==STN_IDENTITY_MALFORMED);
    CHECK(stn_identity_statement(NULL,0,statement,23,&n)==STN_IDENTITY_MALFORMED && n==0);
    printf("Identity/signature: %u checks, %u failures.\n",checks,failures);return failures!=0;
}
