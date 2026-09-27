#pragma once
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
void* malloc(size_t);
void* calloc(size_t, size_t);
void free(void*);
void abort(void);
static inline int abs(int x) { return x < 0 ? -x : x; }
#ifdef __cplusplus
}
#endif
