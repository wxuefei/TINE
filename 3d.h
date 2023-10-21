#pragma once
#ifdef __cplusplus
extern "C" {
#endif
#include "alloc.h"
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <signal.h>
#include "ext/map/src/map.h"
#include "ext/vec/src/vec.h"
#include "ext/ln/linenoise.h"
#define AOT_NO_IMPORT_SYMS (1<<0)
#define AOT_MALLOCED_SYM (1<<1)
extern char CompilerPath[1024];
typedef struct CSymbol {
    enum {
        SYM_VAR,
        SYM_FUNC,
        SYM_LABEL,
    } type;
    int8_t is_importable;
    int8_t add_to_rt_blob;
    int64_t size;
    void *value_ptr;
    char *strings;
} CSymbol;
typedef map_t(struct CSymbol) map_CSymbol_t;
typedef struct {
    map_CSymbol_t symbols;
} CLoader;
extern CLoader Loader;
char *MStrPrint(const char *fmt,int64_t argc,int64_t *argv);
void TOSPrint(const char *fmt,int64_t argc,int64_t *argv);
void RegisterFuncPtrs();
void InitRL();
void __AddTimer(int64_t interval,void (*tos_fptr)());
int64_t ScanKey(int64_t *ch,int64_t *sc);
int64_t FFI_CALL_TOS_0(void *fptr);
int64_t FFI_CALL_TOS_1(void *fptr,int64_t);
int64_t FFI_CALL_TOS_2(void *fptr,int64_t, int64_t);
int64_t FFI_CALL_TOS_3(void *fptr,int64_t, int64_t,int64_t);
int64_t FFI_CALL_TOS_4(void *fptr,int64_t, int64_t,int64_t,int64_t);
int64_t FFI_CALL_TOS_0_ZERO_BP(void *fptr);
void *Load(char *fn,int64_t ld_flags);
#define HTT_INVALID		0
#define HTT_EXPORT_SYS_SYM	0x00001 //CHashExport
#define HTT_IMPORT_SYS_SYM	0x00002 //CHashImport
#define HTT_DEFINE_STR		0x00004 //CHashDefineStr
#define HTT_GLBL_VAR		0x00008 //CHashGlblVar
#define HTT_CLASS		0x00010 //CHashClass
#define HTT_INTERNAL_TYPE	0x00020 //CHashClass
#define HTT_FUN			0x00040 //CHashFun
#define HTT_WORD		0x00080 //CHashAC only in AutoComplete table
#define HTT_DICT_WORD		0x00100 //CHashGeneric only in AutoComplete tbl
#define HTT_KEYWORD		0x00200 //CHashGeneric \dLK,"KEYWORD",A="FF:::/Compiler/OpCodes.DD,KEYWORD"\d
#define HTT_ASM_KEYWORD		0x00400 //CHashGeneric \dLK,"ASM_KEYWORD",A="FF:::/Compiler/OpCodes.DD,ASM_KEYWORD"\d
#define HTT_OPCODE		0x00800 //CHashOpcode
#define HTT_REG			0x01000 //CHashReg
#define HTT_FILE		0x02000 //CHashGeneric
#define HTT_MODULE		0x04000 //CHashGeneric
#define HTT_HELP_FILE		0x08000 //CHashSrcSym
#define HTT_FRAME_PTR		0x10000 //CHashGeneric
#define HTG_TYPE_MASK		0x1FFFF
typedef struct {
    int64_t type;
    union {
        void *val;
        struct {
            int64_t mod_header_entry;
            int64_t mod_base;
        };
    };
} CHash;
typedef vec_t(CHash) vec_CHash_t;
typedef map_t(vec_CHash_t) map_vec_CHash_t;
extern map_vec_CHash_t TOSLoader;
struct CDrawWindow;
void DrawWindowDel();
void DrawWindowUpdate(struct CDrawWindow *win,int8_t *colors,int64_t internal_width,int64_t h);
struct CDrawWindow *NewDrawWindow();
void *GetFs();
void SetFs(void *f);
void __WaitForSpawn(void *sp);
struct CThread;
struct CThread *__Spawn(void *fs,void *fp,void *data,char *name);
void __FreeThread(struct CThread *t);
void __AwakeThread(struct CThread *t);
void __Suspend(struct CThread *t);
void __Yield();
void __Exit();
void __AwaitThread(struct CThread *t);
void __KillThread(struct CThread *t);
void SetMSCallback(void* fptr);
void InitSound();
void SndFreq(int64_t f);
void FualtCB();
int64_t VFsFileRead(char *name,int64_t *len);
int64_t VFsFileWrite(char *name,char *data,int64_t len);
char VFsChDrv(char to);
char *VFsFileNameAbs(char *name);   
#define VFS_CDF_MAKE (1)
//Will not fail if not exists
#define VFS_CDF_FILENAME_ABS (1<<1)
int VFsCd(char *to,int flags);
extern __thread char* cur_dir;
extern __thread char cur_drv;
char *VFsDirCur();
char *__VFsFileNameAbs(char *name);
void VFsThrdInit();
void __Sleep(int64_t t);
int64_t VFsDel(char *p);
char *HostHomeDir();
int CreateTemplateBootDrv(char *to,char *t,int overwrite);
//Not VFs
void* FileRead(char *fn,int64_t *sz);
int64_t FileWrite(char *fn,void *data,int64_t sz);
//See swapctxWIN.yasm
typedef struct {
    int64_t rax;
    int64_t rbx;
    int64_t rcx;
    int64_t rdx;
    int64_t rsp;
    int64_t rbp;
    int64_t rsi;
    int64_t rdi;
    int64_t r8;
    int64_t r9;
    int64_t r10;
    int64_t r11;
    int64_t r12;
    int64_t r13;
    int64_t r14;
    int64_t r15;
    int64_t rip;
    double xmm6;
    double xmm7;
    double xmm8;
    double xmm9;
    double xmm10;
    double xmm11;
    double xmm12;
    double xmm13;
    double xmm14;
    double xmm15;
    void *un1,*un2,*un3;
} ctx_t;
void GetContext(ctx_t *);
void SetContext(ctx_t *);
void __SetThreadPtr(struct CThread *t,void *ptr);
void InputLoop(void *ul);
void __SleepUntilChange(int64_t *ptr,int64_t mask);
int64_t VFsUnixTime(char *name);
int64_t VFsFSize(char *name) ;
void __Shutdown();
void BoundsCheckTests();
struct CMemBlk;
int64_t InBounds(void *ptr,int64_t sz,void **target);
int InitThreadsForCore();
int CoreNum();
void *GetGs();
void SpawnCore();
int VFsFileExists(char *path);
void __SleepUntilValue(int64_t *ptr,int64_t,int64_t);
void VFsGlobalInit();
int VFsMountDrive(char let,char *path);
int64_t IsCmdLine();
char VFsChDrv(char to);
char *CmdLineBootText();
char *ClipboardText();
void SetClipboard(char *text);
void PreInitCores();
struct CPair;
__attribute__((force_align_arg_pointer)) int64_t __SpawnFFI(struct CPair *p);
#ifdef TARGET_WIN32
int _main(int argc,char **argv);
#endif
void CreateCore(int core,void *fp);
void *NewVirtualChunk(int64_t sz,int64_t low32);
void FreeVirtualChunk(void *ptr,size_t s);
void HolyFree(void *ptr);
void *HolyMAlloc(int64_t sz);
char *HolyStrDup(char *str);
void InteruptCore(int core);
void LaunchCore0(void *fp);
void WaitForCore0();
void __ShutdownCore(int core);
int64_t mp_cnt(int64_t *stk);
void __ShutdownCores();
void SetupDebugger();
void GrPalleteSet(int c,int64_t colors);
char *GrPalleteGet(int64_t c);
extern char *cipher_passwd;
char **VFsDir(char *fn);
FILE *VFsFOpen(char *path,char*);
void VFsSetDrv(char d);
void *_3DaysSetResolution(int64_t w,int64_t h);
extern int64_t _shutdown;
extern void SetVolume(double v);
extern double GetVolume();
void multicAwaken(int64_t core);
void multicSleep(int64_t ms);
void UnblockSignals();
void SetKBCallback(void *fptr,void *data);
void VFsSetPwd(char *pwd);
int VFsIsDir(char *path);
void _3DaysScaleScrn();
int64_t __3DaysSwapRGB();
void __3DaysEnableScaling(int64_t s);
void __3DaysSetSoftwareRender(int64_t s);
void _3DaysSetVGAColor(int64_t i,uint32_t color);
int64_t __3DaysVGAMode();
#ifdef __cplusplus
};
#endif
