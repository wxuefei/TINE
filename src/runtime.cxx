#ifdef _WIN32
  #include <winsock2.h>
  #include <windows.h>
  #include <winbase.h>
  #include <memoryapi.h>
#else
  #include <signal.h>
  #include <sys/mman.h>
#endif

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <string_view>

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <dyad.h>
#include <linenoise-ng/linenoise.h>

#include <tos_ffi.h>

#include "TOSPrint.hxx"
#include "alloc.hxx"
#include "main.hxx"
#include "mem.hxx"
#include "multic.hxx"
#include "runtime.hxx"
#include "sdl_window.hxx"
#include "sound.h"
#include "tos_aot.hxx"
#include "vfs.hxx"

void HolyFree(void *ptr) {
  static void *fptr = nullptr;
  if (!fptr)
    fptr = TOSLoader["_FREE"].val;
  FFI_CALL_TOS_1(fptr, (uintptr_t)ptr);
}

void *HolyMAlloc(size_t sz) {
  static void *fptr = nullptr;
  if (!fptr)
    fptr = TOSLoader["_MALLOC"].val;
  return (void *)FFI_CALL_TOS_2(fptr, sz, 0 /*NULL*/);
}

void *HolyCAlloc(size_t sz) {
  auto ret = HolyAlloc<uint8_t>(sz);
  std::fill(ret, ret + sz, 0);
  return ret;
}

char *HolyStrDup(char const *str) {
  return strcpy(HolyAlloc<char>(strlen(str) + 1), str);
}

size_t mp_cnt(void *) {
  return proc_cnt;
}

[[noreturn]] void HolyThrow(std::string_view sv = {}) {
  union {
    char     s[8]{}; // zero-init
    uint64_t i;
  } u;
  static void *fp;
  if (!fp)
    fp = TOSLoader["throw"].val;
  std::copy(sv.begin(), sv.begin() + std::min<size_t>(sv.size(), 8), u.s);
  FFI_CALL_TOS_1(fp, u.i);
  __builtin_unreachable();
}

namespace { // ffi shit goes here

namespace fs     = std::filesystem;
namespace chrono = std::chrono;

using chrono::system_clock;

namespace tine {
uint64_t constexpr Hash(std::string_view sv) { // fnv64-1a
  uint64_t h = 0xCBF29CE484222325;
  for (char c : sv) {
    h *= 0x100000001B3;
    h ^= c;
  }
  return h;
}
} // namespace tine

void STK_DyadInit(void *) {
  static bool init = false;
  if (init)
    return;
  init = true;
  dyad_init();
  dyad_setUpdateTimeout(0.);
}

void STK_DyadUpdate(void *) {
  dyad_update();
}

void STK_DyadShutdown(void *) {
  dyad_shutdown();
}

void *STK_DyadNewStream(void *) {
  return dyad_newStream();
}

int64_t STK_DyadListen(intptr_t *stk) {
  return dyad_listen((dyad_Stream *)stk[0], (int)stk[1]);
}

int64_t STK_DyadConnect(intptr_t *stk) {
  return dyad_connect((dyad_Stream *)stk[0], (char *)stk[1], (int)stk[2]);
}

void STK_DyadWrite(intptr_t *stk) {
  dyad_write((dyad_Stream *)stk[0], (void *)stk[1], (int)stk[2]);
}

void STK_DyadEnd(dyad_Stream **stk) {
  dyad_end(stk[0]);
}

void STK_DyadClose(dyad_Stream **stk) {
  dyad_close(stk[0]);
}

char *STK_DyadGetAddress(dyad_Stream **stk) {
  return HolyStrDup(dyad_getAddress(stk[0]));
}

int64_t STK__DyadGetCallbackMode(char **stk) {
  // i thought of using streamprint but then
  // the variadic calling thing gets too complicated
  switch (tine::Hash(stk[0])) {
    // это говнокод для принудительной оценки порядка
#define D(x)     D_(DYAD_EVENT_, x)
#define D_(x, y) x##y
#define S(x)     S_(x)
#define S_(x...) #x
#define C(x)                \
  case tine::Hash(S(D(x))): \
    return D(x)
    C(LINE);
    C(DATA);
    C(CLOSE);
    C(CONNECT);
    C(DESTROY);
    C(ERROR);
    C(READY);
    C(TICK);
    C(TIMEOUT);
    C(ACCEPT);
#undef D
#undef D_
#undef S
#undef S_
#undef C
  default:
    HolyThrow("InvMode"); // invalid mode
  }
}

void STK_DyadSetReadCallback(void **stk) {
  dyad_addListener((dyad_Stream *)stk[0], (intptr_t)stk[1],
                   [](dyad_Event *e) {
                     FFI_CALL_TOS_4(e->udata, (uintptr_t)e->stream,
                                    (uintptr_t)e->data, e->size,
                                    (uintptr_t)e->udata2);
                   },
                   stk[2], stk[3]);
}

void STK_DyadSetCloseCallback(void **stk) {
  dyad_addListener((dyad_Stream *)stk[0], (intptr_t)stk[1],
                   [](dyad_Event *e) {
                     FFI_CALL_TOS_2(e->udata, (uintptr_t)e->stream,
                                    (uintptr_t)e->udata2);
                   },
                   stk[2], stk[3]);
}

void STK_DyadSetListenCallback(void **stk) {
  dyad_addListener((dyad_Stream *)stk[0], (intptr_t)stk[1],
                   [](dyad_Event *e) {
                     FFI_CALL_TOS_2(e->udata, (uintptr_t)e->remote,
                                    (uintptr_t)e->udata2);
                   },
                   stk[2], stk[3]);
}

// read callbacks -> DYAD_EVENT_{LINE,DATA}
// close callbacks -> DYAD_EVENT_{DESTROY,ERROR,CLOSE}
// listen callbacks -> DYAD_EVENT_{TIMEOUT,CONNECT,READY,TICK,ACCEPT}

void STK_DyadSetTimeout(uintptr_t *stk) {
  dyad_setTimeout((dyad_Stream *)stk[0], ((double *)stk)[1]);
}

void STK_DyadSetNoDelay(intptr_t *stk) {
  dyad_setNoDelay((dyad_Stream *)stk[0], (int)stk[1]);
}

void STK_UnblockSignals(void *) {
#ifndef _WIN32
  sigset_t all;
  sigfillset(&all);
  sigprocmask(SIG_UNBLOCK, &all, nullptr);
#endif
}

void STK__GrPaletteColorSet(uint64_t *stk) {
  GrPaletteColorSet(stk[0], {stk[1]});
}

uint64_t STK___IsValidPtr(uintptr_t *stk) {
#ifdef _WIN32
  // Wine doesnt like the
  // IsBadReadPtr,so use a polyfill

  // wtf IsBadReadPtr gives me a segfault so i just have to use this
  // polyfill lmfao
  // #ifdef __WINE__
  MEMORY_BASIC_INFORMATION mbi{};
  if (VirtualQuery((void *)stk[0], &mbi, sizeof mbi)) {
    // https://archive.md/ehBq4
    DWORD const mask = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
                       PAGE_EXECUTE | PAGE_EXECUTE_READ |
                       PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    return !!(mbi.Protect & mask);
  }
  return false;
  /*#else
    return !IsBadReadPtr((void*)stk[0], 8);
  #endif*/

#else
  // round down to page boundary (equiv to stk[0] / page_size * page_size)
  //   0b100101010 (x)
  // & 0b111110000 <- ~(0b10000 - 1)
  // --------------
  //   0b100100000
  uintptr_t addr = stk[0] & ~(page_size - 1);
  // https://archive.md/Aj0S4
  return -1 != msync((void *)addr, page_size, MS_ASYNC);

#endif
}

void STK_InterruptCore(size_t *stk) {
  InterruptCore(stk[0]);
}

void STK___BootstrapForeachSymbol(void **stk) {
  for (auto &[name, sym] : TOSLoader)
    FFI_CALL_TOS_3(stk[0], (uintptr_t)name.c_str(), (uintptr_t)sym.val,
                   sym.type == HTT_EXPORT_SYS_SYM ? HTT_FUN : sym.type);
}

void STK_TOSPrint(intptr_t *stk) {
  TOSPrint((char *)stk[0], stk[1], stk + 2);
}

void STK_DrawWindowUpdate(uintptr_t *stk) {
  DrawWindowUpdate((uint8_t *)stk[0], stk[1]);
}

uint64_t STK___GetTicksHP(void *) {
#ifndef _WIN32
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  uint64_t tick = ts.tv_nsec / 1000u;
  tick += ts.tv_sec * 1000000u;
  return tick;
#else
  static uint64_t freq = 0;
  if (!freq) {
    QueryPerformanceFrequency((LARGE_INTEGER *)&freq);
    freq /= 1000000U;
  }
  uint64_t cur;
  QueryPerformanceCounter((LARGE_INTEGER *)&cur);
  return cur / freq;
#endif
}

uint64_t STK___GetTicks(void *) {
  return GetTicks();
}

void STK_SetKBCallback(void **stk) {
  SetKBCallback(stk[0], stk[1]);
}

void STK_SetMSCallback(void **stk) {
  SetMSCallback(stk[0]);
}

void STK___AwakeCore(size_t *stk) {
  AwakeFromSleeping(stk[0]);
}

void STK___SleepHP(uint64_t *stk) {
  SleepHP(stk[0]);
}

void STK___Sleep(uint64_t *stk) {
  SleepHP(stk[0] * 1000);
}

void STK_SetFs(void **stk) {
  SetFs(stk[0]);
}

void STK_SetGs(void **stk) {
  SetGs(stk[0]);
}

void STK_SndFreq(uint64_t *stk) {
  SndFreq(stk[0]);
}

void STK_SetClipboardText(char **stk) {
  SetClipboard(stk[0]);
}

char *STK___GetStr(char **stk) {
  char *s = linenoise(stk[0]), *r;
  if (s == nullptr)
    return nullptr;
  linenoiseHistoryAdd(s);
  r = HolyStrDup(s);
  free(s);
  return r;
}

char *STK_GetClipboardText(void *) {
  std::string clip{ClipboardText()};
  return HolyStrDup(clip.c_str());
}

uint64_t STK_FUnixTime(char **stk) {
  return VFsUnixTime(stk[0]);
}

void STK_VFsFTrunc(uintptr_t *stk) {
  fs::resize_file(VFsFileNameAbs((char *)stk[0]), stk[1]);
}

uint64_t STK___FExists(char **stk) {
  return VFsFileExists(stk[0]);
}

uint64_t STK_UnixNow(void *) {
  return system_clock::to_time_t(system_clock::now());
}

void STK___SpawnCore(uintptr_t *stk) {
  CreateCore(stk[0], (void *)stk[1]);
}

void *STK_NewVirtualChunk(size_t *stk) {
  return NewVirtualChunk(stk[0], stk[1]);
}

void STK_FreeVirtualChunk(uintptr_t *stk) {
  FreeVirtualChunk((void *)stk[0], stk[1]);
}

void STK_VFsSetPwd(char **stk) {
  VFsSetPwd(stk[0]);
}

uint64_t STK_VFsExists(char **stk) {
  return VFsFileExists(stk[0]);
}

uint64_t STK_VFsIsDir(char **stk) {
  return VFsIsDir(stk[0]);
}

int64_t STK_VFsFSize(char **stk) {
  return VFsFSize(stk[0]);
}

void *STK_VFsFRead(char **stk) {
  return VFsFileRead(stk[0], (uint64_t *)stk[1]);
}

uint64_t STK_VFsFWrite(char **stk) {
  return VFsFileWrite(stk[0], stk[1], (uintptr_t)stk[2]);
}

uint64_t STK_VFsDirMk(char **stk) {
  return VFsDirMk(stk[0], VFS_CDF_MAKE);
}

char **STK_VFsDir(void *) {
  return VFsDir();
}

uint64_t STK_VFsDel(char **stk) {
  return VFsDel(stk[0]);
}

FILE *VFsFOpen(char const *path, char const *m) {
  std::string p = VFsFileNameAbs(path);
  return fopen(p.c_str(), m);
}

FILE *STK_VFsFOpenW(char **stk) {
  return VFsFOpen(stk[0], "w+b");
}

FILE *STK_VFsFOpenR(char **stk) {
  return VFsFOpen(stk[0], "rb");
}

void STK_VFsFClose(FILE **stk) {
  fclose(stk[0]);
}

uint64_t STK_VFsFBlkRead(uintptr_t *stk) {
  fflush((FILE *)stk[3]);
  return stk[2] == fread((void *)stk[0], stk[1], stk[2], (FILE *)stk[3]);
}

uint64_t STK_VFsFBlkWrite(uintptr_t *stk) {
  bool r = stk[2] == fwrite((void *)stk[0], stk[1], stk[2], (FILE *)stk[3]);
  fflush((FILE *)stk[3]);
  return r;
}

void STK_VFsFSeek(uintptr_t *stk) {
  fseek((FILE *)stk[1], stk[0], SEEK_SET);
}

void STK_VFsSetDrv(uint8_t *stk) {
  VFsSetDrv(stk[0]);
}

uint64_t STK_VFsGetDrv(void *) {
  return (uint64_t)VFsGetDrv();
}

void STK_SetVolume(uint64_t *stk) {
  union {
    uint64_t i;
    double   flt;
  } un = {stk[0]};
  SetVolume(un.flt);
}

uint64_t STK_GetVolume(void *) {
  union {
    double   flt;
    uint64_t i;
  } un = {GetVolume()};
  return un.i;
}

void STK_ExitTINE(int *stk) {
  ShutdownTINE(stk[0]);
}

uint64_t STK___IsCmdLine(void *) {
  return (uint64_t)is_cmd_line;
}

// arity must be <= 0xffFF/sizeof U64
void RegisterFunctionPtr(std::string &blob, char const *name, uintptr_t fp,
                         uint16_t arity) {
  // Function entry point offset from the code blob
  uintptr_t off = blob.size();
  // https://defuse.ca/online-x86-assembler.htm
  // boring register pushing and stack alignment bullshit
  // disas then read the ABIs and Doc/GuideLines.DD if interested
  char const *inst = "\x55\x48\x89\xE5\x48"
                     "\x83\xE4\xF0\x56\x57"
                     "\x41\x52\x41\x53\x41"
                     "\x54\x41\x55\x41\x56"
                     "\x41\x57";
  blob.append(inst, 22);
#ifdef _WIN32
  // https://archive.md/4HDA0#selection-2085.880-2085.1196
  // rcx is the first arg i have to provide in win64 abi
  // last 4 register pushes are for register "home"s
  // that windows wants me to provide
  inst = "\x48\x8D\x4D\x10\x41"
         "\x51\x41\x50\x52\x51";
  blob.append(inst, 10);
#else // sysv
  // rdi is the first arg i have to provide in sysv
  blob.append("\x48\x8D\x7D\x10", 4);
#endif
  // movabs rax, <fp>
  blob.append("\x48\xb8", 2);
  union {
    uintptr_t p;
    char      data[8];
  } fu = {fp};
  blob.append(fu.data, 8);
  // call rax
  blob.append("\xFF\xD0", 2);
#ifdef _WIN32
  // can just add to rsp since
  // those 4 registers are volatile
  blob.append("\x48\x83\xC4\x20", 4);
#endif
  // pops stack. boring stuff
  inst = "\x41\x5F\x41\x5E\x41"
         "\x5D\x41\x5C\x41\x5B"
         "\x41\x5A\x5F\x5E";
  blob.append(inst, 14);
  // leave
  blob.push_back('\xC9');
  // clang-format off
  // ret <arity*8>; (8 == sizeof(uint64_t))
  // HolyC ABI is __stdcall, the callee cleans up its own stack
  // unless its variadic so we pop the stack with ret
  //
  // A bit about HolyC ABI: all args are 8 bytes(64 bits)
  // let there be function Foo(I64 i, ...);
  // Foo(2, 4, 5, 6)
  //   argv[2] 6 // RBP + 48
  //   argv[1] 5 // RBP + 40
  //   argv[0] 4 // RBP + 32 <-points- argv(internal var in function)
  //   argc 3(num of varargs) // RBP + 24 <-value- argc(internal var in function)
  //   i  2    // RBP + 16(this is where the stack starts)
  // clang-format on
  // ret
  blob.push_back('\xc2');
  arity *= 8; // sizeof(uint64_t)
  // imm16
  union {
    uint16_t ar;
    char     data[2];
  } au = {arity};
  blob.append(au.data, 2);
  CHash sym;
  sym.type        = HTT_FUN;
  sym.val         = (uint8_t *)off;
  TOSLoader[name] = sym;
}

} // namespace

void RegisterFuncPtrs() {
  std::string ffi_blob;
#define R(holy, secular, arity) \
  RegisterFunctionPtr(ffi_blob, holy, (uintptr_t)secular, arity)
#define S(name, arity) \
  RegisterFunctionPtr(ffi_blob, #name, (uintptr_t)STK_##name, arity)
  R("__CmdLineBootText", CmdLineBootText, 0);
  R("mp_cnt", mp_cnt, 0);
  R("__CoreNum", CoreNum, 0);
  R("GetFs", GetFs, 0);
  R("GetGs", GetGs, 0);
  S(__IsCmdLine, 0);
  S(__IsValidPtr, 1);
  S(__SpawnCore, 0);
  S(UnixNow, 0);
  S(InterruptCore, 1);
  S(NewVirtualChunk, 2);
  S(FreeVirtualChunk, 2);
  S(ExitTINE, 1);
  S(__GetStr, 1);
  S(__FExists, 1);
  S(FUnixTime, 1);
  S(SetClipboardText, 1);
  S(GetClipboardText, 0);
  S(SndFreq, 1);
  S(__Sleep, 1);
  S(__SleepHP, 1);
  S(__AwakeCore, 1);
  S(SetFs, 1);
  S(SetGs, 1);
  S(SetKBCallback, 2);
  S(SetMSCallback, 1);
  S(__GetTicks, 0);
  S(__BootstrapForeachSymbol, 1);
  S(DrawWindowUpdate, 2);
  S(UnblockSignals, 0);
  /*
   * In TempleOS variadics, functions follow __cdecl, whereas normally
   * they follow __stdcall which is why the arity argument is needed(RET1 x).
   * Thus we don't have to clean up the stack in variadics.
   */
  S(TOSPrint, 0);
  S(DyadInit, 0);
  S(DyadUpdate, 0);
  S(DyadShutdown, 0);
  S(DyadNewStream, 0);
  S(DyadListen, 2);
  S(DyadConnect, 3);
  S(DyadWrite, 3);
  S(DyadEnd, 1);
  S(DyadClose, 1);
  S(DyadGetAddress, 1);
  S(_DyadGetCallbackMode, 1);
  S(DyadSetReadCallback, 4);
  S(DyadSetCloseCallback, 4);
  S(DyadSetListenCallback, 4);
  S(DyadSetTimeout, 2);
  S(DyadSetNoDelay, 2);
  S(VFsFTrunc, 2);
  S(VFsSetPwd, 1);
  S(VFsExists, 1);
  S(VFsIsDir, 1);
  S(VFsFSize, 1);
  S(VFsFRead, 2);
  S(VFsFWrite, 3);
  S(VFsDel, 1);
  S(VFsDir, 0);
  S(VFsDirMk, 1);
  S(VFsFBlkRead, 4);
  S(VFsFBlkWrite, 4);
  S(VFsFOpenW, 1);
  S(VFsFOpenR, 1);
  S(VFsFClose, 1);
  S(VFsFSeek, 2);
  S(VFsSetDrv, 1);
  S(VFsGetDrv, 0);
  S(GetVolume, 0);
  S(SetVolume, 1);
  S(__GetTicksHP, 0);
  S(_GrPaletteColorSet, 2);
  auto blob = VirtAlloc<char>(ffi_blob.size());
  std::copy(ffi_blob.begin(), ffi_blob.end(), blob);
  for (auto &[name, sym] : TOSLoader)
    sym.val += (ptrdiff_t)blob;
}

// vim: set expandtab ts=2 sw=2 :
