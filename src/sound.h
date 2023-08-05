#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "types.h"

void InitSound(void);
void SndFreq(u64);
f64  GetVolume(void);
void SetVolume(f64);

#ifdef __cplusplus
}
#endif

// vim: set expandtab ts=2 sw=2 :
