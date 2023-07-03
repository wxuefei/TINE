#pragma once

#include <stddef.h>
#include <stdint.h>

void* HolyMAlloc(size_t sz);
void* HolyCAlloc(size_t sz);
template <class T = void, bool fill = false>
__attribute__((always_inline)) inline T* HolyAlloc(size_t sz) {
  // I demand a constexpr ternary right now
  if constexpr (fill)
    return static_cast<T*>(HolyCAlloc(alignof(T) * sz));
  else
    return static_cast<T*>(HolyMAlloc(alignof(T) * sz));
}
void HolyFree(void* p);
char* HolyStrDup(char const* s);
void RegisterFuncPtrs();
uint64_t mp_cnt(int64_t*);
