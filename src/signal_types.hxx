#pragma once

#ifndef _WIN32
  #include <signal.h>
using SignalCallback    = auto(int) -> void;
using SigActionCallback = auto(int, siginfo_t*, void*) -> void;
#endif
