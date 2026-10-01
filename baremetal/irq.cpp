// Interrupt handlers. Built with -mgeneral-regs-only so they never touch
// SSE registers (the assembly stubs only save general-purpose registers).
#include <stdint.h>
#include "hw.hpp"

volatile uint32_t ticks;
volatile uint8_t kbd_buf[256];
volatile uint8_t kbd_head, kbd_tail;

extern "C" void irq_timer() {
    ticks = ticks + 1;
    outb(0x20, 0x20);
}

extern "C" void irq_keyboard() {
    uint8_t b = inb(0x60);
    kbd_buf[kbd_head] = b;
    kbd_head = kbd_head + 1;
    outb(0x20, 0x20);
}

volatile uint8_t mouse_buf[256];
volatile uint8_t mouse_head, mouse_tail;

extern "C" void irq_mouse() {                 // IRQ12, on the slave PIC
    uint8_t b = inb(0x60);
    mouse_buf[mouse_head] = b;
    mouse_head = mouse_head + 1;
    outb(0xA0, 0x20);
    outb(0x20, 0x20);
}

extern "C" void irq_spurious() {}
