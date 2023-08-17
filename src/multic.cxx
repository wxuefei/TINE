#include "multic.hxx"
#include "dbg.hxx"
#include "main.hxx"
#include "tos_aot.hxx"
#include "vfs.hxx"

#ifdef _WIN32
  #include <windows.h>
  #include <processthreadsapi.h>
  #include <synchapi.h>
  #include <sysinfoapi.h>
  #include <timeapi.h>
#else
  #ifdef __linux__
    #include <linux/futex.h>
    #include <sys/syscall.h>
  #elif defined(__FreeBSD__)
    #include <sys/types.h>
    #include <sys/umtx.h>
  #endif
  #include <pthread.h>
#endif

#include <utility>
#include <vector>

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include <tos_ffi.h>

auto GetTicks() -> u64 {
#ifdef _WIN32
  return GetTickCount();
#else
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return static_cast<u64>(ts.tv_nsec) / UINT64_C(1000000) //
       + static_cast<u64>(ts.tv_sec) * UINT64_C(1000);
#endif
}

namespace {

struct CCore {
#ifdef _WIN32
  HANDLE thread, event, mtx;
  // When are we going to wake up at?
  // winmm callback checks for this constantly
  // check out GetTicksHP() for an explanation
  // 0 means it's not sleeping
  u64 awake_at;
#else
  pthread_t thread;
  /*
   * man 2 futex
   * > The uaddr argument points to the futex word.  On all platforms,
   * > futexes are four-byte integers that must be aligned on a four-
   * > byte boundary.
   * freebsd doesnt seem to mind about alignment so im just going to use
   * u32 too(though i have to specify UMTX_OP_WAIT_UINT instead of
   * UMTX_OP_WAIT)
   */
  // Is this thread sleeping?
  alignas(4) u32 is_sleeping;
  // not using std::atomic<T> here(instead using atomic builtins that operate on
  // plain values) because i need them for system calls and casting
  // std::atomic<T>* to T* is potentially UB and
  // static_assert(std::is_layout_compatible_v<std::atomic<u32>, u32>)
  // failed on my machine
#endif
  // self-referenced core number
  usize core_num;
  // HolyC function pointers it needs to execute on launch
  std::vector<void*> fps;
};

// TempleOS has a hardcoded maximum core count of 128 but oh well,
// let's be flexible in case we get machines with more of them
//
// Also note: these cores will never be destructed in C++ because this is
// basically an emulation of a real CPU core, CoreAPSethTask never terminates,
// instead let the host OS clean it up and do whatever with it
std::vector<CCore> cores;
// Thread local self-referenced CCore structure
thread_local CCore* self;

#ifndef _WIN32
auto ThreadRoutine(void* arg) -> void* {
#else
auto WINAPI ThreadRoutine(LPVOID arg) -> DWORD {
#endif
  self = static_cast<CCore*>(arg);
  VFsThrdInit();
  SetupDebugger();
#ifndef _WIN32
  static void* sig_fp = nullptr;
  if (!sig_fp)
    sig_fp = TOSLoader["__InterruptCoreRoutine"].val;
  signal(SIGUSR1, reinterpret_cast<SignalCallback*>(sig_fp));
#endif
  // CoreAPSethTask(...) (T/FULL_PACKAGE.HC) <- (non-Core0)
  // IET_MAIN boot functions + kernel entry point <- Core0
  //
  // ZERO_BP so the return addr&rbp is 0 and
  // stack traces don't climb up the C++ stack
  for (auto fp : self->fps) {
    FFI_CALL_TOS_0_ZERO_BP(fp);
  }
  // Note: CoreAPSethTask() will NEVER return
  // so the below things are just to match the return type
#ifdef _WIN32
  return 0;
#else
  return nullptr;
#endif
}

/*
 * (DolDoc code)
 * $ID,-2$$TR-C,"How do you use the FS and GS segment registers."$
 * $ID,2$$FG,2$MOV RAX,FS:[RAX]$FG$ : FS can be set with a $FG,2$WRMSR$FG$,
 * but displacement is RIP relative, so it's tricky to use.  FS is used for
 * the current $LK,"CTask",A="MN:CTask"$, GS for $LK,"CCPU",A="MN:CCPU"$.
 *
 * Note on Fs and Gs: They might seem like very weird names for ThisTask and
 * ThisCPU repectively but it's because they are stored in the F Segment and G
 * Segment registers in native TempleOS. (https://archive.md/pf2td)
 */
// I tried putting this in CCore but it became fucky wucky so yeah, it's here
thread_local void* Fs = nullptr;
thread_local void* Gs = nullptr;

} // namespace

auto GetFs() -> void* {
  return Fs;
}

void SetFs(void* f) {
  Fs = f;
}

auto GetGs() -> void* {
  return Gs;
}

void SetGs(void* g) {
  Gs = g;
}

auto CoreNum() -> usize {
  return self->core_num;
}

// this may look like bad code but HolyC cannot switch
// contexts unless you call Yield() in a loop(eg, `while(TRUE);`)
// so we have to set RIP manually(this routine is called
// when CTRL+ALT+C/X is pressed inside TempleOS while the core is
// stuck in an infinite loop witout yielding
void InterruptCore(usize core) {
  auto& c = cores[core];
#ifdef _WIN32
  CONTEXT ctx{};
  ctx.ContextFlags = CONTEXT_FULL;
  SuspendThread(c.thread);
  GetThreadContext(c.thread, &ctx);
  // push rip
  static_assert(sizeof(DWORD64) == 8);
  ctx.Rsp -= 8;
  ((DWORD64*)ctx.Rsp)[0] = ctx.Rip;
  //
  static void* fp = nullptr;
  if (!fp)
    fp = TOSLoader["__InterruptCoreRoutine"].val;
  // movabs rip, <fp>
  ctx.Rip = reinterpret_cast<uptr>(fp);
  SetThreadContext(c.thread, &ctx);
  ResumeThread(c.thread);
#else
  // block signals temporarily
  // will be unblocked later by __InterruptCoreRoutine
  sigset_t all;
  sigfillset(&all);
  sigprocmask(SIG_BLOCK, &all, nullptr);
  // this will execute the signal handler for SIGUSR1 in the core because i cant
  // remotely suspend threads like Win32 SuspendThread in unix
  pthread_kill(c.thread, SIGUSR1);
#endif
}

void CreateCore(usize n, std::vector<void*>&& fps) {
  if (n == 0) // boot
    cores.resize(proc_cnt);
  auto& c = cores[n];
  // CoreAPSethTask(...) passed from SpawnCore or
  // IET_MAIN function pointers+kernel entry point from LoadHCRT
  c.fps      = std::move(fps);
  c.core_num = n;
#ifdef _WIN32
  c.thread = CreateThread(nullptr, 0, ThreadRoutine, &c, 0, nullptr);
  c.mtx    = CreateMutex(nullptr, FALSE, nullptr);
  c.event  = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  SetThreadPriority(c.thread, THREAD_PRIORITY_HIGHEST);
  // literally wtf, also gcc doesnt support it so whatever
  // https://archive.li/MIMDo
#else
  pthread_create(&c.thread, nullptr, ThreadRoutine, &c);
  char buf[16];
  snprintf(buf, sizeof buf, "Seth(Core%" PRIu64 ")", n);
  pthread_setname_np(c.thread, buf);
#endif
}

void AwakeCore(usize core) {
  auto& c = cores[core];
#ifdef _WIN32
  WaitForSingleObject(c.mtx, INFINITE);
  c.awake_at = 0;
  SetEvent(c.event);
  ReleaseMutex(c.mtx);
#else
  if (c.is_sleeping) {
  #ifdef __linux__
    syscall(SYS_futex, &c.is_sleeping, FUTEX_WAKE, UINT32_C(1), nullptr,
            nullptr, 0);
  #elif defined(__FreeBSD__)
    _umtx_op(&c.is_sleeping, UMTX_OP_WAKE, UINT32_C(1), nullptr, nullptr);
  #endif
  }
  __atomic_store_n(&c.is_sleeping, UINT32_C(0), __ATOMIC_SEQ_CST);
#endif
}

#ifdef _WIN32

namespace {

UINT tick_inc;
u64  ticks = 0;

// To just get ticks we can use QueryPerformanceFrequency
// and QueryPerformanceCounter but we want to set an winmm
// event that updates the tick count while also helping cores wake up
//
// i killed two birds with one stoner
auto GetTicksHP() -> u64 {
  static bool init = false;
  if (!init) {
    init = true;
    TIMECAPS tc;
    timeGetDevCaps(&tc, sizeof tc);
    tick_inc = tc.wPeriodMin;
    timeSetEvent(
        tick_inc, tick_inc,
        [](auto, auto, auto, auto, auto) {
          ticks += tick_inc;
          for (auto& c : cores) {
            WaitForSingleObject(c.mtx, INFINITE);
            // check if ticks reached awake_at
            if (ticks >= c.awake_at && c.awake_at > 0) {
              SetEvent(c.event);
              c.awake_at = 0;
            }
            ReleaseMutex(c.mtx);
          }
        },
        0, TIME_PERIODIC);
  }
  return ticks;
}

} // namespace

#endif

void SleepHP(u64 us) {
  auto& c = cores[CoreNum()];
#ifdef _WIN32
  auto const t = GetTicksHP();
  WaitForSingleObject(c.mtx, INFINITE);
  // windows doesnt have accurate microsecond sleep :(
  c.awake_at = t + us / 1000;
  ReleaseMutex(c.mtx);
  WaitForSingleObject(c.event, INFINITE);
#else
  struct timespec ts {};
  ts.tv_nsec = (us % 1000000) * 1000;
  ts.tv_sec = us / 1000000;
  __atomic_store_n(&c.is_sleeping, UINT32_C(1), __ATOMIC_SEQ_CST);
  #ifdef __linux__
  syscall(SYS_futex, &c.is_sleeping, FUTEX_WAIT, 1u, &ts, nullptr, 0);
  #elif defined(__FreeBSD__)
  _umtx_op(&c.is_sleeping, UMTX_OP_WAIT_UINT, 1u,
           (void*)sizeof(struct timespec), &ts);
  #endif
  __atomic_store_n(&c.is_sleeping, UINT32_C(0), __ATOMIC_SEQ_CST);
#endif
}

// vim: set expandtab ts=2 sw=2 :
