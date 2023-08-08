#pragma once

#include <string>

#include "types.h"

void SetClipboard(char const *text);
auto ClipboardText() -> std::string;
void NewDrawWindow();
void DrawWindowUpdate(u8 *colors, u64 internal_width);
void InputLoop(bool *off);

extern "C" union bgr_48 {
  u64 i;
  struct [[gnu::packed]] {
    u16 b, g, r, pad;
  };
};
void GrPaletteColorSet(u64, bgr_48);

void SetKBCallback(void *fp, void *data);
void SetMSCallback(void *fp);

// vim: set expandtab ts=2 sw=2 :
