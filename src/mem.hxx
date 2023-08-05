#pragma once

#include "types.h"

void *NewVirtualChunk(usize sz, bool low32);
void  FreeVirtualChunk(void *ptr, usize s);

// vim: set expandtab ts=2 sw=2 :
