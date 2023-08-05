#ifdef _WIN32
  #include <winsock2.h>
  #include <windows.h>
  #include <winbase.h>
  #include <memoryapi.h>
#else
  #include <signal.h>
  #include <sys/mman.h>
#endif

#include <chrono>
#include <initializer_list>
#include <string_view>

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
  FFI_CALL_TOS_1(fptr, (uptr)ptr);
}

void *HolyMAlloc(usize sz) {
  static void *fptr = nullptr;
  if (!fptr)
    fptr = TOSLoader["_MALLOC"].val;
  return (void *)FFI_CALL_TOS_2(fptr, sz, 0 /*NULL*/);
}

void *HolyCAlloc(usize sz) {
  auto ret = HolyAlloc<u8>(sz);
  std::fill(ret, ret + sz, 0);
  return ret;
}

char *HolyStrDup(char const *str) {
  return strcpy(HolyAlloc<char>(strlen(str) + 1), str);
}

usize mp_cnt(void *) {
  return proc_cnt;
}

[[noreturn]] void HolyThrow(std::string_view sv) {
  union {
    char s[8]{}; // zero-init
    u64  i;
  } u;
  static void *fp;
  if (!fp)
    fp = TOSLoader["throw"].val;
  memcpy(u.s, sv.data(), std::min<usize>(sv.size(), 8));
  FFI_CALL_TOS_1(fp, u.i);
  __builtin_unreachable();
}

namespace { // ffi shit goes here

namespace chrono = std::chrono;

using chrono::system_clock;

namespace tine {
u64 constexpr Hash(std::string_view sv) { // fnv64-1a
  u64 h = 0xCBF29CE484222325;
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

i64 STK_DyadListen(iptr *stk) {
  return dyad_listen((dyad_Stream *)stk[0], (int)stk[1]);
}

i64 STK_DyadConnect(iptr *stk) {
  return dyad_connect((dyad_Stream *)stk[0], (char *)stk[1], (int)stk[2]);
}

void STK_DyadWrite(iptr *stk) {
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

i64 STK__DyadGetCallbackMode(char **stk) {
  // i thought of using streamprint but then
  // the variadic calling thing gets too complicated
  switch (tine::Hash(stk[0])) {
#define C(x)           \
  case tine::Hash(#x): \
    return x
    C(DYAD_EVENT_LINE);
    C(DYAD_EVENT_DATA);
    C(DYAD_EVENT_CLOSE);
    C(DYAD_EVENT_CONNECT);
    C(DYAD_EVENT_DESTROY);
    C(DYAD_EVENT_ERROR);
    C(DYAD_EVENT_READY);
    C(DYAD_EVENT_TICK);
    C(DYAD_EVENT_TIMEOUT);
    C(DYAD_EVENT_ACCEPT);
#undef C
  default:
    HolyThrow("InvMode"); // invalid mode
  }
}

void STK_DyadSetReadCallback(void **stk) {
  dyad_addListener((dyad_Stream *)stk[0], (iptr)stk[1],
                   [](dyad_Event *e) {
                     FFI_CALL_TOS_4(e->udata, (uptr)e->stream, (uptr)e->data,
                                    e->size, (uptr)e->udata2);
                   },
                   stk[2], stk[3]);
}

void STK_DyadSetCloseCallback(void **stk) {
  dyad_addListener((dyad_Stream *)stk[0], (iptr)stk[1],
                   [](dyad_Event *e) {
                     FFI_CALL_TOS_2(e->udata, (uptr)e->stream, (uptr)e->udata2);
                   },
                   stk[2], stk[3]);
}

void STK_DyadSetListenCallback(void **stk) {
  dyad_addListener((dyad_Stream *)stk[0], (iptr)stk[1],
                   [](dyad_Event *e) {
                     FFI_CALL_TOS_2(e->udata, (uptr)e->remote, (uptr)e->udata2);
                   },
                   stk[2], stk[3]);
}

// read callbacks -> DYAD_EVENT_{LINE,DATA}
// close callbacks -> DYAD_EVENT_{DESTROY,ERROR,CLOSE}
// listen callbacks -> DYAD_EVENT_{TIMEOUT,CONNECT,READY,TICK,ACCEPT}

void STK_DyadSetTimeout(uptr *stk) {
  dyad_setTimeout((dyad_Stream *)stk[0], ((f64 *)stk)[1]);
}

void STK_DyadSetNoDelay(iptr *stk) {
  dyad_setNoDelay((dyad_Stream *)stk[0], (int)stk[1]);
}

void STK_UnblockSignals(void *) {
#ifndef _WIN32
  sigset_t all;
  sigfillset(&all);
  sigprocmask(SIG_UNBLOCK, &all, nullptr);
#endif
}

void STK__GrPaletteColorSet(u64 *stk) {
  GrPaletteColorSet(stk[0], {stk[1]});
}

u64 STK___IsValidPtr(uptr *stk) {
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
  /* round down to page boundary (equiv to stk[0] / page_size * page_size)
   *   0b100101010 (x)
   * & 0b111110000 <- ~(0b10000 - 1)
   * --------------
   *   0b100100000
   */
  // https://archive.md/Aj0S4
  return -1 != msync((void *)(stk[0] & ~(page_size - 1)), page_size, MS_ASYNC);

#endif
}

void STK_InterruptCore(usize *stk) {
  InterruptCore(stk[0]);
}

void STK___BootstrapForeachSymbol(void **stk) {
  for (auto &[name, sym] : TOSLoader)
    FFI_CALL_TOS_3(stk[0], (uptr)name.c_str(), (uptr)sym.val,
                   sym.type == HTT_EXPORT_SYS_SYM ? HTT_FUN : sym.type);
}

void STK_TOSPrint(iptr *stk) {
  TOSPrint((char *)stk[0], stk[1], stk + 2);
}

void STK_DrawWindowUpdate(uptr *stk) {
  DrawWindowUpdate((u8 *)stk[0], stk[1]);
}

u64 STK___GetTicksHP(void *) {
#ifndef _WIN32
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  u64 tick = ts.tv_nsec / 1000u;
  tick += ts.tv_sec * 1000000u;
  return tick;
#else
  static u64 freq = 0;
  if (!freq) {
    QueryPerformanceFrequency((LARGE_INTEGER *)&freq);
    freq /= 1000000U;
  }
  u64 cur;
  QueryPerformanceCounter((LARGE_INTEGER *)&cur);
  return cur / freq;
#endif
}

u64 STK___GetTicks(void *) {
  return GetTicks();
}

void STK_SetKBCallback(void **stk) {
  SetKBCallback(stk[0], stk[1]);
}

void STK_SetMSCallback(void **stk) {
  SetMSCallback(stk[0]);
}

void STK___AwakeCore(usize *stk) {
  AwakeFromSleeping(stk[0]);
}

void STK___SleepHP(u64 *stk) {
  SleepHP(stk[0]);
}

void STK___Sleep(u64 *stk) {
  SleepHP(stk[0] * 1000);
}

void STK_SetFs(void **stk) {
  SetFs(stk[0]);
}

void STK_SetGs(void **stk) {
  SetGs(stk[0]);
}

void STK_SndFreq(u64 *stk) {
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

u64 STK_FUnixTime(char **stk) {
  return VFsUnixTime(stk[0]);
}

u64 STK___FExists(char **stk) {
  return VFsFileExists(stk[0]);
}

u64 STK_UnixNow(void *) {
  return system_clock::to_time_t(system_clock::now());
}

void STK___SpawnCore(uptr *stk) {
  CreateCore(stk[0], (void *)stk[1]);
}

void *STK_NewVirtualChunk(usize *stk) {
  return NewVirtualChunk(stk[0], stk[1]);
}

void STK_FreeVirtualChunk(uptr *stk) {
  FreeVirtualChunk((void *)stk[0], stk[1]);
}

void STK_VFsSetPwd(char **stk) {
  VFsSetPwd(stk[0]);
}

u64 STK_VFsExists(char **stk) {
  return VFsFileExists(stk[0]);
}

u64 STK_VFsIsDir(char **stk) {
  return VFsIsDir(stk[0]);
}

i64 STK_VFsFSize(char **stk) {
  return VFsFSize(stk[0]);
}

void STK_VFsFTrunc(uptr *stk) {
  VFsFTrunc((char *)stk[0], stk[1]);
}

void *STK_VFsFRead(char **stk) {
  return VFsFileRead(stk[0], (u64 *)stk[1]);
}

u64 STK_VFsFWrite(char **stk) {
  return VFsFileWrite(stk[0], stk[1], (uptr)stk[2]);
}

u64 STK_VFsDirMk(char **stk) {
  return VFsDirMk(stk[0]);
}

char **STK_VFsDir(void *) {
  return VFsDir();
}

u64 STK_VFsDel(char **stk) {
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

u64 STK_VFsFBlkRead(uptr *stk) {
  fflush((FILE *)stk[3]);
  return stk[2] == fread((void *)stk[0], stk[1], stk[2], (FILE *)stk[3]);
}

u64 STK_VFsFBlkWrite(uptr *stk) {
  bool r = stk[2] == fwrite((void *)stk[0], stk[1], stk[2], (FILE *)stk[3]);
  fflush((FILE *)stk[3]);
  return r;
}

void STK_VFsFSeek(uptr *stk) {
  fseek((FILE *)stk[1], stk[0], SEEK_SET);
}

void STK_VFsSetDrv(u8 *stk) {
  VFsSetDrv(stk[0]);
}

u64 STK_VFsGetDrv(void *) {
  return (u64)VFsGetDrv();
}

void STK_SetVolume(u64 *stk) {
  union {
    u64 i;
    f64 flt;
  } un = {stk[0]};
  SetVolume(un.flt);
}

u64 STK_GetVolume(void *) {
  union {
    f64 flt;
    u64 i;
  } un = {GetVolume()};
  return un.i;
}

void STK_ExitTINE(int *stk) {
  ShutdownTINE(stk[0]);
}

u64 STK___IsCmdLine(void *) {
  return (u64)is_cmd_line;
}

struct HolyFunc {
  std::string_view m_name;
  uptr             m_fp;
  u16              m_arity; // arity must be <= 0xffFF/sizeof U64
};

template <usize S> struct ByteLiteral {
  usize       m_sz;
  char const *m_lit;
  constexpr ByteLiteral(char const (&s)[S]) : m_sz{S - 1}, m_lit{s} {}
};

void RegisterFunctionPtrs(std::initializer_list<HolyFunc> ffi_list) {
  // clang-format off
  // I used https://defuse.ca/online-x86-assembler.htm
  // boring register pushing and stack alignment bullshit
  // read the SysV/Win64 ABIs and Doc/GuideLines.DD if interested
  ByteLiteral constexpr inst =
  /*
   * 0x0:  55                      push   rbp
   * 0x1:  48 89 e5                mov    rbp,rsp
   * 0x4:  48 83 e4 f0             and    rsp,0xfffffffffffffff0
   *                          // ^ chops stack off to align to 16
   * 0x8:  56                      push   rsi
   * 0x9:  57                      push   rdi
   * 0xa:  41 52                   push   r10
   * 0xc:  41 53                   push   r11
   * 0xe:  41 54                   push   r12
   * 0x10: 41 55                   push   r13
   * 0x12: 41 56                   push   r14
   * 0x14: 41 57                   push   r15
   * len = 0x16
   */
        "\x55"
        "\x48\x89\xE5"
        "\x48\x83\xE4\xF0"
        "\x56"
        "\x57"
        "\x41\x52"
        "\x41\x53"
        "\x41\x54"
        "\x41\x55"
        "\x41\x56"
        "\x41\x57"
#ifdef _WIN32
  // https://archive.md/4HDA0#selection-2085.880-2085.1196
  // rcx is the first arg i have to provide in win64 abi
  // last 4 register pushes are for register "home"s
  // that windows wants me to provide
  /*
   * 0x0:  48 8d 4d 10             lea    rcx,[rbp+0x10]
   * 0x4:  41 51                   push   r9
   * 0x6:  41 50                   push   r8
   * 0x8:  52                      push   rdx
   * 0x9:  51                      push   rcx
   * len = 0xa
   */
        "\x48\x8D\x4D\x10"
        "\x41\x51"
        "\x41\x50"
        "\x52"
        "\x51"
#else // sysv
  // rdi is the first arg i have to provide in sysv
  /*
   * 0x0:  48 8d 7d 10             lea    rdi,[rbp+0x10]
   * len = 0x4
   */
        "\x48\x8D\x7D\x10"
#endif
  /*
   * 0x0:  48 b8 11 22 33 44 55 66 77 88    movabs rax,0x8877665544332211(fp)
   * 0xa:  ff d0                            call   rax
   * len = 0xc
   */
        "\x48\xB8" "\x11\x22\x33\x44\x55\x66\x77\x88" // 0x8877... is a placeholder
        "\xFF\xD0"
#ifdef _WIN32
  // can just add to rsp since
  // those 4 registers are volatile
  /* 0x0:  48 83 c4 20             add    rsp,0x20
   * len = 0x4
   */
        "\x48\x83\xC4\x20"
#endif
  // pops stack. boring stuff
  /*
   * 0x0:  41 5f                   pop    r15
   * 0x2:  41 5e                   pop    r14
   * 0x4:  41 5d                   pop    r13
   * 0x6:  41 5c                   pop    r12
   * 0x8:  41 5b                   pop    r11
   * 0xa:  41 5a                   pop    r10
   * 0xc:  5f                      pop    rdi
   * 0xd:  5e                      pop    rsi
   * 0xe:  c9                      leave
   * 0xf:  c2 11 22                ret    0x2211(arity*8(==sizeof u64))
   * len = 0x12
   */
        "\x41\x5F"
        "\x41\x5E"
        "\x41\x5D"
        "\x41\x5C"
        "\x41\x5B"
        "\x41\x5A"
        "\x5F"
        "\x5E"
        "\xC9"
        "\xC2" "\x11\x22"; // 0x2211 is a placeholder
  // clang-format on
  usize constexpr fp_off =
#ifndef _WIN32
      0x16 + 0x4 + 0x2;
#else
      0x16 + 0xa + 0x2;
#endif
  usize constexpr arity_off = inst.m_sz - 2;

  auto blob = VirtAlloc<u8>(inst.m_sz * ffi_list.size());
  for (usize i = 0; i < ffi_list.size(); ++i) {
    memcpy(blob + i * inst.m_sz, inst.m_lit, inst.m_sz);
  }
  {
    usize i = 0;
    for (HolyFunc hf : ffi_list) {
      usize off = i * inst.m_sz;
      // for the 0x8877... placeholder
      memcpy(blob + off + fp_off, &hf.m_fp, sizeof(uptr));
      // for the 0x2211 placeholder
      hf.m_arity *= sizeof(u64); // all args are u64 in HolyC
      memcpy(blob + off + arity_off, &hf.m_arity, sizeof(u16) /* ret imm16 */);
      TOSLoader.emplace(hf.m_name, CHash{HTT_FUN, {blob + off}});
      ++i;
    }
  }
  // clang-format off
  // ret <arity*8>; (8 == sizeof(u64))
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
  //   i    2    // RBP + 16(this is where the stack starts)
  // clang-format on
}

} // namespace

void BootstrapLoader() {
#define R(holy, secular, arity) {holy, (uptr)secular, arity}
#define S(name, arity)             \
  {                                \
    #name, (uptr)STK_##name, arity \
  }
  RegisterFunctionPtrs({
      R("__CmdLineBootText", CmdLineBootText, 0),
      R("mp_cnt", mp_cnt, 0),
      R("__CoreNum", CoreNum, 0),
      R("GetFs", GetFs, 0),
      R("GetGs", GetGs, 0),
      S(__IsCmdLine, 0),
      S(__IsValidPtr, 1),
      S(__SpawnCore, 0),
      S(UnixNow, 0),
      S(InterruptCore, 1),
      S(NewVirtualChunk, 2),
      S(FreeVirtualChunk, 2),
      S(ExitTINE, 1),
      S(__GetStr, 1),
      S(__FExists, 1),
      S(FUnixTime, 1),
      S(SetClipboardText, 1),
      S(GetClipboardText, 0),
      S(SndFreq, 1),
      S(__Sleep, 1),
      S(__SleepHP, 1),
      S(__AwakeCore, 1),
      S(SetFs, 1),
      S(SetGs, 1),
      S(SetKBCallback, 2),
      S(SetMSCallback, 1),
      S(__GetTicks, 0),
      S(__BootstrapForeachSymbol, 1),
      S(DrawWindowUpdate, 2),
      S(UnblockSignals, 0),
      /*
       * In TempleOS variadics, functions follow __cdecl, whereas normally
       * they follow __stdcall which is why the arity argument is needed(RET1
       * x). Thus we don't have to clean up the stack in variadics.
       */
      S(TOSPrint, 0),
      //
      S(DyadInit, 0),
      S(DyadUpdate, 0),
      S(DyadShutdown, 0),
      S(DyadNewStream, 0),
      S(DyadListen, 2),
      S(DyadConnect, 3),
      S(DyadWrite, 3),
      S(DyadEnd, 1),
      S(DyadClose, 1),
      S(DyadGetAddress, 1),
      S(_DyadGetCallbackMode, 1),
      S(DyadSetReadCallback, 4),
      S(DyadSetCloseCallback, 4),
      S(DyadSetListenCallback, 4),
      S(DyadSetTimeout, 2),
      S(DyadSetNoDelay, 2),
      S(VFsFTrunc, 2),
      S(VFsSetPwd, 1),
      S(VFsExists, 1),
      S(VFsIsDir, 1),
      S(VFsFSize, 1),
      S(VFsFRead, 2),
      S(VFsFWrite, 3),
      S(VFsDel, 1),
      S(VFsDir, 0),
      S(VFsDirMk, 1),
      S(VFsFBlkRead, 4),
      S(VFsFBlkWrite, 4),
      S(VFsFOpenW, 1),
      S(VFsFOpenR, 1),
      S(VFsFClose, 1),
      S(VFsFSeek, 2),
      S(VFsSetDrv, 1),
      S(VFsGetDrv, 0),
      S(GetVolume, 0),
      S(SetVolume, 1),
      S(__GetTicksHP, 0),
      S(_GrPaletteColorSet, 2),
  });
}

// vim: set expandtab ts=2 sw=2 :
