#pragma once

#include "types.h"

// HolyC printf that prints to stderr
// used for debugging purposes from HolyC
void TOSPrint(char const* fmt, u64 argc, i64* argv);

// vim: set expandtab ts=2 sw=2 :
