// Bare-metal frontend for Super Mario Bros. Virtualized.
// Boots via Multiboot (GRUB or QEMU -kernel), takes the ROM as a boot
// module, and runs the engine on a framebuffer with PS/2 keyboard input.
// A PS/2 or USB mouse opens a clickable menu (save/load/reset/help), and
// save states persist on the boot floppy (storage.cpp).
#include <stdint.h>
#include <stddef.h>
#include "include/string.h"
#include "include/stdio.h"
#include "include/iostream"
#include "hw.hpp"
#include "font.h"
#include "audio.hpp"
#include "pci.hpp"
#include "usb.hpp"
#include "floppy.hpp"
#include "storage.hpp"
#include "memfs.hpp"
#include "Configuration.hpp"
#include "SMB/SMBEngine.hpp"
#include "Emulation/Controller.hpp"

// ---------------------------------------------------------------- serial
#define COM1 0x3F8
extern "C" void serial_putc(char c) {
    if (c == '\n') serial_putc('\r');
    while (!(inb(COM1 + 5) & 0x20)) {}
    outb(COM1, (uint8_t)c);
}
extern "C" void serial_puts(const char* s) { while (*s) serial_putc(*s++); }
static void serial_init() {
    outb(COM1 + 1, 0x00); outb(COM1 + 3, 0x80); outb(COM1 + 0, 0x01);
    outb(COM1 + 1, 0x00); outb(COM1 + 3, 0x03); outb(COM1 + 2, 0xC7);
}
namespace std { ostream cout, cerr; }

// ---------------------------------------------------------------- multiboot
struct __attribute__((packed)) MultibootInfo {
    uint32_t flags, mem_lower, mem_upper, boot_device, cmdline;
    uint32_t mods_count, mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length, mmap_addr, drives_length, drives_addr;
    uint32_t config_table, boot_loader_name, apm_table;
    uint32_t vbe_control_info, vbe_mode_info;
    uint16_t vbe_mode, vbe_interface_seg, vbe_interface_off, vbe_interface_len;
    uint64_t fb_addr;
    uint32_t fb_pitch, fb_width, fb_height;
    uint8_t fb_bpp, fb_type;
    uint8_t fb_pad[2];      // GRUB aligns what follows to 4 bytes
    uint8_t fb_color[6];    // type 1 (RGB): red, green, blue (position, size) pairs
};
struct __attribute__((packed)) MultibootModule {
    uint32_t mod_start, mod_end, string, reserved;
};
struct __attribute__((packed)) MultibootMmap { uint32_t size; uint64_t addr, len; uint32_t type; };
extern "C" uint32_t mb_magic, mb_info;
extern "C" uint64_t phys_limit;
uint64_t phys_limit = 0x100000000ull;
extern "C" char __kernel_start[], __kernel_end[];
extern "C" void heap_add(void* p, size_t n);
extern "C" size_t heap_peak_bytes(void);

// ---------------------------------------------------------------- memory
// Give the heap every usable RAM region the boot loader reports, minus the
// kernel image and anything below 1 MiB. Uses the Multiboot memory map (GRUB,
// QEMU, and the UEFI loader, which translates the UEFI map), else the "upper
// memory" size. Call it only once the ROM module and command line have been
// copied: their memory may be handed out.
static void heap_init(const MultibootInfo* mbi) {
    const uint64_t k0 = (uintptr_t)__kernel_start & ~0xFFFull;
    const uint64_t k1 = ((uintptr_t)__kernel_end + 0xFFF) & ~0xFFFull;
    uint64_t total = 0;
    auto add = [&](uint64_t a, uint64_t e) {
        if (e > phys_limit) e = phys_limit;
        if (e > (uint64_t)(uintptr_t)-1) e = (uint64_t)(uintptr_t)-1;   // i586: 32-bit pointers
        if (a < 0x100000) a = 0x100000;
        if (a < k1 && e > k0) {                        // skip the kernel image
            if (a < k0) { heap_add((void*)(uintptr_t)a, (size_t)(k0 - a)); total += k0 - a; }
            a = k1;
        }
        if (e > a) { heap_add((void*)(uintptr_t)a, (size_t)(e - a)); total += e - a; }
    };
    if (mbi->flags & (1 << 6)) {
        static MultibootMmap map[128];               // copy first: the heap may reuse it
        uintptr_t p = mbi->mmap_addr, end = p + mbi->mmap_length;
        int n = 0;
        while (p < end && n < 128) {
            const MultibootMmap* m = (const MultibootMmap*)p;
            map[n++] = *m;
            p += m->size + 4;
        }
        for (int i = 0; i < n; i++)
            if (map[i].type == 1) add(map[i].addr, map[i].addr + map[i].len);
    } else if (mbi->flags & 1) {
        add(0x100000, 0x100000 + (uint64_t)mbi->mem_upper * 1024);
    }
    printf("Heap: %u MB of RAM\n", (unsigned)(total >> 20));
}

// ---------------------------------------------------------------- video
// i586 asks for 640x480 so a frame stays about 1 MB of PCI writes.
// The mode to set when no boot loader set one (QEMU -kernel, via Bochs VBE).
// The Makefile picks it per ARCH.
#ifndef FB_W
#define FB_W 1024
#define FB_H 768
#define FB_BPP 32
#endif

// The framebuffer can be 8 (palettized), 15, 16, 24 or 32 bits per pixel:
// 386-era VESA cards with 1 MB manage 1024x768 only at 8 bits. Everything is
// drawn as 0xRRGGBB and converted on the way out.
static uint8_t* fb;
static uint32_t fb_w, fb_h, fb_pitch;   // pitch in bytes
static int fb_bytes = 4;                // per pixel
static bool fb_indexed;
static uint8_t r_pos = 16, r_len = 8, g_pos = 8, g_len = 8, b_pos = 0, b_len = 8;

// 8-bit modes: each colour gets a palette entry (VGA DAC) when it first
// appears. The NES has 64 colours and the overlays a handful more.
static uint32_t pal_rgb[256];
static int pal_n;
static uint32_t pal_key[1024];          // colour -> entry, open addressing
static uint8_t pal_val[1024];
static int pal_keys;
static void dac_set(int i, uint32_t c) {
    outb(0x3C8, (uint8_t)i);
    outb(0x3C9, (c >> 18) & 63); outb(0x3C9, (c >> 10) & 63); outb(0x3C9, (c >> 2) & 63);
}
static uint8_t pal_index(uint32_t c) {
    uint32_t h = (c * 2654435761u) >> 22;
    for (;; h = (h + 1) & 1023) {
        if (pal_key[h] == c) return pal_val[h];
        if (pal_key[h] == 0xFFFFFFFF) break;
    }
    int idx = 0;
    if (pal_n < 256) {
        idx = pal_n++;
        pal_rgb[idx] = c;
        dac_set(idx, c);
    } else {                                              // full: nearest colour
        int best = 1 << 30;
        for (int i = 0; i < 256; i++) {
            int dr = (int)((c >> 16) & 255) - (int)((pal_rgb[i] >> 16) & 255);
            int dg = (int)((c >> 8) & 255) - (int)((pal_rgb[i] >> 8) & 255);
            int db = (int)(c & 255) - (int)(pal_rgb[i] & 255);
            int d = dr * dr + dg * dg + db * db;
            if (d < best) { best = d; idx = i; }
        }
    }
    if (pal_keys < 900) { pal_key[h] = c; pal_val[h] = (uint8_t)idx; pal_keys++; }
    return (uint8_t)idx;
}
static void pal_reset() {
    for (auto& k : pal_key) k = 0xFFFFFFFF;
    pal_n = pal_keys = 0;
    pal_index(0x000000);                                  // entry 0: black (the border)
}

static uint32_t native(uint32_t c) {
    c &= 0xFFFFFF;
    if (fb_indexed) return pal_index(c);
    uint32_t r = c >> 16, g = (c >> 8) & 255, b = c & 255;
    return (r >> (8 - r_len)) << r_pos | (g >> (8 - g_len)) << g_pos | (b >> (8 - b_len)) << b_pos;
}
static inline uint8_t* put(uint8_t* p, uint32_t v) {
    switch (fb_bytes) {
    case 1: *p = (uint8_t)v; return p + 1;
    case 2: *(uint16_t*)p = (uint16_t)v; return p + 2;
    case 3: p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); return p + 3;
    default: *(uint32_t*)p = v; return p + 4;
    }
}
// Video memory sits on a slow bus (ISA on a 386): move dwords.
static void copy_out(void* d, const void* s, size_t n) {
    size_t words = n / 4, rest = n % 4;
    __asm__ volatile("rep movsl" : "+D"(d), "+S"(s), "+c"(words) :: "memory");
    __asm__ volatile("rep movsb" : "+D"(d), "+S"(s), "+c"(rest) :: "memory");
}

static bool bga_init(uint32_t w, uint32_t h, uint32_t bpp) {
    uint32_t base = 0;
    PciDevice vga;
    if (pci_find_id(0x1234, 0x1111, &vga))                  // QEMU/Bochs std VGA
        base = pci_read(vga, 0x10) & 0xFFFFFFF0;
    outw(0x1CE, 0); if (!base || inw(0x1CF) < 0xB0C0) return false;
    auto w16 = [](uint16_t i, uint16_t v) { outw(0x1CE, i); outw(0x1CF, v); };
    w16(4, 0); w16(1, w); w16(2, h); w16(3, bpp); w16(4, 0x41);
    fb = (uint8_t*)(uintptr_t)base;
    fb_bytes = (bpp + 7) / 8;
    fb_w = w; fb_h = h; fb_pitch = w * fb_bytes;
    fb_indexed = bpp == 8;
    if (bpp == 16) { r_pos = 11; r_len = 5; g_pos = 5; g_len = 6; b_pos = 0; b_len = 5; }
    if (bpp == 15) { r_pos = 10; r_len = 5; g_pos = 5; g_len = 5; b_pos = 0; b_len = 5; }
    return true;
}

static bool video_init(const MultibootInfo* mbi) {
    bool ok = false;
    if ((mbi->flags & (1 << 12)) && mbi->fb_addr < phys_limit) {
        int bpp = mbi->fb_bpp;
        if (mbi->fb_type == 0 && bpp == 8) {
            fb_indexed = ok = true;
        } else if (mbi->fb_type == 1 && (bpp == 15 || bpp == 16 || bpp == 24 || bpp == 32)) {
            const uint8_t* c = mbi->fb_color;
            r_pos = c[0]; r_len = c[1]; g_pos = c[2]; g_len = c[3]; b_pos = c[4]; b_len = c[5];
            ok = r_len && r_len <= 8 && g_len && g_len <= 8 && b_len && b_len <= 8;
        }
        if (ok) {
            fb = (uint8_t*)(uintptr_t)mbi->fb_addr;
            fb_w = mbi->fb_width; fb_h = mbi->fb_height; fb_pitch = mbi->fb_pitch;
            fb_bytes = (bpp + 7) / 8;
            printf("Using bootloader framebuffer %ux%u, %d bits\n", fb_w, fb_h, bpp);
        }
    }
    if (!ok && bga_init(FB_W, FB_H, FB_BPP)) {
        printf("Using Bochs/QEMU VBE %ux%u, %d bits\n", FB_W, FB_H, FB_BPP);
        ok = true;
    }
    if (ok && fb_indexed) pal_reset();
    return ok;
}

static bool frame_shown;                // present() may skip rows that haven't changed
static void fill(int x0, int y0, int w, int h, uint32_t c) {
    uint32_t v = native(c);
    for (int y = y0; y < y0 + h; y++)
        for (int x = x0; x < x0 + w; x++)
            if ((uint32_t)x < fb_w && (uint32_t)y < fb_h) put(fb + y * fb_pitch + x * fb_bytes, v);
    frame_shown = false;
}
static void text(int x, int y, const char* s, int scale, uint32_t c) {
    for (; *s; s++, x += 8 * scale) {
        char ch = (*s < 32 || *s > 126) ? '?' : *s;
        const unsigned char* g = font8x16[ch - 32];
        for (int r = 0; r < 16; r++)
            for (int b = 0; b < 8; b++)
                if (g[r] & (0x80 >> b)) fill(x + b * scale, y + r * scale, scale, scale, c);
    }
}

// Scale the 256x240 NES frame by an integer factor, centered.
static uint32_t frame[256 * 240];

// Text and boxes drawn into the NES frame (8x16 font).
static void frame_text(int x, int y, const char* s, uint32_t color) {
    for (int i = 0; s[i]; i++) {
        const unsigned char* g = font8x16[(s[i] < 32 || s[i] > 126 ? '?' : s[i]) - 32];
        for (int r = 0; r < 16; r++)
            for (int b = 0; b < 8; b++)
                if ((g[r] & (0x80 >> b)) && x + i * 8 + b >= 0 && x + i * 8 + b < 256 && y + r >= 0 && y + r < 240)
                    frame[(y + r) * 256 + x + i * 8 + b] = color;
    }
}
static void frame_fill(int x0, int y0, int w, int h, uint32_t c) {
    for (int y = y0 < 0 ? 0 : y0; y < y0 + h && y < 240; y++)
        for (int x = x0 < 0 ? 0 : x0; x < x0 + w && x < 256; x++) frame[y * 256 + x] = c;
}
static void frame_dim(int x0, int y0, int w, int h) {             // darken to ~25%
    for (int y = y0; y < y0 + h; y++)
        for (int x = x0; x < x0 + w; x++) frame[y * 256 + x] = (frame[y * 256 + x] >> 2) & 0x3F3F3F;
}

// F1 help: a dimmed panel listing the key bindings, drawn into the NES frame.
static const char* const help_lines[] = {
    "        KEY BINDINGS",
    "",
    "         MARIO     LUIGI",
    "MOVE     ARROWS    W A S D",
    "A / B    X / Z     G / F",
    "SELECT   [         Q",
    "START    ]         E",
    "",
    "F5-F8        SAVE STATE",
    "SHIFT+F5-F8  LOAD STATE",
    "P            PAUSE",
    "F12          RESET",
    "RIGHT CLICK  MOUSE MENU",
    "F1 / ESC     CLOSE HELP",
};
static void help_overlay() {
    const int n = sizeof(help_lines) / sizeof(help_lines[0]);
    const int x0 = 8, y0 = (240 - n * 16) / 2 - 4, w = 240, h = n * 16 + 8;
    for (int y = y0; y < y0 + h; y++)
        for (int x = x0; x < x0 + w; x++) {
            uint32_t p = frame[y * 256 + x];                   // darken to ~25%
            frame[y * 256 + x] = (p >> 2) & 0x3F3F3F;
        }
    for (int l = 0; l < n; l++)
        for (int i = 0; help_lines[l][i]; i++) {
            char ch = help_lines[l][i];
            const unsigned char* g = font8x16[(ch < 32 || ch > 126 ? '?' : ch) - 32];
            uint32_t color = l == 0 ? 0xFFD040 : 0xFFFFFF;
            for (int r = 0; r < 16; r++)
                for (int b = 0; b < 8; b++)
                    if (g[r] & (0x80 >> b))
                        frame[(y0 + 4 + l * 16 + r) * 256 + x0 + 8 + i * 8 + b] = color;
        }
}

// Draw a centered status banner into the NES frame (below the HUD).
static void banner(const char* s) {
    int w = (int)strlen(s) * 8 + 8, x0 = (256 - w) / 2, y0 = 40;
    for (int y = y0; y < y0 + 20; y++)
        for (int x = x0; x < x0 + w; x++) frame[y * 256 + x] = 0x000000;
    for (int i = 0; s[i]; i++) {
        const unsigned char* g = font8x16[(s[i] < 32 || s[i] > 126 ? '?' : s[i]) - 32];
        for (int r = 0; r < 16; r++)
            for (int b = 0; b < 8; b++)
                if (g[r] & (0x80 >> b)) frame[(y0 + 2 + r) * 256 + x0 + 4 + i * 8 + b] = 0xFFFFFF;
    }
}
static uint8_t line[256 * 8 * 4];
static uint32_t shown[256 * 240];       // the frame as last drawn on screen
static int scale, off_x, off_y;

// Scale the NES frame onto the screen, redrawing only the rows that changed.
static void present() {
    for (int y = 0; y < 240; y++) {
        const uint32_t* src = &frame[y * 256];
        uint32_t* old = &shown[y * 256];
        int x = 0;
        if (frame_shown) while (x < 256 && src[x] == old[x]) x++;
        if (x == 256) continue;
        memcpy(old, src, 256 * 4);
        uint8_t* d = line;
        uint32_t last = 0xFFFFFFFF, v = 0;
        for (x = 0; x < 256; x++) {
            uint32_t p = src[x];
            if (p != last) { last = p; v = native(p); }
            for (int k = 0; k < scale; k++) d = put(d, v);
        }
        size_t n = (size_t)256 * scale * fb_bytes;
        for (int k = 0; k < scale; k++)
            copy_out(fb + (off_y + y * scale + k) * fb_pitch + off_x * fb_bytes, line, n);
    }
    frame_shown = true;
}

// ---------------------------------------------------------------- interrupts
#ifdef __x86_64__
struct __attribute__((packed)) IdtEntry {
    uint16_t off_lo, sel; uint8_t ist, type; uint16_t off_mid; uint32_t off_hi, zero;
};
#else
struct __attribute__((packed)) IdtEntry {         // 32-bit interrupt gate
    uint16_t off_lo, sel; uint8_t zero, type; uint16_t off_hi;
};
#endif
static IdtEntry idt[256];
extern "C" void isr_timer(), isr_keyboard(), isr_mouse(), isr_spurious(), isr_fault();

static void set_gate(int n, void (*h)()) {
    uintptr_t a = (uintptr_t)h;
#ifdef __x86_64__
    idt[n] = { (uint16_t)a, 0x08, 0, 0x8E, (uint16_t)(a >> 16), (uint32_t)(a >> 32), 0 };
#else
    idt[n] = { (uint16_t)a, 0x08, 0, 0x8E, (uint16_t)(a >> 16) };
#endif
}

extern volatile uint32_t ticks;
extern volatile uint8_t kbd_buf[256];
extern volatile uint8_t kbd_head, kbd_tail;

static void interrupts_init() {
    for (int i = 0; i < 32; i++) set_gate(i, isr_fault);
    for (int i = 32; i < 256; i++) set_gate(i, isr_spurious);
    set_gate(32, isr_timer);
    set_gate(33, isr_keyboard);
    set_gate(44, isr_mouse);
    struct __attribute__((packed)) { uint16_t lim; uintptr_t base; } idtr = { sizeof(idt) - 1, (uintptr_t)idt };
    __asm__ volatile("lidt %0" ::"m"(idtr));

    // Remap the PICs to vectors 32..47; unmask timer, keyboard, cascade and mouse.
    outb(0x20, 0x11); outb(0xA0, 0x11);
    outb(0x21, 32);   outb(0xA1, 40);
    outb(0x21, 4);    outb(0xA1, 2);
    outb(0x21, 1);    outb(0xA1, 1);
    outb(0x21, 0xF8); outb(0xA1, 0xEF);

    // PIT channel 0 at ~60 Hz.
    uint16_t div = 1193182 / 60;
    outb(0x43, 0x36); outb(0x40, div & 0xFF); outb(0x40, div >> 8);

    for (int i = 0; i < 64 && (inb(0x64) & 1); i++) inb(0x60);   // flush stale bytes (bounded: no i8042 reads 0xFF)
    __asm__ volatile("sti");
}

// ---------------------------------------------------------------- input
static bool shift_l, shift_r;
static int pending_save = -1, pending_load = -1;
static bool pending_reset, paused, help, menu;

// Two NES controllers. Player 1 is on the arrows, player 2 on WASD; Luigi
// (player 2 in a 2-player game) reads controller 2, as on the real NES.
static Controller* pads[2];

// A tap can arrive as press+release in the same frame; keep such buttons
// down for one frame so the game still sees them.
static bool pressed_now[2][8], release_later[2][8];
static void set_button(int player, ControllerButton b, bool down) {
    Controller& c = *pads[player];
    Player p = player ? PLAYER_2 : PLAYER_1;
    if (down) { c.setButtonState(p, b, true); pressed_now[player][b] = true; release_later[player][b] = false; }
    else if (pressed_now[player][b]) release_later[player][b] = true;
    else c.setButtonState(p, b, false);
}

// Scancode (set 1; 0x100 = E0-prefixed) -> player and NES button.
static const struct { uint16_t code; uint8_t player; ControllerButton button; } bindings[] = {
    // Player 1: arrows, X = A, Z = B, [ = Select, ] = Start
    {0x148, 0, BUTTON_UP}, {0x150, 0, BUTTON_DOWN}, {0x14B, 0, BUTTON_LEFT}, {0x14D, 0, BUTTON_RIGHT},
    {0x2D, 0, BUTTON_A}, {0x2C, 0, BUTTON_B}, {0x1A, 0, BUTTON_SELECT}, {0x1B, 0, BUTTON_START},
    // Player 2: WASD, G = A, F = B, Q = Select, E = Start
    {0x11, 1, BUTTON_UP}, {0x1F, 1, BUTTON_DOWN}, {0x1E, 1, BUTTON_LEFT}, {0x20, 1, BUTTON_RIGHT},
    {0x22, 1, BUTTON_A}, {0x21, 1, BUTTON_B}, {0x10, 1, BUTTON_SELECT}, {0x12, 1, BUTTON_START},
};

static void key_event(bool ext, uint8_t code, bool down) {
    uint16_t full = code | (ext ? 0x100 : 0);
    for (auto& b : bindings)
        if (b.code == full) { set_button(b.player, b.button, down); return; }
    if (ext) return;
    switch (code) {
    case 0x2A: shift_l = down; break;
    case 0x36: shift_r = down; break;
    case 0x3F: case 0x40: case 0x41: case 0x42:                // F5..F8
        if (down) { int slot = code - 0x3F; if (shift_l || shift_r) pending_load = slot; else pending_save = slot; }
        break;
    case 0x58: if (down) pending_reset = true; break;          // F12
    case 0x19: if (down) paused = !paused; break;              // P
    case 0x3B: if (down) help = !help; break;                  // F1
    case 0x01: if (down) help = menu = false; break;           // Esc
    }
}

// Queue a scancode from a source other than the PS/2 interrupt (USB).
void kbd_push(uint8_t b) {
    uintptr_t flags;
    __asm__ volatile("pushf; pop %0; cli" : "=r"(flags) :: "memory");
    kbd_buf[kbd_head] = b;
    kbd_head = kbd_head + 1;
    __asm__ volatile("push %0; popf" :: "r"(flags) : "memory", "cc");
}

static void poll_keyboard() {
    static bool ext;
    for (int p = 0; p < 2; p++)
        for (int b = 0; b < 8; b++) {
            if (release_later[p][b]) pads[p]->setButtonState(p ? PLAYER_2 : PLAYER_1, (ControllerButton)b, false);
            release_later[p][b] = pressed_now[p][b] = false;
        }
    while (kbd_tail != kbd_head) {
        uint8_t b = kbd_buf[kbd_tail++];
        if (b == 0xE0) { ext = true; continue; }
        if (b == 0xE1) { ext = false; continue; }
        key_event(ext, b & 0x7F, !(b & 0x80));
        ext = false;
    }
}

// ---------------------------------------------------------------- mouse
// PS/2 mouse on the i8042's second port (USB mice arrive via mouse_push too).
extern volatile uint8_t mouse_buf[256];
extern volatile uint8_t mouse_head, mouse_tail;
static bool i8042_wait_write() { for (int i = 0; i < 100000; i++) if (!(inb(0x64) & 2)) return true; return false; }
static bool i8042_wait_read()  { for (int i = 0; i < 100000; i++) if (inb(0x64) & 1) return true; return false; }
static bool mouse_cmd(uint8_t b) {
    i8042_wait_write(); outb(0x64, 0xD4);
    i8042_wait_write(); outb(0x60, b);
    for (int tries = 0; tries < 4; tries++) {          // skip stray bytes until the ACK
        if (!i8042_wait_read()) return false;
        if (inb(0x60) == 0xFA) return true;
    }
    return false;
}
static bool ps2_mouse_init() {
    i8042_wait_write(); outb(0x64, 0xA8);              // enable the aux port
    i8042_wait_write(); outb(0x64, 0x20);              // read controller config
    if (!i8042_wait_read()) return false;
    uint8_t cfg = inb(0x60);
    cfg = (cfg | 0x02) & ~0x20;                        // aux interrupt on, aux clock on
    i8042_wait_write(); outb(0x64, 0x60);
    i8042_wait_write(); outb(0x60, cfg);
    if (!mouse_cmd(0xF6) || !mouse_cmd(0xF4)) {        // defaults, then start streaming
        printf("PS/2 mouse: none\n");
        return false;
    }
    printf("PS/2 mouse: ready\n");
    return true;
}

// The pointer lives in screen pixels inside the game area; the menu works
// in NES pixels (screen / scale).
static int mouse_sx, mouse_sy, mouse_buttons;
static bool mouse_seen;                  // any mouse has reported
static uint32_t mouse_active;            // ticks of the last movement or click
static int click_x = -1, click_y;        // a left click waiting to be handled
static bool right_click;

void mouse_push(int dx, int dy, int b) {
    if (dx || dy || b != mouse_buttons) { mouse_seen = true; mouse_active = ticks; }
    mouse_sx += dx; mouse_sy += dy;
    if (mouse_sx < 0) mouse_sx = 0;
    if (mouse_sy < 0) mouse_sy = 0;
    if (mouse_sx >= 256 * scale) mouse_sx = 256 * scale - 1;
    if (mouse_sy >= 240 * scale) mouse_sy = 240 * scale - 1;
    int pressed = b & ~mouse_buttons;
    if (pressed & 1) { click_x = mouse_sx / scale; click_y = mouse_sy / scale; }
    if (pressed & 2) right_click = true;
    mouse_buttons = b;
}

static void poll_ps2_mouse() {
    static uint8_t pkt[3];
    static int n;
    while (mouse_tail != mouse_head) {
        uint8_t b = mouse_buf[mouse_tail++];
        if (n == 0 && !(b & 0x08)) continue;           // resync: byte 0 always has bit 3 set
        pkt[n++] = b;
        if (n < 3) continue;
        n = 0;
        if (pkt[0] & 0xC0) continue;                   // overflow: drop the packet
        int dx = pkt[1] - ((pkt[0] << 4) & 0x100);
        int dy = pkt[2] - ((pkt[0] << 3) & 0x100);
        mouse_push(dx, -dy, pkt[0] & 7);               // PS/2 y grows upward
    }
}

// The pointer shows while the menu or help is open, and for 3 s after the
// mouse was last used.
static bool cursor_visible() { return mouse_seen && (menu || help || ticks - mouse_active < 180); }

static const char* const CURSOR[] = {
    "X.........", "XX........", "XOX.......", "XOOX......", "XOOOX.....", "XOOOOX....",
    "XOOOOOX...", "XOOOOOOX..", "XOOOOXXXX.", "XOXOOX....", "XX.XOOX...", "X...XOX...", "....XX....",
};
static void draw_cursor() {
    int cx = mouse_sx / scale, cy = mouse_sy / scale;
    for (int r = 0; r < 13; r++)
        for (int c = 0; CURSOR[r][c]; c++) {
            int x = cx + c, y = cy + r;
            if (x >= 256 || y >= 240 || CURSOR[r][c] == '.') continue;
            frame[y * 256 + x] = CURSOR[r][c] == 'X' ? 0x000000 : 0xFFFFFF;
        }
}

// ---------------------------------------------------------------- mouse menu
enum Action { ACT_NONE, ACT_SAVE1, ACT_LOAD1 = ACT_SAVE1 + 4, ACT_RESUME = ACT_LOAD1 + 4, ACT_RESET, ACT_HELP, ACT_OPEN };
struct Button { int x, y, w, h; const char* label; int action; };
static const int MENU_X = 10, MENU_Y = 66, MENU_W = 236, MENU_H = 102;
static const Button menu_buttons[] = {
    {13,  92, 56, 20, "SAVE 1", ACT_SAVE1},     {71,  92, 56, 20, "SAVE 2", ACT_SAVE1 + 1},
    {129, 92, 56, 20, "SAVE 3", ACT_SAVE1 + 2}, {187, 92, 56, 20, "SAVE 4", ACT_SAVE1 + 3},
    {13, 116, 56, 20, "LOAD 1", ACT_LOAD1},     {71, 116, 56, 20, "LOAD 2", ACT_LOAD1 + 1},
    {129, 116, 56, 20, "LOAD 3", ACT_LOAD1 + 2}, {187, 116, 56, 20, "LOAD 4", ACT_LOAD1 + 3},
    {13, 140, 74, 20, "RESUME", ACT_RESUME},     {91, 140, 74, 20, "RESET", ACT_RESET},
    {169, 140, 74, 20, "HELP", ACT_HELP},
};
static const Button open_button = {206, 220, 46, 16, "MENU", ACT_OPEN};   // bottom right, while the pointer shows

static bool slot_used(int slot) {
    char name[] = "save1.dat";
    name[4] = (char)('1' + slot);
    return memfs::exists(name);
}
static bool button_enabled(const Button& b) {
    return !(b.action >= ACT_LOAD1 && b.action < ACT_LOAD1 + 4) || slot_used(b.action - ACT_LOAD1);
}
static bool inside(const Button& b, int x, int y) { return x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + b.h; }

static void draw_button(const Button& b) {
    bool on = button_enabled(b);
    bool hover = on && cursor_visible() && inside(b, mouse_sx / scale, mouse_sy / scale);
    frame_fill(b.x, b.y, b.w, b.h, hover ? 0xFFFFFF : 0x606060);
    frame_fill(b.x + 1, b.y + 1, b.w - 2, b.h - 2, hover ? 0xC84C0C : on ? 0x303030 : 0x181818);
    int len = (int)strlen(b.label);
    frame_text(b.x + (b.w - len * 8) / 2, b.y + (b.h - 16) / 2, b.label, on ? 0xFFFFFF : 0x606060);
}
static void menu_overlay() {
    frame_dim(0, 0, 256, 240);
    frame_fill(MENU_X, MENU_Y, MENU_W, MENU_H, 0xFFFFFF);
    frame_fill(MENU_X + 1, MENU_Y + 1, MENU_W - 2, MENU_H - 2, 0x000000);
    frame_text(MENU_X + (MENU_W - 4 * 8) / 2, MENU_Y + 5, "MENU", 0xFFD040);
    for (auto& b : menu_buttons) draw_button(b);
}

static void menu_action(int a) {
    if (a >= ACT_SAVE1 && a < ACT_SAVE1 + 4) pending_save = a - ACT_SAVE1;
    else if (a >= ACT_LOAD1 && a < ACT_LOAD1 + 4) pending_load = a - ACT_LOAD1;
    else if (a == ACT_RESUME) paused = false;
    else if (a == ACT_RESET) pending_reset = true;
    else if (a == ACT_HELP) help = true;
    menu = false;
}
// Once per frame: act on clicks.
static void handle_mouse() {
    if (right_click) {
        right_click = false;
        if (help) help = false; else menu = !menu;
    }
    if (click_x < 0) return;
    int x = click_x, y = click_y;
    click_x = -1;
    if (help) { help = false; return; }
    if (menu) {
        for (auto& b : menu_buttons)
            if (inside(b, x, y)) { if (button_enabled(b)) menu_action(b.action); return; }
        if (x < MENU_X || x >= MENU_X + MENU_W || y < MENU_Y || y >= MENU_Y + MENU_H) menu = false;
        return;
    }
    if (inside(open_button, x, y)) menu = true;
}

// ---------------------------------------------------------------- main
static uint8_t rom[64 * 1024];

static void fatal_screen(const char* l1, const char* l2) {
    fill(0, 0, fb_w, fb_h, 0x101030);
    text(40, 40, l1, 3, 0xFF6060);
    text(40, 120, l2, 2, 0xFFFFFF);
    printf("%s\n%s\n", l1, l2);
    for (;;) __asm__ volatile("hlt");
}

extern "C" void (*__init_array_start[])(), (*__init_array_end[])();

extern "C" void kmain() {
    serial_init();
    printf("\nSuper Mario Bros. Virtualized - bare metal\n");
    for (auto f = __init_array_start; f != __init_array_end; f++) (*f)();

    const MultibootInfo* mbi = (const MultibootInfo*)(uintptr_t)mb_info;
    if (mb_magic != 0x2BADB002) { printf("Not booted by a Multiboot loader\n"); return; }
    if (!video_init(mbi)) { printf("No usable 32-bit framebuffer found\n"); return; }

    scale = fb_w / 256 < fb_h / 240 ? fb_w / 256 : fb_h / 240;
    if (scale < 1) scale = 1;
    if (scale > 8) scale = 8;
    off_x = (fb_w - 256 * scale) / 2;
    off_y = (fb_h - 240 * scale) / 2;
    fill(0, 0, fb_w, fb_h, 0);

    // The ROM arrives as the first boot module.
    if (!(mbi->flags & (1 << 3)) || mbi->mods_count == 0)
        fatal_screen("No ROM loaded", "Pass your Super Mario Bros. .nes file as a boot module.");
    const MultibootModule* mod = (const MultibootModule*)(uintptr_t)mbi->mods_addr;
    uint32_t size = mod->mod_end - mod->mod_start;
    const uint8_t* m = (const uint8_t*)(uintptr_t)mod->mod_start;
    if (size < 40976 || size > sizeof(rom) || memcmp(m, "NES\x1a", 4) != 0)
        fatal_screen("Bad ROM image", "Expected a 40976-byte iNES Super Mario Bros. ROM.");
    memcpy(rom, m, size);
    printf("ROM module: %u bytes\n", size);

    static char cmdbuf[512];
    const char* cmdline = nullptr;
    if (mbi->flags & (1 << 2)) {
        strncpy(cmdbuf, (const char*)(uintptr_t)mbi->cmdline, sizeof(cmdbuf) - 1);
        cmdline = cmdbuf;
    }
    static MultibootInfo info;
    info = *mbi;
    heap_init(&info);

    static SMBEngine* engine = new SMBEngine(rom);
    engine->reset();
    pads[0] = &engine->getController1();
    pads[1] = &engine->getController2();

#ifndef __x86_64__
    extern uint32_t fpu_present;                       // boot32.S
    if (!fpu_present) printf("No FPU: sound off\n");  // the APU mixer needs one
    else
#endif
    audio_init(cmdline);
    usb_init(cmdline);
    bool debug = false;
    for (const char* p = cmdline; p && *p; p++)
        if (strncmp(p, "debug", 5) == 0) debug = true;

    mouse_sx = 128 * scale; mouse_sy = 120 * scale;
    ps2_mouse_init();
    interrupts_init();
    storage_init(info.flags, info.boot_device, cmdline);
    printf("Running. P1: arrows, X/Z, [ select, ] start. P2: WASD, G/F, Q select, E start.\n"
           "F1 help, F5-F8 save, Shift+F5-F8 load, F12 reset, P pause, right click: menu\n");

    char msg[32] = {0};
    int msg_frames = 0;
    uint32_t last = ticks;
    for (;;) {
        while (ticks == last) __asm__ volatile("hlt");
        last = ticks;

        floppy_poll();
        usb_poll();
        poll_keyboard();
        poll_ps2_mouse();
        handle_mouse();
        if (debug) {
            static uint32_t frames, last_report;
            frames++;
            if (ticks - last_report >= 60) {
                last_report = ticks;
                printf("heartbeat: ticks %u frames %u audio %s pos %u heap peak %u KB\n",
                       ticks, frames, audio_name(), audio_play_pos(), (unsigned)(heap_peak_bytes() >> 10));
            }
        }
        if (pending_reset) { engine->reset(); pending_reset = false; }
        if (pending_save >= 0 || pending_load >= 0) {
            bool save = pending_save >= 0;
            int slot = save ? pending_save : pending_load;
            char name[] = "save1.dat"; name[4] = (char)('1' + slot);
            bool ok = true;
            int disk = 0;
            if (save) { engine->saveState(name); disk = storage_save(name); }
            else ok = engine->loadState(name);
            const char* verb = save ? "SAVED SLOT " : (ok ? "LOADED SLOT " : "EMPTY SLOT ");
            const char* note = disk < 0 ? " - DISK ERROR" : "";
            size_t n = strlen(verb), k = strlen(note);
            memcpy(msg, verb, n); msg[n] = (char)('1' + slot); memcpy(msg + n + 1, note, k + 1);
            msg_frames = 120;
            pending_save = pending_load = -1;
        }

        if (!paused && !help && !menu) {
            engine->update();
            if (Configuration::audioEnabled) {
                static uint8_t samples[1024];
                int n = Configuration::audioFrequency / 60;
                memset(samples, 0, n);
                engine->audioCallback(samples, n);
                audio_submit(samples, n);
            }
        }
        engine->render(frame);
        if (help) help_overlay();
        else if (menu) menu_overlay();
        else if (paused) banner("PAUSED");
        else if (msg_frames > 0) { banner(msg); msg_frames--; }
        if (cursor_visible()) {
            if (!menu && !help) draw_button(open_button);
            draw_cursor();
        }
        present();
    }
}

extern "C" void fault_handler() {
    printf("CPU exception - halted\n");
}
