#pragma once

#include <string_view>

#include "types.h"

void             *HolyMAlloc(usize sz);
void             *HolyCAlloc(usize sz);
void              HolyFree(void *p);
[[noreturn]] void HolyThrow(std::string_view sv = {});
char             *HolyStrDup(char const *s);
void              BootstrapLoader();
u64               mp_cnt(void *);

// vim: set expandtab ts=2 sw=2 :
