#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "types.h"

// self explanatory
void InitSound(void);
// sets frequency
void SndFreq(u64);
// self explanatory
f64 GetVolume(void);
// self explanatory
void SetVolume(f64);

#ifdef __cplusplus
}
#endif

// vim: set expandtab ts=2 sw=2 :
