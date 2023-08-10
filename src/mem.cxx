#ifdef _WIN32
  #include <windows.h>
  #include <memoryapi.h>
extern DWORD dwAllocationGranularity;
#else
  #ifdef __linux__
    #ifndef _GNU_SOURCE
      // for ::getline()
      #define _GNU_SOURCE
    #endif
    #include <stdio.h>
    #include <stdlib.h>
  #endif
  #include <sys/mman.h>
#endif

#include <array>

#include <ctype.h>

#include "main.hxx"
#include "mem.hxx"

#ifdef __linux__
static inline auto Hex2U64(char* ptr, char** res) -> u64 {
  u64  ret = 0;
  char c;
  while (isxdigit(c = *ptr)) {
    ret <<= 4;
    ret |= isalpha(c) ? toupper(c) - 'A' + 10 : c - '0';
    ++ptr;
  }
  *res = ptr;
  return ret;
}
#endif

auto NewVirtualChunk(usize sz, bool low32) -> void* {
#ifndef _WIN32
  // explanation of (x+y-1)&~(y-1) on the bottom windows code
  // page_size is a power of 2 so this works
  usize padded_sz = (sz + page_size - 1) & ~(page_size - 1);
  void* ret;
  if (low32) { // code heap
    ret = mmap(nullptr, padded_sz, PROT_EXEC | PROT_WRITE | PROT_READ,
               MAP_PRIVATE | MAP_ANON | MAP_32BIT, -1, 0);
  #ifdef __linux__
    if (ret == MAP_FAILED) {
      // side note: Linux doesn't seem to like allocating stuff below 31 bits
      // (<0x40000000). I don't know why so technically we have 1GB less space
      // for the code heap than on Windows or maybe FreeBSD(I don't have it
      // installed) but it won't really matter since machine code doesn't take
      // up a lot of space
      std::array<char, 0x1000> buf;
      // we make a generous assumption that one line would not exceed
      // 4096 chars, even 0x100 would have done the job
      char* buffer  = buf.data();
      usize line_sz = buf.size();
      uptr  down    = 0;
      auto  map = fopen("/proc/self/maps", "rb"); // assumes its always there
      // just fs::file_size() wont work lmao
      while (::getline(&buffer, &line_sz, map) > 0) { // NOT std::getline()
        char* ptr   = buffer;
        u64   lower = Hex2U64(ptr, &ptr);
        // MAP_FIXED wants us to align `down` to the page size
        down = (down + page_size - 1) & ~(page_size - 1);
        // basically finds a gap between the previous line's upper address
        // and the current line's lower address so it can allocate there
        if (lower - down >= padded_sz && lower > down)
          goto found;
        // ignore '-'
        // cat /proc/self/maps for an explanation
        ++ptr;
        u64 upper = Hex2U64(ptr, &ptr);
        down      = upper;
        line_sz   = buf.size();
      }
    found:
      fclose(map);
      if (down > UINT64_C(0xFFffFFff))
        return nullptr;
      ret = mmap((void*)down, padded_sz, PROT_EXEC | PROT_WRITE | PROT_READ,
                 MAP_PRIVATE | MAP_ANON | MAP_FIXED, -1, 0);
    } else
      return ret;
  #endif
  } else // data heap
    ret = mmap(nullptr, padded_sz, PROT_WRITE | PROT_READ,
               MAP_PRIVATE | MAP_ANON, -1, 0);
  if (ret == MAP_FAILED)
    return nullptr;
  return ret;
#else
  if (low32) { // code heap
    // https://archive.md/ugIUC
    MEMORY_BASIC_INFORMATION mbi{};
    // we initialize alloc with the granularity because NULL
    // will fail with VirtualQuery so we need to start
    // from a reasonable small value
    uptr alloc = dwAllocationGranularity, addr;
    while (alloc <= UINT64_C(0xFFffFFff)) {
      if (0 == VirtualQuery((void*)alloc, &mbi, sizeof mbi))
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
      addr = ((uptr)mbi.BaseAddress + dwAllocationGranularity - 1) &
             ~(dwAllocationGranularity - 1);
      if (mbi.State & MEM_FREE && sz <= alloc - addr)
        return VirtualAlloc((void*)addr, sz, MEM_COMMIT | MEM_RESERVE,
                            PAGE_EXECUTE_READWRITE);
    }
    return nullptr;
  } else // data heap
    return VirtualAlloc(nullptr, sz, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    // VirtualAlloc returns null on error
#endif
}

void FreeVirtualChunk(void* ptr, [[maybe_unused]] usize sz) {
#ifdef _WIN32
  VirtualFree(ptr, 0, MEM_RELEASE);
#else
  usize padded_sz = (sz + page_size - 1) & ~(page_size - 1);
  munmap(ptr, padded_sz);
#endif
}

// vim: set expandtab ts=2 sw=2 :
