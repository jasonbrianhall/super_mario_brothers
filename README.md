# Super Mario Brothers Virtualized

This is not a remake. This is the original Super Mario Bros. running on a virtualized 6502 CPU with emulated PPU and APU behavior. Unlike cycle-accurate emulators, this virtualizer translates 6502 instructions to native code for maximum performance and won't slow down like a real NES when overwhelmed. Available in two versions:

1. **GTK3+/SDL Version**: Built with GTK3+ for configuration and controls and SDL for input and audio, includes APU and PPU caching optimizations
2. **Allegro DOS/Linux Version**: DOS (Allegro 4 only) and Linux (Allegro 4 or 5) with extensive performance caching systems including scaling optimization

You do have to provide your own NES ROM (md5sum 811b027eaf99c2def7b933c5208636de)

## Core Features

* 🧠 Virtualized 6502 instruction set execution (6502 instructions translated to native C++ code, compiled for maximum performance)
* ⚡ Performance-optimized execution - won't slow down like a real NES when the CPU gets overwhelmed
* 🎮 Joystick/gamepad support (Linux) and keyboard support (both platforms)
* 🔊 Low-latency, platform-sensitive audio output
* 🖥️ Cross-platform GUI interface for launching, configuration, and state feedback
* 🧩 Modular design for hacking, debugging, or embedding
* 🚀 Performance optimizations: APU and PPU caching in both versions, plus scaling cache in Allegro version

## Performance Optimizations

Both versions include core performance optimizations:

### 🚀 **APU Caching System** (Both Versions)
- Caches expensive floating-point operations for audio synthesis
- Pre-calculated waveform tables for all NES audio channels
- Optimized for 8-bit and 16-bit audio output paths

### 🎨 **PPU Rendering Cache** (Both Versions)
- Sprite caching system with dirty flagging
- Tile pattern cache with automatic invalidation
- Palette lookup optimization for faster color conversion
- 16-bit direct rendering paths for maximum performance

## Allegro DOS/Linux Version - Additional Optimizations

The Allegro version (DOS: Allegro 4, Linux: Allegro 4 or 5) includes all the above optimizations plus:

### 📺 **Scaling Cache System** (Allegro Version Only)
- Pre-calculated coordinate mapping tables eliminate runtime multiplication
- Optimized 1x, 2x, and 3x scaling paths with unrolled loops
- DOS-specific 320x200 VGA Mode 13h optimization
- Memory-efficient caching reduces CPU overhead by 2-5x

### 🖥️ **Platform-Specific Features**
- **DOS**: Classic VGA Mode 13h (320x200) support, direct hardware access, keyboard-only input, Allegro 4
- **Linux**: F11 fullscreen toggle, multiple resolution support, full joystick/gamepad support, Allegro 4 or 5
- **Both**: Configurable keyboard controls, save/load configurations

## Convert your legally owned NES ROM (YOU MUST LEGALLY OWN THE GAME AND RIP IT YOURSELF)

``` bash
cd source
g++ rom_to_header.cpp -o rom_to_header
./rom_to_header ~/nes/Super\ Mario\ Bros.\ \(JU\)\ \(PRG0\)\ \[\!].nes SMBRom smbRomData
```
Output:
```
Successfully converted ROM file to header and source:
  Input: /home/yourusername/nes/Super Mario Bros. (JU) (PRG0) [!].nes (40976 bytes)
  Output Header: SMBRom.hpp
  Output Source: SMBRom.cpp
  Variable: smbRomData[40976]
```

## Optional: Reconvert the Super Mario 6502 ASM

This code builds the C++ code from smbdis.asm (maybe you found a bug or just want to see how it works):

``` bash
cd converter
python main.py smbdis.asm ../source/SMB/ smb_config/
```

## Build Requirements

### Linux Version
- Allegro 4.x or 5.x development libraries
- GCC/G++ compiler
- Standard Linux development tools

### DOS Version  
- Docker (for cross-compilation)
- Internet connection (build script downloads DJGPP toolchain, Allegro 4 source, and CSDPMI)
- The build process automatically handles all dependencies via `djfdyuruiry/djgpp` Docker image

## Build & Run

### GTK3+/SDL Version (Original)
``` bash
make
cd build/linux
./smbc

make windows
cd build/windows
# Copy all files to Windows and run smbc.exe
```

### Allegro4/5 DOS/Linux Version (Optimized)
``` bash
# Linux build
cd msdos
make
cd build/linux
./smbc

# DOS build from scratch (requires Docker)
cd msdos
docker run --rm -v $(pwd):/work djgpp/djgpp:latest bash -c "cd /work && ./build_from_scratch.sh /work/path/to/your/Super\ Mario\ Bros.nes"
cd build/dos
# Copy to DOS system and run smbc.exe
```

The DOS version builds in `msdos/build/dos/` and the Linux version builds in `msdos/build/linux/`.

## Controls

### Default Controls
- **Player 1**: Arrow keys, Z/X (A/B), [ ] (Select/Start)
- **Player 2**: WASD, F/G (A/B), O/P (Select/Start)
- **System**: ESC (menu), P (pause), Ctrl+R (reset)
- **Linux only**: F11 (fullscreen toggle)

### Configurable Controls
Keyboard controls are fully configurable through the in-game menu system:
- Individual key mapping for both players
- **Linux**: Joystick/gamepad button assignment and analog stick vs digital pad selection
- **DOS**: Keyboard-only (joystick support disabled due to DOSBox compatibility issues)
- Settings automatically saved to `controls.cfg`

## Performance

The Allegro version is specifically optimized for older hardware:
- **Modern systems**: 500+ FPS rendering capability
- **486 DX2/66**: Smooth 60 FPS gameplay
- **Pentium systems**: Optimal performance with all caching enabled
- **Memory usage**: ~2-500KB for all caching systems combined

## Technical Details

### Input System
- **Linux**: Full keyboard and joystick/gamepad support via Allegro 4 or 5
- **DOS**: Keyboard-only input via Allegro 4 (joystick disabled due to DOSBox modern controller incompatibility)
- Configurable key mappings with real-time capture
- Support for up to 2 players on keyboard

### Audio System
- **DOS**: 8-bit unsigned PCM, optimized for Sound Blaster compatibility
- **Linux**: 16-bit signed PCM with fallback support
- Configurable sample rates (11025Hz to 44100Hz)
- Real-time audio streaming with underrun detection

### Video System
- Native NES resolution (256x240) with intelligent scaling
- Multiple color depth support (8-bit, 16-bit, 24-bit, 32-bit)
- Automatic aspect ratio correction
- VSync support for tear-free rendering

## License

MIT License - The code is freely available under MIT License. However, **the original Super Mario Brothers ROM remains copyrighted by Nintendo**. You must legally own the original game to use this software.

## Notes

Due to fair use, you must own the original Super Mario Brothers Game in order to play this. By playing this game, you acknowledge that you own the original game.

This project demonstrates advanced virtualization techniques including 6502-to-native code translation, sophisticated caching systems, and platform-specific optimizations for maximum performance on both modern and retro hardware. Unlike traditional cycle-accurate emulators, this virtualizer prioritizes performance over timing accuracy.

## 🎮 Other Projects by Jason Brian Hall

Bored? Let me rescue you from the depths of monotony with these digital delights! 🚀

🧩 **Tetrimone**: [Tetrimone](https://github.com/jasonbrianhall/tetrimone) - Pixel-Dropping Pandemonium! A GTK-powered block-stacking adventure with 11 spectacular line-clearing animations, Soviet easter eggs, and enough visual polish to make your neurons dance! 🕹️✨

💣 **Minesweeper Madness**: [Minesweeper](https://github.com/jasonbrianhall/minesweeper) - Not just a game, it's a digital minefield of excitement! (It's actually a really good version, pinky promise! 🤞)

🧩 **Sudoku Solver Spectacular**: [Sudoku Solver](https://github.com/jasonbrianhall/sudoku_solver) - A Sudoku Swiss Army Knife! 🚀 This project is way more than just solving puzzles. Dive into a world where:
- 🧠 Puzzle Generation: Create brain-twisting Sudoku challenges
- 📄 MS-Word Magic: Generate professional puzzle documents
- 🚀 Extreme Solver: Crack instantaneously the most mind-bending Sudoku puzzles
- 🎮 Bonus Game Mode: Check out the playable version hidden in python_generated_puzzles

Numbers have never been this exciting! Prepare for a Sudoku adventure that'll make your brain cells do a happy dance! 🕺

🧊 **Rubik's Cube Chaos**: [Rubik's Cube Solver](https://github.com/jasonbrianhall/rubikscube/) - Crack the code of the most mind-bending 3x3 puzzle known to humanity! Solving optional, frustration guaranteed! 😅

🐛 **Willy the Worm's Wild Ride**: [Willy the worm](https://github.com/jasonbrianhall/willytheworm) - A 2D side-scroller starring the most adventurous invertebrate in gaming history! Who said worms can't be heroes? 🦸‍♂️

🧙‍♂️ **The Wizard's Castle: Choose Your Own Adventure**: [The Wizard's Castle](https://github.com/jasonbrianhall/wizardscastle) - A Text-Based RPG that works on QT5, CLI, and even Android! Magic knows no boundaries! ✨

🗺️ **Zork Android Adventure**: [Zork Android](https://github.com/jasonbrianhall/zork_android) - The legendary text adventure in your pocket! Also available as [DJGPP/MS-DOS version](https://github.com/jasonbrianhall/zork_djgpp). Warning: This 4-hour Android coding project somehow became my most popular on SourceForge - go figure! 🤷‍♂️ Proof that classics never die! 📜

🔤 **Hangman Hijinks**: [Hangman](https://github.com/jasonbrianhall/hangman) - Word-guessing mayhem in your terminal! Prepare for linguistic warfare! 💬

🃏 **Card Games Collection**: [Solitaire, FreeCell & Spider](https://github.com/jasonbrianhall/cardgames) - The most meticulously crafted card games with custom decks, animations, and more features than you can shuffle!