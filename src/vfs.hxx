#pragma once

#include <string>

#include "types.h"

void        VFsThrdInit();
void        VFsSetDrv(u8 d);
u8          VFsGetDrv();
void        VFsSetPwd(char const *pwd);
bool        VFsDirMk(char const *to);
bool        VFsDel(char const *p);
std::string VFsFileNameAbs(char const *name);
u64         VFsUnixTime(char const *name);
i64         VFsFSize(char const *name);
void        VFsFTrunc(char const *name, usize sz);
bool        VFsFileWrite(char const *name, char const *data, usize len);
void       *VFsFileRead(char const *name, u64 *len);
char      **VFsDir();
bool        VFsIsDir(char const *path);
bool        VFsFileExists(char const *path);
void        VFsMountDrive(char const let, char const *path);

// vim: set expandtab ts=2 sw=2 :
