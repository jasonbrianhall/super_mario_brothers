# Super Mario Brothers Virtualized

This is not a remake. This is the original Super Mario Bros. running on a cycle-accurate, virtualized 6502 CPU, complete with emulated PPU and APU behavior. Built with GTK3+ for configuration and controls and SDL for input and audio—no video subsystem required—this project is a deep-dive into retro hardware emulation with a minimalist GUI.  You do have to provide your own NES ROM (md5sum 811b027eaf99c2def7b933c5208636de) 

Core Features

* 🧠 Fully virtualized 6502 instruction set execution (translated real 6502 instructions into their equivalent C++ instructions then compiled natively)

* 🎮 SDL-driven joystick/gamepad support for up to 2 players (includes keyboard support)

* 🔊 Low-latency, platform-sensitive audio output (16-bit Windows / 8-bit Linux)

* 🖥️ GTK3+ interface for launching, configuration, and state feedback

* 🧩 Modular design for hacking, debugging, or embedding

## Convert your legally owned NES ROM (YOU MUST LEGALLY OWN THE GAME AND RIP IT YOURSELF)

``` bash
cd source
g++ rom_to_header.cpp  -o rom_to_header
./rom_to_header  ~/nes/Super\ Mario\ Bros.\ \(JU\)\ \(PRG0\)\ \[\!].nes SMBRom smbRomData
Successfully converted ROM file to header and source:
  Input: /home/yourusername/nes/Super Mario Bros. (JU) (PRG0) [!].nes (40976 bytes)
  Output Header: SMBRom.hpp
  Output Source: SMBRom.cpp
  Variable: smbRomData[40976]
```

## Optional, reconvert the Super Mario 6502 ASM (maybe you found a bug or just want to see how it works)

### This code builds the C++ code from smbdis.asm

``` bash
cd converter
python main.py smbdis.asm ../source/SMB/ smb_config/
```

## Build & Run
``` bash
make
cd build/linux
./smbc

make windows
cd build/windows
Copy All files to windows and run smbc.exe
```

## Notes

Due to fair use, you must own the original Super Mario Brothers Game in order to play this.  By playing this game, you acknowledge that you own the original game.
