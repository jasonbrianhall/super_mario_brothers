// Bare-metal frontend for Super Mario Bros. Virtualized.
// Boots via Multiboot (GRUB or QEMU -kernel), takes the ROM as a boot
// module, and runs the engine on a framebuffer with PS/2 keyboard input.
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
};
struct __attribute__((packed)) MultibootModule {
    uint32_t mod_start, mod_end, string, reserved;
};
extern "C" uint32_t mb_magic, mb_info;

// ---------------------------------------------------------------- video
static volatile uint32_t* fb;
static uint32_t fb_w, fb_h, fb_pitch;   // pitch in pixels

static bool bga_init(uint32_t w, uint32_t h) {
    uint32_t base = 0;
    PciDevice vga;
    if (pci_find_id(0x1234, 0x1111, &vga))                  // QEMU/Bochs std VGA
        base = pci_read(vga, 0x10) & 0xFFFFFFF0;
    outw(0x1CE, 0); if (!base || inw(0x1CF) < 0xB0C0) return false;
    auto w16 = [](uint16_t i, uint16_t v) { outw(0x1CE, i); outw(0x1CF, v); };
    w16(4, 0); w16(1, w); w16(2, h); w16(3, 32); w16(4, 0x41);
    fb = (volatile uint32_t*)(uintptr_t)base;
    fb_w = w; fb_h = h; fb_pitch = w;
    return true;
}

static bool video_init(const MultibootInfo* mbi) {
    if ((mbi->flags & (1 << 12)) && mbi->fb_type == 1 && mbi->fb_bpp == 32 && mbi->fb_addr < 0x100000000ull) {
        fb = (volatile uint32_t*)(uintptr_t)mbi->fb_addr;
        fb_w = mbi->fb_width; fb_h = mbi->fb_height; fb_pitch = mbi->fb_pitch / 4;
        printf("Using bootloader framebuffer %ux%u\n", fb_w, fb_h);
        return true;
    }
    if (bga_init(1024, 768)) { printf("Using Bochs/QEMU VBE 1024x768\n"); return true; }
    return false;
}

static void fill(int x0, int y0, int w, int h, uint32_t c) {
    for (int y = y0; y < y0 + h; y++)
        for (int x = x0; x < x0 + w; x++)
            if ((uint32_t)x < fb_w && (uint32_t)y < fb_h) fb[y * fb_pitch + x] = c;
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
static uint32_t line[256 * 8];
static int scale, off_x, off_y;

static void present() {
    for (int y = 0; y < 240; y++) {
        const uint32_t* src = &frame[y * 256];
        uint32_t* d = line;
        for (int x = 0; x < 256; x++) {
            uint32_t p = src[x] & 0xFFFFFF;
            for (int k = 0; k < scale; k++) *d++ = p;
        }
        for (int k = 0; k < scale; k++)
            memcpy((void*)&fb[(off_y + y * scale + k) * fb_pitch + off_x], line, 256 * scale * 4);
    }
}

// ---------------------------------------------------------------- interrupts
struct __attribute__((packed)) IdtEntry {
    uint16_t off_lo, sel; uint8_t ist, type; uint16_t off_mid; uint32_t off_hi, zero;
};
static IdtEntry idt[256];
extern "C" void isr_timer(), isr_keyboard(), isr_spurious(), isr_fault();

static void set_gate(int n, void (*h)()) {
    uintptr_t a = (uintptr_t)h;
    idt[n] = { (uint16_t)a, 0x08, 0, 0x8E, (uint16_t)(a >> 16), (uint32_t)(a >> 32), 0 };
}

extern volatile uint32_t ticks;
extern volatile uint8_t kbd_buf[256];
extern volatile uint8_t kbd_head, kbd_tail;

static void interrupts_init() {
    for (int i = 0; i < 32; i++) set_gate(i, isr_fault);
    for (int i = 32; i < 256; i++) set_gate(i, isr_spurious);
    set_gate(32, isr_timer);
    set_gate(33, isr_keyboard);
    struct __attribute__((packed)) { uint16_t lim; uint64_t base; } idtr = { sizeof(idt) - 1, (uintptr_t)idt };
    __asm__ volatile("lidt %0" ::"m"(idtr));

    // Remap the PICs to vectors 32..47; unmask only timer and keyboard.
    outb(0x20, 0x11); outb(0xA0, 0x11);
    outb(0x21, 32);   outb(0xA1, 40);
    outb(0x21, 4);    outb(0xA1, 2);
    outb(0x21, 1);    outb(0xA1, 1);
    outb(0x21, 0xFC); outb(0xA1, 0xFF);

    // PIT channel 0 at ~60 Hz.
    uint16_t div = 1193182 / 60;
    outb(0x43, 0x36); outb(0x40, div & 0xFF); outb(0x40, div >> 8);

    for (int i = 0; i < 64 && (inb(0x64) & 1); i++) inb(0x60);   // flush stale bytes (bounded: no i8042 reads 0xFF)
    __asm__ volatile("sti");
}

// ---------------------------------------------------------------- input
static bool shift_l, shift_r;
static int pending_save = -1, pending_load = -1;
static bool pending_reset, paused, help;

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
    case 0x01: if (down) help = false; break;                  // Esc
    }
}

// Queue a scancode from a source other than the PS/2 interrupt (USB).
void kbd_push(uint8_t b) {
    uint64_t flags;
    __asm__ volatile("pushfq; pop %0; cli" : "=r"(flags) :: "memory");
    kbd_buf[kbd_head] = b;
    kbd_head = kbd_head + 1;
    __asm__ volatile("push %0; popfq" :: "r"(flags) : "memory", "cc");
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

    static SMBEngine* engine = new SMBEngine(rom);
    engine->reset();
    pads[0] = &engine->getController1();
    pads[1] = &engine->getController2();

    const char* cmdline = (mbi->flags & (1 << 2)) ? (const char*)(uintptr_t)mbi->cmdline : nullptr;
    audio_init(cmdline);
    usb_init(cmdline);
    bool debug = false;
    for (const char* p = cmdline; p && *p; p++)
        if (strncmp(p, "debug", 5) == 0) debug = true;

    interrupts_init();
    printf("Running. P1: arrows, X/Z, [ select, ] start. P2: WASD, G/F, Q select, E start.\n"
           "F1 help, F5-F8 save, Shift+F5-F8 load, F12 reset, P pause\n");

    char msg[32] = {0};
    int msg_frames = 0;
    uint32_t last = ticks;
    for (;;) {
        while (ticks == last) __asm__ volatile("hlt");
        last = ticks;

        usb_poll();
        poll_keyboard();
        if (debug) {
            static uint32_t frames, last_report;
            frames++;
            if (ticks - last_report >= 60) {
                last_report = ticks;
                printf("heartbeat: ticks %u frames %u audio %s pos %u\n",
                       ticks, frames, audio_name(), audio_play_pos());
            }
        }
        if (pending_reset) { engine->reset(); pending_reset = false; }
        if (pending_save >= 0 || pending_load >= 0) {
            bool save = pending_save >= 0;
            int slot = save ? pending_save : pending_load;
            char name[] = "save1.dat"; name[4] = (char)('1' + slot);
            bool ok = true;
            if (save) engine->saveState(name); else ok = engine->loadState(name);
            const char* verb = save ? "SAVED SLOT " : (ok ? "LOADED SLOT " : "EMPTY SLOT ");
            size_t n = strlen(verb); memcpy(msg, verb, n); msg[n] = (char)('1' + slot); msg[n + 1] = 0;
            msg_frames = 120;
            pending_save = pending_load = -1;
        }

        if (!paused && !help) {
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
        else if (paused) banner("PAUSED");
        else if (msg_frames > 0) { banner(msg); msg_frames--; }
        present();
    }
}

extern "C" void fault_handler() {
    printf("CPU exception - halted\n");
}
