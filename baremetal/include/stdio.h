#pragma once
#include <stdarg.h>
#ifdef __cplusplus
extern "C" {
#endif
int printf(const char*, ...);
int vprintf(const char*, va_list);
int puts(const char*);
int putchar(int);
#ifdef __cplusplus
}
#endif
