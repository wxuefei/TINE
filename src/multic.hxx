#pragma once

#include "types.h"

u64 GetTicks();

void *GetFs();
void  SetFs(void *);

void *GetGs();
void  SetGs(void *);

usize CoreNum();

void InterruptCore(usize core);

using ThreadCallback =
#ifdef _WIN32
    long unsigned /*DWORD on x86_64*/ __stdcall(void *);
#else
    void *(void *);
#endif
void LaunchCore0(ThreadCallback *fp);
void WaitForCore0();

void CreateCore(usize core, void *fp);
void ShutdownCore(usize core);
void ShutdownCores(int ec);

void AwakeFromSleeping(usize core);

void SleepHP(u64 us);

// vim: set expandtab ts=2 sw=2 :
