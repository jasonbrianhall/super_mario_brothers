// Audio output for the bare-metal build: Sound Blaster 16 (ISA DMA) and
// Intel AC'97 (ICH bus master). Both run a looping DMA buffer; each frame the
// main loop hands us the APU's samples and we write them a little ahead of
// the hardware's play position. No interrupts are used.
#include <stdint.h>
#include <stddef.h>
#include "include/string.h"
#include "include/stdio.h"
#include "hw.hpp"
#include "audio.hpp"
#include "Configuration.hpp"

bool Configuration::audioEnabled = false;
int  Configuration::audioFrequency = 44100;

static AudioDriver driver = AUDIO_NONE;
static uint32_t ring_frames;      // ring length in sample frames
static uint32_t write_pos;        // next frame we will write
static uint32_t target_ahead;     // desired latency in frames

static uint32_t pci_read(int bus, int dev, int fn, int off) {
    outl(0xCF8, 0x80000000u | (bus << 16) | (dev << 11) | (fn << 8) | (off & 0xFC));
    return inl(0xCFC);
}
static void pci_write(int bus, int dev, int fn, int off, uint32_t v) {
    outl(0xCF8, 0x80000000u | (bus << 16) | (dev << 11) | (fn << 8) | (off & 0xFC));
    outl(0xCFC, v);
}
static void io_delay(int n) { while (n--) inb(0x80); }

// ================================================================ SB16
// 8-bit signed mono, auto-init DMA on channel 1. The buffer must sit below
// 16 MiB and not cross a 64 KiB boundary: it lives in .dmabuf (placed low by
// linker.ld) and its alignment keeps it inside one 64 KiB page.
#define SB_BASE 0x220
#define SB_RING 16384
static uint8_t sb_buf[SB_RING] __attribute__((aligned(65536), section(".dmabuf")));

static bool sb_write_dsp(uint8_t v) {
    for (int i = 0; i < 100000; i++)
        if (!(inb(SB_BASE + 0xC) & 0x80)) { outb(SB_BASE + 0xC, v); return true; }
    return false;
}

static bool sb16_init() {
    outb(SB_BASE + 0x6, 1); io_delay(10);
    outb(SB_BASE + 0x6, 0);
    bool ok = false;
    for (int i = 0; i < 100000 && !ok; i++)
        if ((inb(SB_BASE + 0xE) & 0x80) && inb(SB_BASE + 0xA) == 0xAA) ok = true;
    if (!ok) return false;

    sb_write_dsp(0xE1);                            // DSP version
    uint8_t major = 0;
    for (int i = 0; i < 100000; i++) if (inb(SB_BASE + 0xE) & 0x80) { major = inb(SB_BASE + 0xA); break; }
    for (int i = 0; i < 100000; i++) if (inb(SB_BASE + 0xE) & 0x80) { inb(SB_BASE + 0xA); break; }
    if (major < 4) { printf("Sound Blaster DSP %u.x found; SB16 (DSP 4.x) required\n", major); return false; }

    uint32_t rate = 44100;
    memset(sb_buf, 0, sizeof(sb_buf));

    // ISA DMA channel 1: single mode, auto-init, memory -> device
    uint32_t phys = (uint32_t)(uintptr_t)sb_buf;
    outb(0x0A, 0x05);                              // mask channel 1
    outb(0x0C, 0x00);                              // clear flip-flop
    outb(0x0B, 0x59);
    outb(0x02, phys & 0xFF); outb(0x02, (phys >> 8) & 0xFF);
    outb(0x83, (phys >> 16) & 0xFF);
    outb(0x03, (SB_RING - 1) & 0xFF); outb(0x03, (SB_RING - 1) >> 8);
    outb(0x0A, 0x01);                              // unmask

    sb_write_dsp(0xD1);                            // speaker on
    sb_write_dsp(0x41);                            // output sample rate
    sb_write_dsp(rate >> 8); sb_write_dsp(rate & 0xFF);
    uint16_t block = SB_RING / 2 - 1;              // IRQ every half; we just ack it
    sb_write_dsp(0xC6);                            // 8-bit, auto-init, FIFO
    sb_write_dsp(0x10);                            // mono, signed
    sb_write_dsp(block & 0xFF); sb_write_dsp(block >> 8);

    ring_frames = SB_RING;
    Configuration::audioFrequency = rate;
    return true;
}

static uint32_t sb16_play_pos() {
    inb(SB_BASE + 0xE);                            // ack any pending 8-bit IRQ
    outb(0x0C, 0x00);
    uint32_t lo = inb(0x03), hi = inb(0x03);
    uint32_t remaining = ((hi << 8) | lo) + 1;     // count register counts down
    if (remaining > SB_RING) remaining = SB_RING;
    return (SB_RING - remaining) % SB_RING;
}

// ================================================================ AC'97
// 16-bit signed stereo at 48 kHz, 32 buffer descriptors used as a ring.
#define AC_BUFS      32
#define AC_BUF_FRAMES 512
struct __attribute__((packed)) BDLEntry { uint32_t addr; uint16_t samples; uint16_t flags; };
static BDLEntry ac_bdl[AC_BUFS] __attribute__((aligned(8)));
static int16_t ac_buf[AC_BUFS * AC_BUF_FRAMES * 2] __attribute__((aligned(4096)));
static uint16_t ac_nam, ac_nabm;

static bool ac97_init() {
    int bus = -1, dev = 0;
    for (int b = 0; b < 256 && bus < 0; b++)
        for (int d = 0; d < 32; d++) {
            uint32_t id = pci_read(b, d, 0, 0);
            if (id == 0xFFFFFFFF) continue;
            uint32_t cls = pci_read(b, d, 0, 8) >> 16;       // class/subclass
            if (id == 0x24158086 || cls == 0x0401) { bus = b; dev = d; break; }
        }
    if (bus < 0) return false;

    ac_nam  = pci_read(bus, dev, 0, 0x10) & 0xFFFC;
    ac_nabm = pci_read(bus, dev, 0, 0x14) & 0xFFFC;
    if (!ac_nam || !ac_nabm) return false;
    pci_write(bus, dev, 0, 0x04, pci_read(bus, dev, 0, 0x04) | 0x05);  // I/O + bus master

    outl(ac_nabm + 0x2C, 0x00000002);              // cold reset release
    io_delay(20000);
    outw(ac_nam + 0x00, 0);                        // codec reset
    io_delay(20000);
    outw(ac_nam + 0x02, 0x0000);                   // master volume: max, unmuted
    outw(ac_nam + 0x18, 0x0808);                   // PCM out volume
    uint32_t rate = 48000;
    if (inw(ac_nam + 0x28) & 1) {                  // variable-rate audio supported
        outw(ac_nam + 0x2A, inw(ac_nam + 0x2A) | 1);
        outw(ac_nam + 0x2C, 48000);
        rate = inw(ac_nam + 0x2C);
    }

    memset(ac_buf, 0, sizeof(ac_buf));
    for (int i = 0; i < AC_BUFS; i++) {
        ac_bdl[i].addr = (uint32_t)(uintptr_t)&ac_buf[i * AC_BUF_FRAMES * 2];
        ac_bdl[i].samples = AC_BUF_FRAMES * 2;      // counted in 16-bit samples
        ac_bdl[i].flags = 0;
    }
    outb(ac_nabm + 0x1B, 0x02);                    // reset PCM-out engine
    io_delay(1000);
    outl(ac_nabm + 0x10, (uint32_t)(uintptr_t)ac_bdl);
    outb(ac_nabm + 0x15, AC_BUFS - 1);             // last valid index
    outb(ac_nabm + 0x1B, 0x01);                    // run

    ring_frames = AC_BUFS * AC_BUF_FRAMES;
    Configuration::audioFrequency = rate;
    return true;
}

static uint32_t ac97_play_pos() {
    uint8_t civ = inb(ac_nabm + 0x14) & 31;
    outb(ac_nabm + 0x15, (civ + AC_BUFS - 1) & 31);  // keep LVI behind us so it loops forever
    outw(ac_nabm + 0x16, 0x1C);                      // clear status bits
    uint32_t left = inw(ac_nabm + 0x18) / 2;         // frames left in current buffer
    if (left > AC_BUF_FRAMES) left = AC_BUF_FRAMES;
    return civ * AC_BUF_FRAMES + (AC_BUF_FRAMES - left);
}

// ================================================================ common
static bool arg_is(const char* cmdline, const char* want) {
    if (!cmdline) return false;
    for (const char* p = cmdline; *p; p++)
        if (strncmp(p, "audio=", 6) == 0 && strncmp(p + 6, want, strlen(want)) == 0) return true;
    return false;
}

AudioDriver audio_init(const char* cmdline) {
    bool want_off = arg_is(cmdline, "off");
    bool want_sb = arg_is(cmdline, "sb16");
    bool want_ac = arg_is(cmdline, "ac97");
    if (!want_off) {
        if (!want_sb && ac97_init()) driver = AUDIO_AC97;
        else if (!want_ac && sb16_init()) driver = AUDIO_SB16;
    }
    Configuration::audioEnabled = driver != AUDIO_NONE;
    if (driver == AUDIO_NONE) { printf("Audio: none\n"); return driver; }

    target_ahead = Configuration::audioFrequency / 15;   // ~4 frames of latency
    write_pos = (audio_play_pos() + target_ahead) % ring_frames;
    printf("Audio: %s at %d Hz\n", driver == AUDIO_AC97 ? "AC97" : "SB16", Configuration::audioFrequency);
    return driver;
}

uint32_t audio_play_pos() {
    return driver == AUDIO_AC97 ? ac97_play_pos() : driver == AUDIO_SB16 ? sb16_play_pos() : 0;
}

void audio_submit(const uint8_t* samples, int n) {
    if (driver == AUDIO_NONE) return;
    uint32_t play = audio_play_pos();
    uint32_t ahead = (write_pos + ring_frames - play) % ring_frames;
    // If we've drifted too close (underrun) or too far (overrun), resync.
    if (ahead < target_ahead / 4 || ahead > target_ahead * 3)
        write_pos = (play + target_ahead) % ring_frames;

    // The APU's samples are unsigned mix levels (0 = silence, ~130 = loudest).
    // Scale them up and run a one-pole DC blocker so the output is centered.
    static int32_t prev_in, prev_out;
    for (int i = 0; i < n; i++) {
        int32_t in = samples[i] * 160;
        int32_t out = in - prev_in + ((prev_out * 255) >> 8);
        prev_in = in;
        prev_out = out;
        if (out > 32767) out = 32767;
        if (out < -32768) out = -32768;
        if (driver == AUDIO_SB16) {
            sb_buf[write_pos] = (uint8_t)(int8_t)(out >> 8);
        } else {
            ac_buf[write_pos * 2] = (int16_t)out;
            ac_buf[write_pos * 2 + 1] = (int16_t)out;
        }
        write_pos = (write_pos + 1) % ring_frames;
    }
}

const char* audio_name() {
    return driver == AUDIO_AC97 ? "AC97" : driver == AUDIO_SB16 ? "SB16" : "none";
}
