#include "3d.h"
#ifndef TARGET_WIN32
#include <signal.h>
#include <ucontext.h>
#ifndef __FreeBSD__ 
#ifndef REG_R8
enum
{
  REG_R8 = 0,
# define REG_R8		REG_R8
  REG_R9,
# define REG_R9		REG_R9
  REG_R10,
# define REG_R10	REG_R10
  REG_R11,
# define REG_R11	REG_R11
  REG_R12,
# define REG_R12	REG_R12
  REG_R13,
# define REG_R13	REG_R13
  REG_R14,
# define REG_R14	REG_R14
  REG_R15,
# define REG_R15	REG_R15
  REG_RDI,
# define REG_RDI	REG_RDI
  REG_RSI,
# define REG_RSI	REG_RSI
  REG_RBP,
# define REG_RBP	REG_RBP
  REG_RBX,
# define REG_RBX	REG_RBX
  REG_RDX,
# define REG_RDX	REG_RDX
  REG_RAX,
# define REG_RAX	REG_RAX
  REG_RCX,
# define REG_RCX	REG_RCX
  REG_RSP,
# define REG_RSP	REG_RSP
  REG_RIP,
# define REG_RIP	REG_RIP
  REG_EFL,
# define REG_EFL	REG_EFL
  REG_CSGSFS,		/* Actually short cs, gs, fs, __pad0.  */
# define REG_CSGSFS	REG_CSGSFS
  REG_ERR,
# define REG_ERR	REG_ERR
  REG_TRAPNO,
# define REG_TRAPNO	REG_TRAPNO
  REG_OLDMASK,
# define REG_OLDMASK	REG_OLDMASK
  REG_CR2
# define REG_CR2	REG_CR2
};
#endif
static void routine(int sig,struct siginfo_t *info,ucontext_t *ctx) {
	FualtCB();
	vec_CHash_t *d;
	void *fun;
	int64_t regs[16+3];
	if(d=map_get(&TOSLoader,"DebuggerLand")) {
		fun=d->data[0].val;
		regs[0]=ctx->uc_mcontext.gregs[REG_RAX];
		regs[1]=ctx->uc_mcontext.gregs[REG_RCX];
		regs[2]=ctx->uc_mcontext.gregs[REG_RDX];
		regs[3]=ctx->uc_mcontext.gregs[REG_RBX];
		regs[4]=ctx->uc_mcontext.gregs[REG_RSP];
		regs[5]=ctx->uc_mcontext.gregs[REG_RBP];
		regs[6]=ctx->uc_mcontext.gregs[REG_RSI];
		regs[7]=ctx->uc_mcontext.gregs[REG_RDI];
		regs[8]=ctx->uc_mcontext.gregs[REG_R8];
		regs[9]=ctx->uc_mcontext.gregs[REG_R9];
		regs[10]=ctx->uc_mcontext.gregs[REG_R10];
		regs[11]=ctx->uc_mcontext.gregs[REG_R11];
		regs[12]=ctx->uc_mcontext.gregs[REG_R12];
		regs[13]=ctx->uc_mcontext.gregs[REG_R13];
		regs[14]=ctx->uc_mcontext.gregs[REG_R14];
		regs[15]=ctx->uc_mcontext.gregs[REG_R15];
		//I added these
		regs[16]=ctx->uc_mcontext.gregs[REG_RIP];
		regs[17]=ctx->uc_mcontext.fpregs;
		regs[18]=ctx->uc_mcontext.gregs[REG_EFL];
		FFI_CALL_TOS_2(fun,sig,regs);
	} else 
		abort();
}
#else
static void routine(int sig,struct siginfo_t *info,ucontext_t *ctx) {
	vec_CHash_t *d;
	void *fun;
	int64_t regs[16+3];
	if(d=map_get(&TOSLoader,"DebuggerLand")) {
		fun=d->data[0].val;
		regs[0]=ctx->uc_mcontext.mc_rax;
		regs[1]=ctx->uc_mcontext.mc_rcx;
		regs[2]=ctx->uc_mcontext.mc_rdx;
		regs[3]=ctx->uc_mcontext.mc_rbx;
		regs[4]=ctx->uc_mcontext.mc_rsp;
		regs[5]=ctx->uc_mcontext.mc_rbp;
		regs[6]=ctx->uc_mcontext.mc_rsi;
		regs[7]=ctx->uc_mcontext.mc_rdi;
		regs[8]=ctx->uc_mcontext.mc_r8;
		regs[9]=ctx->uc_mcontext.mc_r9;
		regs[10]=ctx->uc_mcontext.mc_r10;
		regs[11]=ctx->uc_mcontext.mc_r11;
		regs[12]=ctx->uc_mcontext.mc_r12;
		regs[13]=ctx->uc_mcontext.mc_r13;
		regs[14]=ctx->uc_mcontext.mc_r14;
		regs[15]=ctx->uc_mcontext.mc_r15;
		//I added these
		regs[16]=ctx->uc_mcontext.mc_rip;
		regs[17]=&ctx->uc_mcontext.mc_fpstate;
		regs[18]=ctx->uc_mcontext.mc_rflags;
		FFI_CALL_TOS_2(fun,sig,regs);
	} else 
		abort();
}

#endif
void SetupDebugger() {
	struct sigaction inf;
	inf.sa_flags=SA_SIGINFO|SA_NODEFER;
	inf.sa_sigaction=routine;
	sigemptyset(&inf.sa_mask);
	sigaction(SIGTRAP,&inf,NULL);
	sigaction(SIGBUS,&inf,NULL);
	sigaction(SIGSEGV,&inf,NULL);
	sigaction(SIGFPE,&inf,NULL);
}
#else
#include <windows.h>
#include <errhandlingapi.h>
static LONG WINAPI VectorHandler (struct _EXCEPTION_POINTERS *info) {
  vec_CHash_t *d;
  void *fun;
  int64_t regs[16+3];	
  switch(info->ExceptionRecord->ExceptionCode) {
    #define FERR(code) case code: goto exit;
    FERR(EXCEPTION_ACCESS_VIOLATION);
    FERR(EXCEPTION_ARRAY_BOUNDS_EXCEEDED);
    FERR(EXCEPTION_DATATYPE_MISALIGNMENT);
    FERR(EXCEPTION_FLT_DENORMAL_OPERAND);
    FERR(EXCEPTION_FLT_DIVIDE_BY_ZERO);
    FERR(EXCEPTION_FLT_INEXACT_RESULT);
    FERR(EXCEPTION_FLT_INVALID_OPERATION);
    FERR(EXCEPTION_FLT_OVERFLOW);
    FERR(EXCEPTION_FLT_STACK_CHECK);
    FERR(EXCEPTION_FLT_UNDERFLOW);
    FERR(EXCEPTION_ILLEGAL_INSTRUCTION);
    FERR(EXCEPTION_IN_PAGE_ERROR);
    FERR(EXCEPTION_INT_DIVIDE_BY_ZERO);
    FERR(EXCEPTION_INVALID_DISPOSITION);
    FERR(EXCEPTION_STACK_OVERFLOW);
    FERR(EXCEPTION_BREAKPOINT);
    //https://stackoverflow.com/questions/16271828/is-set-single-step-trap-available-on-win-7
    FERR(STATUS_SINGLE_STEP);
    default:;
  }
  //SignalHandler(0);
  return EXCEPTION_CONTINUE_EXECUTION;
  exit:;
  CONTEXT *ctx=info->ContextRecord;
  if(d=map_get(&TOSLoader,"DebuggerLandWin")) {
		fun=d->data[0].val;
		regs[0]=ctx->Rax;
		regs[1]=ctx->Rcx;
		regs[2]=ctx->Rdx;
		regs[3]=ctx->Rbx;
		regs[4]=ctx->Rsp;
		regs[5]=ctx->Rbp;
		regs[6]=ctx->Rsi;
		regs[7]=ctx->Rdi;
		regs[8]=ctx->R8;
		regs[9]=ctx->R9;
		regs[10]=ctx->R10;
		regs[11]=ctx->R11;
		regs[12]=ctx->R12;
		regs[13]=ctx->R13;
		regs[14]=ctx->R14;
		regs[15]=ctx->R15;
		//I added these
		regs[16]=ctx->Rip;
		regs[17]=&ctx->FltSave;
		regs[18]=ctx->EFlags;
		int c=info->ExceptionRecord->ExceptionCode;
		int sig=c==EXCEPTION_BREAKPOINT||c==STATUS_SINGLE_STEP?5:0;
		FFI_CALL_TOS_2(fun,sig,regs);
		return EXCEPTION_CONTINUE_EXECUTION;
	} else 
		abort();
}
void SetupDebugger() {
	AddVectoredExceptionHandler(1,&VectorHandler);
}
#endif
