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
  #include "signal_types.hxx"
  #include <pthread.h>
#endif

#include <atomic>
#include <vector>

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include <tos_ffi.h>

#include "dbg.hxx"
#include "main.hxx"
#include "multic.hxx"
#include "tos_aot.hxx"
#include "vfs.hxx"

auto GetTicks() -> u64 {
#ifdef _WIN32
  return GetTickCount();
#else
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (u64)ts.tv_nsec / UINT64_C(1000000) //
       + UINT64_C(1000) * (u64)ts.tv_sec;
#endif
}

namespace {

struct CCore {
#ifdef _WIN32
  HANDLE thread;
  HANDLE event;
  HANDLE mtx;
  u64    awake_at;
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
  alignas(4) u32 is_sleeping;
  // not using atomics here(instead using atomic builtins that operate on plain
  // values) because i need them for system calls and casting std::atomic<T>* to
  // T* is potentially UB and
  // static_assert(std::is_layout_compatible_v<std::atomic<u32>, u32>)
  // failed on my machine
#endif
  void* fp;
};

std::vector<CCore> cores;
thread_local usize core_num;

#ifndef _WIN32
auto LaunchCore(void* c) -> void* {
#else
auto WINAPI LaunchCore(LPVOID c) -> DWORD {
#endif
  VFsThrdInit();
  SetupDebugger();
  core_num = (uptr)c;
#ifndef _WIN32
  static void* fp = nullptr;
  if (!fp)
    fp = TOSLoader["__InterruptCoreRoutine"].val;
  signal(SIGUSR2, (SignalCallback*)fp);
  signal(SIGUSR1, [](int) {
    pthread_exit(nullptr);
  });
#endif
  // CoreAPSethTask(...) (T/FULL_PACKAGE.HC)
  // ZERO_BP so the return addr&rbp is 0 and
  // stack traces don't climb up the C++ stack
  FFI_CALL_TOS_0_ZERO_BP(cores[core_num].fp);
#ifdef _WIN32
  return 0;
#else
  return nullptr;
#endif
}

/*
 * (DolDoc code)
 * $ID,-2$$TR-C,"How do you use the FS and GS segment registers."$
 * $ID,2$$FG,2$MOV RAX,FS:[RAX]$FG$ : FS can be set with a $FG,2$WRMSR$FG$, but
 * displacement is RIP relative, so it's tricky to use.  FS is used for the
 * current $LK,"CTask",A="MN:CTask"$, GS for $LK,"CCPU",A="MN:CCPU"$.
 *
 * Note on Fs and Gs: They might seem like very weird names for ThisTask and
 * ThisCPU repectively but it's because they are stored in the F Segment and G
 * Segment registers. (https://archive.md/pf2td)
 */
thread_local std::atomic<void*> Fs = nullptr;
thread_local std::atomic<void*> Gs = nullptr;

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
  return core_num;
}

// this may look like bad code but HolyC cannot switch
// contexts unless you call Yield() in a loop so
// we have to set RIP manually(this routine is called
// when CTRL+ALT+C is pressed inside TempleOS
void InterruptCore(usize core) {
#ifdef _WIN32
  CONTEXT ctx{};
  ctx.ContextFlags = CONTEXT_FULL;
  SuspendThread(cores[core].thread);
  GetThreadContext(cores[core].thread, &ctx);
  // push rip
  ctx.Rsp -= 8;
  ((DWORD64*)ctx.Rsp)[0] = ctx.Rip;
  //
  static void* fp = nullptr;
  if (!fp)
    fp = TOSLoader["__InterruptCoreRoutine"].val;
  // movabs rip, <fp>
  ctx.Rip = (uptr)fp;
  SetThreadContext(cores[core].thread, &ctx);
  ResumeThread(cores[core].thread);
#else
  pthread_kill(cores[core].thread, SIGUSR2);
#endif
}

void LaunchCore0(ThreadCallback* fp) {
  cores.resize(proc_cnt);
  cores[0].fp = nullptr;
#ifdef _WIN32
  cores[0].thread = CreateThread(nullptr, 0, fp, nullptr, 0, nullptr);
  cores[0].mtx    = CreateMutex(nullptr, FALSE, nullptr);
  cores[0].event  = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  SetThreadPriority(cores[0].thread, THREAD_PRIORITY_HIGHEST);
  // im not going to use SEH or some crazy bulllshit to set the
  // thread name on windows(https://archive.md/9jiD5)
#else
  pthread_create(&cores[0].thread, nullptr, fp, nullptr);
  pthread_setname_np(cores[0].thread, "Seth(Core0)");
#endif
}

void CreateCore(usize core, void* fp) {
  // CoreAPSethTask(...) passed from SpawnCore
  cores[core].fp = fp;
  auto core_n    = (void*)core;
#ifdef _WIN32
  cores[core].thread = CreateThread(nullptr, 0, LaunchCore, core_n, 0, nullptr);
  cores[core].mtx    = CreateMutex(nullptr, FALSE, nullptr);
  cores[core].event  = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  SetThreadPriority(cores[core].thread, THREAD_PRIORITY_HIGHEST);
#else
  pthread_create(&cores[core].thread, nullptr, LaunchCore, core_n);
  char buf[16]{};
  snprintf(buf, sizeof buf, "Seth(Core%" PRIu64 ")", core);
  pthread_setname_np(cores[core].thread, buf);
#endif
}

void WaitForCore0() {
#ifdef _WIN32
  WaitForSingleObject(cores[0].thread, INFINITE);
#else
  pthread_join(cores[0].thread, nullptr);
#endif
}

void ShutdownCore(usize core) {
#ifdef _WIN32
  TerminateThread(cores[core].thread, 0);
#else
  // you actually cant terminate a thread from core 0
  // with pthreads, you need some signal handler
  pthread_kill(cores[core].thread, SIGUSR1);
  pthread_join(cores[core].thread, nullptr);
#endif
}

void ShutdownCores(int ec) {
  for (usize c = 0; c < proc_cnt; ++c)
    if (c != core_num)
      ShutdownCore(c);
  // on Windows this might not fully "close"
  // the application so we must issue an
  // ExitProcess() after this
  exit(ec);
}

void AwakeFromSleeping(usize core) {
#ifdef _WIN32
  WaitForSingleObject(cores[core].mtx, INFINITE);
  cores[core].awake_at = 0;
  SetEvent(cores[core].event);
  ReleaseMutex(cores[core].mtx);
#else
  u32 old = 1;
  __atomic_compare_exchange_n(&cores[core].is_sleeping, &old, 0u, false,
                              __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
  #ifdef __linux__
  syscall(SYS_futex, &cores[core].is_sleeping, FUTEX_WAKE, 1u, nullptr, nullptr,
          0);
  #elif defined(__FreeBSD__)
  _umtx_op(&cores[core].is_sleeping, UMTX_OP_WAKE, 1u, nullptr, nullptr);
  #endif
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
#ifdef _WIN32
  auto const s = GetTicksHP();
  WaitForSingleObject(cores[core_num].mtx, INFINITE);
  cores[core_num].awake_at = s + us / 1000;
  ReleaseMutex(cores[core_num].mtx);
  WaitForSingleObject(cores[core_num].event, INFINITE);
#else
  struct timespec ts {};
  ts.tv_nsec = us * 1000;
  __atomic_store_n(&cores[core_num].is_sleeping, 1u, __ATOMIC_SEQ_CST);
  #ifdef __linux__
  syscall(SYS_futex, &cores[core_num].is_sleeping, FUTEX_WAIT, 1u, &ts, nullptr,
          0);
  #elif defined(__FreeBSD__)
  _umtx_op(&cores[core_num].is_sleeping, UMTX_OP_WAIT_UINT, 1u,
           (void*)sizeof(struct timespec), &ts);
  #endif
#endif
}

// vim: set expandtab ts=2 sw=2 :
