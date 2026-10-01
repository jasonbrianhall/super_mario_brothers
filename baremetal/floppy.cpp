// Floppy controller driver. See floppy.hpp.
#include "floppy.hpp"
#include "hw.hpp"
#include "include/stdio.h"
#include "include/string.h"

extern volatile uint32_t ticks;          // irq.cpp: the PIT at 60 Hz

namespace {

enum : uint16_t { DOR = 0x3F2, MSR = 0x3F4, FIFO = 0x3F5, DIR = 0x3F7, CCR = 0x3F7 };
enum : uint8_t { RQM = 0x80, DIO = 0x40, CB = 0x10 };
enum : uint8_t { DOR_DMA = 0x08, DOR_RUN = 0x04, DOR_MOTOR_A = 0x10 };
enum : uint8_t {
    CMD_SPECIFY = 0x03, CMD_RECALIBRATE = 0x07, CMD_SENSE_INT = 0x08, CMD_SEEK = 0x0F,
    CMD_READ = 0x66,     // READ DATA, MFM, skip deleted sectors
    CMD_WRITE = 0x45,    // WRITE DATA, MFM
};

// One track's worth of DMA buffer. ISA DMA reaches only the first 16 MiB and
// can't cross a 64 KiB boundary; the alignment takes care of the second.
alignas(65536) uint8_t dma_buf[36 * 512];

bool present;
uint32_t spt = 18, heads = 2, total = 2880;
uint8_t rate;                // CCR data rate: 0 = 500 kbit/s, 2 = 250, 3 = 1 Mbit/s
int cur_cyl = -1;
bool motor, wprot;
uint32_t motor_off_at;       // tick to stop the motor (0: not scheduled)

// Milliseconds, in 1/60 s steps.
uint32_t now() { return ticks * 50 / 3; }
void delay_ms(uint32_t ms) {
    if (ms < 20) { for (uint32_t i = 0; i < ms * 1000; i++) outb(0x80, 0); return; }   // ~1 us each
    uint32_t t = now();
    while (now() - t < ms) __asm__ volatile("pause");
}

bool send(uint8_t b) {
    for (uint32_t t = now(); now() - t < 500;)
        if ((inb(MSR) & (RQM | DIO)) == RQM) { outb(FIFO, b); return true; }
    return false;
}
bool recv(uint8_t& b) {
    for (uint32_t t = now(); now() - t < 500;)
        if ((inb(MSR) & (RQM | DIO)) == (RQM | DIO)) { b = inb(FIFO); return true; }
    return false;
}
// SENSE INTERRUPT STATUS. Returns false if nothing is pending (ST0 0x80).
bool sense(uint8_t& st0, uint8_t& pcn) {
    if (!send(CMD_SENSE_INT) || !recv(st0)) return false;
    if (st0 == 0x80) return false;
    return recv(pcn);
}
// After SEEK or RECALIBRATE: poll until the drive reports seek end.
bool wait_seek(int cyl) {
    for (uint32_t t = now(); now() - t < 3000; delay_ms(1)) {
        uint8_t st0, pcn;
        if (!sense(st0, pcn)) continue;
        if (!(st0 & 0x20)) continue;                     // not our seek-end yet
        return (st0 & 0xC0) == 0 && pcn == cyl;
    }
    return false;
}

uint8_t dor() { return DOR_DMA | DOR_RUN | (motor ? DOR_MOTOR_A : 0); }
void motor_on() {
    motor_off_at = 0;
    if (motor) return;
    motor = true;
    outb(DOR, dor());
    delay_ms(500);                                       // spin-up
}

bool reset() {
    outb(DOR, 0);
    for (int i = 0; i < 20; i++) outb(0x80, 0);          // > 4 us in reset (not by reading MSR:
                                                         // QEMU leaves reset on that, without the interrupt)
    outb(DOR, dor());
    int got = 0;                                         // one interrupt per drive (4)
    for (uint32_t t = now(); got < 4 && now() - t < 500;) {
        uint8_t st0, pcn;
        if (sense(st0, pcn)) got++; else delay_ms(1);
    }
    if (!got) return false;
    outb(CCR, rate);
    cur_cyl = -1;
    return send(CMD_SPECIFY) && send(0xDF) && send(0x02);   // step 3 ms, unload 240 ms; load 4 ms, DMA
}
bool recalibrate() {
    motor_on();
    for (int i = 0; i < 2; i++) {                        // one pass steps at most 77 tracks
        if (!send(CMD_RECALIBRATE) || !send(0)) return false;
        if (wait_seek(0)) { cur_cyl = 0; return true; }
    }
    cur_cyl = -1;
    return false;
}
bool seek(int c) {
    if (c == cur_cyl) return true;
    if (!send(CMD_SEEK) || !send(0) || !send((uint8_t)c)) return false;
    if (wait_seek(c)) { cur_cyl = c; return true; }
    cur_cyl = -1;
    return false;
}

void dma_setup(bool to_disk, uint32_t len) {
    uint32_t a = (uint32_t)(uintptr_t)dma_buf;
    outb(0x0A, 0x06);                                    // mask channel 2
    outb(0x0C, 0xFF); outb(0x04, a & 0xFF); outb(0x04, (a >> 8) & 0xFF);
    outb(0x81, (a >> 16) & 0xFF);
    outb(0x0C, 0xFF); outb(0x05, (len - 1) & 0xFF); outb(0x05, (len - 1) >> 8);
    outb(0x0B, to_disk ? 0x4A : 0x46);                   // single mode, memory->disk or disk->memory
    outb(0x0A, 0x02);                                    // unmask
}
bool transfer(bool to_disk, uint32_t c, uint32_t h, uint32_t s, uint32_t n) {
    dma_setup(to_disk, n * 512);
    const uint8_t cmd[9] = {to_disk ? CMD_WRITE : CMD_READ, (uint8_t)(h << 2), (uint8_t)c, (uint8_t)h,
                            (uint8_t)s, 2, (uint8_t)spt, 0x1B, 0xFF};
    for (uint8_t b : cmd) if (!send(b)) return false;
    // The DMA terminal count ends the command; wait for its result phase.
    uint32_t t = now();
    while ((inb(MSR) & (RQM | DIO | CB)) != (RQM | DIO | CB))
        if (now() - t > 3000) return false;
    uint8_t r[7];
    for (uint8_t& b : r) if (!recv(b)) return false;
    if ((r[0] & 0xC0) == 0) return true;
    if (r[1] & 0x02) wprot = true;                       // ST1: not writable
    return false;
}

bool xfer(bool to_disk, uint32_t lba, uint32_t n, uint8_t* buf) {
    if (!present || lba + n > total) return false;
    wprot = false;
    motor_on();
    while (n) {
        uint32_t c = lba / (spt * heads), h = (lba / spt) % heads, s = lba % spt + 1;
        uint32_t cnt = spt - (s - 1);
        if (cnt > n) cnt = n;
        if (to_disk) memcpy(dma_buf, buf, cnt * 512);
        bool ok = false;
        for (int attempt = 0; attempt < 5 && !ok && !wprot; attempt++) {
            if (attempt == 3) reset();
            if (attempt) recalibrate();
            ok = seek((int)c) && transfer(to_disk, c, h, s, cnt);
        }
        if (!ok) {
            printf("Floppy: %s error at sector %u%s\n", to_disk ? "write" : "read", lba,
                   wprot ? " (write-protected)" : "");
            return false;
        }
        if (!to_disk) memcpy(buf, dma_buf, cnt * 512);
        lba += cnt; buf += cnt * 512; n -= cnt;
    }
    return true;
}

uint8_t cmos(uint8_t reg) { outb(0x70, reg); return inb(0x71); }

} // namespace

bool floppy_init() {
    if ((uintptr_t)dma_buf + sizeof dma_buf > 0x1000000) return false;   // ISA DMA can't reach it
    // CMOS register 0x10, high nibble: drive A: type.
    switch (cmos(0x10) >> 4) {
    case 2: rate = 0; spt = 15; break;                   // 5.25" 1.2 MB
    case 3: rate = 2; spt = 9;  break;                   // 3.5" 720 KB
    case 4: rate = 0; spt = 18; break;                   // 3.5" 1.44 MB
    case 5: rate = 3; spt = 36; break;                   // 3.5" 2.88 MB
    default: printf("Floppy: no drive A:\n"); return false;
    }
    heads = 2; total = spt * heads * 80;
    if (inb(MSR) == 0xFF || !reset() || !recalibrate()) {
        printf("Floppy: no controller\n");
        motor = false; outb(DOR, dor());
        return false;
    }
    present = true;
    // Geometry from the boot sector (sector 1 of track 0 is the same on all formats).
    uint8_t b[512];
    bool ok = xfer(false, 0, 1, b);
    if (!ok && rate != 2) { rate = 2; outb(CCR, rate); ok = xfer(false, 0, 1, b); }   // 720 KB disk
    if (!ok) { printf("Floppy: can't read the disk\n"); present = false; floppy_idle(); return false; }
    uint32_t bspt = b[24] | b[25] << 8, bh = b[26] | b[27] << 8;
    uint32_t tot = b[19] | b[20] << 8;
    if (!tot) tot = b[32] | b[33] << 8 | b[34] << 16 | (uint32_t)b[35] << 24;
    if (bspt >= 8 && bspt <= 36 && (bh == 1 || bh == 2) && tot && tot <= bspt * bh * 84) {
        spt = bspt; heads = bh; total = tot;
    }
    floppy_check_media();                                // clear the change line
    printf("Floppy: drive A: %u sectors (%u/track, %u heads)\n", total, spt, heads);
    floppy_idle();
    return true;
}

bool floppy_read(uint32_t lba, uint32_t count, void* buf) { return xfer(false, lba, count, (uint8_t*)buf); }
bool floppy_write(uint32_t lba, uint32_t count, const void* buf) { return xfer(true, lba, count, (uint8_t*)buf); }
bool floppy_write_protected() { return wprot; }

int floppy_check_media() {
    if (!present) return -1;
    motor_on();
    if (!(inb(DIR) & 0x80)) return 0;
    // The change line clears on a step once a disk is in the drive.
    cur_cyl = -1;
    seek(1);
    seek(0);
    if (cur_cyl != 0) recalibrate();
    return (inb(DIR) & 0x80) ? -1 : 1;
}

void floppy_idle() { motor_off_at = (now() + 2000) | 1; }
void floppy_poll() {
    if (motor && motor_off_at && (int32_t)(now() - motor_off_at) >= 0) {
        motor = false;
        motor_off_at = 0;
        outb(DOR, dor());
    }
}
