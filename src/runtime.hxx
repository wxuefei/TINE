#pragma once

#include <string_view>

#include "types.h"

void             *HolyMAlloc(usize sz);
void             *HolyCAlloc(usize sz);
void              HolyFree(void *p);
[[noreturn]] void HolyThrow(std::string_view sv = {});
char             *HolyStrDup(char const *s);
void              BootstrapLoader();

// vim: set expandtab ts=2 sw=2 :
