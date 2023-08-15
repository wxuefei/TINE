#pragma once

#include <string>

#include "types.h"

// Sets clipboard text from TempleOS
void SetClipboard(char const* text);
// Gets TempleOS clipboard text
auto ClipboardText() -> std::string;
// New window
void NewDrawWindow();
// self-explanatory
void DrawWindowUpdate(u8* colors, u64 internal_width);
// loops til you close the window or whatever
void InputLoop(bool* off);

extern "C" union bgr_48 {
  u64 i;
  struct [[gnu::packed]] {
    u16 b, g, r, pad;
  };
};
// sets SDL palette from  TempleOS
void GrPaletteColorSet(u64, bgr_48);

// self-explanatory
void SetKBCallback(void* fp, void* data);
// self-explanatory
void SetMSCallback(void* fp);

// vim: set expandtab ts=2 sw=2 :
