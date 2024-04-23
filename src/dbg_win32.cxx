#include "backtrace.hxx"
#include "dbg.hxx"
#include "tos_aot.hxx"
#include "types.h"

#include <windows.h>
#include <winnt.h>
#include <errhandlingapi.h>

#include <tos_callconv.h>

namespace {

    auto WINAPI VEHandler(struct _EXCEPTION_POINTERS* info) -> LONG {
        return 0;
#if 0
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
#endif
    }
} // namespace

void SetupDebugger() {
    AddVectoredExceptionHandler(1, VEHandler);
}

// vim: set expandtab ts=2 sw=2 :
