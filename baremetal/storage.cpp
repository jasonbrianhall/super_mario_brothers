// Save states on the boot floppy. See storage.hpp.
#include "storage.hpp"
#include "fat12.hpp"
#include "floppy.hpp"
#include "memfs.hpp"
#include "hw.hpp"
#include "include/stdio.h"
#include "include/string.h"

static bool enabled, mounted;
static const char DIR_PATH[] = "/smb";

static void idle() { floppy_idle(); }
static const FatDisk disk = {floppy_read, floppy_write, idle};

// File dates from the CMOS clock (whatever zone the machine keeps).
static uint8_t cmos(uint8_t r) { outb(0x70, r); return inb(0x71); }
long fat_clock() {
    uint8_t s, m, h, d, mo, y, b;
    do {                                                 // read until two reads agree
        while (cmos(0x0A) & 0x80) {}
        s = cmos(0); m = cmos(2); h = cmos(4); d = cmos(7); mo = cmos(8); y = cmos(9);
    } while (s != cmos(0));
    b = cmos(0x0B);
    auto bin = [&](uint8_t v) { return (b & 4) ? v : (uint8_t)((v & 15) + (v >> 4) * 10); };
    bool pm = !(b & 2) && (h & 0x80);
    h = bin(h & 0x7F);
    if (!(b & 2)) h = (uint8_t)(h % 12 + (pm ? 12 : 0));
    long Y = 2000 + bin(y), M = bin(mo), D = bin(d);
    Y -= M <= 2;                                         // days_from_civil (H. Hinnant)
    long era = (Y >= 0 ? Y : Y - 399) / 400, yoe = Y - era * 400;
    long doy = (153 * (M + (M > 2 ? -3 : 9)) + 2) / 5 + D - 1;
    long days = era * 146097 + yoe * 365 + yoe / 4 - yoe / 100 + doy - 719468;
    return days * 86400 + h * 3600L + bin(m) * 60L + bin(s);
}

static void make_path(char* out, const char* name) {
    size_t a = sizeof(DIR_PATH) - 1, b = strlen(name);
    memcpy(out, DIR_PATH, a);
    out[a] = '/';
    memcpy(out + a + 1, name, b + 1);
}

// The disk can be taken out or swapped while the game runs: check before
// each use, and remount when a different disk is in.
static bool ready() {
    if (!enabled) return false;
    int m = floppy_check_media();
    if (m < 0) {
        if (mounted) printf("Storage: floppy removed\n");
        mounted = false;
    } else if (m == 1 || !mounted) {
        mounted = fat_mount(disk);
        if (mounted) printf("Storage: FAT12 floppy mounted, %u KB free\n", fat_free_bytes() / 1024);
        else printf("Storage: the floppy isn't FAT12\n");
    }
    if (!mounted) floppy_idle();
    return mounted;
}

void storage_init(uint32_t mb_flags, uint32_t boot_device, const char* cmdline) {
    for (const char* p = cmdline; p && *p; p++)
        if (strncmp(p, "floppy=off", 10) == 0) return;
    // Multiboot boot_device: BIOS drive number in the top byte; 0x00 is A:.
    if (!(mb_flags & (1 << 1)) || (boot_device >> 24) != 0x00) return;
    if (!floppy_init()) return;
    enabled = true;
    if (!ready()) return;
    static uint8_t buf[memfs::MAX_SIZE];
    int loaded = 0;
    for (int slot = 0; slot < 4; slot++) {
        char name[] = "save1.dat", path[64];
        name[4] = (char)('1' + slot);
        make_path(path, name);
        size_t len;
        if (!fat_read(path, buf, sizeof buf, &len)) continue;
        memfs::File* f = memfs::open(name, true);
        if (!f) break;
        memcpy(f->data, buf, len);
        f->size = len;
        loaded++;
    }
    printf("Storage: %d save state%s loaded from the floppy\n", loaded, loaded == 1 ? "" : "s");
}

int storage_save(const char* name) {
    memfs::File* f = memfs::open(name, false);
    if (!f || !ready()) return 0;
    char path[64];
    make_path(path, name);
    fat_mkdir(DIR_PATH);
    if (fat_write(path, f->data, f->size)) return 1;
    printf("Storage: couldn't write %s%s\n", path, floppy_write_protected() ? " (write-protected)" : "");
    return -1;
}

bool storage_read(const char* name, void* buf, size_t cap, size_t* len) {
    if (!mounted) return false;                          // at boot only: no media check
    char path[64];
    make_path(path, name);
    return fat_read(path, buf, cap, len);
}

bool storage_write(const char* name, const void* data, size_t len) {
    if (!ready()) return false;
    char path[64];
    make_path(path, name);
    fat_mkdir(DIR_PATH);
    return fat_write(path, data, len);
}
