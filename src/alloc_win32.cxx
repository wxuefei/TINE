#include "alloc.hxx"

#include <windows.h>
#include <memoryapi.h>
#include <sysinfoapi.h>

#include <limits>

auto NewVirtualChunk(usize sz, bool low32) -> void* {
  static bool  first_run = true;
  static DWORD alloc_granularity;
  if (first_run) {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    alloc_granularity = si.dwAllocationGranularity;
    //
    first_run = false;
  }
  if (low32) { // code heap
    // https://archive.md/ugIUC
    MEMORY_BASIC_INFORMATION mbi{};
    // we initialize alloc with the granularity
    // because nullptr will just give me 0
    // with VirtualQuery so we need to start from
    // a reasonable small value, a good one being the allocation granularity
    uptr alloc = alloc_granularity;
    while (alloc <= std::numeric_limits<u32>::max()) {
      if (!VirtualQuery((void*)alloc, &mbi, sizeof mbi))
        return nullptr;
      alloc = (uptr)mbi.BaseAddress + mbi.RegionSize;
      // clang-format off
      //
      // Fancy code to align to round up to the nearest allocation granularity unit
      // since VirtualAlloc() will round down the address to the nearest
      // granularity unit multiple and mbi.BaseAddress set by VirtualQuery()
      // is aligned to the page boundary which can interlap with an occupied
      // allocation granularity unit
      //
      // This works because dwAllocationGranularity is a power of 2
      // (so it has only 1 bit that's 1, eg 0b10000)
      //
      // lets say we want to align to 0b100(an arbitrary dwAllocationGranularity)
      //   0b1001
      // + 0b0011 <- (0b100 - 1), flips bottom bits
      // ----------
      //   0b1110
      // & 0b1100 <- ~(0b100 - 1), flipped
      // ----------
      //   0b1100 <- rounded up aligned value
      //
      // It'll be the same if it's already aligned
      //
      // clang-format on
      uptr addr = ((uptr)mbi.BaseAddress + alloc_granularity - 1) &
                  ~(alloc_granularity - 1);
      if (mbi.State & MEM_FREE && sz <= alloc - addr)
        return VirtualAlloc((void*)addr, sz, MEM_RESERVE | MEM_COMMIT,
                            PAGE_EXECUTE_READWRITE);
    }
    return nullptr;
  } else // data heap
    return VirtualAlloc(nullptr, sz, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
  // VirtualAlloc returns null on error
}

void FreeVirtualChunk(void* ptr, usize) {
  VirtualFree(ptr, 0, MEM_RELEASE);
}

// vim: set expandtab ts=2 sw=2 :
