# Makefile for warpnes with SDL and Allegro Support for Linux and Windows
# Supports dual builds: SDL (modern) and Allegro (retro) versions
# Last modification: $(shell date +%m/%d/%Y)

# Compiler settings
CXX_LINUX = g++
CXX_WIN = x86_64-w64-mingw32-g++
CXXFLAGS_COMMON = -s -fpermissive

# Debug flags
DEBUG_FLAGS = -g -DDEBUG

# SDL flags for Linux and Windows
SDL_CFLAGS_LINUX = $(shell pkg-config --cflags sdl2 2>/dev/null || echo "-I/usr/include/SDL2")
SDL_LIBS_LINUX = $(shell pkg-config --libs sdl2 2>/dev/null || echo "-lSDL2")

SDL_CFLAGS_WIN = -I/usr/x86_64-w64-mingw32/include/SDL2
SDL_LIBS_WIN = -lmingw32 -lSDL2main -lSDL2 -static-libgcc -static-libstdc++

# Allegro flags for Linux and Windows
ALLEGRO_CFLAGS_LINUX = -I/usr/include
ALLEGRO_LIBS_LINUX = -lalleg

ALLEGRO_CFLAGS_WIN = -I/usr/x86_64-w64-mingw32/include
ALLEGRO_LIBS_WIN = -lalleg -lwinmm -static-libgcc -static-libstdc++

# SDL Platform-specific settings
CXXFLAGS_LINUX_SDL = $(CXXFLAGS_COMMON) $(SDL_CFLAGS_LINUX) -DLINUX -DSDL_BUILD -std=c++11
CXXFLAGS_WIN_SDL = $(CXXFLAGS_COMMON) $(SDL_CFLAGS_WIN) -DWIN32 -DSDL_BUILD -std=c++11

# Allegro Platform-specific settings  
CXXFLAGS_LINUX_ALLEGRO = $(CXXFLAGS_COMMON) $(ALLEGRO_CFLAGS_LINUX) -DLINUX -DALLEGRO_BUILD
CXXFLAGS_WIN_ALLEGRO = $(CXXFLAGS_COMMON) $(ALLEGRO_CFLAGS_WIN) -DWIN32 -DALLEGRO_BUILD

# Debug-specific flags
CXXFLAGS_LINUX_SDL_DEBUG = $(CXXFLAGS_LINUX_SDL) $(DEBUG_FLAGS)
CXXFLAGS_WIN_SDL_DEBUG = $(CXXFLAGS_WIN_SDL) $(DEBUG_FLAGS)
CXXFLAGS_LINUX_ALLEGRO_DEBUG = $(CXXFLAGS_LINUX_ALLEGRO) $(DEBUG_FLAGS)
CXXFLAGS_WIN_ALLEGRO_DEBUG = $(CXXFLAGS_WIN_ALLEGRO) $(DEBUG_FLAGS)

# Linker flags
LDFLAGS_LINUX_SDL = $(SDL_LIBS_LINUX)
LDFLAGS_WIN_SDL = $(SDL_LIBS_WIN)
LDFLAGS_LINUX_ALLEGRO = $(ALLEGRO_LIBS_LINUX)
LDFLAGS_WIN_ALLEGRO = $(ALLEGRO_LIBS_WIN)

# Common source files (used by all platforms)
COMMON_SOURCE_FILES = \
    source/Configuration.cpp \
    source/Emulation/APU.cpp \
    source/Emulation/PPU.cpp \
    source/Zapper.cpp \
    source/SMB/SMBEmulator.cpp \
    source/SMB/Battery.cpp

# Platform-specific source files
SDL_SOURCE_FILES = $(COMMON_SOURCE_FILES) \
    source/SDLMainWindow.cpp \
    source/SDLCacheScaling.cpp \
    source/Emulation/ControllerSDL.cpp


ALLEGRO_SOURCE_FILES = $(COMMON_SOURCE_FILES) \
    source/dos_main.cpp \
    source/Emulation/AllegroMidi.cpp \
    source/Emulation/Controller.cpp


# Object files for SDL versions
OBJS_LINUX_SDL = $(patsubst %.cpp,%.sdl.o,$(SDL_SOURCE_FILES))
OBJS_WIN_SDL = $(patsubst %.cpp,%.win.sdl.o,$(SDL_SOURCE_FILES))
OBJS_LINUX_SDL_DEBUG = $(patsubst %.cpp,%.sdl.debug.o,$(SDL_SOURCE_FILES))
OBJS_WIN_SDL_DEBUG = $(patsubst %.cpp,%.win.sdl.debug.o,$(SDL_SOURCE_FILES))

# Object files for Allegro versions
OBJS_LINUX_ALLEGRO = $(patsubst %.cpp,%.allegro.o,$(ALLEGRO_SOURCE_FILES))
OBJS_WIN_ALLEGRO = $(patsubst %.cpp,%.win.allegro.o,$(ALLEGRO_SOURCE_FILES))
OBJS_LINUX_ALLEGRO_DEBUG = $(patsubst %.cpp,%.allegro.debug.o,$(ALLEGRO_SOURCE_FILES))
OBJS_WIN_ALLEGRO_DEBUG = $(patsubst %.cpp,%.win.allegro.debug.o,$(ALLEGRO_SOURCE_FILES))

# Target executables - SDL versions
TARGET_LINUX_SDL = warpnes-sdl
TARGET_WIN_SDL = warpnes-sdl.exe
TARGET_LINUX_SDL_DEBUG = warpnes-sdl_debug
TARGET_WIN_SDL_DEBUG = warpnes-sdl_debug.exe

# Target executables - Allegro versions
TARGET_LINUX_ALLEGRO = warpnes-allegro
TARGET_WIN_ALLEGRO = warpnes-allegro.exe
TARGET_LINUX_ALLEGRO_DEBUG = warpnes-allegro_debug
TARGET_WIN_ALLEGRO_DEBUG = warpnes-allegro_debug.exe

# Build directories
BUILD_DIR = build
BUILD_DIR_LINUX_SDL = $(BUILD_DIR)/linux-sdl
BUILD_DIR_WIN_SDL = $(BUILD_DIR)/windows-sdl
BUILD_DIR_LINUX_ALLEGRO = $(BUILD_DIR)/linux-allegro
BUILD_DIR_WIN_ALLEGRO = $(BUILD_DIR)/windows-allegro
BUILD_DIR_LINUX_SDL_DEBUG = $(BUILD_DIR)/linux-sdl-debug
BUILD_DIR_WIN_SDL_DEBUG = $(BUILD_DIR)/windows-sdl-debug
BUILD_DIR_LINUX_ALLEGRO_DEBUG = $(BUILD_DIR)/linux-allegro-debug
BUILD_DIR_WIN_ALLEGRO_DEBUG = $(BUILD_DIR)/windows-allegro-debug

# Create necessary directories
$(shell mkdir -p $(BUILD_DIR_LINUX_SDL)/source/Emulation $(BUILD_DIR_LINUX_SDL)/source/SMB \
	$(BUILD_DIR_WIN_SDL)/source/Emulation $(BUILD_DIR_WIN_SDL)/source/SMB \
	$(BUILD_DIR_LINUX_ALLEGRO)/source/Emulation $(BUILD_DIR_LINUX_ALLEGRO)/source/SMB \
	$(BUILD_DIR_WIN_ALLEGRO)/source/Emulation $(BUILD_DIR_WIN_ALLEGRO)/source/SMB \
	$(BUILD_DIR_LINUX_SDL_DEBUG)/source/Emulation $(BUILD_DIR_LINUX_SDL_DEBUG)/source/SMB \
	$(BUILD_DIR_WIN_SDL_DEBUG)/source/Emulation $(BUILD_DIR_WIN_SDL_DEBUG)/source/SMB \
	$(BUILD_DIR_LINUX_ALLEGRO_DEBUG)/source/Emulation $(BUILD_DIR_LINUX_ALLEGRO_DEBUG)/source/SMB \
	$(BUILD_DIR_WIN_ALLEGRO_DEBUG)/source/Emulation $(BUILD_DIR_WIN_ALLEGRO_DEBUG)/source/SMB)

# Default target - build SDL versions for Linux
.PHONY: all
all: linux-sdl

# Main build targets
.PHONY: linux
linux: linux-sdl linux-allegro

.PHONY: windows  
windows: windows-sdl windows-allegro

# SDL-specific targets
.PHONY: linux-sdl
linux-sdl: $(BUILD_DIR_LINUX_SDL)/$(TARGET_LINUX_SDL)

.PHONY: windows-sdl
windows-sdl: $(BUILD_DIR_WIN_SDL)/$(TARGET_WIN_SDL)

# Allegro-specific targets
.PHONY: linux-allegro
linux-allegro: $(BUILD_DIR_LINUX_ALLEGRO)/$(TARGET_LINUX_ALLEGRO)

.PHONY: windows-allegro
windows-allegro: $(BUILD_DIR_WIN_ALLEGRO)/$(TARGET_WIN_ALLEGRO)

# Debug targets
.PHONY: debug
debug: linux-sdl-debug windows-sdl-debug linux-allegro-debug windows-allegro-debug

.PHONY: linux-sdl-debug
linux-sdl-debug: $(BUILD_DIR_LINUX_SDL_DEBUG)/$(TARGET_LINUX_SDL_DEBUG)

.PHONY: windows-sdl-debug
windows-sdl-debug: $(BUILD_DIR_WIN_SDL_DEBUG)/$(TARGET_WIN_SDL_DEBUG)

.PHONY: linux-allegro-debug
linux-allegro-debug: $(BUILD_DIR_LINUX_ALLEGRO_DEBUG)/$(TARGET_LINUX_ALLEGRO_DEBUG)

.PHONY: windows-allegro-debug
windows-allegro-debug: $(BUILD_DIR_WIN_ALLEGRO_DEBUG)/$(TARGET_WIN_ALLEGRO_DEBUG)

#
# Linux SDL build targets
#
$(BUILD_DIR_LINUX_SDL)/$(TARGET_LINUX_SDL): $(addprefix $(BUILD_DIR_LINUX_SDL)/,$(OBJS_LINUX_SDL))
	@echo "Linking Linux SDL executable..."
	$(CXX_LINUX) $^ -o $@ $(LDFLAGS_LINUX_SDL)
	@echo "Linux SDL build complete: $@"

$(BUILD_DIR_LINUX_SDL)/%.sdl.o: %.cpp
	@echo "Compiling $< for Linux SDL..."
	$(CXX_LINUX) $(CXXFLAGS_LINUX_SDL) -c $< -o $@

#
# Linux SDL debug build targets  
#
$(BUILD_DIR_LINUX_SDL_DEBUG)/$(TARGET_LINUX_SDL_DEBUG): $(addprefix $(BUILD_DIR_LINUX_SDL_DEBUG)/,$(OBJS_LINUX_SDL_DEBUG))
	@echo "Linking Linux SDL debug executable..."
	$(CXX_LINUX) $^ -o $@ $(LDFLAGS_LINUX_SDL)
	@echo "Linux SDL debug build complete: $@"

$(BUILD_DIR_LINUX_SDL_DEBUG)/%.sdl.debug.o: %.cpp
	@echo "Compiling $< for Linux SDL debug..."
	$(CXX_LINUX) $(CXXFLAGS_LINUX_SDL_DEBUG) -c $< -o $@

#
# Windows SDL build targets
#
$(BUILD_DIR_WIN_SDL)/$(TARGET_WIN_SDL): $(addprefix $(BUILD_DIR_WIN_SDL)/,$(OBJS_WIN_SDL))
	@echo "Linking Windows SDL executable..."
	$(CXX_WIN) $^ -o $@ $(LDFLAGS_WIN_SDL)
	@echo "Windows SDL build complete: $@"

$(BUILD_DIR_WIN_SDL)/%.win.sdl.o: %.cpp
	@echo "Compiling $< for Windows SDL..."
	$(CXX_WIN) $(CXXFLAGS_WIN_SDL) -c $< -o $@

#
# Windows SDL debug build targets
#
$(BUILD_DIR_WIN_SDL_DEBUG)/$(TARGET_WIN_SDL_DEBUG): $(addprefix $(BUILD_DIR_WIN_SDL_DEBUG)/,$(OBJS_WIN_SDL_DEBUG))
	@echo "Linking Windows SDL debug executable..."
	$(CXX_WIN) $(CXXFLAGS_WIN_SDL_DEBUG) $^ -o $@ $(LDFLAGS_WIN_SDL)
	@echo "Windows SDL debug build complete: $@"

$(BUILD_DIR_WIN_SDL_DEBUG)/%.win.sdl.debug.o: %.cpp
	@echo "Compiling $< for Windows SDL debug..."
	$(CXX_WIN) $(CXXFLAGS_WIN_SDL_DEBUG) -c $< -o $@

#
# Linux Allegro build targets
#
$(BUILD_DIR_LINUX_ALLEGRO)/$(TARGET_LINUX_ALLEGRO): $(addprefix $(BUILD_DIR_LINUX_ALLEGRO)/,$(OBJS_LINUX_ALLEGRO))
	@echo "Linking Linux Allegro executable..."
	$(CXX_LINUX) $^ -o $@ $(LDFLAGS_LINUX_ALLEGRO)
	@echo "Linux Allegro build complete: $@"

$(BUILD_DIR_LINUX_ALLEGRO)/%.allegro.o: %.cpp
	@echo "Compiling $< for Linux Allegro..."
	$(CXX_LINUX) $(CXXFLAGS_LINUX_ALLEGRO) -c $< -o $@

#
# Linux Allegro debug build targets
#
$(BUILD_DIR_LINUX_ALLEGRO_DEBUG)/$(TARGET_LINUX_ALLEGRO_DEBUG): $(addprefix $(BUILD_DIR_LINUX_ALLEGRO_DEBUG)/,$(OBJS_LINUX_ALLEGRO_DEBUG))
	@echo "Linking Linux Allegro debug executable..."
	$(CXX_LINUX) $^ -o $@ $(LDFLAGS_LINUX_ALLEGRO)
	@echo "Linux Allegro debug build complete: $@"

$(BUILD_DIR_LINUX_ALLEGRO_DEBUG)/%.allegro.debug.o: %.cpp
	@echo "Compiling $< for Linux Allegro debug..."
	$(CXX_LINUX) $(CXXFLAGS_LINUX_ALLEGRO_DEBUG) -c $< -o $@

#
# Windows Allegro build targets
#
$(BUILD_DIR_WIN_ALLEGRO)/$(TARGET_WIN_ALLEGRO): $(addprefix $(BUILD_DIR_WIN_ALLEGRO)/,$(OBJS_WIN_ALLEGRO))
	@echo "Linking Windows Allegro executable..."
	$(CXX_WIN) $^ -o $@ $(LDFLAGS_WIN_ALLEGRO)
	@echo "Windows Allegro build complete: $@"

$(BUILD_DIR_WIN_ALLEGRO)/%.win.allegro.o: %.cpp
	@echo "Compiling $< for Windows Allegro..."
	$(CXX_WIN) $(CXXFLAGS_WIN_ALLEGRO) -c $< -o $@

#
# Windows Allegro debug build targets
#
$(BUILD_DIR_WIN_ALLEGRO_DEBUG)/$(TARGET_WIN_ALLEGRO_DEBUG): $(addprefix $(BUILD_DIR_WIN_ALLEGRO_DEBUG)/,$(OBJS_WIN_ALLEGRO_DEBUG))
	@echo "Linking Windows Allegro debug executable..."
	$(CXX_WIN) $(CXXFLAGS_WIN_ALLEGRO_DEBUG) $^ -o $@ $(LDFLAGS_WIN_ALLEGRO)
	@echo "Windows Allegro debug build complete: $@"

$(BUILD_DIR_WIN_ALLEGRO_DEBUG)/%.win.allegro.debug.o: %.cpp
	@echo "Compiling $< for Windows Allegro debug..."
	$(CXX_WIN) $(CXXFLAGS_WIN_ALLEGRO_DEBUG) -c $< -o $@

# Check dependencies target
.PHONY: check-deps
check-deps:
	@echo "Checking dependencies..."
	@echo "Linux SDL2:"; pkg-config --exists sdl2 && echo "  ✓ SDL2 found" || echo "  ✗ SDL2 not found - install libsdl2-dev"
	@echo "Linux Allegro:"; test -f /usr/lib/liballeg.so && echo "  ✓ Allegro 4 found" || echo "  ✗ Allegro 4 not found - install liballegro4-dev"
	@echo "Windows SDL2:"; test -f /usr/x86_64-w64-mingw32/include/SDL2/SDL.h && echo "  ✓ MinGW SDL2 found" || echo "  ✗ MinGW SDL2 not found - install mingw64-SDL2-devel"
	@echo "Windows Allegro:"; test -f /usr/x86_64-w64-mingw32/lib/liballeg.a && echo "  ✓ MinGW Allegro 4 found" || echo "  ✗ MinGW Allegro 4 not found - install mingw64-allegro4"

# Install dependencies (Ubuntu/Debian)
.PHONY: install-deps
install-deps:
	@echo "Installing dependencies for Ubuntu/Debian..."
	sudo apt-get update
	sudo apt-get install -y libsdl2-dev liballegro4-dev pkg-config
	sudo apt-get install -y mingw-w64 || echo "MinGW may need manual installation"
	@echo "Note: MinGW SDL2 and Allegro libraries may need manual installation"

# Clean target
.PHONY: clean
clean:
	@echo "Cleaning build files..."
	find $(BUILD_DIR) -type f -name "*.o" -delete 2>/dev/null || true
	find $(BUILD_DIR) -type f -name "*.exe" -delete 2>/dev/null || true
	rm -f $(BUILD_DIR_LINUX_SDL)/$(TARGET_LINUX_SDL) 2>/dev/null || true
	rm -f $(BUILD_DIR_LINUX_SDL_DEBUG)/$(TARGET_LINUX_SDL_DEBUG) 2>/dev/null || true
	rm -f $(BUILD_DIR_WIN_SDL)/$(TARGET_WIN_SDL) 2>/dev/null || true
	rm -f $(BUILD_DIR_WIN_SDL_DEBUG)/$(TARGET_WIN_SDL_DEBUG) 2>/dev/null || true
	rm -f $(BUILD_DIR_LINUX_ALLEGRO)/$(TARGET_LINUX_ALLEGRO) 2>/dev/null || true
	rm -f $(BUILD_DIR_LINUX_ALLEGRO_DEBUG)/$(TARGET_LINUX_ALLEGRO_DEBUG) 2>/dev/null || true
	rm -f $(BUILD_DIR_WIN_ALLEGRO)/$(TARGET_WIN_ALLEGRO) 2>/dev/null || true
	rm -f $(BUILD_DIR_WIN_ALLEGRO_DEBUG)/$(TARGET_WIN_ALLEGRO_DEBUG) 2>/dev/null || true

# Test builds (quick compilation test)
.PHONY: test-builds
test-builds:
	@echo "Testing compilation for all platforms..."
	@echo "Testing Linux SDL build..."
	@$(MAKE) linux-sdl >/dev/null 2>&1 && echo "  ✓ Linux SDL builds successfully" || echo "  ✗ Linux SDL build failed"
	@echo "Testing Linux Allegro build..."
	@$(MAKE) linux-allegro >/dev/null 2>&1 && echo "  ✓ Linux Allegro builds successfully" || echo "  ✗ Linux Allegro build failed"
	@echo "Testing Windows SDL build..."
	@$(MAKE) windows-sdl >/dev/null 2>&1 && echo "  ✓ Windows SDL builds successfully" || echo "  ✗ Windows SDL build failed"
	@echo "Testing Windows Allegro build..."
	@$(MAKE) windows-allegro >/dev/null 2>&1 && echo "  ✓ Windows Allegro builds successfully" || echo "  ✗ Windows Allegro build failed"

# Debug what files the makefile thinks it should build
.PHONY: debug-files
debug-files:
	@echo "SDL Source Files:"
	@for file in $(SDL_SOURCE_FILES); do echo "  $file"; done
	@echo ""
	@echo "SDL Object Files (Linux):"
	@for file in $(OBJS_LINUX_SDL); do echo "  $file"; done
	@echo ""
	@echo "Allegro Source Files:"
	@for file in $(ALLEGRO_SOURCE_FILES); do echo "  $file"; done
	@echo ""
	@echo "Allegro Object Files (Linux):"
	@for file in $(OBJS_LINUX_ALLEGRO); do echo "  $file"; done

# Help target
.PHONY: help
help:
	@echo "Available targets:"
	@echo "  make               - Build warpnes for Linux with SDL (default)"
	@echo "  make linux         - Build both SDL and Allegro versions for Linux"
	@echo "  make windows       - Build both SDL and Allegro versions for Windows"
	@echo ""
	@echo "  make linux-sdl     - Build warpnes for Linux with SDL"
	@echo "  make linux-allegro - Build warpnes for Linux with Allegro"
	@echo "  make windows-sdl   - Build warpnes for Windows with SDL (requires MinGW)"
	@echo "  make windows-allegro - Build warpnes for Windows with Allegro (requires MinGW)"
	@echo ""
	@echo "  make debug         - Build debug versions for all platforms"
	@echo "  make linux-sdl-debug     - Build Linux SDL with debug symbols"
	@echo "  make linux-allegro-debug - Build Linux Allegro with debug symbols"
	@echo "  make windows-sdl-debug   - Build Windows SDL with debug symbols"
	@echo "  make windows-allegro-debug - Build Windows Allegro with debug symbols"
	@echo ""
	@echo "Utility targets:"
	@echo "  make check-deps    - Check if required dependencies are installed"
	@echo "  make install-deps  - Install dependencies on Ubuntu/Debian"
	@echo "  make test-builds   - Test compilation for all platforms"
	@echo "  make clean         - Remove all build files"
	@echo "  make help          - Show this help message"
	@echo ""
	@echo "Build outputs:"
	@echo "  Linux SDL:     $(BUILD_DIR_LINUX_SDL)/$(TARGET_LINUX_SDL)"
	@echo "  Linux Allegro: $(BUILD_DIR_LINUX_ALLEGRO)/$(TARGET_LINUX_ALLEGRO)"
	@echo "  Windows SDL:   $(BUILD_DIR_WIN_SDL)/$(TARGET_WIN_SDL)"
	@echo "  Windows Allegro: $(BUILD_DIR_WIN_ALLEGRO)/$(TARGET_WIN_ALLEGRO)"
	@echo ""
	@echo "Dependencies required:"
	@echo "  Linux SDL:       libsdl2-dev"
	@echo "  Linux Allegro:   liballegro4-dev"
	@echo "  Windows SDL:     MinGW SDL2 development libraries"
	@echo "  Windows Allegro: MinGW Allegro 4 development libraries"
	@echo ""
	@echo "Platform differences:"
	@echo "  - SDL versions: Modern cross-platform support, hardware acceleration"
	@echo "  - Allegro versions: Retro compatibility, authentic DOS-style experience"
	@echo "  - All versions share the same game engine code"
	@echo ""
	@echo "Note: DOS builds are handled by a separate shell script"
