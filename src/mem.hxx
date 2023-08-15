#pragma once

#include "types.h"

// New virtual memory region, low32 indicates whether
// its executable or not
auto NewVirtualChunk(usize sz, bool low32) -> void*;
// Frees virtual memory region
void FreeVirtualChunk(void* ptr, usize s);

// vim: set expandtab ts=2 sw=2 :
