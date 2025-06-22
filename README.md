# Super Mario Brothers Virtualized

This is not a remake. This is the original Super Mario Bros. running on a cycle-accurate, virtualized 6502 CPU, complete with emulated PPU and APU behavior. Built with GTK3+ for configuration and controls and SDL for input and audio—no video subsystem required—this project is a deep-dive into retro hardware emulation with a minimalist GUI.
Core Features

* 🧠 Fully virtualized 6502 instruction set execution (translated real 6502 instructions into their equivalent C++ instructions then compiled natively)

* 🎮 SDL-driven joystick/gamepad support for up to 2 players (includes keyboard support)

* 🔊 Low-latency, platform-sensitive audio output (16-bit Windows / 8-bit Linux)

* 🖥️ GTK3+ interface for launching, configuration, and state feedback

* 🧩 Modular design for hacking, debugging, or embedding

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
