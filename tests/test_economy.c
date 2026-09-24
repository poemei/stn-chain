/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_economy.h"
#include <stdio.h>
#include <string.h>

static unsigned checks,failures;
#define CHECK(e) do { ++checks; if(!(e)){++failures;fprintf(stderr,"economy line %d: %s\n",__LINE__,#e);} } while(0)

static void max_target(uint8_t target[32])
{
    memset(target,0xff,32);
    target[0]=0x7f;
}

static void share_target_vectors(void)
{
    uint8_t chain[32]={0},out[32],saved[32],expected[32]={0};

    chain[31]=1;
    memset(out,0xa5,32);
    CHECK(stn_economy_share_target(chain,out)==STN_DATA_OK);
    memset(expected,0,32);expected[31]=10;
    CHECK(memcmp(out,expected,32)==0);

    memset(chain,0,32);chain[30]=0x12;chain[31]=0x34;
    CHECK(stn_economy_share_target(chain,out)==STN_DATA_OK);
    memset(expected,0,32);expected[30]=0xb6;expected[31]=0x08;
    CHECK(memcmp(out,expected,32)==0);

    /* Carry across bytes remains exact. */
    memset(chain,0,32);chain[30]=0x19;chain[31]=0x99;
    CHECK(stn_economy_share_target(chain,out)==STN_DATA_OK);
    memset(expected,0,32);expected[30]=0xff;expected[31]=0xfa;
    CHECK(memcmp(out,expected,32)==0);

    /* Largest unsaturated integer floor(MAX_TARGET / 10). */
    memset(chain,0x99,32);chain[0]=0x0c;
    CHECK(stn_economy_share_target(chain,out)==STN_DATA_OK);
    memset(expected,0xff,32);expected[0]=0x7f;expected[31]=0xfa;
    CHECK(memcmp(out,expected,32)==0);

    /* One larger target saturates exactly at MAX_TARGET. */
    chain[31]=0x9a;
    CHECK(stn_economy_share_target(chain,out)==STN_DATA_OK);
    max_target(expected);
    CHECK(memcmp(out,expected,32)==0);

    max_target(chain);
    CHECK(stn_economy_share_target(chain,out)==STN_DATA_OK);
    CHECK(memcmp(out,chain,32)==0);

    /* Aliasing is explicitly supported. */
    memset(chain,0,32);chain[31]=7;
    CHECK(stn_economy_share_target(chain,chain)==STN_DATA_OK);
    CHECK(chain[31]==70 && chain[0]==0);

    memset(saved,0xa5,32);memcpy(out,saved,32);
    CHECK(stn_economy_share_target(NULL,out)==STN_DATA_ARGUMENT);
    CHECK(memcmp(out,saved,32)==0);

    memset(chain,0,32);
    CHECK(stn_economy_share_target(chain,out)==STN_DATA_TARGET);
    CHECK(memcmp(out,saved,32)==0);

    max_target(chain);chain[0]=0x80;
    CHECK(stn_economy_share_target(chain,out)==STN_DATA_TARGET);
    CHECK(memcmp(out,saved,32)==0);

    CHECK(stn_economy_share_target(chain,NULL)==STN_DATA_ARGUMENT);
}

int test_economy(void);
int test_economy(void)
{
    share_target_vectors();
    printf("Economy share target: %u checks, %u failures.\n",checks,failures);
    return failures==0 ? 0 : 1;
}

#ifdef STN_ECONOMY_TEST_MAIN
int main(void)
{
    return test_economy();
}
#endif
