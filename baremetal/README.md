# Bare-metal Super Mario Bros.

Boots straight into the game on x86_64: no OS, no libc, no SDL. GRUB (or
QEMU's `-kernel`) loads the kernel and your ROM, a small stub switches to
64-bit long mode, and the unchanged engine core runs on a framebuffer.

You must supply your own ROM (md5 `811b027eaf99c2def7b933c5208636de`).
It is loaded at boot as a module and is never compiled in.

## Build and run

```
sudo apt install build-essential qemu-system-x86 grub-pc-bin grub-common xorriso mtools
# Fedora: sudo dnf install gcc-c++ qemu-system-x86 grub2-tools grub2-tools-extra grub2-pc-modules xorriso mtools
# For UEFI (smb.efi): Debian gnu-efi ovmf / Fedora gnu-efi-devel edk2-ovmf
cd baremetal
make
make run ROM=~/nes/smb.nes          # QEMU/KVM, direct kernel boot
make iso ROM=~/nes/smb.nes          # bootable smb.iso (GRUB)
make run-iso ROM=~/nes/smb.nes
make floppy ROM=~/nes/smb.nes       # 1.44 MB boot floppy (smb-floppy.img), ~19% full
make run-floppy ROM=~/nes/smb.nes
make efi                            # smb.efi, a UEFI application
make run-efi ROM=~/nes/smb.nes      # boots it under OVMF UEFI firmware
```

8 MB of RAM is enough; the heap uses whatever RAM the machine has.

The floppy image holds GRUB, the kernel, the ROM and `grub.cfg` in GRUB's
compressed core image, with no filesystem. Write it with
`dd if=smb-floppy.img of=/dev/fdX`, or use it with a USB floppy drive or
emulator that boots as drive A:.

### UEFI machines

`smb.efi` is a UEFI application with the kernel embedded; it loads `smb.nes`
from its own folder, takes the screen mode UEFI's graphics output is using,
and runs full screen at whatever resolution that is. To add it to Fedora's
GRUB menu, put both files on the EFI system partition:

```
sudo mkdir -p /boot/efi/EFI/smb
sudo cp smb.efi smb.nes /boot/efi/EFI/smb/
```

then add this to `/etc/grub.d/40_custom` and run
`sudo grub2-mkconfig -o /boot/grub2/grub.cfg`:

```
menuentry "Super Mario Bros. (bare metal)" {
    search --no-floppy --set=root --file /EFI/smb/smb.efi
    chainloader /EFI/smb/smb.efi
}
```

Boot options go after the path (`chainloader /EFI/smb/smb.efi audio=ac97`).
With Secure Boot on, the firmware only runs signed binaries: either turn
Secure Boot off, or sign `smb.efi` with your own Machine Owner Key (see below).

**Signing for Secure Boot** (Fedora: `sudo dnf install sbsigntools mokutil openssl`):

```
openssl req -new -x509 -newkey rsa:2048 -nodes -days 3650 -subj "/CN=SMB bare metal/" \
        -keyout MOK.key -out MOK.crt
openssl x509 -in MOK.crt -outform DER -out MOK.cer
sudo mokutil --import MOK.cer            # set a one-time password; enroll it in MokManager on reboot
sbsign --key MOK.key --cert MOK.crt --output smb.efi smb.efi
```

### Sound

Intel HD Audio is used if present (every PC since ~2005), otherwise AC97.
The driver finds the codec's line-out, speaker and headphone pins and routes
each to a DAC. `make run` gives QEMU an HD Audio card by default; pick with
`SOUND=ac97` or `SOUND=none`. The QEMU audio backend defaults to PulseAudio
(`AUDIODEV=pa`; try `pipewire`, `alsa` or `sdl` if needed). HDMI audio isn't
supported.

### Keyboards

PS/2 keyboards and USB keyboards on an xHCI controller both work, including
USB keyboards plugged in after boot. USB keyboards must be on a root port
(not behind a hub). `make run` adds a QEMU USB keyboard; to prove it's the one
being used, run QEMU with `-machine pc,i8042=off`.

### Boot options

Add these after `multiboot /boot/smb.elf` in `grub.cfg`, or via `ARGS=` with
`make run`:

| Option | Effect |
|---|---|
| `audio=hda` / `audio=ac97` / `audio=off` | Force a sound card, or none |
| `usb=off` | Leave the USB controller to the BIOS |
| `debug` | Once-a-second heartbeat on the serial port |

The ISO and floppy boot on BIOS (legacy/CSM) PCs; `smb.efi` covers UEFI.

## Controls

| | Player 1 (Mario) | Player 2 (Luigi) |
|---|---|---|
| D-pad | Arrows | W A S D |
| A / B | X / Z | G / F |
| Select / Start | `[` / `]` | Q / E |

In a 2-player game Luigi reads controller 2, as on the NES. Both players can
share one keyboard, and either set of keys works on any PS/2 or USB keyboard.

| Key | Action |
|---|---|
| F1 | Show/hide key bindings (Esc also closes) |
| F5-F8 | Save state to slot 1-4 (RAM, lost on reboot) |
| Shift+F5-F8 | Load state |
| P | Pause |
| F12 | Reset |

## How it's put together

- `boot.S`: Multiboot header, long-mode switch, identity-mapped 4 GiB, interrupt stubs; UEFI entry point
- `efi/loader.c`: the UEFI loader in `smb.efi`: reads the ROM, sets up graphics, relocates the embedded kernel, exits boot services and passes Multiboot-style info
- `kernel.cpp`: framebuffer (GRUB's, or QEMU's VBE adapter), 60 Hz PIT timer, PS/2 keyboard, main loop
- `usb.cpp`: polled xHCI driver: BIOS handoff, enumeration, HID boot-protocol keyboards, hot-plug
- `runtime.cpp`, `include/`: memcpy/printf/malloc, a minimal `std::string`/`iostream`/`fstream` (RAM-backed, so save states work unchanged)
- `audio.cpp`: HD Audio (CORB/RIRB codec setup, one output stream) and AC97 drivers sharing one looping DMA ring, polled once per frame
- `pci.cpp`: PCI configuration-space helpers
- `overrides/`: bare-metal replacements for `Configuration`, `Controller`, `Video.hpp`
- Everything else, including `APU.cpp`, is compiled straight from `../source`

The APU's samples are unsigned mix levels (0 to about 130). The SDL build
opens the device as `AUDIO_S8`, so anything above 127 wraps to -128 and
clicks. Here they're treated as unsigned and passed through a DC-blocking
filter instead.

`overrides/Emulation/MemoryAccess.*` carries a fix for a dangling pointer in
`MemoryAccess(SMBEngine&, uint8_t constant)`: it stored `&constant` (the
parameter) instead of `&this->constant`. The game only worked at `-O0`; any
optimization level hangs at boot. The same fix applies to `../source`.

Serial output (COM1) logs boot progress; `make run` shows it in the terminal.
