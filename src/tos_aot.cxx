#include <algorithm>
#include <filesystem>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <inttypes.h>
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
void LoadOneImport(u8** src_, u8* module_base) {
  u8* __restrict src = *src_;
  u8*   ptr          = nullptr;
  char* st_ptr;
  uptr  i     = 0;
  bool  first = true;
  u8    etype;
  // i know this is a GNU extension, problem?
  // this won't actually call memcpy
  // anyway(compiles down to a mov call)
  // so it respects strict aliasing
  // while not compromising on speed
#define READ_NUM(x, T)          \
  ({                            \
    T ret;                      \
    memcpy(&ret, x, sizeof(T)); \
    ret;                        \
  })
  while ((etype = *src++)) {
    ptr = module_base + READ_NUM(src, u32);
    src += sizeof(u32);
    st_ptr = (char*)src;
    src += strlen(st_ptr) + 1;
    // First occurance of a string means
    // "repeat this until another name is found"
    if (*st_ptr) {
      if (!first) {
        *src_ = (u8*)st_ptr - sizeof(u32) - 1;
        return;
      } else {
        first   = false;
        auto it = TOSLoader.find(st_ptr);
        if (it == TOSLoader.end()) {
          fprintf(stderr, "Unresolved reference %s\n", st_ptr);
          TOSLoader.try_emplace(st_ptr, //
                                /*CSymbol*/ HTT_IMPORT_SYS_SYM, module_base,
                                (u8*)st_ptr - sizeof(u32) - 1);
        } else {
          auto const& [_, sym] = *it;
          if (sym.type != HTT_IMPORT_SYS_SYM)
            i = (uptr)sym.val;
        }
      }
    }
#define OFF(T) ((u8*)i - ptr - sizeof(T))
// same stuff to respect strict aliasing
#define REL(T)                    \
  {                               \
    usize off = OFF(T);           \
    memcpy(ptr, &off, sizeof(T)); \
  }
#define IMM(T) \
  { memcpy(ptr, &i, sizeof(T)); }
    switch (etype) {
    case IET_REL_I8:
      REL(i8);
      break;
    case IET_REL_I16:
      REL(i16);
      break;
    case IET_REL_I32:
      REL(i32);
      break;
    case IET_REL_I64:
      REL(i64);
      break;
    case IET_IMM_U8:
      IMM(u8);
      break;
    case IET_IMM_U16:
      IMM(u16);
      break;
    case IET_IMM_U32:
      IMM(u32);
      break;
    case IET_IMM_I64:
      IMM(i64);
      break;
    }
#undef OFF
#undef REL
#undef IMM
  }
  *src_ = src - 1;
}

void SysSymImportsResolve(char* st_ptr) {
  auto it = TOSLoader.find(st_ptr);
  if (it == TOSLoader.end())
    return;
  auto& [_, sym] = *it;
  if (sym.type != HTT_IMPORT_SYS_SYM)
    return;
  LoadOneImport(&sym.module_header_entry, sym.module_base);
  sym.type = HTT_INVALID;
}

void LoadPass1(u8* src, u8* module_base) {
  u8*   ptr;
  char* st_ptr;
  uptr  i;
  u8    etype;
  while ((etype = *src++)) {
    i = READ_NUM(src, u32);
    src += sizeof(u32);
    st_ptr = (char*)src;
    src += strlen(st_ptr) + 1;
    switch (etype) {
    case IET_REL32_EXPORT:
    case IET_IMM32_EXPORT:
    case IET_REL64_EXPORT:
    case IET_IMM64_EXPORT:
      if (etype != IET_IMM32_EXPORT && etype != IET_IMM64_EXPORT)
        i += (uptr)module_base;     // i gets reset at the
                                    // top of the loop so its fine
      TOSLoader.try_emplace(st_ptr, //
                            /*CSymbol*/ HTT_EXPORT_SYS_SYM, (u8*)i);
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
      src = (u8*)st_ptr - sizeof(u32) - 1;
      LoadOneImport(&src, module_base);
      break;
    // 32bit addrs
    case IET_ABS_ADDR: {
      for (usize j = 0; j < i /*count*/; j++, src += sizeof(u32)) {
        ptr = module_base + READ_NUM(src, u32);
        // compiles down to `add DWORD PTR[ptr],module_base`
        u32 off;
        memcpy(&off, ptr, sizeof(u32));
        off += (uptr)module_base;
        memcpy(ptr, &off, sizeof(u32));
      }
    } break;
      // the other ones wont be used
      // so im not implementing them
    }
  }
}

void LoadPass2(u8* src, u8* module_base) {
  char* st_ptr;
  u32   i;
  u8    etype;
  while ((etype = *src++)) {
    i = READ_NUM(src, u32);
    src += sizeof(u32);
    st_ptr = (char*)src;
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

#undef READ_NUM

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

void LoadHCRT(std::string const& name) {
  auto f = fopen(name.c_str(), "rb");
  if (!f) {
    fprintf(stderr, "CANNOT FIND TEMPLEOS BINARY FILE %s\n", name.c_str());
    _Exit(1);
  }
  std::error_code e;
  umax            sz;
  if ((sz = fs::file_size(name, e)) == static_cast<umax>(-1)) {
    fprintf(stderr, "CANNOT DETERMINE SIZE OF FILE, ERROR MESSAGE: %s\n",
            e.message().c_str());
    fclose(f);
    _Exit(1);
  }
  u8* bfh_addr;
  fread(bfh_addr = VirtAlloc<u8>(sz), 1, sz, f);
  fclose(f);
  // I think this breaks strict aliasing but
  // I dont think it matters because its packed(?)
  auto bfh = reinterpret_cast<CBinFile*>(bfh_addr);
  if (memcmp(bfh->bin_signature, "TOSB" /*BIN_SIGNATURE_VAL*/, 4)) {
    fprintf(stderr, "INVALID TEMPLEOS BINARY FILE %s\n", name.c_str());
    _Exit(1);
  }
  LoadPass1(bfh_addr + bfh->patch_table_offset, bfh->data);
  LoadPass2(bfh_addr + bfh->patch_table_offset, bfh->data);
}

namespace {
std::vector<std::string> sorted_syms;
bool                     sorted_syms_init = false;
std::string const        unknown_fun{"UNKNOWN"};

void InitSortedSyms() {
  sorted_syms.reserve(TOSLoader.size());
  for (auto const& [name, _] : TOSLoader)
    sorted_syms.emplace_back(name);
  std::sort(sorted_syms.begin(), sorted_syms.end(),
            [](auto const& a, auto const& b) -> bool {
              return TOSLoader[a].val < TOSLoader[b].val;
            });
  sorted_syms_init = true;
}
} // namespace

void BackTrace(uptr ctx_rbp, uptr ctx_rip) {
  if (!sorted_syms_init)
    InitSortedSyms();
  fputc('\n', stderr);
  auto  rbp = reinterpret_cast<void*>(ctx_rbp);
  auto  ptr = reinterpret_cast<void*>(ctx_rip);
  void* oldp;
  //
  std::string const* last;
  while (rbp) {
    oldp = nullptr;
    last = &unknown_fun;
    // linear search that iterates over symbols sorted in ascending address
    // order to find out where we were
    for (auto const& s : sorted_syms) {
      void* curp = TOSLoader[s].val;
      if (curp == ptr) {
        fprintf(stderr, "%s [%#" PRIx64 "]\n", s.c_str(), (uptr)ptr);
      } else if (curp > ptr) {
        // i know im supposed to use %p but it's weird beecause on windows it's
        // fucky wucky(prints numbers with 0s)
        fprintf(stderr, "%s [%#" PRIx64 "+%#" PRIx64 "] %#" PRIx64 "\n",
                last->c_str(), (uptr)ptr, (u8*)ptr - (u8*)oldp, (uptr)curp);
        goto next;
      }
      oldp = curp;
      last = &s;
    }
  next:
    ptr = static_cast<void**>(rbp)[1]; // [RBP+0x8] is the return address
    rbp = static_cast<void**>(rbp)[0]; // [RBP] is the previous base pointer
  }
  fputc('\n', stderr);
}

// who the fuck cares about memory leaks
// its gonna be executed once or twice in
// the entire debug session not to mention
// WhichFun() wont even be called in normal
// circumstances
#define STR_DUP(s) strcpy(new (std::nothrow) char[s->size() + 1], s->c_str())

// great when you use lldb and get a fault
// (lldb) p (char*)WhichFun($pc)
[[gnu::used, gnu::visibility("default")]] auto WhichFun(void* ptr) -> char* {
  if (!sorted_syms_init)
    InitSortedSyms();
  std::string const* last = &unknown_fun;
  for (auto const& s : sorted_syms) {
    if (TOSLoader[s].val >= ptr)
      return STR_DUP(last);
    last = &s;
  }
  return STR_DUP(last);
}

// vim: set expandtab ts=2 sw=2 :
