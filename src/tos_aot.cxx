#include <algorithm>
#include <filesystem>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <inttypes.h>
#include <signal.h>
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

std::unordered_map<std::string, CSymbol> TOSLoader;

namespace {
// This code is mostly copied from TempleOS
// and does not look very C++-y
void LoadOneImport(u8 **src_, u8 *module_base) {
  u8   *src = *src_, *ptr = nullptr;
  char *st_ptr;
  uptr  i     = 0;
  bool  first = true;
  u8    etype;
#define AS(x, T) (*(T *)x) // yuck
  while ((etype = *src++)) {
    ptr = module_base + AS(src, u32);
    src += sizeof(u32);
    st_ptr = (char *)src;
    src += strlen(st_ptr) + 1;
    // First occurance of a string means
    // "repeat this until another name is found"
    if (*st_ptr) {
      if (!first) {
        *src_ = (u8 *)st_ptr - sizeof(u32) - 1;
        return;
      } else {
        first = false;
        decltype(TOSLoader)::iterator it;
        if ((it = TOSLoader.find(st_ptr)) == TOSLoader.end()) {
          fprintf(stderr, "Unresolved reference %p\n", st_ptr);
          TOSLoader.emplace(st_ptr, CSymbol{HTT_IMPORT_SYS_SYM, module_base,
                                            (u8 *)st_ptr - sizeof(u32) - 1});
        } else {
          auto &sym = it->second;
          if (sym.type != HTT_IMPORT_SYS_SYM)
            i = (uptr)sym.val;
        }
      }
    }
    // probably breaks strict aliasing :(
#define OFF(T) ((u8 *)i - ptr - sizeof(T))
    switch (etype) {
    case IET_REL_I8:
      AS(ptr, i8) = OFF(i8);
      break;
    case IET_IMM_U8:
      AS(ptr, u8) = i;
      break;
    case IET_REL_I16:
      AS(ptr, i16) = OFF(i16);
      break;
    case IET_IMM_U16:
      AS(ptr, u16) = i;
      break;
    case IET_REL_I32:
      AS(ptr, i32) = OFF(i32);
      break;
    case IET_IMM_U32:
      AS(ptr, u32) = i;
      break;
    case IET_REL_I64:
      AS(ptr, i64) = OFF(i64);
      break;
    case IET_IMM_I64:
      AS(ptr, i64) = (i64)i;
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
  LoadOneImport(&sym.module_header_entry, sym.module_base);
  sym.type = HTT_INVALID;
}

void LoadPass1(u8 *src, u8 *module_base) {
  u8   *ptr;
  char *st_ptr;
  uptr  i;
  usize cnt;
  u8    etype;
  while ((etype = *src++)) {
    i = AS(src, u32);
    src += sizeof(u32);
    st_ptr = (char *)src;
    src += strlen(st_ptr) + 1;
    switch (etype) {
    case IET_REL32_EXPORT:
    case IET_IMM32_EXPORT:
    case IET_REL64_EXPORT:
    case IET_IMM64_EXPORT:
      if (etype != IET_IMM32_EXPORT && etype != IET_IMM64_EXPORT)
        i += (uptr)module_base; // i gets reset at the
                                // top of the loop so its fine
      TOSLoader.emplace(st_ptr, CSymbol{HTT_EXPORT_SYS_SYM, (u8 *)i});
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
      src = (u8 *)st_ptr - 5;
      LoadOneImport(&src, module_base);
      break;
    // 32bit addrs
    case IET_ABS_ADDR: {
      cnt = i;
      for (usize j = 0; j < cnt; j++) {
        ptr = module_base + AS(src, u32);
        src += sizeof(u32);
        AS(ptr, u32) += (uptr)module_base;
      }
    } break;
      // the other ones wont be used
      // so im not implementing them
    }
  }
}

void LoadPass2(u8 *src, u8 *module_base) {
  char *st_ptr;
  u32   i;
  u8    etype;
  while ((etype = *src++)) {
    i = AS(src, u32);
    src += sizeof(u32);
    st_ptr = (char *)src;
    src += strlen(st_ptr) + 1;
    switch (etype) {
    case IET_MAIN:
      // we use ZERO_BP here for it to not climb
      // up the C++ stack
      FFI_CALL_TOS_0_ZERO_BP(module_base + i);
      break;
    case IET_ABS_ADDR:
      src += sizeof(u32) * i;
      break;
    case IET_CODE_HEAP:
    case IET_ZEROED_CODE_HEAP:
      src += 4 + sizeof(u32) * i;
      break;
    case IET_DATA_HEAP:
    case IET_ZEROED_DATA_HEAP:
      src += 8 + sizeof(u32) * i;
      break;
    }
  }
}

#undef AS

extern "C" struct [[gnu::packed]] CBinFile {
  u16 jmp;
  u8  module_align_bits, reserved /*padding*/;
  union {
    char bin_signature[4];
    u32  sig;
  };
  i64 org, patch_table_offset, file_size;
  u8  data[]; // FAMs are technically illegal in
              // standard c++ but whatever
};

} // namespace

void LoadHCRT(std::string const &name) {
  auto f = fopen(name.c_str(), "rb");
  if (!f) {
    fprintf(stderr, "CANNOT FIND TEMPLEOS BINARY FILE %s\n", name.c_str());
    exit(1);
  }
  std::error_code e;
  auto            sz = fs::file_size(name, e);
  if (e) {
    fprintf(stderr, "CANNOT DETERMINE SIZE OF FILE, ERROR MESSAGE: %s\n",
            e.message().c_str());
    fclose(f);
    exit(1);
  }
  u8 *bfh_addr;
  fread(bfh_addr = VirtAlloc<u8>(sz), 1, sz, f);
  fclose(f);
  // I think this breaks strict aliasing but
  // I dont think it matters because its packed(?)
  auto bfh = (CBinFile *)bfh_addr;
  if (memcmp(bfh->bin_signature, "TOSB" /*BIN_SIGNATURE_VAL*/, 4)) {
    fprintf(stderr, "INVALID TEMPLEOS BINARY FILE %s\n", name.c_str());
    exit(1);
  }
  LoadPass1(bfh_addr + bfh->patch_table_offset, bfh->data);
#ifndef _WIN32
  static void *fp = nullptr;
  if (!fp)
    fp = TOSLoader["__InterruptCoreRoutine"].val;
  signal(SIGUSR2, (void (*)(int))fp);
#endif
  LoadPass2(bfh_addr + bfh->patch_table_offset, bfh->data);
}

void BackTrace() {
  std::string                     last;
  static usize                    sz = 0;
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
    usize idx;
    for (idx = 0; idx < sz; idx++) {
      void *curp = TOSLoader[sorted[idx]].val;
      if (curp == ptr) {
        fprintf(stderr, "%s\n", sorted[idx].c_str());
      } else if (curp > ptr) {
        fprintf(stderr, "%s [%p+%#" PRIx64 "]\n", last.c_str(), ptr,
                (u8 *)ptr - (u8 *)oldp);
        goto next;
      }
      oldp = curp;
      last = sorted[idx];
    }
  next:
    ptr = rbp[1];          // [RBP+0x8] is the return address
    rbp = (void **)rbp[0]; // [RBP] is the previous base pointer
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
  static usize                    sz = 0;
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
  for (usize idx = 0; idx < sz; idx++) {
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
