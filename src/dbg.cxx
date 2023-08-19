#include "tos_aot.hxx"
#include "types.h"

#ifdef _WIN32
  #include <windows.h>
  #include <winnt.h>
  #include <errhandlingapi.h>
#else
  #include <ucontext.h>
#endif

#include <algorithm>
#include <initializer_list>
#include <string>
#include <vector>

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include <tos_ffi.h>

namespace {
std::vector<std::string> sorted_syms;
std::string const        unknown_fun{"UNKNOWN"};

void InitSortedSyms() {
  static bool init = false;
  if (init)
    return;
  sorted_syms.reserve(TOSLoader.size());
  for (auto const& [name, _] : TOSLoader)
    sorted_syms.emplace_back(name);
  std::sort(sorted_syms.begin(), sorted_syms.end(),
            [](auto const& a, auto const& b) -> bool {
              return TOSLoader[a].val < TOSLoader[b].val;
            });
  init = true;
}

void BackTrace(uptr ctx_rbp, uptr ctx_rip) {
  InitSortedSyms();
  fputc('\n', stderr);
  auto  rbp = reinterpret_cast<void*>(ctx_rbp);
  auto  ptr = reinterpret_cast<void*>(ctx_rip);
  void* oldp;
  //
  std::string const* last;
  while (rbp) {
    oldp = nullptr;
    last = &unknown_fun;
    // linear search that iterates over symbols sorted in ascending address
    // order to find out where we were
    for (auto const& s : sorted_syms) {
      void* curp = TOSLoader[s].val;
      if (curp == ptr) {
        fprintf(stderr, "%s [%#" PRIx64 "]\n", s.c_str(), (uptr)ptr);
      } else if (curp > ptr) {
        // i know im supposed to use %p but it's weird beecause on windows it's
        // fucky wucky(prints numbers with 0s)
        fprintf(stderr, "%s [%#" PRIx64 "+%#" PRIx64 "] %#" PRIx64 "\n",
                last->c_str(), (uptr)ptr, (u8*)ptr - (u8*)oldp, (uptr)curp);
        goto next;
      }
      oldp = curp;
      last = &s;
    }
  next:
    ptr = static_cast<void**>(rbp)[1]; // [RBP+0x8] is the return address
    rbp = static_cast<void**>(rbp)[0]; // [RBP] is the previous base pointer
  }
  fputc('\n', stderr);
}

} // namespace

#ifdef _WIN32

namespace {

auto WINAPI VectorHandler(struct _EXCEPTION_POINTERS* info) -> LONG {
  u64 sig;
  #define E(code) EXCEPTION_##code
  switch (info->ExceptionRecord->ExceptionCode) {
  case E(ACCESS_VIOLATION):
  case E(ARRAY_BOUNDS_EXCEEDED):
  case E(DATATYPE_MISALIGNMENT):
  case E(FLT_DENORMAL_OPERAND):
  case E(FLT_DIVIDE_BY_ZERO):
  case E(FLT_INEXACT_RESULT):
  case E(FLT_INVALID_OPERATION):
  case E(FLT_OVERFLOW):
  case E(FLT_STACK_CHECK):
  case E(FLT_UNDERFLOW):
  case E(ILLEGAL_INSTRUCTION):
  case E(IN_PAGE_ERROR):
  case E(INT_DIVIDE_BY_ZERO):
  case E(INVALID_DISPOSITION):
  case E(STACK_OVERFLOW):
    sig = 0;
    break;
  case E(BREAKPOINT):
  case STATUS_SINGLE_STEP: /*https://archive.md/sZzVj*/
    sig = 5 /*SIGTRAP*/;
    break;
  default:
    return E(CONTINUE_EXECUTION);
  }
  #define REG(x) static_cast<u64>(info->ContextRecord->x)
  u64 regs[] = {
      REG(Rax),    REG(Rcx), REG(Rdx),
      REG(Rbx),    REG(Rsp), REG(Rbp),
      REG(Rsi),    REG(Rdi), REG(R8),
      REG(R9),     REG(R10), REG(R11),
      REG(R12),    REG(R13), REG(R14),
      REG(R15),    REG(Rip), (uptr)&info->ContextRecord->FltSave,
      REG(EFlags),
  };
  BackTrace(REG(Rbp), REG(Rip));
  static void* fp = nullptr;
  if (!fp)
    fp = TOSLoader["DebuggerLandWin"].val;
  FFI_CALL_TOS_2(fp, sig, (uptr)regs);
  return E(CONTINUE_EXECUTION);
  #undef E
}
} // namespace

void SetupDebugger() {
  AddVectoredExceptionHandler(1, VectorHandler);
}

#else

namespace {

void routine(int sig, siginfo_t*, void* ctx_ucontext) {
  // block signals temporarily, will be unblocked later by DebuggerLand
  sigset_t all;
  sigfillset(&all);
  sigprocmask(SIG_BLOCK, &all, nullptr);
  auto ctx = static_cast<ucontext_t*>(ctx_ucontext);
  #ifdef __linux__
    #define REG(x) static_cast<u64>(ctx->uc_mcontext.gregs[REG_##x])
  // clang-format off
  //
  // apparently ucontext is implementation defined idk
  // if your on musl or something fix this yourself and send me a patch
  // probably only works on glibc lmao
  // https://github.com/bminor/glibc/blob/4290aed05135ae4c0272006442d147f2155e70d7/sysdeps/unix/sysv/linux/x86/sys/ucontext.h#L239
  //
  // clang-format on
  u64 regs[] = {
      REG(RAX), REG(RCX), REG(RDX),
      REG(RBX), REG(RSP), REG(RBP),
      REG(RSI), REG(RDI), REG(R8),
      REG(R9),  REG(R10), REG(R11),
      REG(R12), REG(R13), REG(R14),
      REG(R15), REG(RIP), (uptr)ctx->uc_mcontext.fpregs,
      REG(EFL),
  };
  #elif defined(__FreeBSD__)
    #define REG(X) static_cast<u64>(ctx->uc_mcontext.mc_##X)
  // freebsd seems to just use an
  // array of longs for their floating point context lmao
  u64 regs[] = {
      REG(rax),    REG(rcx), REG(rdx),
      REG(rbx),    REG(rsp), REG(rbp),
      REG(rsi),    REG(rdi), REG(r8),
      REG(r9),     REG(r10), REG(r11),
      REG(r12),    REG(r13), REG(r14),
      REG(r15),    REG(rip), (uptr)ctx->uc_mcontext.mc_fpstate,
      REG(rflags),
  };
  #endif
  BackTrace(regs[5] /*RBP*/, regs[15] /*RIP*/);
  static void* fp = nullptr;
  if (!fp)
    fp = TOSLoader["DebuggerLand"].val;
  FFI_CALL_TOS_2(fp, sig, (uptr)regs);
}
} // namespace

void SetupDebugger() {
  struct sigaction inf;
  inf.sa_flags     = SA_SIGINFO | SA_NODEFER;
  inf.sa_sigaction = routine;
  sigemptyset(&inf.sa_mask);
  for (auto i : {SIGTRAP, SIGBUS, SIGSEGV, SIGFPE})
    sigaction(i, &inf, nullptr);
}

#endif

// who the fuck cares about memory leaks
// its gonna be executed once or twice in
// the entire debug session not to mention
// WhichFun() wont even be called in normal
// circumstances
#define STR_DUP(s) strcpy(new (std::nothrow) char[s->size() + 1], s->c_str())

// great when you use lldb and get a fault
// (lldb) p (char*)WhichFun($pc)
[[gnu::used, gnu::visibility("default")]] auto WhichFun(void* ptr) -> char* {
  InitSortedSyms();
  std::string const* last = &unknown_fun;
  for (auto const& s : sorted_syms) {
    if (TOSLoader[s].val >= ptr)
      return STR_DUP(last);
    last = &s;
  }
  return STR_DUP(last);
}

// vim: set expandtab ts=2 sw=2 :
