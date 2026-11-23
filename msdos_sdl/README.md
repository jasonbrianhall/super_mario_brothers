Super Mario Bros. for MS-DOS (SDL3)
===================================

This is the SDL version of Super Mario Brothers Virtualized -- the original
game's 6502 code translated to C++, with the emulated PPU and APU -- built
for MS-DOS as a 32-bit protected-mode (DPMI) program. It is the normal
source (`../source`), on SDL3, whose DOS port provides the video (VESA and
VGA), the Sound Blaster, the keyboard and a gameport joystick. It is built
the same way as SDLPoP's DOS port (SDLPoPDOS/msdos).

(The `msdos` folder next to this one is the older Allegro 4 port, with its
own copy of the sources.)


Running it
----------

Copy the SMB folder (from `make dist`) anywhere and run SMB.EXE. The DPMI
host (CWSDPMI) is built into the EXE; under Windows 9x or with HDPMI32
loaded, that one is used instead.

    C:\SMB> SMB.EXE
    C:\SMB> SMB.EXE /?           keys and settings
    C:\SMB> SMB.EXE > LOG.TXT    keep the messages (video mode, sound card, speed)

Requirements:

  * 486DX (or a 486SX with a 487) or better; a fast Pentium to draw all 60
    frames a second. The game always runs at full speed: on a slower
    machine frames are skipped (drawn less often), not slowed down. In
    DOSBox, use `cycles=max` (at 100000 cycles it draws about 50 a second
    in 640x480, 53 in 320x240 or mode 13h).
  * 8 MB of RAM or more.
  * VGA. A VESA BIOS (or UniVBE) gives 640x480 with the picture doubled;
    without one the game runs in VGA mode 13h.
  * Optional: Sound Blaster (1.x/2.0, Pro, 16) or compatible, with the
    BLASTER environment variable set (e.g. `SET BLASTER=A220 I7 D1 H5 T6`).
  * Optional: gameport joystick.

When you quit, SMB.EXE prints how many frames it ran and drew, and how long
each part took (game, picture, copying to the screen): the place to look if
it is slow.


Video modes
-----------

By default the game picks the smallest hicolor (VESA) mode the 256x240
picture fits in twice -- 640x480 -- and draws it doubled, with sharp pixels.
Without one, a mode it fits in once (320x240, 400x300 ...), then an 8-bit
VESA mode, then VGA mode 13h (320x200: the 8 overscan lines at the top and
bottom are dropped, as a TV would, and the other 224 are squeezed into 200).

The NES has 64 colors, so 8-bit modes show the picture exactly, with half as
much to copy as hicolor. To choose, set SMB_VIDEO to WIDTHxHEIGHTxBITS before
starting:

    SET SMB_VIDEO=640x480x8      VESA 640x480, 256 colors, picture doubled
    SET SMB_VIDEO=320x240x16     VESA 320x240, picture 1:1 (least to copy)
    SET SMB_VIDEO=320x200x8      VGA mode 13h (any VGA card)
    SET SMB_VIDEO=640x480x16     the default where there is one


Sound
-----

The APU's output goes to the Sound Blaster's DSP (SDL3 configures it from
BLASTER), 8-bit mono at `audio.frequency` from SMBC.CON: 22050 Hz as
shipped, which every Sound Blaster plays and which costs the least CPU;
44100 on a Sound Blaster 16. `SET SMB_NOSOUND=1` turns it off.


Keys
----

    Arrows              move
    X                   A (jump)
    Z                   B (run / fireball)
    Enter               Start
    Backspace, R.Shift  Select
    R                   reset
    F5 .. F8            save state 1-4 (SAVE1.SAV ...)
    Shift+F5 .. F8      load state 1-4
    Esc                 quit

The player-1 keys can be changed in SMBC.CON (as SDL scancodes; the file
lists the common ones).

Gameport joystick: stick = d-pad, button 1 = A, button 2 = B, button 3 =
Select, button 4 = Start. `SET SMB_NOJOY=1` ignores the gameport.


Settings
--------

SMBC.CON (`smbc.conf` elsewhere; DOS file names are 8.3) is the same file as
on other systems; this folder has one with DOS defaults. What matters on
DOS:

  * `[audio] enabled`, `frequency`
  * `[video] palette_file` -- a 192-byte NES palette (.PAL)
  * `[video] hqdn3d`, `antialiasing` -- the smoothing filters; slow on old
    machines, and they turn off the fast path to the screen (below)
  * `[input] player1.key.*`, `joystick.deadzone`

`video.scale`, `scanlines` and `vsync` are for the windowed versions.

`SET SMB_FRAMESKIP=n` draws only every (n+1)th frame (default: automatic).


Building
--------

Everything is in this folder; no CMake, no sources are downloaded. The
compiler is a DJGPP cross compiler on the PATH (`i586-pc-msdosdjgpp-gcc` and
`-g++`; prebuilt ones: https://github.com/andrewwutw/build-djgpp/releases)
when there is one, otherwise the `djfdyuruiry/djgpp` Docker image (pulled
automatically). Force either with `make DOCKER=1` / `make DOCKER=0`. Plus GNU
make.

    cd msdos_sdl
    make            # SDL3 (into build/libSDL3.a), the game, smb.exe with CWSDPMI
                    # inside, and SMBC.CON next to it
    make run        # play it in DOSBox, this folder as C:
    make dist       # build/dist/SMB: just the game files, to copy to a DOS machine
    make clean      # leaves SMBC.CON and your save states alone

After `make`, this folder is a working game: run SMB.EXE here (DOSBox, a
shared drive, ...). For a real DOS machine, copy smb.exe and SMBC.CON (or use
`make dist`); SDL3\, sdl3-config\, cwsdpmi\, compat\ and build\ are only for
building. The ROM is compiled in (source/SMBRom.cpp, made from your own ROM
with the converter -- see the main README).

`make msdos_original` makes an smb.exe that needs CWSDPMI.EXE (copied next
to it) or another DPMI host.

What is here:

  * SDL3/ -- the complete SDL3 source (https://libsdl.org, commit 4ca1b1c),
    which includes the DOS port; the same tree as SDLPoPDOS's msdos/SDL3,
    with its DOS fixes (keyboard and Sound Blaster interrupt handlers,
    event pumping, timer; marked "SDLPoP-DOS" in the code). The Makefile
    compiles the files SDL's own build uses for DJGPP.
  * sdl3-config/ -- SDL_build_config.h and SDL_revision.h, which SDL's CMake
    would generate; made once for DJGPP.
  * cwsdpmi/ -- CWSDPMI r7 by Charles W. Sandmann (binaries, source and
    documentation), the DPMI host. CWSDSTUB.EXE is put in front of the game.
  * compat/SDL2/SDL.h -- the shared sources are written for SDL2
    (`#include <SDL2/SDL.h>`); on DOS this header is found first and maps
    the SDL2 names they use (joystick and game controller calls, event
    types, ...) onto SDL3.
  * compat/boost/property_tree -- Configuration.cpp reads smbc.conf with
    Boost's property_tree; DJGPP has no Boost, so this is the small part of
    it that is used (get, put, read_ini, write_ini).
  * smbc.conf -- the settings with DOS defaults (becomes SMBC.CON).

How it fits together:

  * source/DOSMain.cpp -- the DOS front end, instead of SDLMain.cpp: SDL3
    set-up, keyboard and gameport, the Sound Blaster through an SDL3 audio
    stream, 60 frames a second with frame skipping, save states. It
    includes SDL_main.h, which on DOS sets up the near pointers SDL needs.
  * source/DOSVideo.cpp -- on DOS there is no SDL renderer worth using: the
    picture goes straight to the screen surface, scaled by a whole number
    (or squeezed into 200 lines in mode 13h). Every pixel the PPU draws
    comes from the 64-entry NES palette table, so that table is filled with
    the screen's own pixel values (palette indexes in 8-bit modes, RGB565 in
    16-bit ones) and showing a frame is a plain copy; with a smoothing
    filter on, the frame is converted instead.

Changes to the shared sources, made for this port (they apply to every
version; the output is bit-for-bit the same as before):

  * Emulation/Controller.cpp -- builds with SDL3 too (`event.key.scancode`).
  * Emulation/MemoryAccess.cpp/.hpp -- a MemoryAccess made from a constant
    pointed at the constructor's parameter (gone once the constructor
    returns) instead of its own copy; it worked by luck and hung in the
    sprite-0 wait loop when compiled differently.
  * Emulation/APU.cpp/.hpp -- the channel timers are advanced in one step
    between samples instead of ~15,000 single steps a frame, the mixer is a
    table lookup instead of a search of a 256-entry cache, and an integer
    comparison replaces two divisions per step: the game logic (with sound)
    takes under a third of the time it did.
  * Emulation/PPU.cpp -- background tiles skip fully transparent rows and
    look up their three colors once per tile: the picture takes about half
    the time.
