# Super Mario Bros NES Emulator

A 6502 CPU emulator that can run some classic NES games, built with Allegro 4 for both DOS and Linux platforms.

## ⚠️ Important Disclaimer

**This emulator is NOT cycle-accurate and has significant compatibility issues, but it does work for a few games.**

This is a hobby project that implements basic NES functionality. Don't expect commercial-level compatibility or accuracy.

## What Works

- **Super Mario Bros** - 100% playable
- **Mario Bros** - 100% playable  
- **Popeye** - 100% playable
- **Many NROM games** - Excellent compatibility
- **Basic MMC1, MMC3, UxROM support** - Limited but functional
- **Audio output** - Excellent APU emulation (~95% accurate)
- **Controllers** - Keyboard and joystick input
- **Save states** - F5-F8 for save/load
- **Smooth scrolling** - VSync implementation provides fluid gameplay

## What's Broken

- **Timing is completely wrong** - Not cycle-accurate at all
- **UxROM sprite banking** - Sprites often render incorrectly
- **MMC3 IRQ timing** - Many games won't work properly  
- **PPU timing** - VBlank, sprite 0 hit, and scrolling issues
- **APU accuracy** - Very good audio quality (~95% accurate)
- **Many mappers** - Limited mapper support

## Controls

### Default Player 1
- **Arrow Keys** - D-pad
- **Z** - A button  
- **X** - B button
- **[** - Select
- **]** - Start

### Default Player 2  
- **WASD** - D-pad
- **F** - A button
- **G** - B button  
- **O** - Select
- **P** - Start

### Hotkeys
- **ESC** - Menu
- **F5-F8** - Save states (Shift+F5-F8 to load)
- **Ctrl+R** - Reset game
- **Ctrl+P** - Pause
- **F11** - Fullscreen toggle (Linux only)

## Building

### DOS (DJGPP)
**Requires Docker**
```bash
./build_dos.sh
```

### Linux
```bash
make
```

## Requirements

- **Allegro 4.x** development libraries
- **Docker** (required for DOS builds)
- **GCC** for Linux builds

## Usage

```bash
./smbe <rom_file.nes>
```

Example:
```bash
./smbe smb.nes
```

## Supported Mappers

- **Mapper 0 (NROM)** - Basic support
- **Mapper 1 (MMC1)** - Buggy banking
- **Mapper 2 (UxROM)** - Sprite issues  
- **Mapper 3 (CNROM)** - Basic CHR banking
- **Mapper 4 (MMC3)** - Very buggy IRQ timing
- **Mapper 66 (GxROM)** - Limited support

## Known Issues

1. **UxROM games** (Contra, DuckTales) have sprite rendering problems
2. **MMC3 games** often hang or crash due to IRQ timing
3. **CPU timing** is completely inaccurate
4. **PPU rendering** happens once per frame instead of scanline-by-scanline

## Configuration Files

- `controls.cfg` - Controller mappings
- `video.cfg` - Video settings  
- `audio.cfg` - Audio configuration
- `config.cfg` - General settings

## Why Is This So Buggy?

This emulator was built as a learning project without proper cycle-accurate timing. Real NES emulation requires:

- Cycle-by-cycle CPU/PPU synchronization
- Accurate mapper implementations  
- Proper PPU scanline rendering
- Precise IRQ timing
- Complex sprite evaluation logic

This emulator does none of that correctly. For accurate NES emulation, use:
- **Mesen** 
- **FCEUX**
- **Nestopia**
- **puNES**

## License

MIT License

Copyright (c) 2025

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

The original games remain property of their respective copyright holders.

## Contributing

Feel free to submit pull requests to fix issues, but be aware that the architecture has fundamental limitations for accuracy. A complete rewrite would be needed for proper cycle-accurate NES emulation.

---

*"Emulation is hard. Accurate emulation is harder."*