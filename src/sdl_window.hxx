#pragma once

#include <string>

#include "types.h"

// Sets clipboard text from TempleOS
void SetClipboard(char const* text);
// Gets TempleOS clipboard text
auto ClipboardText() -> std::string;
// New window, called from HolyC
void DrawWindowNew();
// self-explanatory, called from HolyC
void DrawWindowUpdate(u8* px);
// loops til you close the window or whatever
void EventLoop();
// Inits audio, called from HolyC
void PCSpkInit();

extern "C" union bgr_48 {
  u64 i;
  struct [[gnu::packed]] {
    u16 b, g, r, pad;
  };
};
// sets SDL palette from TempleOS
void GrPaletteColorSet(u64, bgr_48);

// self-explanatory, called from HOlyC
void SetKBCallback(void* fp);
// self-explanatory, called from HolyC
void SetMSCallback(void* fp);

// vim: set expandtab ts=2 sw=2 :
