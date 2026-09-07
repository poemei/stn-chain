/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "../stn_backend.h"
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include "stn_windows_peer.h"

static stn_peer_status ready(stn_windows_peer *p,int writing)
{
    fd_set f,e;TIMEVAL time;uint64_t now=GetTickCount64(),remaining;int result;
    if(!p->opened){return STN_PEER_IO;}
    if(now>=p->deadline){return STN_PEER_TIMEOUT;}
    remaining=p->deadline-now;time.tv_sec=(long)(remaining/1000);time.tv_usec=(long)((remaining%1000)*1000);
    FD_ZERO(&f);FD_ZERO(&e);FD_SET((SOCKET)p->socket,&f);FD_SET((SOCKET)p->socket,&e);
    result=select(0,writing?NULL:&f,writing?&f:NULL,&e,&time);
    if(result==0){return STN_PEER_TIMEOUT;}
    if(result==SOCKET_ERROR || FD_ISSET((SOCKET)p->socket,&e)){return STN_PEER_IO;}
    return STN_PEER_OK;
}
static stn_peer_status send_all(void *u,const uint8_t *bytes,size_t n)
{
    stn_windows_peer *p=u;size_t offset=0;
    if(n>STN_PEER_MAX_FRAME){return STN_PEER_CAPACITY;}
    while(offset<n){
        int sent;stn_peer_status s=ready(p,1);if(s!=STN_PEER_OK){return s;}
        sent=send((SOCKET)p->socket,(const char *)bytes+offset,(int)(n-offset),0);
        if(sent==SOCKET_ERROR){if(WSAGetLastError()==WSAEWOULDBLOCK){continue;}return STN_PEER_IO;}
        if(sent==0){return STN_PEER_DISCONNECTED;}offset+=(size_t)sent;
    }return STN_PEER_OK;
}
static stn_peer_status receive_all(void *u,uint8_t *bytes,size_t n)
{
    stn_windows_peer *p=u;size_t offset=0;
    if(n>STN_PEER_MAX_FRAME){return STN_PEER_CAPACITY;}
    while(offset<n){
        int got;stn_peer_status s=ready(p,0);if(s!=STN_PEER_OK){return s;}
        got=recv((SOCKET)p->socket,(char *)bytes+offset,(int)(n-offset),0);
        if(got==SOCKET_ERROR){if(WSAGetLastError()==WSAEWOULDBLOCK){continue;}return STN_PEER_IO;}
        if(got==0){return STN_PEER_DISCONNECTED;}offset+=(size_t)got;
    }return STN_PEER_OK;
}
void stn_windows_peer_close(stn_windows_peer *p)
{
    if(p!=NULL && p->opened){closesocket((SOCKET)p->socket);p->opened=0;WSACleanup();}
}
static stn_peer_status setup(SOCKET socket_value,unsigned timeout,stn_windows_peer *out,stn_peer_transport *t)
{
    u_long mode=1;
    if(ioctlsocket(socket_value,FIONBIO,&mode)!=0){closesocket(socket_value);WSACleanup();return STN_PEER_IO;}
    out->socket=(uintptr_t)socket_value;out->deadline=GetTickCount64()+timeout;out->opened=1;
    if(t!=NULL){t->user=out;t->send=send_all;t->receive=receive_all;}return STN_PEER_OK;
}
stn_peer_status stn_windows_peer_connect(const char *ip,uint16_t port,unsigned timeout,
    stn_windows_peer *out,stn_peer_transport *transport)
{
    WSADATA data;SOCKET socket_value;struct sockaddr_in address={0};stn_peer_status s;int error=0,n=sizeof(error);
    if(ip==NULL || out==NULL || transport==NULL || timeout==0 || timeout>60000 || port==0){return STN_PEER_ARGUMENT;}
    address.sin_family=AF_INET;address.sin_port=htons(port);
    if(InetPtonA(AF_INET,ip,&address.sin_addr)!=1){return STN_PEER_ARGUMENT;}
    if(WSAStartup(MAKEWORD(2,2),&data)!=0){return STN_PEER_IO;}
    socket_value=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if(socket_value==INVALID_SOCKET){WSACleanup();return STN_PEER_IO;}
    s=setup(socket_value,timeout,out,transport);if(s!=STN_PEER_OK){return s;}
    if(connect(socket_value,(struct sockaddr *)&address,sizeof(address))==SOCKET_ERROR && WSAGetLastError()!=WSAEWOULDBLOCK){stn_windows_peer_close(out);return STN_PEER_IO;}
    s=ready(out,1);
    if(s==STN_PEER_OK && (getsockopt(socket_value,SOL_SOCKET,SO_ERROR,(char *)&error,&n)!=0 || error!=0)){s=STN_PEER_IO;}
    if(s!=STN_PEER_OK){stn_windows_peer_close(out);}return s;
}
stn_peer_status stn_windows_peer_listen(uint16_t port,stn_windows_peer *out,uint16_t *bound)
{
    WSADATA data;SOCKET socket_value;struct sockaddr_in address={0};int n=sizeof(address),exclusive=1;stn_peer_status s;
    if(out==NULL || bound==NULL){return STN_PEER_ARGUMENT;}
    if(WSAStartup(MAKEWORD(2,2),&data)!=0){return STN_PEER_IO;}
    socket_value=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);if(socket_value==INVALID_SOCKET){WSACleanup();return STN_PEER_IO;}
    s=setup(socket_value,60000,out,NULL);if(s!=STN_PEER_OK){return s;}
    address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);address.sin_port=htons(port);
    if(setsockopt(socket_value,SOL_SOCKET,SO_EXCLUSIVEADDRUSE,(const char *)&exclusive,sizeof(exclusive))!=0 ||
        bind(socket_value,(struct sockaddr *)&address,sizeof(address))!=0 || listen(socket_value,1)!=0 ||
        getsockname(socket_value,(struct sockaddr *)&address,&n)!=0){stn_windows_peer_close(out);return STN_PEER_IO;}
    *bound=ntohs(address.sin_port);return STN_PEER_OK;
}
stn_peer_status stn_windows_peer_accept(stn_windows_peer *listener,unsigned timeout,
    stn_windows_peer *out,stn_peer_transport *transport)
{
    WSADATA data;SOCKET accepted;stn_peer_status s;
    if(listener==NULL || out==NULL || transport==NULL || !listener->opened || timeout==0 || timeout>60000){return STN_PEER_ARGUMENT;}
    listener->deadline=GetTickCount64()+timeout;s=ready(listener,0);if(s!=STN_PEER_OK){return s;}
    accepted=accept((SOCKET)listener->socket,NULL,NULL);if(accepted==INVALID_SOCKET){return STN_PEER_IO;}
    if(WSAStartup(MAKEWORD(2,2),&data)!=0){closesocket(accepted);return STN_PEER_IO;}
    return setup(accepted,timeout,out,transport);
}
