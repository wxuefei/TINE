#pragma once

#include "types.h"

#include <vector>

// elapsed milliseconds since an unspecified time
auto GetTicks() -> u64;

// Set calling thread's CTask
void SetFs(void*);
// Get calling thread's CTask
auto GetFs() -> void*;

// Set calling thread's CCPU
void SetGs(void*);
// Get calling thread's CCPU
auto GetGs() -> void*;

// Get calling thread's core number
auto CoreNum() -> usize;

// Set rip to an interrupt routine
void InterruptCore(usize core);

// Wait til Core 0 dies
void WaitForCore0();

using HolyFP = void*;
// Launch Core, fps: the function pointers
// the thread will run on launch
void CreateCore(usize core, std::vector<HolyFP>&& fps);

// Wake up core number `core`
void AwakeCore(usize core);

// Sleep for us microseconds
void SleepHP(u64 us);

// vim: set expandtab ts=2 sw=2 :
