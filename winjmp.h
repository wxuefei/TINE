#pragma once
typedef uint8_t ExceptBuf[208];
extern int HCSetJmp(void *frame);
extern void HCLongJmp(void *frame);
