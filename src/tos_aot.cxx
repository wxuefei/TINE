#include <algorithm>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <inttypes.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <tos_ffi.h>

#include "alloc.hxx"
#include "dbg.hxx"
#include "mem.hxx"
#include "tos_aot.hxx"

namespace fs = std::filesystem;

using std::ios;

std::unordered_map<std::string, CHash> TOSLoader;

namespace {
// This code is mostly copied from TempleOS
// and does not look very C++-y
void LoadOneImport(char **src_, char *mod_base) {
  char     *src = *src_, *ptr = nullptr, *st_ptr;
  uintptr_t i     = 0;
  bool      first = true;
  uint8_t   etype;
#define AS(x, T) (*(T *)x) // yuck
  while ((etype = *src++)) {
    ptr = mod_base + AS(src, uint32_t);
    src += sizeof(uint32_t);
    st_ptr = src;
    src += strlen(st_ptr) + 1;
    // First occurance of a string means
    // "repeat this until another name is found"
    if (*st_ptr) {
      if (!first) {
        *src_ = st_ptr - sizeof(uint32_t) - 1;
        return;
      } else {
        first = false;
        decltype(TOSLoader)::iterator it;
        if ((it = TOSLoader.find(st_ptr)) == TOSLoader.end()) {
          fprintf(stderr, "Unresolved reference %p\n", st_ptr);
          CHash tmpiss;
          tmpiss.type             = HTT_IMPORT_SYS_SYM;
          tmpiss.mod_header_entry = st_ptr - sizeof(uint32_t) - 1;
          tmpiss.mod_base         = mod_base;
          TOSLoader[st_ptr]       = tmpiss;
        } else {
          auto &tmp_sym = it->second;
          if (tmp_sym.type != HTT_IMPORT_SYS_SYM)
            i = (uintptr_t)tmp_sym.val;
        }
      }
    }
    // probably breaks strict aliasing :(
#define OFF(T) ((char *)i - ptr - sizeof(T))
    switch (etype) {
    case IET_REL_I8:
      AS(ptr, int8_t) = OFF(int8_t);
      break;
    case IET_IMM_U8:
      AS(ptr, uint8_t) = i;
      break;
    case IET_REL_I16:
      AS(ptr, int16_t) = OFF(int16_t);
      break;
    case IET_IMM_U16:
      AS(ptr, uint16_t) = i;
      break;
    case IET_REL_I32:
      AS(ptr, int32_t) = OFF(int32_t);
      break;
    case IET_IMM_U32:
      AS(ptr, uint32_t) = i;
      break;
    case IET_REL_I64:
      AS(ptr, int64_t) = OFF(int64_t);
      break;
    case IET_IMM_I64:
      AS(ptr, int64_t) = (int64_t)i;
      break;
    }
#undef OFF
  }
  *src_ = src - 1;
}

void SysSymImportsResolve(char *st_ptr) {
  decltype(TOSLoader)::iterator it;
  if ((it = TOSLoader.find(st_ptr)) == TOSLoader.end())
    return;
  auto &sym = it->second;
  if (sym.type != HTT_IMPORT_SYS_SYM)
    return;
  LoadOneImport(&sym.mod_header_entry, sym.mod_base);
  sym.type = HTT_INVALID;
}

void LoadPass1(char *src, char *mod_base) {
  char     *ptr, *st_ptr;
  uintptr_t i;
  size_t    cnt;
  uint8_t   etype;
  CHash     tmpex;
  while ((etype = *src++)) {
    i = AS(src, uint32_t);
    src += sizeof(uint32_t);
    st_ptr = src;
    src += strlen(st_ptr) + 1;
    switch (etype) {
    case IET_REL32_EXPORT:
    case IET_IMM32_EXPORT:
    case IET_REL64_EXPORT:
    case IET_IMM64_EXPORT:
      tmpex.type = HTT_EXPORT_SYS_SYM;
      tmpex.val  = (uint8_t *)i;
      if (etype != IET_IMM32_EXPORT && etype != IET_IMM64_EXPORT)
        tmpex.val += (ptrdiff_t)mod_base;
      TOSLoader[st_ptr] = tmpex;
      SysSymImportsResolve(st_ptr);
      break;
    case IET_REL_I0:
    case IET_IMM_U0:
    case IET_REL_I8:
    case IET_IMM_U8:
    case IET_REL_I16:
    case IET_IMM_U16:
    case IET_REL_I32:
    case IET_IMM_U32:
    case IET_REL_I64:
    case IET_IMM_I64:
      src = st_ptr - 5;
      LoadOneImport(&src, mod_base);
      break;
    // 32bit addrs
    case IET_ABS_ADDR: {
      cnt = i;
      for (size_t j = 0; j < cnt; j++) {
        ptr = mod_base + AS(src, uint32_t);
        src += sizeof(uint32_t);
        AS(ptr, uint32_t) += (uintptr_t)mod_base;
      }
    } break;
      // the other ones wont be used
      // so im not implementing them
    }
  }
}

void LoadPass2(char *src, char *mod_base) {
  char    *st_ptr;
  uint32_t i;
  uint8_t  etype;
  while ((etype = *src++)) {
    i = AS(src, uint32_t);
    src += sizeof(uint32_t);
    st_ptr = src;
    src += strlen(st_ptr) + 1;
    switch (etype) {
    case IET_MAIN:
      FFI_CALL_TOS_0_ZERO_BP(mod_base + i);
      break;
    case IET_ABS_ADDR:
      src += sizeof(uint32_t) * i;
      break;
    case IET_CODE_HEAP:
    case IET_ZEROED_CODE_HEAP:
      src += 4 + sizeof(uint32_t) * i;
      break;
    case IET_DATA_HEAP:
    case IET_ZEROED_DATA_HEAP:
      src += 8 + sizeof(uint32_t) * i;
      break;
    }
  }
}

#undef AS

extern "C" struct [[gnu::packed]] CBinFile {
  uint16_t jmp;
  uint8_t  mod_align_bits, pad;
  union {
    char     bin_signature[4];
    uint32_t sig;
  };
  int64_t org, patch_table_offset, file_size;
  char    data[]; // FAMs are technically illegal in
                  // standard c++ but whatever
};

} // namespace

void LoadHCRT(std::string const &name) {
  FILE *f = fopen(name.c_str(), "rb");
  if (f == nullptr) {
    fprintf(stderr, "CANNOT FIND TEMPLEOS BINARY FILE %s\n", name.c_str());
    exit(1);
  }
  char *bfh_addr;
  auto  sz = fs::file_size(name);
  fread(bfh_addr = VirtAlloc<char>(sz), 1, sz, f);
  fclose(f);
  // I think this breaks strict aliasing but
  // I dont think it matters because its packed(?)
  auto bfh = (CBinFile *)bfh_addr;
  if (std::string_view{bfh->bin_signature, 4} != "TOSB") {
    fprintf(stderr, "INVALID TEMPLEOS BINARY FILE %s\n", name.c_str());
    exit(1);
  }
  LoadPass1(bfh_addr + bfh->patch_table_offset, bfh->data);
#ifndef _WIN32
  static void *fp = nullptr;
  if (fp == nullptr)
    fp = TOSLoader["__InterruptCoreRoutine"].val;
  signal(SIGUSR2, (void (*)(int))fp);
#endif
  LoadPass2(bfh_addr + bfh->patch_table_offset, bfh->data);
}

void BackTrace() {
  std::string                     last;
  static size_t                   sz = 0;
  static std::vector<std::string> sorted;
  static bool                     init = false;
  if (!init) {
    for (auto const &e : TOSLoader)
      sorted.emplace_back(e.first);
    sz = sorted.size();
    std::sort(sorted.begin(), sorted.end(), [](auto const &a, auto const &b) {
      return TOSLoader[a].val < TOSLoader[b].val;
    });
    init = true;
  }
  putchar('\n');
  auto  rbp = (void **)__builtin_frame_address(0);
  void *oldp;
  // its 1 because we want to know the return
  // addr of BackTrace()'s caller
  void *ptr = __builtin_return_address(1);
  while (rbp) {
    oldp = nullptr;
    last = "UNKOWN";
    size_t idx;
    for (idx = 0; idx < sz; idx++) {
      void *curp = TOSLoader[sorted[idx]].val;
      if (curp == ptr) {
        fprintf(stderr, "%s\n", sorted[idx].c_str());
      } else if (curp > ptr) {
        fprintf(stderr, "%s [%p+%#" PRIx64 "]\n", last.c_str(), ptr,
                (char *)ptr - (char *)oldp);
        goto next;
      }
      oldp = curp;
      last = sorted[idx];
    }
  next:
    ptr = rbp[1];
    rbp = (void **)*rbp; // yuck
  }
  putchar('\n');
}

// who the fuck cares about memory leaks
// its gonna be executed once or twice in
// the entire debug session not to mention
// WhichFun() wont even be called in normal
// circumstances
#define STR_DUP(s) (strcpy(new (std::nothrow) char[s.size() + 1], s.c_str()))

// great when you use lldb and get a fault
// (lldb) p (char*)WhichFun($pc)
[[gnu::used, gnu::visibility("default")]] char *WhichFun(void *ptr) {
  std::string                     last;
  static size_t                   sz = 0;
  static std::vector<std::string> sorted;
  static bool                     init = false;
  if (!init) {
    for (auto const &e : TOSLoader)
      sorted.emplace_back(e.first);
    sz = sorted.size();
    std::sort(sorted.begin(), sorted.end(), [](auto const &a, auto const &b) {
      return TOSLoader[a].val < TOSLoader[b].val;
    });
    init = true;
  }
  for (size_t idx = 0; idx < sz; idx++) {
    void *curp = TOSLoader[sorted[idx]].val;
    if (curp == ptr) {
      fprintf(stderr, "%s\n", sorted[idx].c_str());
    } else if (curp > ptr) {
      return STR_DUP(last);
    }
    last = sorted[idx];
  }
  return STR_DUP(last);
}

// vim: set expandtab ts=2 sw=2 :
