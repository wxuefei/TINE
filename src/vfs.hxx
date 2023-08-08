#pragma once

#include <string>

#include <stdio.h>

#include "types.h"

void VFsThrdInit();
void VFsSetDrv(u8 d);
auto VFsGetDrv() -> u8;
void VFsSetPwd(char const *pwd);
auto VFsDirMk(char const *to) -> bool;
auto VFsDel(char const *p) -> bool;
auto VFsFileNameAbs(char const *name) -> std::string;
auto VFsUnixTime(char const *name) -> u64;
auto VFsFSize(char const *name) -> i64;
auto VFsFOpen(char const *path, char const *m) -> FILE *;
void VFsFTrunc(char const *name, usize sz);
auto VFsFileWrite(char const *name, char const *data, usize len) -> bool;
auto VFsFRead(char const *name, u64 *len) -> u8 *;
auto VFsDir() -> char **;
auto VFsIsDir(char const *path) -> bool;
auto VFsFileExists(char const *path) -> bool;
void VFsMountDrive(char const let, char const *path);

// vim: set expandtab ts=2 sw=2 :
