#pragma once

#include "types.h"

// Backtrace invoked when something goes fucky wucky
void BackTrace(uptr ctx_rbp, uptr ctx_rip);

// vim: set expandtab ts=2 sw=2 :
