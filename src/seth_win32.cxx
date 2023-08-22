#include "dbg.hxx"
#include "main.hxx"
#include "seth.hxx"
#include "tos_aot.hxx"
#include "vfs.hxx"

#include <windows.h>
#include <processthreadsapi.h>
#include <synchapi.h>
#include <timeapi.h>

#include <thread>
#include <utility>
#include <vector>

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include <tos_callconv.h>

namespace {

struct CCore {
  HANDLE thread, event, mtx;
  // When are we going to wake up at?
  // winmm callback checks for this constantly
  // check out GetTicksHP() for an explanation
  // 0 means it's not sleeping
  u64 awake_at;
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

auto WINAPI ThreadRoutine(LPVOID arg) -> DWORD {
  self = static_cast<CCore*>(arg);
  VFsThrdInit();
  SetupDebugger();
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
  return 0;
}

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
  auto&   c = cores[core];
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
}

void CreateCore(usize n, std::vector<void*>&& fps) {
  using std::thread;
  if (n == 0) // boot
    cores.resize(thread::hardware_concurrency());
  auto& c = cores[n];
  // CoreAPSethTask(...) passed from SpawnCore or
  // IET_MAIN function pointers+kernel entry point from LoadHCRT
  c.fps      = std::move(fps);
  c.core_num = n;
  c.thread   = CreateThread(nullptr, 0, ThreadRoutine, &c, 0, nullptr);
  c.mtx      = CreateMutex(nullptr, FALSE, nullptr);
  c.event    = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  SetThreadPriority(c.thread, THREAD_PRIORITY_HIGHEST);
  // I wanted to set thread names but literally wtf,
  // also gcc doesnt support it so whatever
  // https://archive.li/MIMDo
}

void AwakeCore(usize core) {
  auto& c = cores[core];
  WaitForSingleObject(c.mtx, INFINITE);
  c.awake_at = 0;
  SetEvent(c.event);
  ReleaseMutex(c.mtx);
}

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

void SleepHP(u64 us) {
  auto&      c = cores[CoreNum()];
  auto const t = GetTicksHP();
  WaitForSingleObject(c.mtx, INFINITE);
  // windows doesnt have accurate microsecond sleep :(
  c.awake_at = t + us / 1000;
  ReleaseMutex(c.mtx);
  WaitForSingleObject(c.event, INFINITE);
}

// vim: set expandtab ts=2 sw=2 :
