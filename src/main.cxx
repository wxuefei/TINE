#ifdef _WIN32
  // clang-format off
  #include <windows.h>
  #include <winbase.h>
  #include <wincon.h>
  #include <winerror.h>
  #include <processenv.h>
  #include <processthreadsapi.h>
  // clang-format on

  // for mingw
  // https://archive.md/HEZm2#selection-3667.0-3698.0
  #ifndef ERROR_CONTROL_C_EXIT
    #define ERROR_CONTROL_C_EXIT 0x23C
  #endif

static BOOL WINAPI CtrlCHandlerRoutine(DWORD) {
  #define STR_(x) x, lstrlenA(x)
  WriteConsoleA(GetStdHandle(STD_ERROR_HANDLE), STR_("User Abort.\n"), nullptr,
                nullptr);
  ExitProcess(ERROR_CONTROL_C_EXIT);
  return TRUE;
}

#else
  #include <signal.h>
  #include <sys/resource.h>
  #include <unistd.h>
#endif

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <thread>

#include "dbg.hxx"
#include "main.hxx"
#include "multic.hxx"
#include "runtime.hxx"
#include "sdl_window.hxx"
#include "sound.h"
#include "tos_aot.hxx"
#include "vfs.hxx"

#include "argtable3.h"

namespace fs = std::filesystem;

using std::thread;

namespace {

constexpr bool is_win =
#ifdef _WIN32
    true;
#else
    false;
#endif

struct arg_lit *helpArg, *sixty_fps, *commandLineArg, *cb_sanitize, *ndebug,
    *noans;
struct arg_file *cmdLineFiles, *TDriveArg, *HCRTArg;

std::string boot_str;
std::string bin_path{"HCRT.BIN"};

int exit_code = 0;
bool prog_exit = false;

void* __stdcall Core0(void*) {
  VFsThrdInit();
  LoadHCRT(bin_path);
  SetupDebugger();
#ifndef _WIN32
  signal(SIGUSR1, [](int) {
    pthread_exit(nullptr);
  });
#endif
  return nullptr;
}

} // namespace

char const* CmdLineBootText() {
  return boot_str.c_str();
}

void ShutdownTINE(int ec) {
  prog_exit = true;
  exit_code = ec;
  ShutdownCores(ec);
}

bool sanitize_clipboard = false;
bool is_cmd_line = false;

#ifndef _WIN32
size_t page_size; // used for allocation
                  // and pointer checks
#else
DWORD dwAllocationGranularity;
#endif
size_t proc_cnt;

int main(int argc, char** argv) {
#ifndef _WIN32
  // https://archive.md/5cufN#selection-2369.223-2369.272
  // Hilarious how Linux manpages won't teach me anything
  // about why I got an EPERM when I raised rl.rlim_max
  struct rlimit rl;
  getrlimit(RLIMIT_NOFILE, &rl);
  rl.rlim_cur = rl.rlim_max;
  setrlimit(RLIMIT_NOFILE, &rl);
  page_size = sysconf(_SC_PAGESIZE);
#else
  SYSTEM_INFO si;
  GetSystemInfo(&si);
  dwAllocationGranularity = si.dwAllocationGranularity;
#endif
  proc_cnt = thread::hardware_concurrency();
  // i wanted to use cli11 but i dont want exceptions in this codebase
  void* argtable[] = {
      helpArg = arg_lit0("h", "help", "Display this help message."),
      sixty_fps = arg_lit0("6", "60fps", "Run in 60 fps mode."),
      commandLineArg = arg_lit0("c", "com",
                                "Start in command line "
                                "mode, mount cwd at Z:/"),
      HCRTArg = arg_file0("f", "file", nullptr,
                          "Specify where your HolyC runtime is"),
      TDriveArg = arg_file0("t", "root", nullptr,
                            "This tells TINE where the boot folder is"),
      cb_sanitize = arg_lit0("s", "sanitize-cb",
                             "Remove characters in clipboard that may collide "
                             "with DolDoc control chars"),
      ndebug = arg_lit0("n", "ndebug", "Silence compiler output"),
      noans = arg_lit0("a", "noans", "Do not print expression results"),
      cmdLineFiles = arg_filen(nullptr, nullptr, "<files>", 0, 100,
                               "Files for use with command "
                               "line mode"),
      arg_end_(1),
  };
  int errs = arg_parse(argc, argv, argtable);
  if (helpArg->count > 0 || errs > 0 || TDriveArg->count == 0) {
    std::cerr << "Usage is: " << argv[0];
    arg_print_syntaxv(stderr, argtable, "\n");
    arg_print_glossary_gnu(stderr, argtable);
    return 1;
  }

  if (fs::exists(TDriveArg->filename[0])) {
    VFsMountDrive('T', TDriveArg->filename[0]);
  } else {
    std::cerr << TDriveArg->filename[0] << " DOES NOT EXIST\n";
    std::terminate();
  }
  if (commandLineArg->count > 0) {
    VFsMountDrive('Z', ".");
    is_cmd_line = true;
  }
  // This is called before LoadHCRT so TOSLoader will not be
  // all fucked up, fyi
  RegisterFuncPtrs();

  if (ndebug->count == 0)
    boot_str += "__EnableDbg;\n";
  if (noans->count == 0)
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
  if (sixty_fps->count != 0)
    boot_str += "SetFPS(60.);\n";
  if (!is_cmd_line)
    NewDrawWindow();
  if (is_win || !is_cmd_line)
    InitSound();

  if (HCRTArg->count > 0)
    bin_path = HCRTArg->filename[0];
  if (fs::exists(bin_path)) {
    if (ndebug->count == 0)
      std::cerr << "Using " << bin_path << " as the kernel.\n";
    LaunchCore0(Core0);
  } else {
    std::cerr << bin_path << " DOES NOT EXIST\n";
    return 1;
  }

  if (!is_cmd_line) {
#ifdef _WIN32
    SetConsoleCtrlHandler(CtrlCHandlerRoutine, TRUE);
#endif
    if (cb_sanitize->count > 0)
      sanitize_clipboard = true;
    InputLoop(&prog_exit);
  } else {
    WaitForCore0();
  }
  return exit_code;
}

// vim: set expandtab ts=2 sw=2 :
