#pragma once

#include <string_view>

#include "types.h"

auto              HolyMAlloc(usize sz) -> void*;
auto              HolyCAlloc(usize sz) -> void*;
void              HolyFree(void* p);
[[noreturn]] void HolyThrow(std::string_view sv = {});
auto              HolyStrDup(char const* s) -> char*;
void              BootstrapLoader();

// vim: set expandtab ts=2 sw=2 :
