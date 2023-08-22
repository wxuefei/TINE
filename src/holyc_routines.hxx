#pragma once

#include "types.h"

#include <string_view>

// use MAlloc() from C++
auto HolyMAlloc(usize sz) -> void*;
// use CAlloc() from C++
auto HolyCAlloc(usize sz) -> void*;
// use Free() from C++
void HolyFree(void* p);
// use throw() from C++
[[noreturn]] void HolyThrow(std::string_view sv = {});
// use StrNew() from C++
auto HolyStrDup(char const* s) -> char*;

// vim: set expandtab ts=2 sw=2 :
