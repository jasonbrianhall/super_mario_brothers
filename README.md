# Super Mario Brothers Virtualized

This is not a remake. This is the original Super Mario Bros. running on a cycle-accurate, virtualized 6502 CPU, complete with emulated PPU and APU behavior. Built with GTK3+ for configuration and controls and SDL for input and audio—no video subsystem required—this project is a deep-dive into retro hardware emulation with a minimalist GUI.
Core Features

* 🧠 Fully virtualized 6502 instruction set execution

* 🎮 SDL-driven joystick/gamepad support for up to 2 players

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
``

`
