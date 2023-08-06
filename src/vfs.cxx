// clang-format off
// Windows requires sys/stat to be included after sys/types
// fuck bill gates
#include <sys/types.h>
#include <sys/stat.h>
// clang-format on

#include <array>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "alloc.hxx"
#include "vfs.hxx"

#ifdef _WIN32
  #define delim '\\'
  #define stat  _stati64
#else
  #define delim '/'
#endif

namespace fs = std::filesystem;

using std::ios;

namespace {

thread_local std::string thrd_pwd;
thread_local u8          thrd_drv;

inline bool FExists(std::string const &path) {
  std::error_code e;
  bool            ret = fs::exists(path, e);
  return e ? false : ret;
}

inline bool FIsDir(std::string const &path) {
  std::error_code e;
  bool            ret = fs::is_directory(path, e);
  return e ? false : ret;
}

std::array<std::string, 'z' - 'a' + 1> mount_points;

} // namespace

void VFsThrdInit() {
  thrd_pwd = "/";
  thrd_drv = 'T';
}

void VFsSetDrv(u8 d) {
  if (!isalpha(d))
    return;
  thrd_drv = toupper(d);
}

u8 VFsGetDrv() {
  return thrd_drv;
}

void VFsSetPwd(char const *pwd) {
  if (!pwd)
    pwd = "/";
  thrd_pwd = pwd;
}

std::string VFsFileNameAbs(char const *name) {
  std::string ret;
  // thrd_drv is always uppercase
  ret += mount_points[thrd_drv - 'A']; // T
  ret += delim;                        // /
  if (thrd_pwd.size() > 1) {
    ret.pop_back();
    ret += thrd_pwd; // /
    ret += delim;    // /
  }
  ret += name; // Name
  return ret;
}

bool VFsDirMk(char const *to) {
  std::string p = VFsFileNameAbs(to);
  if (FExists(p) && FIsDir(p)) {
    return true;
  }
  std::error_code e;
  fs::create_directory(p, e);
  return !e;
}

bool VFsDel(char const *p) {
  std::string path = VFsFileNameAbs(p);
  if (!FExists(path))
    return false;
  std::error_code e;
  fs::remove_all(path, e);
  return !e;
}

i64 VFsFSize(char const *name) {
  std::string fn = VFsFileNameAbs(name);
  if (!FExists(fn)) {
    return -1;
  } else if (FIsDir(fn)) {
    fs::directory_iterator it{fn};
    return std::distance(fs::begin(it), fs::end(it));
  }
  std::error_code e;
  auto            sz = fs::file_size(fn, e);
  return e ? -1 : sz;
}

void VFsFTrunc(char const *name, usize sz) {
  std::error_code e;
  fs::resize_file(VFsFileNameAbs(name), sz, e);
  if (e)
    HolyThrow("SysError");
}

u64 VFsUnixTime(char const *name) {
  std::string fn = VFsFileNameAbs(name);
  if (!FExists(fn))
    return 0;
  struct stat s;
  stat(fn.c_str(), &s);
  return s.st_mtime;
}

bool VFsFileWrite(char const *name, char const *data, usize len) {
  std::string p = VFsFileNameAbs(name);
  if (name) {
    auto fp = fopen(p.c_str(), "wb");
    if (fp != nullptr) {
      fwrite(data, 1, len, fp);
      fclose(fp);
    }
  }
  return !!name;
}

void *VFsFileRead(char const *name, u64 *len_ptr) {
  if (len_ptr)
    *len_ptr = 0;
  if (!name)
    return nullptr;
  std::string p = VFsFileNameAbs(name);
  if (!FExists(p))
    return nullptr;
  if (FIsDir(p))
    return nullptr;
  auto fp = fopen(p.c_str(), "rb");
  if (!fp)
    return nullptr;
  std::error_code e;
  usize           sz = fs::file_size(p, e);
  if (e) {
    fclose(fp);
    return nullptr;
  }
  u8 *data = nullptr;
  fread(data = HolyAlloc<u8, true>(sz + 1), 1, sz, fp);
  fclose(fp);
  if (len_ptr)
    *len_ptr = sz;
  return data;
}

char **VFsDir() {
  std::string file = VFsFileNameAbs("");
  if (!FIsDir(file))
    return nullptr;
#define SD(s) HolyStrDup(s)
  // https://archive.md/1Ojr3#7
  using DirEnt = char *;
  std::vector<DirEnt> items{
      SD("."),
      SD(".."),
  };
  for (auto const &e : fs::directory_iterator{file}) {
    auto const &s = e.path().filename().string();
    // CDIR_FILENAME_LEN is 38(includes '\0')
    // do not touch, fat32 legacy
    // will break opening ISOs if touched
    if (s.size() <= 38 - 1)
      items.emplace_back(SD(s.c_str()));
  }
  // force null pointer terminator
  auto ret = HolyAlloc<DirEnt, true>(items.size() + 1);
  memcpy(ret, items.data(), items.size() * sizeof(DirEnt));
  return ret;
#undef SD
}

bool VFsIsDir(char const *path) {
  return FIsDir(VFsFileNameAbs(path));
}

bool VFsFileExists(char const *path) {
  return FExists(VFsFileNameAbs(path));
}

void VFsMountDrive(char const let, char const *path) {
  mount_points[toupper(let) - 'A'] = path;
}

// vim: set expandtab ts=2 sw=2 :
