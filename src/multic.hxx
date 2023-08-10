#pragma once

#include "types.h"

auto GetTicks() -> u64;

auto GetFs() -> void*;
void SetFs(void*);

auto GetGs() -> void*;
void SetGs(void*);

auto CoreNum() -> usize;

void InterruptCore(usize core);

using ThreadCallback =
#ifdef _WIN32
    auto __stdcall(void*) -> long unsigned /*DWORD on x86_64*/;
#else
    auto(void*) -> void*;
#endif
void LaunchCore0(ThreadCallback* fp);
void WaitForCore0();

void CreateCore(usize core, void* fp);
void ShutdownCore(usize core);
void ShutdownCores(int ec);

void AwakeFromSleeping(usize core);

void SleepHP(u64 us);

// vim: set expandtab ts=2 sw=2 :
