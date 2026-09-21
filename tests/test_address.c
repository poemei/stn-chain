/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_address.h"
#include <stdio.h>
#include <string.h>
static unsigned checks, failures;
#define CHECK(e) do { ++checks; if(!(e)) { ++failures; fprintf(stderr,"address line %d: %s\n",__LINE__,#e); } } while(0)

int test_address(void)
{
    static const char *const hashes[] = {
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
        "a52d159f262b2c6ddb724a61840befc36eb30c88877a4030b65cbe86298449c9",
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
        "dc1114cd074914bd872cc1f9a23ec910ea2203bc79779ab2e17da25782a624fc"
    };
    static const uint8_t *const sources[] = {
        (const uint8_t *)"abc", (const uint8_t *)"abd", NULL, (const uint8_t *)"abc"
    };
    static const size_t lengths[] = {3,3,0,4};
    static const char *const prefixes[] = {"stn0_","stnc0_","stnw0_"};
    stn_address a, b, before;
    char text[STN_ADDRESS_TEXT_CAPACITY], expected[STN_ADDRESS_TEXT_CAPACITY];
    char short_text[STN_ADDRESS_SHORT_CAPACITY], bad[STN_ADDRESS_TEXT_CAPACITY+1];
    size_t i,j,k,n,w,offset;
    checks=failures=0;
    for(i=0;i<3;++i) {
        offset=strlen(prefixes[i]);
        for(j=0;j<4;++j) {
            CHECK(stn_address_derive((stn_address_type)(i+1),sources[j],lengths[j],&a)==STN_DATA_OK);
            CHECK(stn_address_derive((stn_address_type)(i+1),sources[j],lengths[j],&b)==STN_DATA_OK);
            CHECK(a.type==b.type && memcmp(a.identifier,b.identifier,32)==0);
            memcpy(expected,prefixes[i],offset);memcpy(expected+offset,hashes[j],65);
            CHECK(stn_address_encode(&a,text,sizeof(text),&n)==STN_DATA_OK);
            CHECK(n==offset+64 && strcmp(text,expected)==0);
            CHECK(stn_address_validate(text,n)==STN_DATA_OK);
            CHECK(stn_address_decode(text,n,&b)==STN_DATA_OK);
            CHECK(a.type==b.type && memcmp(a.identifier,b.identifier,32)==0);
            CHECK(stn_address_encode(&b,text,sizeof(text),&w)==STN_DATA_OK && w==n && strcmp(text,expected)==0);
            CHECK(stn_address_abbreviate(&a,short_text,sizeof(short_text),&w)==STN_DATA_OK);
            memcpy(expected,prefixes[i],offset);memcpy(expected+offset,".....",5);memcpy(expected+offset+5,hashes[j]+58,7);
            CHECK(w==offset+11 && strcmp(short_text,expected)==0);
            CHECK(stn_address_validate(short_text,w)==STN_DATA_LENGTH);
            before=b;
            CHECK(stn_address_decode(short_text,w,&b)==STN_DATA_LENGTH && memcmp(&b,&before,sizeof(b))==0);
            CHECK(stn_address_validate(text,n-1)==STN_DATA_LENGTH);
            CHECK(stn_address_validate(text,n+1)!=STN_DATA_OK);
            memcpy(bad,text,n);bad[n]='0';
            CHECK(stn_address_validate(bad,n+1)!=STN_DATA_OK);
            for(k=offset;k<n;++k) {
                memcpy(bad,text,n);bad[k]='G';
                CHECK(stn_address_decode(bad,n,&b)==STN_DATA_CONTENT);
                CHECK(memcmp(&b,&before,sizeof(b))==0);
                bad[k]='A';CHECK(stn_address_validate(bad,n)==STN_DATA_CONTENT);
                bad[k]='\0';CHECK(stn_address_validate(bad,n)==STN_DATA_CONTENT);
            }
            for(k=0;k<offset;++k) {
                memcpy(bad,text,n);bad[k]='X';
                CHECK(stn_address_validate(bad,n)==STN_DATA_TYPE);
            }
            for(k=0;k<=n;++k) {
                memset(bad,'!',sizeof(bad));w=999;
                CHECK(stn_address_encode(&a,bad,k,&w)==STN_DATA_CAPACITY && w==0);
                CHECK(bad[0]=='!' && bad[sizeof(bad)-1]=='!');
            }
            memset(bad,'!',sizeof(bad));
            CHECK(stn_address_abbreviate(&a,bad,offset+11,&w)==STN_DATA_CAPACITY && w==0 && bad[0]=='!');
        }
    }
    CHECK(stn_address_derive(STN_ADDRESS_IDENTITY,sources[0],3,&a)==STN_DATA_OK);
    CHECK(stn_address_derive(STN_ADDRESS_IDENTITY,sources[1],3,&b)==STN_DATA_OK);
    CHECK(memcmp(a.identifier,b.identifier,32)!=0);
    before=a;
    CHECK(stn_address_derive((stn_address_type)0,sources[0],3,&a)==STN_DATA_TYPE && memcmp(&a,&before,sizeof(a))==0);
    CHECK(stn_address_derive(STN_ADDRESS_IDENTITY,NULL,1,&a)==STN_DATA_ARGUMENT);
    CHECK(stn_address_derive(STN_ADDRESS_IDENTITY,NULL,0,NULL)==STN_DATA_ARGUMENT);
    CHECK(stn_address_decode(NULL,69,&a)==STN_DATA_ARGUMENT);
    CHECK(stn_address_decode(text,70,NULL)==STN_DATA_ARGUMENT);
    CHECK(stn_address_encode(NULL,text,sizeof(text),&w)==STN_DATA_ARGUMENT && w==0);
    CHECK(stn_address_encode(&a,NULL,sizeof(text),&w)==STN_DATA_ARGUMENT && w==0);
    CHECK(stn_address_encode(&a,text,sizeof(text),NULL)==STN_DATA_ARGUMENT);
    a.type=(stn_address_type)99;
    CHECK(stn_address_encode(&a,text,sizeof(text),&w)==STN_DATA_TYPE && w==0);
    CHECK(stn_address_abbreviate(&a,text,sizeof(text),&w)==STN_DATA_TYPE && w==0);
#if SIZE_MAX > UINT32_MAX
    CHECK(stn_address_derive(STN_ADDRESS_IDENTITY,sources[0],(size_t)UINT32_MAX+1,&a)==STN_DATA_LENGTH);
#endif
    a.type=STN_ADDRESS_IDENTITY;memset(a.identifier,0,32);
    CHECK(stn_address_encode(&a,text,sizeof(text),&n)==STN_DATA_OK);
    CHECK(stn_address_decode(text,n,&b)==STN_DATA_OK && memcmp(a.identifier,b.identifier,32)==0);
    CHECK(stn_address_abbreviate(&a,short_text,sizeof(short_text),&w)==STN_DATA_OK && strcmp(short_text,"stn0_.....000000")==0);
    printf("Typed addresses: %u checks, %u failures.\n",checks,failures);
    return failures!=0;
}

#ifdef STN_ADDRESS_TEST_MAIN
int main(void) { return test_address(); }
#endif
