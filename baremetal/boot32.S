# Multiboot entry for i586 (Pentium) and up: stay in 32-bit protected mode
# with a flat GDT and paging off, so every physical address (RAM, the
# framebuffer, PCI MMIO) below 4 GiB is directly addressable. Then call kmain().

.set MB_MAGIC,  0x1BADB002
.set MB_FLAGS,  (1<<0) | (1<<1) | (1<<2)   # page-align modules, memory info, video mode
.set MB_CHECK,  -(MB_MAGIC + MB_FLAGS)

.section .multiboot, "a"
.align 4
    .long MB_MAGIC, MB_FLAGS, MB_CHECK
    .long 0, 0, 0, 0, 0          # a.out kludge fields (unused for ELF)
    .long 0                      # mode_type: 0 = linear graphics
    .long 640, 480, 32           # preferred width, height, depth

.section .bss
.align 16
stack_bottom:
        .skip 65536
.global stack_top
stack_top:

.section .data
.global mb_magic, mb_info
mb_magic: .long 0
mb_info:  .long 0

.align 8
gdt32:
    .quad 0
    .quad 0x00CF9A000000FFFF     # 0x08: 32-bit code, base 0, limit 4 GiB
    .quad 0x00CF92000000FFFF     # 0x10: data, base 0, limit 4 GiB
gdt32_ptr:
    .word gdt32_ptr - gdt32 - 1
    .long gdt32

.section .text
.code32
.global _start
_start:
    cli
    mov  $stack_top, %esp
    mov  %eax, mb_magic
    mov  %ebx, mb_info

    # Zero .bss ourselves rather than trusting the loader.
    mov  $__bss_start, %edi
    mov  $__bss_end, %ecx
    sub  %edi, %ecx
    xor  %eax, %eax
    cld
    rep stosb
    mov  $stack_top, %esp        # the stack lives in .bss too

    # Our own GDT: the loader's may be gone or temporary.
    lgdt gdt32_ptr
    ljmp $0x08, $1f
1:  mov  $0x10, %ax
    mov  %ax, %ds
    mov  %ax, %es
    mov  %ax, %ss
    mov  %ax, %fs
    mov  %ax, %gs

    # CR0: paging off, x87 on (EM clear), MP and NE set (native FPU errors).
    # No SSE: a Pentium has none, so CR4 is left alone.
    mov  %cr0, %eax
    and  $~((1<<31) | (1<<2)), %eax
    or   $((1<<5) | (1<<1)), %eax
    mov  %eax, %cr0
    fninit

    call kmain
3:  cli
    hlt
    jmp  3b

# ---- Interrupt stubs (save caller-saved registers, call C, iret) ----
.macro IRQ_STUB name, handler
.global \name
\name:
    push %eax
    push %ecx
    push %edx
    cld
    call \handler
    pop  %edx
    pop  %ecx
    pop  %eax
    iret
.endm

IRQ_STUB isr_timer,    irq_timer
IRQ_STUB isr_keyboard, irq_keyboard
IRQ_STUB isr_mouse,    irq_mouse
IRQ_STUB isr_spurious, irq_spurious

.global isr_fault
isr_fault:
    cli
    call fault_handler
4:  hlt
    jmp 4b

.section .note.GNU-stack,"",@progbits
