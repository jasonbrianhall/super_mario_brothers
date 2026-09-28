// Audio output for the bare-metal build: Intel HD Audio and Intel AC'97.
// Both play from the same looping DMA ring (16-bit stereo, 48 kHz). Each frame
// the main loop hands us the APU's samples and we write them a little ahead
// of the hardware's play position. No interrupts are used.
#include <stdint.h>
#include <stddef.h>
#include "include/string.h"
#include "include/stdio.h"
#include "hw.hpp"
#include "pci.hpp"
#include "audio.hpp"
#include "Configuration.hpp"

bool Configuration::audioEnabled = false;
int  Configuration::audioFrequency = 48000;

static AudioDriver driver = AUDIO_NONE;

// Shared ring: RING_BUFS chunks of CHUNK_FRAMES stereo frames.
#define RING_BUFS    32
#define CHUNK_FRAMES 512
#define RING_FRAMES  (RING_BUFS * CHUNK_FRAMES)
static int16_t ring[RING_FRAMES * 2] __attribute__((aligned(4096)));
static uint32_t write_pos;        // next frame we will write
static uint32_t target_ahead;     // desired latency in frames

static void io_delay(int n) { while (n--) inb(0x80); }   // ~1 us each

// ================================================================ AC'97
struct __attribute__((packed)) AcBDL { uint32_t addr; uint16_t samples; uint16_t flags; };
static AcBDL ac_bdl[RING_BUFS] __attribute__((aligned(8)));
static uint16_t ac_nam, ac_nabm;

static bool ac97_init() {
    PciDevice d;
    if (!pci_find_class(0x04, 0x01, -1, &d)) return false;
    ac_nam  = pci_read(d, 0x10) & 0xFFFC;
    ac_nabm = pci_read(d, 0x14) & 0xFFFC;
    if (!ac_nam || !ac_nabm) return false;
    pci_write(d, 0x04, pci_read(d, 0x04) | 0x05);     // I/O + bus master

    outl(ac_nabm + 0x2C, 0x00000002);                 // cold reset release
    io_delay(20000);
    outw(ac_nam + 0x00, 0);                           // codec reset
    io_delay(20000);
    outw(ac_nam + 0x02, 0x0000);                      // master volume: max, unmuted
    outw(ac_nam + 0x18, 0x0808);                      // PCM out volume
    if (inw(ac_nam + 0x28) & 1) {                     // variable rate: ask for 48 kHz
        outw(ac_nam + 0x2A, inw(ac_nam + 0x2A) | 1);
        outw(ac_nam + 0x2C, 48000);
    }

    for (int i = 0; i < RING_BUFS; i++) {
        ac_bdl[i].addr = (uint32_t)(uintptr_t)&ring[i * CHUNK_FRAMES * 2];
        ac_bdl[i].samples = CHUNK_FRAMES * 2;         // counted in 16-bit samples
        ac_bdl[i].flags = 0;
    }
    outb(ac_nabm + 0x1B, 0x02);                       // reset PCM-out engine
    io_delay(1000);
    outl(ac_nabm + 0x10, (uint32_t)(uintptr_t)ac_bdl);
    outb(ac_nabm + 0x15, RING_BUFS - 1);              // last valid index
    outb(ac_nabm + 0x1B, 0x01);                       // run
    return true;
}

static uint32_t ac97_play_pos() {
    uint8_t civ = inb(ac_nabm + 0x14) & 31;
    outb(ac_nabm + 0x15, (civ + RING_BUFS - 1) & 31); // keep LVI behind us so it loops forever
    outw(ac_nabm + 0x16, 0x1C);                       // clear status bits
    uint32_t left = inw(ac_nabm + 0x18) / 2;          // frames left in current buffer
    if (left > CHUNK_FRAMES) left = CHUNK_FRAMES;
    return civ * CHUNK_FRAMES + (CHUNK_FRAMES - left);
}

// ================================================================ HD Audio
// Controller: CORB/RIRB for codec verbs, one output stream whose BDL covers
// the ring. Codec: walk the audio function group, find every connected
// output pin (line out, speaker, headphone), follow its connections to a
// DAC, unmute everything on the way and point every DAC at our stream.
static volatile uint8_t* hda;
static uint32_t corb[256] __attribute__((aligned(1024)));
static uint64_t rirb[256] __attribute__((aligned(2048)));
struct __attribute__((packed)) HdaBDL { uint64_t addr; uint32_t len; uint32_t ioc; };
static HdaBDL hda_bdl[RING_BUFS] __attribute__((aligned(128)));
static uint16_t rirb_rp;
static uint32_t sd;               // output stream descriptor offset

static inline uint8_t  r8 (uint32_t o) { return *(volatile uint8_t*)(hda + o); }
static inline uint16_t r16(uint32_t o) { return *(volatile uint16_t*)(hda + o); }
static inline uint32_t r32(uint32_t o) { return *(volatile uint32_t*)(hda + o); }
static inline void w8 (uint32_t o, uint8_t v)  { *(volatile uint8_t*)(hda + o) = v; }
static inline void w16(uint32_t o, uint16_t v) { *(volatile uint16_t*)(hda + o) = v; }
static inline void w32(uint32_t o, uint32_t v) { *(volatile uint32_t*)(hda + o) = v; }

static bool wait_bits32(uint32_t off, uint32_t mask, uint32_t want, int us) {
    while (us-- > 0) { if ((r32(off) & mask) == want) return true; io_delay(1); }
    return false;
}

// Send one verb and wait for its response. Returns false on timeout.
static bool hda_cmd(uint32_t verb, uint32_t* resp) {
    uint16_t wp = (r16(0x48) + 1) & 0xFF;
    corb[wp] = verb;
    w16(0x48, wp);
    for (int i = 0; i < 20000; i++) {
        uint16_t rwp = r16(0x58) & 0xFF;
        if (rwp != rirb_rp) {
            rirb_rp = (rirb_rp + 1) & 0xFF;
            if (resp) *resp = (uint32_t)rirb[rirb_rp];
            w8(0x5D, 0x05);                           // clear RIRB status
            return true;
        }
        io_delay(1);
    }
    return false;
}
static uint32_t verb(int cad, int nid, uint32_t v, uint32_t payload) {   // 12-bit verb, 8-bit payload
    uint32_t r = 0;
    hda_cmd((uint32_t)cad << 28 | (uint32_t)nid << 20 | v << 8 | payload, &r);
    return r;
}
static uint32_t verb4(int cad, int nid, uint32_t v, uint32_t payload) {  // 4-bit verb, 16-bit payload
    uint32_t r = 0;
    hda_cmd((uint32_t)cad << 28 | (uint32_t)nid << 20 | v << 16 | payload, &r);
    return r;
}
static uint32_t param(int cad, int nid, int p) { return verb(cad, nid, 0xF00, p); }

struct Widget { uint8_t type; uint8_t nconn; uint8_t conn[16]; uint32_t caps; };
static Widget widgets[128];

static void unmute_out(int cad, int nid) {
    uint32_t caps = param(cad, nid, 0x12);            // output amp caps
    uint32_t gain = caps & 0x7F;                      // offset = 0 dB
    verb4(cad, nid, 0x3, 0xB000 | gain);              // output, L+R
}
static void unmute_in(int cad, int nid, int index) {
    uint32_t caps = param(cad, nid, 0x0D);            // input amp caps
    uint32_t gain = caps & 0x7F;
    verb4(cad, nid, 0x3, 0x7000 | (index << 8) | gain);
}

// Depth-first search from a pin to a DAC; on success configures the path.
static int route_to_dac(int cad, int nid, int depth) {
    if (depth > 6 || nid >= 128) return -1;
    Widget& w = widgets[nid];
    if (w.type == 0) return nid;                      // audio output (DAC)
    for (int i = 0; i < w.nconn; i++) {
        int dac = route_to_dac(cad, w.conn[i], depth + 1);
        if (dac < 0) continue;
        if (w.type != 2 && w.nconn > 1)
            verb(cad, nid, 0x701, i);                 // connection select (mixers sum instead)
        if (w.caps & (1 << 1)) unmute_in(cad, nid, i);
        if (w.caps & (1 << 2)) unmute_out(cad, nid);
        verb(cad, nid, 0x705, 0);                     // power D0
        return dac;
    }
    return -1;
}

static bool hda_setup_codec(int cad, uint8_t stream_tag, uint16_t fmt) {
    uint32_t sub = param(cad, 0, 0x04);
    int fg_start = (sub >> 16) & 0xFF, fg_count = sub & 0xFF;
    bool any = false;
    for (int fg = fg_start; fg < fg_start + fg_count; fg++) {
        if ((param(cad, fg, 0x05) & 0xFF) != 0x01) continue;  // audio function group only
        verb(cad, fg, 0x705, 0);                               // power up the group
        io_delay(10000);
        uint32_t ws = param(cad, fg, 0x04);
        int start = (ws >> 16) & 0xFF, count = ws & 0xFF;

        // Read every widget's type and connection list.
        for (int n = start; n < start + count && n < 128; n++) {
            Widget& w = widgets[n];
            w.caps = param(cad, n, 0x09);
            w.type = (w.caps >> 20) & 0xF;
            uint32_t cl = param(cad, n, 0x0E);
            int len = cl & 0x7F;
            bool lng = cl & 0x80;
            w.nconn = 0;
            if (!(w.caps & (1 << 8)) || lng) continue;          // no list, or long form (rare)
            for (int i = 0; i < len && w.nconn < 16; i += 4) {
                uint32_t e = verb(cad, n, 0xF02, i);
                for (int k = 0; k < 4 && i + k < len; k++)
                    w.conn[w.nconn++] = (e >> (8 * k)) & 0xFF;
            }
        }

        // Route every connected analog output pin.
        for (int n = start; n < start + count && n < 128; n++) {
            Widget& w = widgets[n];
            if (w.type != 4) continue;                               // pin complex
            uint32_t pcaps = param(cad, n, 0x0C);
            if (!(pcaps & (1 << 4))) continue;                       // not output capable
            uint32_t cfg = verb(cad, n, 0xF1C, 0);
            int conn = cfg >> 30, dev = (cfg >> 20) & 0xF;
            if (conn == 1) continue;                                 // nothing attached
            if (dev != 0x0 && dev != 0x1 && dev != 0x2) continue;    // line out, speaker, HP
            int dac = route_to_dac(cad, n, 0);
            if (dac < 0) continue;
            verb(cad, n, 0x707, dev == 0x2 ? 0xC0 : 0x40);           // out enable (+HP amp)
            if (pcaps & (1 << 16)) verb(cad, n, 0x70C, 0x02);        // EAPD on
            verb4(cad, dac, 0x2, fmt);                               // converter format
            verb(cad, dac, 0x706, stream_tag << 4);                  // stream tag, channel 0
            unmute_out(cad, dac);
            verb(cad, dac, 0x705, 0);
            printf("HDA: codec %d pin %d -> DAC %d (%s)\n", cad, n, dac,
                   dev == 0 ? "line out" : dev == 1 ? "speaker" : "headphone");
            any = true;
        }
    }
    return any;
}

static bool hda_init() {
    PciDevice d;
    if (!pci_find_class(0x04, 0x03, -1, &d)) return false;
    uint32_t bar = pci_read(d, 0x10);
    uint64_t base = bar & 0xFFFFFFF0;
    if ((bar & 0x6) == 0x4) base |= (uint64_t)pci_read(d, 0x14) << 32;
    if (base == 0 || base >= phys_limit) { printf("HDA: BAR out of reach\n"); return false; }
    hda = (volatile uint8_t*)(uintptr_t)base;
    pci_write(d, 0x04, pci_read(d, 0x04) | 0x06);    // memory + bus master

    // Controller reset.
    w32(0x08, r32(0x08) & ~1u);
    if (!wait_bits32(0x08, 1, 0, 100000)) return false;
    w32(0x08, r32(0x08) | 1);
    if (!wait_bits32(0x08, 1, 1, 100000)) return false;
    io_delay(2000);                                   // codecs report in within 521 us
    uint16_t codecs = r16(0x0E);
    if (!codecs) { printf("HDA: no codecs\n"); return false; }

    // CORB / RIRB, 256 entries each.
    w8(0x4C, 0); w8(0x5C, 0);                         // stop DMA engines
    io_delay(1000);
    w32(0x40, (uint32_t)(uintptr_t)corb); w32(0x44, 0);
    w8(0x4E, 0x02);
    w16(0x48, 0);
    w16(0x4A, 0x8000);                                // reset read pointer
    for (int i = 0; i < 1000 && !(r16(0x4A) & 0x8000); i++) io_delay(1);
    w16(0x4A, 0);
    for (int i = 0; i < 1000 && (r16(0x4A) & 0x8000); i++) io_delay(1);
    w32(0x50, (uint32_t)(uintptr_t)rirb); w32(0x54, 0);
    w8(0x5E, 0x02);
    w16(0x58, 0x8000);                                // reset write pointer
    w16(0x5A, 0xFF);                                  // response interrupt count (unused)
    rirb_rp = 0;
    w8(0x4C, 0x02);                                   // CORB run
    w8(0x5C, 0x02);                                   // RIRB run

    // First output stream: descriptors follow the input streams.
    uint16_t gcap = r16(0x00);
    int iss = (gcap >> 8) & 0xF, oss = (gcap >> 12) & 0xF;
    if (!oss) { printf("HDA: no output streams\n"); return false; }
    sd = 0x80 + iss * 0x20;
    const uint8_t tag = 1;
    const uint16_t fmt = 0x0011;                      // 48 kHz, 16-bit, 2 channels

    bool routed = false;
    for (int cad = 0; cad < 15; cad++)
        if (codecs & (1 << cad)) routed |= hda_setup_codec(cad, tag, fmt);
    if (!routed) { printf("HDA: no usable output pin\n"); return false; }

    // Stream reset, then program the BDL over the shared ring.
    w8(sd + 0, r8(sd + 0) | 1);
    for (int i = 0; i < 1000 && !(r8(sd + 0) & 1); i++) io_delay(1);
    w8(sd + 0, r8(sd + 0) & ~1);
    for (int i = 0; i < 1000 && (r8(sd + 0) & 1); i++) io_delay(1);

    for (int i = 0; i < RING_BUFS; i++) {
        hda_bdl[i].addr = (uint64_t)(uintptr_t)&ring[i * CHUNK_FRAMES * 2];
        hda_bdl[i].len = CHUNK_FRAMES * 4;
        hda_bdl[i].ioc = 0;
    }
    w32(sd + 0x18, (uint32_t)(uintptr_t)hda_bdl);
    w32(sd + 0x1C, 0);
    w32(sd + 0x08, sizeof(ring));                     // cyclic buffer length
    w16(sd + 0x0C, RING_BUFS - 1);                    // last valid index
    w16(sd + 0x12, fmt);
    w8(sd + 0x02, tag << 4);                          // stream number (CTL bits 23:20)
    w8(sd + 0x00, 0x02);                              // run
    return true;
}

static uint32_t hda_play_pos() {
    return (r32(sd + 0x04) / 4) % RING_FRAMES;       // link position in buffer
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
    bool want_hda = arg_is(cmdline, "hda");
    bool want_ac  = arg_is(cmdline, "ac97");
    bool any = !want_hda && !want_ac;
    memset(ring, 0, sizeof(ring));
    if (!want_off) {
        if ((any || want_hda) && hda_init()) driver = AUDIO_HDA;
        else if ((any || want_ac) && ac97_init()) driver = AUDIO_AC97;
    }
    Configuration::audioEnabled = driver != AUDIO_NONE;
    if (driver == AUDIO_NONE) { printf("Audio: none\n"); return driver; }

    target_ahead = Configuration::audioFrequency / 15;   // ~4 frames of latency
    write_pos = (audio_play_pos() + target_ahead) % RING_FRAMES;
    printf("Audio: %s at %d Hz\n", audio_name(), Configuration::audioFrequency);
    return driver;
}

uint32_t audio_play_pos() {
    switch (driver) {
    case AUDIO_HDA:  return hda_play_pos();
    case AUDIO_AC97: return ac97_play_pos();
    default:         return 0;
    }
}

void audio_submit(const uint8_t* samples, int n) {
    if (driver == AUDIO_NONE) return;
    uint32_t play = audio_play_pos();
    uint32_t ahead = (write_pos + RING_FRAMES - play) % RING_FRAMES;
    // If we've drifted too close (underrun) or too far (overrun), resync.
    if (ahead < target_ahead / 4 || ahead > target_ahead * 3)
        write_pos = (play + target_ahead) % RING_FRAMES;

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
        ring[write_pos * 2] = (int16_t)out;
        ring[write_pos * 2 + 1] = (int16_t)out;
        write_pos = (write_pos + 1) % RING_FRAMES;
    }
}

const char* audio_name() {
    return driver == AUDIO_HDA ? "HD Audio" : driver == AUDIO_AC97 ? "AC97" : "none";
}
