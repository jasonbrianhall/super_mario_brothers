#pragma once
#include <stddef.h>
#include <stdint.h>

// Tiny in-RAM file store so the engine's save/load state code works
// unchanged. Contents are lost on reboot.
namespace memfs {
constexpr int MAX_FILES = 8;
constexpr size_t MAX_SIZE = 16384;
struct File {
    bool used;
    char name[64];
    size_t size;
    uint8_t data[MAX_SIZE];
};
File* open(const char* name, bool create);
bool exists(const char* name);
}
