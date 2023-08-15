#pragma once

#include "types.h"

// Shuts down the loader
void ShutdownTINE(int);
// Gets command line boot text that will be executed
// after boot, basically AUTOEXEC.BAT in text form
auto CmdLineBootText() -> char const*;

extern bool sanitize_clipboard;
extern bool is_cmd_line;

#ifndef _WIN32
extern usize page_size;
#endif
extern usize proc_cnt;

// vim: set expandtab ts=2 sw=2 :
