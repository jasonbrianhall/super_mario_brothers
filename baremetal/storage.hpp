#pragma once
// Save states on the boot floppy. When the game was booted from a FAT12
// floppy (drive A:), the four save slots are read from /smb/save1.dat ..
// save4.dat at boot and written back whenever one is saved. Otherwise they
// live in RAM only (memfs), as before. Boot option floppy=off disables it.
#include <stdint.h>
#include <stddef.h>

void storage_init(uint32_t mb_flags, uint32_t boot_device, const char* cmdline);
// After the engine has saved `name` into memfs: 1 = also written to the
// floppy, 0 = no floppy (RAM only), -1 = the floppy write failed.
int storage_save(const char* name);
// Small settings files in /smb on the floppy (false if there's no floppy).
bool storage_read(const char* name, void* buf, size_t cap, size_t* len);
bool storage_write(const char* name, const void* data, size_t len);
