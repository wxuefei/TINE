#include "holyc_routines.hxx"
#include "alloc.hxx"
#include "tos_aot.hxx"

#include <algorithm>
#include <string_view>

#include <string.h>

#include <tos_callconv.h>

void HolyFree(void* ptr) {
  static void* fptr = nullptr;
  if (!fptr)
    fptr = TOSLoader["_FREE"].val;
  FFI_CALL_TOS_1(fptr, (uptr)ptr);
}

auto HolyMAlloc(usize sz) -> void* {
  static void* fptr = nullptr;
  if (!fptr)
    fptr = TOSLoader["_MALLOC"].val;
  return (void*)FFI_CALL_TOS_2(fptr, sz, 0 /*NULL*/);
}

auto HolyCAlloc(usize sz) -> void* {
  return memset(HolyAlloc<u8>(sz), 0, sz);
}

auto HolyStrDup(char const* str) -> char* {
  return strcpy(HolyAlloc<char>(strlen(str) + 1), str);
}

[[noreturn]] void HolyThrow(std::string_view sv) {
  union {
    char s[8];
    u64  i = 0;
  } u; // mov QWORD PTR[&u],0
  static void* fp = nullptr;
  if (!fp)
    fp = TOSLoader["throw"].val;
  memcpy(u.s, sv.data(), std::min<usize>(sv.size(), 8));
  FFI_CALL_TOS_1(fp, u.i);
  __builtin_unreachable();
}
