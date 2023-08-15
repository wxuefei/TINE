#pragma once

#include <string>

#include <stdio.h>

#include "types.h"

// Inits virtual filesytem for calling thread
void VFsThrdInit();
// Sets virtual drive letter for calling thread
void VFsSetDrv(u8 d);
// Gets current thread's pwd
auto VFsGetDrv() -> u8;
// Sets current thread's pwd
void VFsSetPwd(char const* pwd);
// mkdir
auto VFsDirMk(char const* to) -> bool;
// Delete file/directory/whatever, recursively if needed
auto VFsDel(char const* p) -> bool;
// Last modified file time since epoch
auto VFsFUnixTime(char const* name) -> u64;
// How big?(-1 on failure)
auto VFsFSize(char const* name) -> i64;
// Open file
auto VFsFOpen(char const* path, char const* mode) -> FILE*;
// truncate()
void VFsFTrunc(char const* name, usize sz);
// Write data to file
auto VFsFWrite(char const* name, char const* data, usize len) -> bool;
// Reads file contents as null terminated &mut [u8]
auto VFsFRead(char const* name, u64* len) -> u8*;
// Does it exist?
auto VFsFExists(char const* path) -> bool;
// Is it a directory?
auto VFsIsDir(char const* path) -> bool;
// ls, nullptr sentinel-terminated
auto VFsDir() -> char**;
// Mounts path at virtual drive letter
void VFsMountDrive(char let, char const* path);

// vim: set expandtab ts=2 sw=2 :
