#pragma once

#include "types.h"

void ShutdownTINE(int);
auto CmdLineBootText() -> char const *;

extern bool sanitize_clipboard;
extern bool is_cmd_line;

#ifndef _WIN32
extern usize page_size;
#endif
extern usize proc_cnt;

// vim: set expandtab ts=2 sw=2 :
