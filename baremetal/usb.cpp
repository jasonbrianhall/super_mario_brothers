// xHCI USB keyboard driver (polled, boot protocol).
//
// Takes the controller from the BIOS, resets it, enumerates keyboards on the
// root ports (and ones plugged in later), and turns their 8-byte boot reports
// into the same PS/2 set-1 scancodes the rest of the kernel already handles.
// Hubs aren't supported: keyboards must be on a root port.
#include <stdint.h>
#include <stddef.h>
#include "include/string.h"
#include "include/stdio.h"
#include "hw.hpp"
#include "pci.hpp"
#include "usb.hpp"

void kbd_push(uint8_t b);                 // kernel.cpp: feeds the scancode queue

static void io_delay(int n) { while (n--) inb(0x80); }   // ~1 us each
static void delay_ms(int ms) { io_delay(ms * 1000); }
#define barrier() __asm__ volatile("" ::: "memory")

// ---------------------------------------------------------------- memory
// All controller-visible structures come from one zeroed, identity-mapped pool.
static uint8_t pool[2 << 20] __attribute__((aligned(4096)));
static size_t pool_used;
static void* dma_alloc(size_t size, size_t align) {
    pool_used = (pool_used + align - 1) & ~(align - 1);
    if (pool_used + size > sizeof(pool)) return nullptr;
    void* p = pool + pool_used;
    pool_used += size;
    memset(p, 0, size);
    return p;
}
static inline uint64_t phys(const volatile void* p) { return (uint64_t)(uintptr_t)p; }

// ---------------------------------------------------------------- registers
static volatile uint8_t *cap, *op, *rt;
static volatile uint32_t* db;
static inline uint32_t rd(volatile uint8_t* b, uint32_t o) { return *(volatile uint32_t*)(b + o); }
static inline void wr(volatile uint8_t* b, uint32_t o, uint32_t v) { *(volatile uint32_t*)(b + o) = v; }
static inline void wr64(volatile uint8_t* b, uint32_t o, uint64_t v) { wr(b, o, (uint32_t)v); wr(b, o + 4, (uint32_t)(v >> 32)); }
static inline uint32_t portsc(int port) { return rd(op, 0x400 + 0x10 * (port - 1)); }
static inline void set_portsc(int port, uint32_t v) { wr(op, 0x400 + 0x10 * (port - 1), v); }

// Bits that must be written back unchanged when touching PORTSC; everything
// else (the write-1-to-clear change bits, PED) is written as 0.
static const uint32_t PORT_KEEP = (1u << 0) | (1u << 3) | (0xFu << 10) | (1u << 30) |
                                  (0xFu << 5) | (1u << 9) | (3u << 14) | (7u << 25);
enum {
    PORT_CCS = 1 << 0, PORT_PED = 1 << 1, PORT_PR = 1 << 4, PORT_PP = 1 << 9,
    PORT_CSC = 1 << 17, PORT_PRC = 1 << 21, PORT_CHANGES = 0x7F << 17,
};

// ---------------------------------------------------------------- rings
struct Trb { uint32_t d0, d1, d2, d3; };
enum {
    TRB_NORMAL = 1, TRB_SETUP = 2, TRB_DATA = 3, TRB_STATUS = 4, TRB_LINK = 6,
    TRB_ENABLE_SLOT = 9, TRB_ADDRESS_DEVICE = 11, TRB_CONFIGURE_EP = 12, TRB_EVALUATE_CTX = 13,
    TRB_TRANSFER_EVENT = 32, TRB_CMD_COMPLETION = 33, TRB_PORT_STATUS = 34,
};
#define RING_TRBS 64

struct Ring {
    volatile Trb* trb;
    int enq;
    uint32_t cycle;
};

static bool ring_init(Ring& r) {
    r.trb = (volatile Trb*)dma_alloc(RING_TRBS * sizeof(Trb), 64);
    if (!r.trb) return false;
    r.enq = 0;
    r.cycle = 1;
    volatile Trb& link = r.trb[RING_TRBS - 1];     // last TRB links back, toggling cycle
    link.d0 = (uint32_t)phys(r.trb);
    link.d1 = (uint32_t)(phys(r.trb) >> 32);
    link.d3 = TRB_LINK << 10 | (1 << 1);
    return true;
}

static uint64_t ring_push(Ring& r, uint32_t d0, uint32_t d1, uint32_t d2, uint32_t d3) {
    volatile Trb& t = r.trb[r.enq];
    uint64_t addr = phys(&t);
    t.d0 = d0; t.d1 = d1; t.d2 = d2;
    barrier();
    t.d3 = (d3 & ~1u) | r.cycle;
    if (++r.enq == RING_TRBS - 1) {
        volatile Trb& link = r.trb[RING_TRBS - 1];
        link.d3 = (link.d3 & ~1u) | r.cycle;       // hand the link TRB over too
        r.enq = 0;
        r.cycle ^= 1;
    }
    return addr;
}

static Ring cmd_ring;
static volatile Trb* evt;
static int evt_deq;
static uint32_t evt_cycle = 1;
#define EVT_TRBS 256

static bool next_event(Trb* out) {
    volatile Trb& e = evt[evt_deq];
    if ((e.d3 & 1) != evt_cycle) return false;
    barrier();
    out->d0 = e.d0; out->d1 = e.d1; out->d2 = e.d2; out->d3 = e.d3;
    if (++evt_deq == EVT_TRBS) { evt_deq = 0; evt_cycle ^= 1; }
    wr64(rt, 0x38, phys(&evt[evt_deq]) | (1 << 3));  // ERDP, clear busy
    return true;
}

// ---------------------------------------------------------------- devices
static size_t csz;                         // context size: 32 or 64 bytes
static volatile uint64_t* dcbaa;
static int max_slots, num_ports;

struct Keyboard {
    bool active;
    int port, slot, speed, dci, mps;
    volatile uint8_t* out_ctx;
    volatile uint8_t* in_ctx;
    Ring ep0, intr;
    volatile uint8_t* reports;             // one 8-byte buffer per ring slot
    uint8_t prev[8];
};
#define MAX_KBD 4
static Keyboard kbds[MAX_KBD];
static bool port_dirty[256];
static bool ready;

static inline volatile uint32_t* ctx(volatile uint8_t* base, int idx) {
    return (volatile uint32_t*)(base + idx * csz);
}

// ---------------------------------------------------------------- key reports
// HID usage -> PS/2 set-1 scancode (0x100 flag = E0-prefixed).
static const struct { uint8_t usage; uint16_t code; } keymap[] = {
    {0x52, 0x148}, {0x51, 0x150}, {0x50, 0x14B}, {0x4F, 0x14D},     // arrows
    {0x1B, 0x2D}, {0x1D, 0x2C}, {0x2F, 0x1A}, {0x30, 0x1B},         // X, Z, [, ]
    {0x1A, 0x11}, {0x04, 0x1E}, {0x16, 0x1F}, {0x07, 0x20},         // W, A, S, D
    {0x0A, 0x22}, {0x09, 0x21}, {0x14, 0x10}, {0x08, 0x12},         // G, F, Q, E
    {0x3E, 0x3F}, {0x3F, 0x40}, {0x40, 0x41}, {0x41, 0x42},         // F5-F8
    {0x45, 0x58}, {0x13, 0x19}, {0x29, 0x01}, {0x3A, 0x3B},         // F12, P, Esc, F1
};

static void emit(uint16_t code, bool down) {
    if (code & 0x100) kbd_push(0xE0);
    kbd_push((uint8_t)(code & 0x7F) | (down ? 0 : 0x80));
}
static void emit_usage(uint8_t usage, bool down) {
    for (auto& k : keymap) if (k.usage == usage) { emit(k.code, down); return; }
}

static void handle_report(Keyboard& k, const volatile uint8_t* r) {
    uint8_t cur[8];
    for (int i = 0; i < 8; i++) cur[i] = r[i];
    if (cur[2] == 1) return;                               // rollover error: ignore
    uint8_t mchg = cur[0] ^ k.prev[0];
    if (mchg & 0x02) emit(0x2A, cur[0] & 0x02);            // left shift
    if (mchg & 0x20) emit(0x36, cur[0] & 0x20);            // right shift
    for (int i = 2; i < 8; i++) {                          // releases
        uint8_t u = k.prev[i];
        if (!u) continue;
        bool still = false;
        for (int j = 2; j < 8; j++) still |= cur[j] == u;
        if (!still) emit_usage(u, false);
    }
    for (int i = 2; i < 8; i++) {                          // presses
        uint8_t u = cur[i];
        if (!u) continue;
        bool was = false;
        for (int j = 2; j < 8; j++) was |= k.prev[j] == u;
        if (!was) emit_usage(u, true);
    }
    memcpy(k.prev, cur, 8);
}

static void queue_report(Keyboard& k) {
    int idx = k.intr.enq;
    uint64_t buf = phys(k.reports + idx * 8);
    ring_push(k.intr, (uint32_t)buf, (uint32_t)(buf >> 32), 8,
              TRB_NORMAL << 10 | (1 << 5) | (1 << 2));     // IOC, ISP
}

static void release_all(Keyboard& k) {
    uint8_t empty[8] = {0};
    handle_report(k, empty);
}

// Events that arrive while we're waiting for something else.
static void dispatch(const Trb& e) {
    int type = (e.d3 >> 10) & 0x3F;
    if (type == TRB_PORT_STATUS) {
        int port = (e.d0 >> 24) & 0xFF;
        if (port >= 1 && port <= num_ports) port_dirty[port] = true;
        return;
    }
    if (type != TRB_TRANSFER_EVENT) return;
    int slot = e.d3 >> 24, ep = (e.d3 >> 16) & 0x1F, cc = e.d2 >> 24;
    for (auto& k : kbds) {
        if (!k.active || k.slot != slot || k.dci != ep) continue;
        uint64_t trb = (uint64_t)e.d1 << 32 | e.d0;
        int idx = (int)((trb - phys(k.intr.trb)) / sizeof(Trb));
        if (idx >= 0 && idx < RING_TRBS && (cc == 1 || cc == 13))
            handle_report(k, k.reports + idx * 8);
        if (cc == 1 || cc == 13) queue_report(k);
        else { release_all(k); k.active = false; printf("USB: keyboard on port %d stopped (code %d)\n", k.port, cc); }
        db[slot] = k.dci;
    }
}

// Wait for an event of 'type' (and, for transfers, matching slot/endpoint).
static bool wait_event(int type, int slot, int ep, Trb* out, int ms = 500) {
    for (int t = 0; t < ms * 20; t++) {
        Trb e;
        while (next_event(&e)) {
            int et = (e.d3 >> 10) & 0x3F;
            bool match = et == type;
            if (match && type == TRB_TRANSFER_EVENT)
                match = (int)(e.d3 >> 24) == slot && (int)((e.d3 >> 16) & 0x1F) == ep;
            if (match) { *out = e; return true; }
            dispatch(e);
        }
        io_delay(50);
    }
    return false;
}

static int command(uint32_t d0, uint32_t d1, uint32_t d2, uint32_t d3, Trb* ev) {
    ring_push(cmd_ring, d0, d1, d2, d3);
    db[0] = 0;
    Trb e;
    if (!wait_event(TRB_CMD_COMPLETION, 0, 0, &e)) return -1;
    if (ev) *ev = e;
    return e.d2 >> 24;                                  // completion code (1 = success)
}

// A control transfer on endpoint 0. Returns true on success.
static bool control(Keyboard& k, uint8_t type, uint8_t req, uint16_t value, uint16_t index,
                    uint16_t len, volatile void* buf) {
    bool in = type & 0x80;
    uint32_t trt = len ? (in ? 3 : 2) : 0;
    ring_push(k.ep0, type | req << 8 | (uint32_t)value << 16, index | (uint32_t)len << 16, 8,
              TRB_SETUP << 10 | (1 << 6) | trt << 16);
    if (len) {
        uint64_t p = phys(buf);
        ring_push(k.ep0, (uint32_t)p, (uint32_t)(p >> 32), len, TRB_DATA << 10 | (in ? 1 << 16 : 0));
    }
    ring_push(k.ep0, 0, 0, 0, TRB_STATUS << 10 | (1 << 5) | ((len && in) ? 0 : 1 << 16));
    db[k.slot] = 1;
    Trb e;
    if (!wait_event(TRB_TRANSFER_EVENT, k.slot, 1, &e)) return false;
    int cc = e.d2 >> 24;
    return cc == 1 || cc == 13;
}

// ---------------------------------------------------------------- enumeration
static bool reset_port(int port) {
    uint32_t sc = portsc(port);
    if (!(sc & PORT_CCS)) return false;
    if (!(sc & PORT_PED)) {                             // USB2 ports need a reset; USB3 enable themselves
        set_portsc(port, (sc & PORT_KEEP) | PORT_PR);
        for (int i = 0; i < 500 && !(portsc(port) & PORT_PRC); i++) delay_ms(1);
    }
    sc = portsc(port);
    set_portsc(port, (sc & PORT_KEEP) | (sc & PORT_CHANGES));   // ack change bits
    delay_ms(20);                                               // reset recovery
    return portsc(port) & PORT_PED;
}

static void setup_port(int port) {
    for (auto& k : kbds) if (k.active && k.port == port) return;
    Keyboard* kp = nullptr;
    for (auto& k : kbds) if (!k.active) { kp = &k; break; }
    if (!kp) return;
    Keyboard& k = *kp;
    memset((void*)&k, 0, sizeof(k));
    if (!reset_port(port)) return;
    k.port = port;
    k.speed = (portsc(port) >> 10) & 0xF;               // 1 FS, 2 LS, 3 HS, 4+ SS

    Trb ev;
    if (command(0, 0, 0, TRB_ENABLE_SLOT << 10, &ev) != 1) { printf("USB: port %d: no slot\n", port); return; }
    k.slot = ev.d3 >> 24;
    if (k.slot < 1 || k.slot > max_slots) return;

    k.out_ctx = (volatile uint8_t*)dma_alloc(32 * csz, 64);
    k.in_ctx  = (volatile uint8_t*)dma_alloc(33 * csz, 64);
    k.reports = (volatile uint8_t*)dma_alloc(RING_TRBS * 8, 64);
    if (!k.out_ctx || !k.in_ctx || !k.reports || !ring_init(k.ep0) || !ring_init(k.intr)) return;
    dcbaa[k.slot] = phys(k.out_ctx);

    // Address Device: slot context + endpoint 0.
    int mps0 = k.speed == 2 || k.speed == 1 ? 8 : k.speed == 3 ? 64 : 512;
    ctx(k.in_ctx, 0)[1] = 0x3;                          // add slot + EP0
    volatile uint32_t* slot = ctx(k.in_ctx, 1);
    slot[0] = (uint32_t)k.speed << 20 | 1u << 27;
    slot[1] = (uint32_t)port << 16;
    volatile uint32_t* ep0 = ctx(k.in_ctx, 2);
    ep0[1] = 3 << 1 | 4 << 3 | (uint32_t)mps0 << 16;    // CErr 3, control, max packet
    uint64_t r0 = phys(k.ep0.trb) | 1;
    ep0[2] = (uint32_t)r0; ep0[3] = (uint32_t)(r0 >> 32);
    ep0[4] = 8;
    uint64_t ic = phys(k.in_ctx);
    int cc = command((uint32_t)ic, (uint32_t)(ic >> 32), 0, TRB_ADDRESS_DEVICE << 10 | (uint32_t)k.slot << 24, nullptr);
    if (cc != 1) { printf("USB: port %d: address failed (%d)\n", port, cc); return; }
    delay_ms(2);

    static volatile uint8_t desc[256] __attribute__((aligned(64)));
    if (!control(k, 0x80, 6, 0x0100, 0, 8, desc)) { printf("USB: port %d: no descriptor\n", port); return; }
    int mps = desc[7];
    if (mps && mps != mps0 && k.speed < 4) {            // fix EP0 max packet size
        memset((void*)ctx(k.in_ctx, 0), 0, csz);
        ctx(k.in_ctx, 0)[1] = 0x2;
        ep0[1] = (ep0[1] & 0xFFFF) | (uint32_t)mps << 16;
        command((uint32_t)ic, (uint32_t)(ic >> 32), 0, TRB_EVALUATE_CTX << 10 | (uint32_t)k.slot << 24, nullptr);
    }
    if (!control(k, 0x80, 6, 0x0200, 0, 9, desc)) return;
    int total = desc[2] | desc[3] << 8;
    if (total > (int)sizeof(desc)) total = sizeof(desc);
    if (!control(k, 0x80, 6, 0x0200, 0, total, desc)) return;
    int config = desc[5];

    // Find a boot keyboard interface and its interrupt IN endpoint.
    int iface = -1, ep_addr = 0, ep_mps = 8, ep_interval = 10;
    bool in_kbd = false;
    for (int i = 0; i + 1 < total && desc[i] >= 2; i += desc[i]) {
        uint8_t type = desc[i + 1];
        if (type == 4) {
            in_kbd = desc[i + 5] == 3 && desc[i + 6] == 1 && desc[i + 7] == 1;
            if (in_kbd && iface < 0) iface = desc[i + 2];
        } else if (type == 5 && in_kbd && !ep_addr && (desc[i + 2] & 0x80) && (desc[i + 3] & 3) == 3) {
            ep_addr = desc[i + 2];
            ep_mps = (desc[i + 4] | desc[i + 5] << 8) & 0x7FF;
            ep_interval = desc[i + 6];
        }
    }
    if (iface < 0 || !ep_addr) { printf("USB: port %d: not a keyboard\n", port); return; }

    if (!control(k, 0x00, 9, config, 0, 0, nullptr)) return;          // SET_CONFIGURATION
    control(k, 0x21, 0x0B, 0, iface, 0, nullptr);                      // SET_PROTOCOL boot
    control(k, 0x21, 0x0A, 0, iface, 0, nullptr);                      // SET_IDLE (may stall)

    // Configure the interrupt IN endpoint.
    k.dci = (ep_addr & 0xF) * 2 + 1;
    k.mps = ep_mps;
    int interval;
    if (k.speed == 1 || k.speed == 2) {                 // bInterval in ms -> 2^n x 125 us
        interval = 3;
        while (interval < 10 && (1 << (interval + 1)) <= ep_interval * 8) interval++;
    } else {
        interval = ep_interval ? ep_interval - 1 : 0;
        if (interval > 15) interval = 15;
    }
    memset((void*)k.in_ctx, 0, 33 * csz);
    ctx(k.in_ctx, 0)[1] = 1 | 1u << k.dci;
    slot = ctx(k.in_ctx, 1);
    slot[0] = (uint32_t)k.speed << 20 | (uint32_t)k.dci << 27;
    slot[1] = (uint32_t)port << 16;
    volatile uint32_t* ep = ctx(k.in_ctx, 1 + k.dci);
    ep[0] = (uint32_t)interval << 16;
    ep[1] = 3 << 1 | 7 << 3 | (uint32_t)ep_mps << 16;  // CErr 3, interrupt IN
    uint64_t ri = phys(k.intr.trb) | 1;
    ep[2] = (uint32_t)ri; ep[3] = (uint32_t)(ri >> 32);
    ep[4] = 8 | (uint32_t)ep_mps << 16;
    cc = command((uint32_t)ic, (uint32_t)(ic >> 32), 0, TRB_CONFIGURE_EP << 10 | (uint32_t)k.slot << 24, nullptr);
    if (cc != 1) { printf("USB: port %d: configure failed (%d)\n", port, cc); return; }

    k.active = true;
    for (int i = 0; i < 8; i++) queue_report(k);
    db[k.slot] = k.dci;
    printf("USB: keyboard on port %d (slot %d, %s speed)\n", port, k.slot,
           k.speed == 2 ? "low" : k.speed == 1 ? "full" : k.speed == 3 ? "high" : "super");
}

// ---------------------------------------------------------------- init
static bool bios_handoff() {
    uint32_t xecp = (rd(cap, 0x10) >> 16) * 4;
    for (int guard = 0; xecp && guard < 64; guard++) {
        uint32_t v = rd(cap, xecp);
        if ((v & 0xFF) == 1) {                          // USB legacy support
            wr(cap, xecp, v | (1u << 24));              // OS owned
            for (int i = 0; i < 1000 && (rd(cap, xecp) & (1u << 16)); i++) delay_ms(1);
            if (rd(cap, xecp) & (1u << 16)) { printf("USB: BIOS won't release the controller\n"); return false; }
            wr(cap, xecp + 4, 0xE0000000);              // disable SMIs, clear their status
            return true;
        }
        uint32_t next = (v >> 8) & 0xFF;
        if (!next) break;
        xecp += next * 4;
    }
    return true;
}

static bool init_controller(const PciDevice& d) {
    uint32_t bar = pci_read(d, 0x10);
    uint64_t base = bar & 0xFFFFFFF0;
    if ((bar & 0x6) == 0x4) base |= (uint64_t)pci_read(d, 0x14) << 32;
    if (!base || base >= phys_limit) { printf("USB: xHCI registers above 4 GiB\n"); return false; }
    pci_write(d, 0x04, pci_read(d, 0x04) | 0x06);      // memory + bus master

    cap = (volatile uint8_t*)(uintptr_t)base;
    op = cap + (rd(cap, 0x00) & 0xFF);
    rt = cap + (rd(cap, 0x18) & ~0x1Fu);
    db = (volatile uint32_t*)(cap + (rd(cap, 0x14) & ~0x3u));
    uint32_t hcs1 = rd(cap, 0x04), hcs2 = rd(cap, 0x08), hcc1 = rd(cap, 0x10);
    max_slots = hcs1 & 0xFF;
    num_ports = hcs1 >> 24;
    csz = (hcc1 & (1 << 2)) ? 64 : 32;
    if (max_slots > 32) max_slots = 32;

    if (!bios_handoff()) return false;

    // Halt and reset.
    wr(op, 0x00, rd(op, 0x00) & ~1u);
    for (int i = 0; i < 100 && !(rd(op, 0x04) & 1); i++) delay_ms(1);
    wr(op, 0x00, 1 << 1);
    for (int i = 0; i < 1000 && (rd(op, 0x00) & (1 << 1)); i++) delay_ms(1);
    for (int i = 0; i < 1000 && (rd(op, 0x04) & (1 << 11)); i++) delay_ms(1);
    if (rd(op, 0x00) & (1 << 1)) { printf("USB: controller reset timed out\n"); return false; }

    // Device context array, with scratchpad buffers if the controller wants them.
    dcbaa = (volatile uint64_t*)dma_alloc((max_slots + 1) * 8, 64);
    int scratch = ((hcs2 >> 27) & 0x1F) | (((hcs2 >> 21) & 0x1F) << 5);
    if (scratch) {
        volatile uint64_t* arr = (volatile uint64_t*)dma_alloc(scratch * 8, 64);
        for (int i = 0; i < scratch; i++) {
            void* page = dma_alloc(4096, 4096);
            if (!arr || !page) { printf("USB: out of memory for scratchpad\n"); return false; }
            arr[i] = phys(page);
        }
        dcbaa[0] = phys(arr);
    }
    if (!dcbaa || !ring_init(cmd_ring)) return false;

    // One event ring segment.
    evt = (volatile Trb*)dma_alloc(EVT_TRBS * sizeof(Trb), 64);
    volatile uint64_t* erst = (volatile uint64_t*)dma_alloc(16, 64);
    if (!evt || !erst) return false;
    erst[0] = phys(evt);
    erst[1] = EVT_TRBS;

    wr(op, 0x38, max_slots);                            // CONFIG: slots enabled
    wr64(op, 0x30, phys(dcbaa));
    wr64(op, 0x18, phys(cmd_ring.trb) | 1);             // CRCR, cycle 1
    wr(rt, 0x28, 1);                                    // ERSTSZ
    wr64(rt, 0x38, phys(evt));                          // ERDP
    wr64(rt, 0x30, phys(erst));                         // ERSTBA
    wr(rt, 0x20, 0);                                    // IMAN: no interrupts, we poll
    wr(op, 0x00, 1);                                    // run
    for (int i = 0; i < 100 && (rd(op, 0x04) & 1); i++) delay_ms(1);

    for (int p = 1; p <= num_ports; p++) {              // make sure ports are powered
        uint32_t sc = portsc(p);
        if (!(sc & PORT_PP)) set_portsc(p, (sc & PORT_KEEP) | PORT_PP);
    }
    delay_ms(100);                                      // let devices connect
    return true;
}

bool usb_init(const char* cmdline) {
    for (const char* p = cmdline; p && *p; p++)
        if (strncmp(p, "usb=off", 7) == 0) { printf("USB: disabled\n"); return false; }
    PciDevice d;
    if (!pci_find_class(0x0C, 0x03, 0x30, &d)) { printf("USB: no xHCI controller\n"); return false; }
    if (!init_controller(d)) return false;
    ready = true;
    for (int p = 1; p <= num_ports; p++)
        if (portsc(p) & PORT_CCS) setup_port(p);
    int n = 0;
    for (auto& k : kbds) n += k.active;
    printf("USB: xHCI with %d ports, %d keyboard%s\n", num_ports, n, n == 1 ? "" : "s");
    return true;
}

void usb_poll() {
    if (!ready) return;
    Trb e;
    for (int i = 0; i < 64 && next_event(&e); i++) dispatch(e);
    for (int p = 1; p <= num_ports; p++) {
        if (!port_dirty[p]) continue;
        port_dirty[p] = false;
        uint32_t sc = portsc(p);
        set_portsc(p, (sc & PORT_KEEP) | (sc & PORT_CHANGES));   // ack
        if (sc & PORT_CCS) {
            if (sc & PORT_CSC || !(sc & PORT_PED)) setup_port(p);
        } else {
            for (auto& k : kbds)
                if (k.active && k.port == p) {
                    release_all(k);
                    k.active = false;
                    command(0, 0, 0, 10 << 10 | (uint32_t)k.slot << 24, nullptr);   // disable slot
                    printf("USB: keyboard on port %d unplugged\n", p);
                }
        }
    }
}
