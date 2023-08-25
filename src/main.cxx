#include "main.hxx"
#include "cpp2holyc.hxx"
#include "dbg.hxx"
#include "sdl_window.hxx"
#include "seth.hxx"
#include "sound.h"
#include "tos_aot.hxx"
#include "vfs.hxx"

#ifdef _WIN32
  #include <windows.h>
  #include <winbase.h>
  #include <wincon.h>
  #include <winerror.h>
  #include <processthreadsapi.h>
  // for mingw
  // https://archive.md/HEZm2#selection-3667.0-3698.0
  #ifndef ERROR_CONTROL_C_EXIT
    #define ERROR_CONTROL_C_EXIT 0x23C
  #endif
#else
  #include <string.h>
  #include <sys/resource.h>
  #include <unistd.h>
#endif

#include <algorithm>
#include <filesystem>
#include <system_error>
#include <utility>

#include <stdio.h>
#include <stdlib.h>

#include <argtable3.h>

#include <tos_callconv.h>

namespace fs = std::filesystem;

namespace {

std::string boot_str;
std::string bin_path = "HCRT.BIN";

#ifdef _WIN32
[[noreturn]] auto WINAPI CtrlCHandlerRoutine(DWORD) -> BOOL {
  #define S(x) x, lstrlenA(x)
  WriteConsoleA(GetStdHandle(STD_ERROR_HANDLE), S("User Abort.\n"), nullptr,
                nullptr);
  _Exit(ERROR_CONTROL_C_EXIT);
  #undef S
  return TRUE;
}
#endif

} // namespace

bool sanitize_clipboard = false;
bool is_cmd_line        = false;

auto CmdLineBootText() -> char const* {
  return boot_str.c_str();
}

auto main(int argc, char** argv) -> int {
#ifndef _WIN32
  // https://archive.md/5cufN#selection-2369.223-2369.272
  // Hilarious how Linux manpages won't teach me anything
  // about why I got an EPERM when I raised rl.rlim_max
  struct rlimit rl;
  getrlimit(RLIMIT_NOFILE, &rl);
  rl.rlim_cur = rl.rlim_max;
  setrlimit(RLIMIT_NOFILE, &rl);
  signal(SIGINT, [](int) {
  #define S(x) x, strlen(x)
    write(2, S("User abort.\n"));
    _Exit(1);
  #undef S
  });
#else
  SetConsoleCtrlHandler(CtrlCHandlerRoutine, TRUE);
#endif
  // i wanted to use cli11 but i dont want exceptions in this codebase
  struct arg_lit *helpArg, *sixty_fps, *commandLineArg, *cb_sanitize, *ndebug,
      *noans;
  struct arg_file *cmdLineFiles, *TDriveArg, *HCRTArg;
  //
  void* argtable[] = {
      helpArg        = arg_lit0("h", "help", "Display this help message"),
      sixty_fps      = arg_lit0("6", "60fps", "Run in 60 fps mode."),
      commandLineArg = arg_lit0("c", "com", "Command line mode, cwd -> Z:/"),
      cb_sanitize = arg_lit0("s", "sanitize-cb", "Sanitize clipboard contents"),
      ndebug      = arg_lit0("n", "ndebug", "Silence compiler output"),
      noans       = arg_lit0("a", "noans", "Silence expression results"),
      HCRTArg     = arg_file0("f", "file", nullptr, "Specify HolyC runtime"),
      TDriveArg   = arg_file0("t", "root", nullptr, "Specify boot folder"),
      cmdLineFiles = arg_filen(nullptr, nullptr, "<files>", 0, 100,
                               "Files that run on boot(cmdline mode specific)"),
      arg_end_(1),
  };
  int errs = arg_parse(argc, argv, argtable);
  if (helpArg->count > 0 || errs > 0 || !TDriveArg->count) {
    fprintf(stderr, "Usage: %s", argv[0]);
    arg_print_syntaxv(stderr, argtable, "\n");
    arg_print_glossary_gnu(stderr, argtable);
    return 1;
  }
  if (std::error_code e; fs::exists(TDriveArg->filename[0], e)) {
    VFsMountDrive('T', TDriveArg->filename[0]);
  } else if (e) {
    fprintf(stderr, "SYSTEM ERROR OCCURED: %s\n", e.message().c_str());
    return 1;
  } else {
    fprintf(stderr, "%s DOES NOT EXIST\n", TDriveArg->filename[0]);
    return 1;
  }
  if (commandLineArg->count > 0) {
    VFsMountDrive('Z', ".");
    is_cmd_line = true;
  }
  if (cb_sanitize->count > 0)
    sanitize_clipboard = true;
  if (!ndebug->count)
    boot_str += "__EnableDbg;\n";
  if (!noans->count)
    boot_str += "__EnableAns;\n";
  if (is_cmd_line) {
    boot_str += "#exe {Drv('Z');};\n";
    for (int i = 0; i < cmdLineFiles->count; ++i) {
      boot_str += "#include \"";
      boot_str += cmdLineFiles->filename[i];
      boot_str += "\";\n";
    }
#ifdef _WIN32
    std::replace(boot_str.begin(), boot_str.end(), '\\', '/');
#endif
  }
  if (sixty_fps->count)
    boot_str += "SetFPS(60.);\n";
  if (HCRTArg->count > 0)
    bin_path = HCRTArg->filename[0];
  if (std::error_code e; fs::exists(bin_path, e)) {
    if (!ndebug->count)
      fprintf(stderr, "Using %s as the kernel.\n", bin_path.c_str());
  } else if (e) {
    fprintf(stderr, "SYSTEM ERROR OCCURED: %s\n", e.message().c_str());
    return 1;
  } else {
    fprintf(stderr,
            "%s DOES NOT EXIST, MAYBE YOU FORGOT TO BOOTSTRAP IT? REFER TO "
            "README FOR GUIDANCE\n",
            bin_path.c_str());
    return 1;
  }
  arg_freetable(argtable, sizeof argtable / sizeof argtable[0]);
  BootstrapLoader();
  CreateCore(0, LoadHCRT(bin_path));
  EventLoop();
}

// vim: set expandtab ts=2 sw=2 :
