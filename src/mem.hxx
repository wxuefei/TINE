#pragma once

#include "types.h"

auto NewVirtualChunk(usize sz, bool low32) -> void*;
void FreeVirtualChunk(void* ptr, usize s);

// vim: set expandtab ts=2 sw=2 :
