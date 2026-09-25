/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_config.h"
#include <string.h>

typedef struct parser { const uint8_t *p,*end; } parser;
static void ws(parser *q){while(q->p<q->end&&(*q->p==' '||*q->p=='\t'||*q->p=='\r'||*q->p=='\n'))++q->p;}
static int ch(parser *q,uint8_t c){ws(q);if(q->p>=q->end||*q->p!=c)return 0;++q->p;return 1;}
static int lit(parser *q,const char *s){size_t n=strlen(s);ws(q);if((size_t)(q->end-q->p)<n||memcmp(q->p,s,n)!=0)return 0;q->p+=n;return 1;}
static int str(parser *q,char *out,size_t cap,size_t *n){
 size_t k=0;ws(q);if(q->p>=q->end||*q->p!='"')return 0;++q->p;
 while(q->p<q->end&&*q->p!='"'){uint8_t c=*q->p++;if(c<0x20u||c=='\\')return 0;if(k+1u>=cap)return 0;out[k++]=(char)c;}
 if(q->p>=q->end)return 0;++q->p;out[k]='\0';*n=k;return 1;
}
stn_data_status stn_config_decode(const uint8_t *bytes,size_t length,stn_config *out){
 parser q;stn_config v={0};int seen_miner=0,seen_enabled=0,seen_identity=0;
 if(bytes==NULL||out==NULL)return STN_DATA_ARGUMENT;
 if(length==0u||length>STN_CONFIG_MAX_BYTES)return STN_DATA_CONTENT;
 q.p=bytes;q.end=bytes+length;if(!ch(&q,'{'))return STN_DATA_CONTENT;ws(&q);
 if(q.p<q.end&&*q.p=='}'){++q.p;ws(&q);if(q.p!=q.end)return STN_DATA_CONTENT;*out=v;return STN_DATA_OK;}
 for(;;){
  char key[64];size_t kn;if(!str(&q,key,sizeof(key),&kn)||!ch(&q,':'))return STN_DATA_CONTENT;
  if(kn==14u&&memcmp(key,"internal_miner",14u)==0){
   if(seen_miner++)return STN_DATA_CONTENT;if(!ch(&q,'{'))return STN_DATA_CONTENT;
   for(;;){
    char mk[64],value[STN_ADDRESS_TEXT_CAPACITY];size_t mn,vn;
    if(!str(&q,mk,sizeof(mk),&mn)||!ch(&q,':'))return STN_DATA_CONTENT;
    if(mn==7u&&memcmp(mk,"enabled",7u)==0){
     if(seen_enabled++)return STN_DATA_CONTENT;
     if(lit(&q,"true"))v.internal_miner_enabled=1;else if(lit(&q,"false"))v.internal_miner_enabled=0;else return STN_DATA_CONTENT;
    }else if(mn==8u&&memcmp(mk,"identity",8u)==0){
     if(seen_identity++||!str(&q,value,sizeof(value),&vn))return STN_DATA_CONTENT;
     if(stn_address_decode(value,vn,&v.miner_identity)!=STN_DATA_OK||v.miner_identity.type!=STN_ADDRESS_IDENTITY)return STN_DATA_TYPE;
     v.has_miner_identity=1;
    }else return STN_DATA_CONTENT;
    ws(&q);if(q.p<q.end&&*q.p==','){++q.p;continue;}if(!ch(&q,'}'))return STN_DATA_CONTENT;break;
   }
  }else return STN_DATA_CONTENT;
  ws(&q);if(q.p<q.end&&*q.p==','){++q.p;continue;}if(!ch(&q,'}'))return STN_DATA_CONTENT;break;
 }
 ws(&q);if(q.p!=q.end)return STN_DATA_CONTENT;
 if(v.internal_miner_enabled&&!v.has_miner_identity)return STN_DATA_CONTENT;
 *out=v;return STN_DATA_OK;
}
