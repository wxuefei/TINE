#include "main.hxx"
#include "dbg.hxx"
#include "ffi.h"
#include "multic.hxx"
#include "runtime.hxx"
#include "sdl_window.hxx"
#include "sound.h"
#include "tos_aot.hxx"
#include "vfs.hxx"
// its dangerous and lonely out here socrates

#include "ext/argtable3.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <utility>
namespace fs = std::filesystem;

static constexpr bool is_win =
#ifdef _WIN32
    true;
#else
    false;
#endif

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
#endif

static struct arg_lit *helpArg, *sixty_fps, *commandLineArg, *cb_sanitize,
    *ndebug, *noans;
static struct arg_file *cmdLineFiles, *TDriveArg, *HCRTArg;

static bool is_cmd_line = false;
bool IsCmdLine() {
  return is_cmd_line;
}

static std::string boot_str;
char const* CmdLineBootText() {
  return boot_str.c_str();
}

static bool prog_exit = false;
void ShutdownTINE(int32_t ec) {
  prog_exit = true;
  ShutdownCores(ec);
}

bool sanitize_clipboard = false;

static std::string bin_path{"HCRT.BIN"};
static void* __stdcall Core0(void*) {
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

int main(int argc, char** argv) {
  void* argtable[] = {
      helpArg = arg_lit0("h", "help", "Display this help message."),
      sixty_fps = arg_lit0("6", "60fps", "Run in 60 fps mode."),
      commandLineArg = arg_lit0("c", "com",
                                "Start in command line "
                                "mode,mount drive '/' at /."),
      HCRTArg = arg_file0("f", "file", nullptr,
                          "Specifies where your HolyC runtime is"),
      TDriveArg = arg_file0("t", nullptr, "T(boot) Drive",
                            "This tells TINE where to "
                            "use(or create) the boot "
                            "drive folder."),
      cmdLineFiles = arg_filen(nullptr, nullptr, "<files>", 0, 100,
                               "Files for use with command "
                               "line mode."),
      cb_sanitize = arg_lit0("s", "sanitize-cb",
                             "Remove characters in clipboard that may collide "
                             "with DolDoc control chars)"),
      ndebug = arg_lit0("n", "ndebug", "Silence compiler output"),
      noans = arg_lit0("a", "noans", "Do not print expression results"),
      arg_end_(1),
  };
  int errs = arg_parse(argc, argv, argtable);
  if (helpArg->count > 0 || errs != 0 || TDriveArg->count == 0) {
    std::cerr << "Usage is: " << argv[0];
    arg_print_syntaxv(stderr, argtable, "\n");
    arg_print_glossary(stderr, argtable, "  %-25s %s\n");
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
  VFsThrdInit();
  // This is called before LoadHCRT so TOSLoader will not be
  // all fucked up, fyi
  RegisterFuncPtrs();
  if (ndebug->count == 0)
    boot_str += "__EnableDbg;\n";
  if (noans->count == 0)
    boot_str += "__EnableAns;\n";
  if (is_cmd_line) {
    VFsSetDrv('Z');
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
  if (!is_cmd_line)
    NewDrawWindow();
  if (is_win || !is_cmd_line)
    InitSound();
  if (HCRTArg->count > 0)
    bin_path = HCRTArg->filename[0];
  if (fs::exists(bin_path)) {
    if (ndebug->count == 0)
      std::cerr << "Using " << bin_path << " as the default binary.\n";
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
  return 0;
}
