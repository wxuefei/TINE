#include "3d.h"
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <time.h>
#include <stddef.h>
#include <stdalign.h>
#ifdef TARGET_WIN32
#include <windows.h>
#include <fileapi.h>
#include <shlwapi.h>
#include <memoryapi.h>
#include <winbase.h>
#include "ext/wineditline-2.206/include/editline/readline.h"
#else
#include <sys/mman.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <errno.h>
#endif
void HolyFree(void *ptr) {
	static void *fptr;
	if(!fptr) {
		fptr=map_get(&TOSLoader,"_FREE")->data->val;
	}
	FFI_CALL_TOS_1(fptr,(int64_t)ptr);
}
void *HolyMAlloc(int64_t sz) {
	static void *fptr;
	if(!fptr) {
		fptr=map_get(&TOSLoader,"_MALLOC")->data->val;
	}
	return (void*)FFI_CALL_TOS_2(fptr,(int64_t)sz,(int64_t)NULL);
}
char *HolyStrDup(char *str) {
	return strcpy(HolyMAlloc(strlen(str)+1),str);
}
#ifdef USE_NETWORKING
#include "ext/dyad/src/dyad.h"
static void STK_DyadInit() {
		static int64_t i;
		if(!i) {
			i=1;
			dyad_init();
			dyad_setUpdateTimeout(0.);
		}
}
static void STK_DyadUpdate() {
	dyad_update();
}
static void STK_DyadShutdown() {
	dyad_shutdown();
}
static void *STK_DyadNewStream() {
	return dyad_newStream();
}
static void *STK_DyadListen(int64_t *stk) {
	return dyad_listen(stk[0],stk[1]);
}
static void *STK_DyadConnect(int64_t *stk) {
	return dyad_connect(stk[0],stk[1],stk[2]);
}
static void STK_DyadWrite(int64_t *stk) {
	dyad_write(stk[0],stk[1],stk[2]);
}
static void STK_DyadEnd(int64_t *stk) {
	dyad_end(stk[0]);
}
static void STK_DyadClose(int64_t *stk) {
	dyad_close(stk[0]);
}
static char *STK_DyadGetAddress(int64_t *stk) {
	const char *ret=dyad_getAddress(stk[0]);
	return HolyStrDup(ret);
}
static void DyadReadCB(dyad_Event *e) {
	FFI_CALL_TOS_4(e->udata,e->stream,e->data,e->size,e->udata2);
}
static void STK_DyadSetReadCallback(int64_t *stk) {
	dyad_addListener((void*)stk[0],DYAD_EVENT_LINE,&DyadReadCB,stk[1],stk[2]);
}
static void DyadListenCB(dyad_Event *e) {
	FFI_CALL_TOS_2(e->udata,e->remote,e->udata2);
}
static void DyadCloseCB(dyad_Event *e) {
	FFI_CALL_TOS_2(e->udata,e->stream,e->udata2);
}
static void STK_DyadSetCloseCallback(int64_t *stk) {
	dyad_addListener((void*)stk[0],DYAD_EVENT_CLOSE,&DyadCloseCB,stk[1],stk[2]);
}
static void STK_DyadSetConnectCallback(int64_t *stk) {
	dyad_addListener((void*)stk[0],DYAD_EVENT_CONNECT,&DyadListenCB,stk[1],stk[2]);
}
static void STK_DyadSetDestroyCallback(int64_t *stk) {
	dyad_addListener((void*)stk[0],DYAD_EVENT_DESTROY,&DyadCloseCB,stk[1],stk[2]);
}
static void STK_DyadSetErrorCallback(int64_t *stk) {
	dyad_addListener((void*)stk[0],DYAD_EVENT_ERROR,&DyadCloseCB,stk[1],stk[2]);
}
static void STK_DyadSetReadyCallback(int64_t *stk) {
	dyad_addListener((void*)stk[0],DYAD_EVENT_READY,&DyadListenCB,stk[1],stk[2]);
}
static void STK_DyadSetTickCallback(int64_t *stk) {
	dyad_addListener((void*)stk[0],DYAD_EVENT_TICK,&DyadListenCB,stk[1],stk[2]);
}
static void STK_DyadSetTimeoutCallback(int64_t *stk) {
	dyad_addListener((void*)stk[0],DYAD_EVENT_TIMEOUT,&DyadListenCB,stk[1],stk[2]);
}
static void STK_DyadSetOnListenCallback(int64_t *stk) {
	dyad_addListener(stk[0],DYAD_EVENT_ACCEPT,&DyadListenCB,stk[1],stk[2]);
}
static void STK_DyadSetTimeout(int64_t *stk) {
	dyad_setTimeout(stk[0],((double*)stk)[1]);
}
static void STK_DyadSetNoDelay(int64_t *stk) {
	dyad_setNoDelay(stk[0],stk[1]);
}

#endif
void UnblockSignals() {
	#ifndef TARGET_WIN32
	sigset_t all;
	sigfillset(&all);
	sigprocmask(SIG_UNBLOCK,&all,NULL);
	#endif
}
typedef struct CType CType;
static int64_t BFFS(int64_t v) {
	if(!v) return -1;
    return __builtin_ffsl(v)-1;
}
static int64_t BCLZ(int64_t v) {
	if(!v) return -1;
    return 63-__builtin_clzl(v);
}
static void *MemNCpy(void *d,void *s,long sz) {
    return memcpy(d,s,sz);
}
static int64_t IsValidPtr(int64_t *stk) {
	#ifdef TARGET_WIN32
	//Wine doesnt like the IsBadReadPtr,so use a polyfill
	//return !IsBadReadPtr(stk[0],8);
    MEMORY_BASIC_INFORMATION mbi;
    memset(&mbi,0,sizeof mbi);
    if(VirtualQuery(stk[0], &mbi, sizeof(mbi))) {
		//https://stackoverflow.com/questions/496034/most-efficient-replacement-for-isbadreadptr
        DWORD mask=(PAGE_READONLY|PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY);
        int64_t b=!!(mbi.Protect&mask);
        return b;
    }
    return 0;
	#else
	static int64_t ps;
	if(!ps)
		ps=getpagesize();
	stk[0]/=ps;
	stk[0]*=ps;
	//https://renatocunha.com/2015/12/msync-pointer-validity/
	return -1!=msync((void*)stk[0],ps,MS_ASYNC);
	#endif
}
static int64_t __Move(char *old,char *new) {
	int ret=0;
	old=__VFsFileNameAbs(old);
	new=__VFsFileNameAbs(new);
	if(old&&new)
		ret=0==rename(old,new);
	TD_FREE(old);
	TD_FREE(new);
    return ret;
}
static int64_t IsDir(char *fn) {
    fn=__VFsFileNameAbs(fn);
    if(!fn) return 0;
    struct stat buf;
    stat(fn, &buf);
    TD_FREE(fn);
    return S_ISDIR(buf.st_mode);
}
int64_t FileWrite(char *fn,void *data,int64_t sz) {
    FILE *f=fopen(fn,"wb");
    if(!f) return 0;
    fwrite(data, 1, sz, f);
    fclose(f);
    return 1;
}
void* FileRead(char *fn,int64_t *sz) {
    FILE *f=fopen(fn,"rb");
    if(!f) return NULL;
    fseek(f,0,SEEK_END);
    size_t len=ftell(f);
    fseek(f,0, SEEK_SET);
    len-=ftell(f);
    void *data=HolyMAlloc(len+1);
    fread(data, 1, len, f);
    ((char*)data)[len]=0;
    fclose(f);
    if(sz) *sz=len;
    return data;
}
static void STK_InteruptCore(int64_t *stk) {
	InteruptCore(stk[0]);
}
static void ForeachFunc(void(*func)(const char *name,void *ptr,long sz)) {
  map_iter_t iter;
  const char *key;
  CSymbol *sym;
  iter=map_iter(&Loader.symbols);
  CHash *h;
  while(key=map_next(&Loader.symbols,&iter)) {
      if(!map_get(&TOSLoader,key)) {
          FFI_CALL_TOS_3(func,(int64_t)key,map_get(&Loader.symbols,key)->value_ptr,HTT_FUN);
      }
  }
  iter=map_iter(&TOSLoader);
  while(key=map_next(&TOSLoader,&iter)) {
    int64_t iter;
    vec_CHash_t *var=map_get(&TOSLoader, key);
    vec_foreach_ptr(var,h,iter) {
        if(h->type==HTT_EXPORT_SYS_SYM)
            FFI_CALL_TOS_3(func,(int64_t)key,(int64_t)h->val,HTT_FUN);
        else
            FFI_CALL_TOS_3(func,(int64_t)key,(int64_t)h->val,h->type);
    }
  }
}
static char *GetCipherPasswd() {
	if(!cipher_passwd)
		return NULL;
	return HolyStrDup(cipher_passwd);
}
static void STK_RegisterFunctionPtr(vec_char_t *blob,char *name,void *fptr,int64_t arity) {
	int64_t blob_off=blob->length,i;
    #ifndef TARGET_WIN32
	/*
	// RBP+16 This is where TempleOS puts the argument
	// RBP+8 the return address
	// RBP+0 The old TRBP 
	PUSH RBP
	MOV RBP,RSP 
	AND RSP,-0x10
	PUSH RSI
	PUSH RDI
	PUSH R10
	PUSH R11
	LEA RDI,[RBP+8+8]
	 */
	char *atxt="\x55\x48\x89\xE5\x48\x83\xE4\xF0\x56\x57\x41\x52\x41\x53\x48\x8D\x7D\x10";
	vec_pusharr(blob,atxt,0x12);
	//MOV RAX,fptr
	atxt="\x48\xb8";
	vec_pusharr(blob,atxt,0x2);
	for(i=0;i!=8;i++)
		vec_push(blob,(((int64_t)fptr)>>(i*8))&0xff);
	/*
	CALL RAX
	POP R11
	POP R10
	POP RDI
	POP RSI
	LEAVE
	*/
	atxt="\xFF\xD0\x41\x5B\x41\x5A\x5F\x5E\xC9";
	vec_pusharr(blob,atxt,0x9);
    #else
    /*
    PUSH RBP
    MOV RBP,RSP
    AND RSP,-0x10
    PUSH R10
    PUSH R11
    SUB RSP,0x20 //Manditory 4 stack arguments must be "pushed"
    LEA RCX,[RBP+8+8]
    PUSH R9
    PUSH R8
    PUSH RDX
    PUSH RCX
     */
    char *atxt="\x55\x48\x89\xE5\x48\x83\xE4\xF0\x41\x52\x41\x53\x48\x83\xEC\x20\x48\x8D\x4D\x10\x41\x51\x41\x50\x52\x51";
    vec_pusharr(blob,atxt,0x1a);
    //MOV RAX,fptr
	atxt="\x48\xb8";
	vec_pusharr(blob,atxt,0x2);
	for(i=0;i!=8;i++)
		vec_push(blob,(((int64_t)fptr)>>(i*8))&0xff);
    /*
    CALL RAX
    ADD RSP,0x40
    POP R11
    POP R10
    LEAVE
    */
    atxt="\xFF\xD0\x48\x83\xC4\x40\x41\x5B\x41\x5A\xC9";
    vec_pusharr(blob,atxt,0xb);
    #endif
	//RET1 ARITY*8
	atxt="\xc2";
	vec_pusharr(blob,atxt,0x1);
	arity*=8;
	vec_push(blob,arity&0xff);
    vec_push(blob,arity>>8);
    CSymbol sym;
    memset(&sym,0,sizeof(sym));
    sym.type=SYM_FUNC;
    sym.add_to_rt_blob=1;
    sym.value_ptr=(void*)blob_off;
    sym.is_importable=1;
    map_set(&Loader.symbols, name, sym);
}
int64_t STK_ForeachFunc(int64_t *stk) {
    ForeachFunc((void*)stk[0]);
    return 0;
}
int64_t STK_TOSPrint(int64_t *stk) {
    TOSPrint((char*)stk[0],stk[1],stk+2);
    return 0;
}
int64_t STK_IsDir(int64_t *stk) {
    return IsDir((char*)stk[0]);
}
int64_t STK_NewDrawWindow(int64_t *stk) {
    return (int64_t)NewDrawWindow();
}
int64_t STK_DrawWindowUpdate(int64_t *stk) {
    DrawWindowUpdate((void*)stk[0],stk[1]);
    return 0;
}
int64_t STK_DrawWindowDel(int64_t *stk) {
    DrawWindowDel();
    return 0;
}
int64_t STK__GetTicksHP() {
	#ifndef TARGET_WIN32
	struct timespec ts;
    int64_t theTick = 0U;
    clock_gettime(CLOCK_REALTIME,&ts);
    theTick  = ts.tv_nsec / 1000;
    theTick += ts.tv_sec*1000000U;
    return theTick;
    #else
    static int64_t freq;
    int64_t cur;
    if(!freq) {
		QueryPerformanceFrequency(&freq);
		freq/=1000000U;
	}
	QueryPerformanceCounter(&cur);
	return cur/freq;
    #endif
}
int64_t STK___GetTicks() {
	#ifndef TARGET_WIN32
	//https://stackoverflow.com/questions/2958291/equivalent-to-gettickcount-on-linux
    struct timespec ts;
    int64_t theTick = 0U;
    clock_gettime( CLOCK_REALTIME, &ts );
    theTick  = ts.tv_nsec / 1000000;
    theTick += ts.tv_sec * 1000;
    return theTick;
    #else
    static int64_t freq;
    int64_t cur;
    if(!freq) {
		QueryPerformanceFrequency(&freq);
		freq/=1000;
	}
	QueryPerformanceCounter(&cur);
    return cur/freq;
    #endif
}
int64_t STK_SetKBCallback(int64_t *stk) {
    SetKBCallback((void*)stk[0],(void*)stk[1]);
    return 0;
}
int64_t STK_SetMSCallback(int64_t *stk) {
    SetMSCallback((void*)stk[0]);
    return 0;
}
int64_t STK_AwakeFromSleeping(int64_t *stk) {
	multicAwaken(stk[0]);
	return 0;
}
int64_t STK_SleepHP(int64_t *stk) {
	multicSleepHP(stk[0]);
	return 0;
}
int64_t STK_Sleep(int64_t *stk) {
	multicSleep(stk[0]);
	return 0;
}
int64_t STK_GetFs(int64_t *stk) {
    return (int64_t)GetFs();
}
int64_t STK_SetFs(int64_t *stk) {
    SetFs((void*)stk[0]);
    return 0;
}
int64_t STK_SndFreq(int64_t *stk) {
    SndFreq(stk[0]);
    return 0;
}
int64_t STK_SetClipboardText(int64_t *stk) {
    //SDL_SetClipboardText(stk[0]);
    SetClipboard((char*)stk[0]);
    return 0; 
}
int64_t STK___GetStr(int64_t *stk) {
	#ifndef TARGET_WIN32
	char *s=linenoise((char*)stk[0]);
	if(!s) return (int64_t)s;
	linenoiseHistoryAdd(s);
	char *r=HolyStrDup(s);
	free(s);
	return (int64_t)r;
	#else
	char *s=readline((char*)stk[0]),*r;
	if(!s) return (int64_t)s;
	r=HolyStrDup(s);
	add_history(r);
	rl_free(s);
	return (int64_t)r;
	#endif
}
int64_t STK_GetClipboardText(int64_t *stk) {
    char *r=ClipboardText();
    char *r2=HolyStrDup(r);
    TD_FREE(r);
    return (int64_t)r2;
}
int64_t STK_FSize(int64_t *stk) {
	return VFsFSize((char*)stk[0]);
}
int64_t STK_FUnixTime(int64_t *stk) {
	return VFsUnixTime((char*)stk[0]);
}
int64_t STK_FTrunc(int64_t *stk) {
	char *fn=__VFsFileNameAbs((char*)stk[0]);
	if(fn) {
		#ifndef TARGET_WIN32
		truncate(fn,stk[1]);
		#else
		HANDLE fh=CreateFileA(fn,GENERIC_WRITE,FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,NULL);
		SetFilePointer(fh,(char*)stk[1],0,FILE_BEGIN);
		SetEndOfFile(fh);
		CloseHandle(fh);
		#endif
		TD_FREE(fn);
	}
	return 0;
}
int64_t STK___FExists(int64_t *stk) {
	return VFsFileExists((char*)stk[0]);
}
#ifndef TARGET_WIN32
#include <time.h>
int64_t STK_Now(int64_t *stk) {
	(void*)stk;
	int64_t t;
	t=time(NULL);
	return t;
}
#else
static int64_t FILETIME2Unix(FILETIME *t) {
	//https://www.frenk.com/2009/12/convert-filetime-to-unix-timestamp/	
	int64_t time=t->dwLowDateTime|((int64_t)t->dwHighDateTime<<32),adj;
	adj=10000*(int64_t)11644473600000ll;
	time-=adj;
	return time/10000000ll;
}
int64_t STK_Now(int64_t *stk) {
	int64_t r;
	FILETIME ft;
	GetSystemTimeAsFileTime(&ft);
	return FILETIME2Unix(&ft);
}
#endif
int64_t mp_cnt(int64_t *stk) {
	(void*)stk;
	#ifndef TARGET_WIN32
	return sysconf(_SC_NPROCESSORS_ONLN);
	#else
	SYSTEM_INFO info;
	GetSystemInfo(&info);
	return info.dwNumberOfProcessors;
	#endif
} 
void SpawnCore(int64_t *stk) {
	CreateCore(stk[0],(void*)stk[1]);
}
int64_t STK_NewVirtualChunk(int64_t *stk) {
	return (int64_t)NewVirtualChunk(stk[0],stk[1]);
}
int64_t STK_FreeVirtualChunk(int64_t *stk) {
	FreeVirtualChunk((void*)stk[0],stk[1]);
	return 0;
}
int64_t STK_VFsSetPwd(int64_t *stk) {
    VFsSetPwd((char*)stk[0]);
    return 1;
}
int64_t STK_VFsExists(int64_t *stk) {
	return VFsFileExists((char*)stk[0]);
}
int64_t STK_VFsIsDir(int64_t *stk) {
	return VFsIsDir((char*)stk[0]);
}
int64_t STK_VFsFileSize(int64_t *stk) {
	return VFsFSize((char*)stk[0]);
}
int64_t STK_VFsFRead(int64_t *stk) {
	return VFsFileRead((char*)stk[0],(int64_t*)stk[1]);
}
int64_t STK_VFsFWrite(int64_t *stk) {
	return VFsFileWrite((char*)stk[0],(char*)stk[1],stk[2]);
}
int64_t STK_VFsDirMk(int64_t *stk) {
	return VFsCd((char*)stk[0],VFS_CDF_MAKE);
}
int64_t STK_VFsDir(int64_t *stk) {
	return (int64_t)VFsDir((char*)stk[0]);
}
int64_t STK_VFsDel(int64_t *stk) {
	return VFsDel((char*)stk[0]);
}
int64_t STK_VFsFOpenW(int64_t *stk) {
	return (int64_t)VFsFOpen((char*)stk[0],"wb+");
}
int64_t STK_VFsFOpenR(int64_t *stk) {
	return (int64_t)VFsFOpen((char*)stk[0],"rb");
}
int64_t STK_VFsFClose(int64_t *stk) {
	fclose((FILE*)stk[0]);
	return 0;
}
int64_t STK_VFsFBlkRead(int64_t *stk) {
	return stk[2]==fread((void*)stk[0],stk[1],stk[2],(FILE*)stk[3]);
}
int64_t STK_VFsFBlkWrite(int64_t *stk) {
	return stk[2]==fwrite((void*)stk[0],stk[1],stk[2],(FILE*)stk[3]);
}
int64_t STK_VFsFSeek(int64_t *stk) {
	fseek((FILE*)stk[1],stk[0],SEEK_SET);
	return 0;
}
int64_t STK_VFsDrv(int64_t *stk) {
	VFsSetDrv(stk[0]);
	return 0;
}
int64_t STK__3DaysSetResolution(int64_t *stk) {
	return (int64_t)_3DaysSetResolution(stk[0],stk[1]);
}
int64_t STK__3DaysScaleScrn(int64_t *stk) {
	_3DaysScaleScrn();
	return 0;
}
int64_t STK_SetVolume(int64_t *stk) {
	SetVolume(*(double*)stk);
	return 0;
}
int64_t STK___3DaysSwapRGB(int64_t *stk) {
	return __3DaysSwapRGB();
}
int64_t STK___3DaysEnableScaling(int64_t *stk){
	__3DaysEnableScaling(stk[0]);
	return 0;
}
int64_t STK_GetVolume(int64_t *stk) {
	union {
		double flt;
		int64_t i;
	} un;
	un.flt=GetVolume();
	return un.i;
}
int64_t STK__3DaysSetVGAColor(int64_t *stk) {
	_3DaysSetVGAColor(stk[0],stk[1]);
}
int64_t STK__3DaysVGAMode(int64_t *stk) {
	return __3DaysVGAMode();
} 

int64_t STK__GrPalColorSet(int64_t *stk) {
	GrPaletteColorSet(stk[0],stk[1]);
}

int64_t STK_SetGs(int64_t *stk) {
	SetGs(stk[0]);
}
void TOS_RegisterFuncPtrs() {
	map_iter_t miter;
	const char *key;
	CSymbol *s;
	vec_char_t ffi_blob;
	vec_init(&ffi_blob);
	STK_RegisterFunctionPtr(&ffi_blob,"__3DaysVGAMode",STK__3DaysVGAMode,0);
	STK_RegisterFunctionPtr(&ffi_blob,"__3DaysSetVGAColor",STK__3DaysSetVGAColor,2);
	STK_RegisterFunctionPtr(&ffi_blob,"_GrPaletteColorSet",STK__GrPalColorSet,2);
	STK_RegisterFunctionPtr(&ffi_blob,"_BootDrv",BootDrv,0);
	STK_RegisterFunctionPtr(&ffi_blob,"SetGs",STK_SetGs,1);
	STK_RegisterFunctionPtr(&ffi_blob,"UnixNow",STK_Now,0);
	STK_RegisterFunctionPtr(&ffi_blob,"InterruptCore",STK_InteruptCore,1);
	STK_RegisterFunctionPtr(&ffi_blob,"NewVirtualChunk",STK_NewVirtualChunk,2);
	STK_RegisterFunctionPtr(&ffi_blob,"FreeVirtualChunk",STK_FreeVirtualChunk,2);
	STK_RegisterFunctionPtr(&ffi_blob,"__CmdLineBootText",CmdLineBootText,0);
	STK_RegisterFunctionPtr(&ffi_blob,"ExitTOS",__Shutdown,0);
	STK_RegisterFunctionPtr(&ffi_blob,"__GetStr",STK___GetStr,1);
	STK_RegisterFunctionPtr(&ffi_blob,"__IsCmdLine",IsCmdLine,0);
	STK_RegisterFunctionPtr(&ffi_blob,"__FExists",STK___FExists,1);
	STK_RegisterFunctionPtr(&ffi_blob,"mp_cnt",mp_cnt,0);
	STK_RegisterFunctionPtr(&ffi_blob,"GetGs",GetGs,0);
	STK_RegisterFunctionPtr(&ffi_blob,"__SpawnCore",SpawnCore,2);
	STK_RegisterFunctionPtr(&ffi_blob,"__CoreNum",CoreNum,0);
	STK_RegisterFunctionPtr(&ffi_blob,"FUnixTime",STK_FUnixTime,1);
	STK_RegisterFunctionPtr(&ffi_blob,"VFsFTrunc",STK_FTrunc,2);
	STK_RegisterFunctionPtr(&ffi_blob,"FSize",STK_FSize,1);
    STK_RegisterFunctionPtr(&ffi_blob,"SetClipboardText",STK_SetClipboardText,1);
    STK_RegisterFunctionPtr(&ffi_blob,"GetClipboardText",STK_GetClipboardText,0);
    STK_RegisterFunctionPtr(&ffi_blob,"SndFreq",STK_SndFreq,1);
    STK_RegisterFunctionPtr(&ffi_blob,"__Sleep",&STK_Sleep,1);
    STK_RegisterFunctionPtr(&ffi_blob,"__SleepHP",&STK_SleepHP,1);
    STK_RegisterFunctionPtr(&ffi_blob,"__AwakeCore",&STK_AwakeFromSleeping,1);
    STK_RegisterFunctionPtr(&ffi_blob,"GetFs",STK_GetFs,0);
    STK_RegisterFunctionPtr(&ffi_blob,"SetFs",STK_SetFs,1);
    STK_RegisterFunctionPtr(&ffi_blob,"SetKBCallback",STK_SetKBCallback,2);
    STK_RegisterFunctionPtr(&ffi_blob,"SetMSCallback",STK_SetMSCallback,1);
    STK_RegisterFunctionPtr(&ffi_blob,"__GetTicks",STK___GetTicks,0);
    STK_RegisterFunctionPtr(&ffi_blob,"__BootstrapForeachSymbol",STK_ForeachFunc,1);
    STK_RegisterFunctionPtr(&ffi_blob,"IsDir",STK_IsDir,1);
    STK_RegisterFunctionPtr(&ffi_blob,"DrawWindowDel",STK_DrawWindowDel,1);
    STK_RegisterFunctionPtr(&ffi_blob,"DrawWindowUpdate",STK_DrawWindowUpdate,2);
    STK_RegisterFunctionPtr(&ffi_blob,"DrawWindowNew",STK_NewDrawWindow,0);
    STK_RegisterFunctionPtr(&ffi_blob,"UnblockSignals",UnblockSignals,0);
    //SPECIAL
    STK_RegisterFunctionPtr(&ffi_blob,"TOSPrint",STK_TOSPrint,0);
    #ifdef USE_NETWORKING
    STK_RegisterFunctionPtr(&ffi_blob,"DyadInit",&STK_DyadInit,0);
    STK_RegisterFunctionPtr(&ffi_blob,"DyadUpdate",&STK_DyadUpdate,0);
    STK_RegisterFunctionPtr(&ffi_blob,"DyadShutdown",&STK_DyadShutdown,0);
    STK_RegisterFunctionPtr(&ffi_blob,"DyadNewStream",&STK_DyadNewStream,0);
    STK_RegisterFunctionPtr(&ffi_blob,"DyadListen",&STK_DyadListen,2);
    STK_RegisterFunctionPtr(&ffi_blob,"DyadConnect",&STK_DyadConnect,3);
    STK_RegisterFunctionPtr(&ffi_blob,"DyadWrite",&STK_DyadWrite,3);
    STK_RegisterFunctionPtr(&ffi_blob,"DyadEnd",&STK_DyadEnd,1);
    STK_RegisterFunctionPtr(&ffi_blob,"DyadClose",&STK_DyadClose,1);
    STK_RegisterFunctionPtr(&ffi_blob,"DyadGetAddress",STK_DyadGetAddress,1);
    STK_RegisterFunctionPtr(&ffi_blob,"DyadSetReadCallback",STK_DyadSetReadCallback,3);
    STK_RegisterFunctionPtr(&ffi_blob,"DyadSetOnListenCallback",STK_DyadSetOnListenCallback,3);
    STK_RegisterFunctionPtr(&ffi_blob,"DyadSetOnConnectCallback",STK_DyadSetConnectCallback,3);
    STK_RegisterFunctionPtr(&ffi_blob,"DyadSetOnCloseCallback",STK_DyadSetCloseCallback,3);
    STK_RegisterFunctionPtr(&ffi_blob,"DyadSetOnReadyCallback",STK_DyadSetReadyCallback,3);
    STK_RegisterFunctionPtr(&ffi_blob,"DyadSetOnTimeoutCallback",STK_DyadSetTimeoutCallback,3);
    STK_RegisterFunctionPtr(&ffi_blob,"DyadSetOnTickCallback",STK_DyadSetTimeoutCallback,3);
    STK_RegisterFunctionPtr(&ffi_blob,"DyadSetOnErrorCallback",STK_DyadSetErrorCallback,3);
    STK_RegisterFunctionPtr(&ffi_blob,"DyadSetOnDestroyCallback",STK_DyadSetDestroyCallback,3);
    STK_RegisterFunctionPtr(&ffi_blob,"DyadSetTimeout",STK_DyadSetTimeout,2);
    STK_RegisterFunctionPtr(&ffi_blob,"DyadSetNoDelay",STK_DyadSetNoDelay,2);
    #endif
    STK_RegisterFunctionPtr(&ffi_blob,"VFsSetPwd",STK_VFsSetPwd,1);
    STK_RegisterFunctionPtr(&ffi_blob,"VFsExists",STK_VFsExists,1);
    STK_RegisterFunctionPtr(&ffi_blob,"VFsIsDir",STK_VFsIsDir,1);
    STK_RegisterFunctionPtr(&ffi_blob,"VFsFSize",STK_VFsFileSize,1);
    STK_RegisterFunctionPtr(&ffi_blob,"VFsFRead",STK_VFsFRead,2);
    STK_RegisterFunctionPtr(&ffi_blob,"VFsFWrite",STK_VFsFWrite,3);
    STK_RegisterFunctionPtr(&ffi_blob,"VFsDel",STK_VFsDel,1);
    STK_RegisterFunctionPtr(&ffi_blob,"VFsDir",STK_VFsDir,0);
    STK_RegisterFunctionPtr(&ffi_blob,"VFsDirMk",STK_VFsDirMk,1);
    STK_RegisterFunctionPtr(&ffi_blob,"GetCipherPasswd",GetCipherPasswd,0);
    STK_RegisterFunctionPtr(&ffi_blob,"__IsValidPtr",IsValidPtr,1);
    STK_RegisterFunctionPtr(&ffi_blob,"VFsFBlkRead",STK_VFsFBlkRead,4);
    STK_RegisterFunctionPtr(&ffi_blob,"VFsFBlkWrite",STK_VFsFBlkWrite,4);
    STK_RegisterFunctionPtr(&ffi_blob,"VFsFOpenW",STK_VFsFOpenW,1);
    STK_RegisterFunctionPtr(&ffi_blob,"VFsFOpenR",STK_VFsFOpenR,1);
    STK_RegisterFunctionPtr(&ffi_blob,"VFsFClose",STK_VFsFClose,1);
    STK_RegisterFunctionPtr(&ffi_blob,"VFsFSeek",STK_VFsFSeek,2);
    STK_RegisterFunctionPtr(&ffi_blob,"VFsSetDrv",STK_VFsDrv,1);
    STK_RegisterFunctionPtr(&ffi_blob,"_3DaysSetResolution",STK__3DaysSetResolution,2);
    STK_RegisterFunctionPtr(&ffi_blob,"_3DaysScaleScrn",STK__3DaysScaleScrn,0);
    STK_RegisterFunctionPtr(&ffi_blob,"GetVolume",STK_GetVolume,0);
    STK_RegisterFunctionPtr(&ffi_blob,"SetVolume",STK_SetVolume,1);
    STK_RegisterFunctionPtr(&ffi_blob,"__3DaysSwapRGB",STK___3DaysSwapRGB,0);
    STK_RegisterFunctionPtr(&ffi_blob,"__GetTicksHP",STK__GetTicksHP,0);
    STK_RegisterFunctionPtr(&ffi_blob,"__3DaysEnableScaling",STK___3DaysEnableScaling,1);
    char *blob=NewVirtualChunk(ffi_blob.length,1);
    memcpy(blob,ffi_blob.data,ffi_blob.length);
    vec_deinit(&ffi_blob);
    miter=map_iter(&Loader.symbols);
    while(key=map_next(&Loader.symbols,&miter)) {
		if((s=map_get(&Loader.symbols,key))->add_to_rt_blob) {
			s->value_ptr=blob+(int64_t)s->value_ptr;
			s->add_to_rt_blob=0;
		}
	}
}
