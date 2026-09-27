// Minimal C/C++ runtime for the bare-metal build: memory, strings,
// printf to the serial port, a heap, and a tiny RAM "filesystem"
// backing std::ofstream/std::ifstream (used for save states).
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include "include/string.h"
#include "include/stdio.h"
#include "include/stdlib.h"
#include "memfs.hpp"
#include "hw.hpp"

extern "C" {

void* memset(void* d, int c, size_t n) {
    void* r = d;
    __asm__ volatile("rep stosb" : "+D"(d), "+c"(n) : "a"(c) : "memory");
    return r;
}
void* memcpy(void* d, const void* s, size_t n) {
    void* r = d;
    __asm__ volatile("rep movsb" : "+D"(d), "+S"(s), "+c"(n) :: "memory");
    return r;
}
void* memmove(void* d, const void* s, size_t n) {
    uint8_t* dp = (uint8_t*)d; const uint8_t* sp = (const uint8_t*)s;
    if (dp < sp) while (n--) *dp++ = *sp++;
    else { dp += n; sp += n; while (n--) *--dp = *--sp; }
    return d;
}
int memcmp(const void* a, const void* b, size_t n) {
    const uint8_t* x = (const uint8_t*)a; const uint8_t* y = (const uint8_t*)b;
    for (; n; n--, x++, y++) if (*x != *y) return *x - *y;
    return 0;
}
size_t strlen(const char* s) { size_t n = 0; while (s[n]) n++; return n; }
int strcmp(const char* a, const char* b) {
    while (*a && *a == *b) { a++; b++; }
    return (uint8_t)*a - (uint8_t)*b;
}
int strncmp(const char* a, const char* b, size_t n) {
    for (; n; n--, a++, b++) { if (*a != *b || !*a) return (uint8_t)*a - (uint8_t)*b; }
    return 0;
}
char* strcpy(char* d, const char* s) { char* r = d; while ((*d++ = *s++)) {} return r; }
char* strncpy(char* d, const char* s, size_t n) {
    size_t i = 0;
    for (; i < n && s[i]; i++) d[i] = s[i];
    for (; i < n; i++) d[i] = 0;
    return d;
}

// ---- printf -> serial ----
static void out_num(unsigned long long v, int base, int neg, int width, char pad) {
    char buf[32]; int i = 0;
    do { buf[i++] = "0123456789abcdef"[v % base]; v /= base; } while (v);
    if (neg) buf[i++] = '-';
    while (i < width) buf[i++] = pad;
    while (i) serial_putc(buf[--i]);
}
int vprintf(const char* f, va_list ap) {
    for (; *f; f++) {
        if (*f != '%') { serial_putc(*f); continue; }
        f++;
        char pad = ' '; int width = 0, lng = 0;
        if (*f == '0') { pad = '0'; f++; }
        while (*f >= '0' && *f <= '9') width = width * 10 + (*f++ - '0');
        while (*f == 'l' || *f == 'z') { lng++; f++; }
        switch (*f) {
        case 'd': case 'i': {
            long long v = lng ? va_arg(ap, long) : va_arg(ap, int);
            out_num(v < 0 ? -v : v, 10, v < 0, width, pad); break; }
        case 'u': out_num(lng ? va_arg(ap, unsigned long) : va_arg(ap, unsigned), 10, 0, width, pad); break;
        case 'x': case 'X': out_num(lng ? va_arg(ap, unsigned long) : va_arg(ap, unsigned), 16, 0, width, pad); break;
        case 'p': serial_puts("0x"); out_num((uintptr_t)va_arg(ap, void*), 16, 0, 0, ' '); break;
        case 's': { const char* s = va_arg(ap, const char*); serial_puts(s ? s : "(null)"); break; }
        case 'c': serial_putc((char)va_arg(ap, int)); break;
        case 'f': (void)va_arg(ap, double); serial_puts("<float>"); break;
        case '%': serial_putc('%'); break;
        default: serial_putc('%'); serial_putc(*f); break;
        }
    }
    return 0;
}
int printf(const char* f, ...) { va_list ap; va_start(ap, f); vprintf(f, ap); va_end(ap); return 0; }
int puts(const char* s) { serial_puts(s); serial_putc('\n'); return 0; }
int putchar(int c) { serial_putc((char)c); return c; }

// ---- heap: first-fit free list over a static arena ----
struct Block { size_t size; Block* next; int free; };
static uint8_t heap_area[16 << 20] __attribute__((aligned(16)));
static Block* heap_head;

void* malloc(size_t n) {
    n = (n + 15) & ~(size_t)15;
    if (!heap_head) {
        heap_head = (Block*)heap_area;
        heap_head->size = sizeof(heap_area) - sizeof(Block);
        heap_head->next = nullptr;
        heap_head->free = 1;
    }
    for (Block* b = heap_head; b; b = b->next) {
        if (!b->free || b->size < n) continue;
        if (b->size >= n + sizeof(Block) + 64) {
            Block* rest = (Block*)((uint8_t*)(b + 1) + n);
            rest->size = b->size - n - sizeof(Block);
            rest->next = b->next;
            rest->free = 1;
            b->next = rest;
            b->size = n;
        }
        b->free = 0;
        return b + 1;
    }
    printf("malloc: out of memory (%lu bytes)\n", (unsigned long)n);
    return nullptr;
}
void free(void* p) {
    if (!p) return;
    Block* b = (Block*)p - 1;
    b->free = 1;
    while (b->next && b->next->free) {          // merge with following free blocks
        b->size += sizeof(Block) + b->next->size;
        b->next = b->next->next;
    }
}
void* calloc(size_t a, size_t b) { void* p = malloc(a * b); if (p) memset(p, 0, a * b); return p; }

void abort(void) { printf("abort()\n"); for (;;) __asm__ volatile("cli; hlt"); }

// ---- C++ ABI bits ----
void* __dso_handle = nullptr;
int __cxa_atexit(void (*)(void*), void*, void*) { return 0; }
void __cxa_pure_virtual() { printf("pure virtual call\n"); abort(); }

} // extern "C"

void* operator new(size_t n) { return malloc(n); }
void* operator new[](size_t n) { return malloc(n); }
void operator delete(void* p) noexcept { free(p); }
void operator delete[](void* p) noexcept { free(p); }
void operator delete(void* p, size_t) noexcept { free(p); }
void operator delete[](void* p, size_t) noexcept { free(p); }

// ---- RAM filesystem ----
namespace memfs {
static File files[MAX_FILES];

File* open(const char* name, bool create) {
    for (auto& f : files)
        if (f.used && strcmp(f.name, name) == 0) return &f;
    if (!create) return nullptr;
    for (auto& f : files)
        if (!f.used) {
            f.used = true;
            strncpy(f.name, name, sizeof(f.name) - 1);
            f.size = 0;
            return &f;
        }
    return nullptr;
}
bool exists(const char* name) { return open(name, false) != nullptr; }
}
