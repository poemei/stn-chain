/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "../stn_backend.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "stn_windows_storage.h"
#include <string.h>

static int ordinary(HANDLE h)
{
    BY_HANDLE_FILE_INFORMATION info;
    return GetFileType(h)==FILE_TYPE_DISK && GetFileInformationByHandle(h,&info) &&
        !(info.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY));
}
static stn_storage_status acquire(void *user)
{
    stn_windows_storage *s=user;HANDLE h;
    if(s->lock_handle!=NULL) { return STN_STORAGE_BUSY; }
    h=CreateFileW(s->lock_path,GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,NULL);
    if(h==INVALID_HANDLE_VALUE) { return GetLastError()==ERROR_SHARING_VIOLATION ? STN_STORAGE_BUSY : STN_STORAGE_IO; }
    if(!ordinary(h)) { CloseHandle(h);return STN_STORAGE_IO; }
    s->lock_handle=h;return STN_STORAGE_OK;
}
static void release(void *user)
{
    stn_windows_storage *s=user;
    if(s->lock_handle!=NULL) { CloseHandle((HANDLE)s->lock_handle);s->lock_handle=NULL; }
}
static stn_storage_status read_snapshot(void *user,uint8_t *bytes,size_t capacity,size_t *length)
{
    stn_windows_storage *s=user;HANDLE h;LARGE_INTEGER size;size_t offset=0;stn_storage_status result=STN_STORAGE_IO;
    if(s->lock_handle==NULL || bytes==NULL || length==NULL) { return STN_STORAGE_IO; }
    h=CreateFileW(s->path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,NULL);
    if(h==INVALID_HANDLE_VALUE) { return GetLastError()==ERROR_FILE_NOT_FOUND ? STN_STORAGE_NOT_FOUND : STN_STORAGE_IO; }
    if(!ordinary(h) || !GetFileSizeEx(h,&size) || size.QuadPart<0) { goto done; }
    if((ULONGLONG)size.QuadPart>STN_STORAGE_MAX_SIZE || (ULONGLONG)size.QuadPart>capacity) { result=STN_STORAGE_CAPACITY;goto done; }
    while(offset<(size_t)size.QuadPart) {
        DWORD got=0,request=(DWORD)(((size_t)size.QuadPart-offset)>65536 ? 65536 : (size_t)size.QuadPart-offset);
        if(!ReadFile(h,bytes+offset,request,&got,NULL) || got==0) { goto done; }
        offset+=got;
    }
    *length=offset;result=STN_STORAGE_OK;
done:
    if(!CloseHandle(h)) { result=STN_STORAGE_IO; }
    return result;
}
static stn_storage_status replace_snapshot(void *user,const uint8_t *bytes,size_t length)
{
    stn_windows_storage *s=user;HANDLE h;size_t offset=0;int ok=1;
    if(s->lock_handle==NULL || bytes==NULL || length>STN_STORAGE_MAX_SIZE || length<STN_STORAGE_OVERHEAD) { return STN_STORAGE_IO; }
    /* Never truncate/reuse a previous staging file. Same directory and volume. */
    h=CreateFileW(s->staging,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if(h==INVALID_HANDLE_VALUE) { return STN_STORAGE_IO; }
    while(offset<length) {
        DWORD wrote=0,request=(DWORD)(length-offset>65536 ? 65536 : length-offset);
        if(!WriteFile(h,bytes+offset,request,&wrote,NULL) || wrote==0) { ok=0;break; }
        offset+=wrote;
    }
    if(ok && !FlushFileBuffers(h)) { ok=0; }
    if(!CloseHandle(h)) { ok=0; }
    if(ok && MoveFileExW(s->staging,s->path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) {
        return STN_STORAGE_OK; /* Publication point; no fallible work follows. */
    }
    /* Only our CREATE_NEW staging file is removed; authoritative file untouched. */
    DeleteFileW(s->staging);return STN_STORAGE_IO;
}
stn_storage_status stn_windows_storage_init(stn_windows_storage *s,const wchar_t *path,stn_storage_provider *p)
{
    stn_windows_storage temp={0};stn_storage_provider provider;
    wchar_t root[260],fs[32];DWORD n;size_t i;
    if(s==NULL || path==NULL || p==NULL) { return STN_STORAGE_ARGUMENT; }
    n=GetFullPathNameW(path,260,temp.path,NULL);
    if(n<4 || n>=250 || temp.path[1]!=L':' || temp.path[2]!=L'\\') { return STN_STORAGE_ARGUMENT; }
    for(i=3;i<n;++i) { if(temp.path[i]==L':' || temp.path[i]==L'*' || temp.path[i]==L'?') { return STN_STORAGE_ARGUMENT; } }
    if(temp.path[n-1]==L'\\' || temp.path[n-1]==L'.' || temp.path[n-1]==L' ') { return STN_STORAGE_ARGUMENT; }
    if(!GetVolumePathNameW(temp.path,root,260) || GetDriveTypeW(root)!=DRIVE_FIXED ||
        !GetVolumeInformationW(root,NULL,0,NULL,NULL,NULL,fs,32) || wcscmp(fs,L"NTFS")!=0) { return STN_STORAGE_IO; }
    memcpy(temp.staging,temp.path,(n+1)*sizeof(wchar_t));memcpy(temp.lock_path,temp.path,(n+1)*sizeof(wchar_t));
    memcpy(temp.staging+n,L".stage",7*sizeof(wchar_t));memcpy(temp.lock_path+n,L".lock",6*sizeof(wchar_t));
    provider.user=s;provider.acquire=acquire;provider.release=release;
    provider.read=read_snapshot;provider.replace=replace_snapshot;
    *s=temp;*p=provider;return STN_STORAGE_OK;
}
