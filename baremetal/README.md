# Bare-metal Super Mario Bros.

Boots straight into the game on x86_64: no OS, no libc, no SDL. GRUB (or
QEMU's `-kernel`) loads the kernel and your ROM, a small stub switches to
64-bit long mode, and the unchanged engine core runs on a framebuffer.

You must supply your own ROM (md5 `811b027eaf99c2def7b933c5208636de`).
It is loaded at boot as a module and is never compiled in.

## Build and run

```
sudo apt install build-essential qemu-system-x86 grub-pc-bin grub-common xorriso mtools
cd baremetal
make
make run ROM=~/nes/smb.nes          # QEMU/KVM, direct kernel boot
make iso ROM=~/nes/smb.nes          # bootable smb.iso (GRUB)
make run-iso ROM=~/nes/smb.nes
```

The ISO can be written to a USB stick with `dd` and booted on a BIOS
(legacy/CSM) PC with a PS/2 or BIOS-emulated USB keyboard.

## Controls

| Key | Action |
|---|---|
| Arrows | D-pad |
| X / Z | A / B |
| Enter | Start |
| Tab or Right Shift | Select |
| F5-F8 | Save state to slot 1-4 (RAM, lost on reboot) |
| Shift+F5-F8 | Load state |
| P | Pause |
| F12 | Reset |

## How it's put together

- `boot.S`: Multiboot header, long-mode switch, identity-mapped 4 GiB, interrupt stubs
- `kernel.cpp`: framebuffer (GRUB's, or QEMU's VBE adapter), 60 Hz PIT timer, PS/2 keyboard, main loop
- `runtime.cpp`, `include/`: memcpy/printf/malloc, a minimal `std::string`/`iostream`/`fstream` (RAM-backed, so save states work unchanged)
- `overrides/`: bare-metal replacements for `Configuration`, `Controller`, `APU` (silent for now), `Video.hpp`
- Everything else is compiled straight from `../source`

`overrides/Emulation/MemoryAccess.*` carries a fix for a dangling pointer in
`MemoryAccess(SMBEngine&, uint8_t constant)`: it stored `&constant` (the
parameter) instead of `&this->constant`. The game only worked at `-O0`; any
optimization level hangs at boot. The same fix applies to `../source`.

Serial output (COM1) logs boot progress; `make run` shows it in the terminal.
