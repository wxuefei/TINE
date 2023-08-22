#include "backtrace.hxx"
#include "dbg.hxx"
#include "tos_aot.hxx"
#include "types.h"

#include <ucontext.h>

#include <initializer_list>

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include <tos_callconv.h>

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

// vim: set expandtab ts=2 sw=2 :
