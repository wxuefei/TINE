#pragma once

#include "mem.hxx"
#include "runtime.hxx"
#include "types.h"

// sizeof(void) is _TECHNICALLY_ illegal in standard C++
// so I'm going to do some template fuckery just to be safe
// in case GCC deprecates the sizeof(void) == 1 behavior
namespace detail {
template <class T> struct size_of {
  static constexpr usize sz = sizeof(T);
};
template <> struct size_of<void> : size_of<u8> {};
} // namespace detail

// Allocates memory in the TempleOS data heap
// in the default CTask of whatever RIP is on
template <class T = void, bool fill = false>
[[gnu::always_inline]] inline auto HolyAlloc(usize sz) -> T* {
  using namespace detail;
  // I demand a constexpr ternary right now
  if constexpr (fill)
    return static_cast<T*>(HolyCAlloc(size_of<T>::sz * sz));
  else
    return static_cast<T*>(HolyMAlloc(size_of<T>::sz * sz));
}

// Allocates a chunk of virtual memory(not linked to the TempleOS heap)
// use with caution as its executable by default
template <class T = void, bool exec = true>
[[gnu::always_inline]] inline auto VirtAlloc(usize sz) -> T* {
  using namespace detail;
  return static_cast<T*>(NewVirtualChunk(size_of<T>::sz * sz, exec));
}

// vim: set expandtab ts=2 sw=2 :
