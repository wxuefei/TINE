#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "types.h"

// self explanatory, invoked my HolyC activating an SDL event
void InitSound(void);
// sets frequency, called from HolyC
void SndFreq(u64);
// self explanatory, called from HolyC
f64 GetVolume(void);
// self explanatory, called from HolyC
void SetVolume(f64);

#ifdef __cplusplus
}
#endif

// vim: set expandtab ts=2 sw=2 :
