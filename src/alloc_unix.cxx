#include "alloc.hxx"

#ifndef _GNU_SOURCE
  // for ::getline()
  #define _GNU_SOURCE
#endif
#include <sys/mman.h>
#include <unistd.h>

#include <limits>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

namespace {
[[gnu::used]] inline auto Hex2U64(char* ptr, char** res) -> u64 {
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

usize page_size;
} // namespace

auto NewVirtualChunk(usize sz, bool low32) -> void* {
  static bool first_run = true;
  if (first_run) {
    page_size = sysconf(_SC_PAGESIZE);
    first_run = false;
  }
  // page_size is a power of 2 so this works
  usize padded_sz = (sz + page_size - 1) & ~(page_size - 1);
  void* ret;
  if (low32) { // code heap
    ret = mmap(nullptr, padded_sz, PROT_EXEC | PROT_WRITE | PROT_READ,
               MAP_PRIVATE | MAP_ANON | MAP_32BIT, -1, 0);
#ifdef __linux__ // parse /proc/maps for extra pages that mmap missed
    if (ret == MAP_FAILED) {
      // side note: Linux doesn't seem to like allocating stuff below 31 bits
      // (<0x40000000). I don't know why so technically we have 1GB less space
      // for the code heap than on Windows or maybe FreeBSD(I don't have it
      // installed) but it won't really matter since machine code doesn't take
      // up a lot of space
      char* buffer  = nullptr;
      usize line_sz = 0;
      uptr  down    = 0;
      auto  map     = fopen("/proc/self/maps", "rb"); // assume its always there
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
      }
    found:
      free(buffer);
      fclose(map);
      if (down > std::numeric_limits<u32>::max())
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
}

void FreeVirtualChunk(void* ptr, usize sz) {
  usize padded_sz = (sz + page_size - 1) & ~(page_size - 1);
  munmap(ptr, padded_sz);
}

// vim: set expandtab ts=2 sw=2 :
