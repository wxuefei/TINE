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

inline auto FExists(std::string const &path) -> bool {
  std::error_code e;
  return fs::exists(path, e) && !e; // shortcircuit ops are well defined
}

inline auto FIsDir(std::string const &path) -> bool {
  std::error_code e;
  return fs::is_directory(path, e) && !e;
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

auto VFsGetDrv() -> u8 {
  return thrd_drv;
}

void VFsSetPwd(char const *pwd) {
  if (!pwd)
    pwd = "/";
  thrd_pwd = pwd;
}

auto VFsFileNameAbs(char const *name) -> std::string {
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

auto VFsDirMk(char const *to) -> bool {
  std::string p = VFsFileNameAbs(to);
  if (FExists(p) && FIsDir(p)) {
    return true;
  }
  std::error_code e;
  return fs::create_directory(p, e) && !e;
}

auto VFsDel(char const *p) -> bool {
  std::string path = VFsFileNameAbs(p);
  if (!FExists(path))
    return false;
  std::error_code e;
  // remove_all can still throw but
  // only when it's out of memory which
  // SHOULD terminate
  // https://archive.md/kkVNq#selection-56944.0-56944.3
  return (static_cast<umax>(-1) != fs::remove_all(path, e)) && !e;
}

auto VFsFSize(char const *name) -> i64 {
  std::string fn = VFsFileNameAbs(name);
  if (!FExists(fn)) {
    return -1;
  } else if (FIsDir(fn)) {
    fs::directory_iterator it{fn};
    return std::distance(fs::begin(it), fs::end(it));
  }
  std::error_code e;
  // https://archive.md/kkVNq#selection-50768.3-50766.3
  // thankfully for us fs::file_size will return -1 for its
  // error_code overload when something goes fucky wucky
  return static_cast<i64>(fs::file_size(fn, e));
}

auto VFsFOpen(char const *path, char const *m) -> FILE * {
  std::string p = VFsFileNameAbs(path);
  return fopen(p.c_str(), m);
}

void VFsFTrunc(char const *name, usize sz) {
  std::error_code e;
  fs::resize_file(VFsFileNameAbs(name), sz, e);
  if (e)
    HolyThrow("SysError");
}

auto VFsUnixTime(char const *name) -> u64 {
  std::string fn = VFsFileNameAbs(name);
  if (!FExists(fn))
    return 0;
  struct stat s;
  stat(fn.c_str(), &s);
  return s.st_mtime;
}

auto VFsFileWrite(char const *name, char const *data, usize len) -> bool {
  std::string p = VFsFileNameAbs(name);
  if (name) {
    auto fp = fopen(p.c_str(), "wb");
    if (fp) {
      fwrite(data, 1, len, fp);
      fclose(fp);
    }
  }
  return !!name;
}

auto VFsFileRead(char const *name, u64 *len_ptr) -> void * {
  if (len_ptr)
    *len_ptr = 0;
  if (!name)
    return nullptr;
  std::string p = VFsFileNameAbs(name);
  if (!FExists(p) || FIsDir(p))
    return nullptr;
  auto fp = fopen(p.c_str(), "rb");
  if (!fp)
    return nullptr;
  std::error_code e;
  umax            sz;
  // no need to check for e(see comment in VFsFSize)
  if (static_cast<umax>(-1) == (sz = fs::file_size(p, e))) {
    fclose(fp);
    return nullptr;
  }
  u8 *data = nullptr;
  fread(data = HolyAlloc<u8>(sz + 1), 1, sz, fp);
  fclose(fp);
  data[sz] = 0;
  if (len_ptr)
    *len_ptr = sz;
  return data;
}

auto VFsDir() -> char ** {
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
  auto ret          = HolyAlloc<DirEnt>(items.size() + 1);
  ret[items.size()] = nullptr;
  memcpy(ret, items.data(), items.size() * sizeof(DirEnt));
  return ret;
#undef SD
}

auto VFsIsDir(char const *path) -> bool {
  return FIsDir(VFsFileNameAbs(path));
}

auto VFsFileExists(char const *path) -> bool {
  return FExists(VFsFileNameAbs(path));
}

void VFsMountDrive(char const let, char const *path) {
  mount_points[toupper(let) - 'A'] = path;
}

// vim: set expandtab ts=2 sw=2 :
