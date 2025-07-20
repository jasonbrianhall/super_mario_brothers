# Makefile for warpnes with Windows, Linux, and DOS Support
# Based on Tetrimone Makefile structure
# Last modification: $(shell date +%m/%d/%Y)

# Compiler settings
CXX_LINUX = g++
CXX_WIN = x86_64-w64-mingw32-g++
CXX_DOS = gxx
CXXFLAGS_COMMON = -s -fpermissive

# Debug flags
DEBUG_FLAGS = -g -DDEBUG

# Allegro flags for all platforms
ALLEGRO_CFLAGS_LINUX = -I/usr/include
ALLEGRO_LIBS_LINUX = -lalleg

ALLEGRO_CFLAGS_WIN = -I/usr/x86_64-w64-mingw32/include
ALLEGRO_LIBS_WIN = -lalleg -lwinmm -static-libgcc -static-libstdc++

ALLEGRO_CFLAGS_DOS = -I/djgpp/include
ALLEGRO_LIBS_DOS = -lalleg -lm

# Platform-specific settings
CXXFLAGS_LINUX = $(CXXFLAGS_COMMON) $(ALLEGRO_CFLAGS_LINUX) -DLINUX
CXXFLAGS_WIN = $(CXXFLAGS_COMMON) $(ALLEGRO_CFLAGS_WIN) -DWIN32
CXXFLAGS_DOS = -O2 -Wall -fomit-frame-pointer -ffast-math $(ALLEGRO_CFLAGS_DOS) -DDOS
# Debug-specific flags
CXXFLAGS_LINUX_DEBUG = $(CXXFLAGS_LINUX) $(DEBUG_FLAGS)
CXXFLAGS_WIN_DEBUG = $(CXXFLAGS_WIN) $(DEBUG_FLAGS)
CXXFLAGS_DOS_DEBUG = $(CXXFLAGS_DOS) $(DEBUG_FLAGS)

# Linker flags
LDFLAGS_LINUX = $(ALLEGRO_LIBS_LINUX)
LDFLAGS_WIN = $(ALLEGRO_LIBS_WIN)
LDFLAGS_DOS = $(ALLEGRO_LIBS_DOS)

# All platforms use the same source files (with SDL wrapper)
SOURCE_FILES = \
    source/Configuration.cpp \
    source/dos_main.cpp \
    source/Emulation/APU.cpp \
    source/Emulation/Controller.cpp \
    source/Emulation/AllegroMidi.cpp \
    source/Emulation/PPU.cpp \
    source/Zapper.cpp \
    source/SMB/SMBEmulator.cpp

# Object files
OBJS_LINUX = $(SOURCE_FILES:.cpp=.o)
OBJS_WIN = $(SOURCE_FILES:.cpp=.win.o)
OBJS_DOS = $(SOURCE_FILES:.cpp=.dos.o)
OBJS_LINUX_DEBUG = $(SOURCE_FILES:.cpp=.debug.o)
OBJS_WIN_DEBUG = $(SOURCE_FILES:.cpp=.win.debug.o)
OBJS_DOS_DEBUG = $(SOURCE_FILES:.cpp=.dos.debug.o)

# Target executables
TARGET_LINUX = warpnes
TARGET_WIN = warpnes.exe
TARGET_DOS = warpnes.exe
TARGET_LINUX_DEBUG = warpnes_debug
TARGET_WIN_DEBUG = warpnes_debug.exe
TARGET_DOS_DEBUG = warpnesd.exe

# Build directories
BUILD_DIR = build
BUILD_DIR_LINUX = $(BUILD_DIR)/linux
BUILD_DIR_WIN = $(BUILD_DIR)/windows
BUILD_DIR_DOS = $(BUILD_DIR)/dos
BUILD_DIR_LINUX_DEBUG = $(BUILD_DIR)/linux_debug
BUILD_DIR_WIN_DEBUG = $(BUILD_DIR)/windows_debug
BUILD_DIR_DOS_DEBUG = $(BUILD_DIR)/dos_debug

# Windows DLL settings (for Allegro DLLs if needed)
DLL_SOURCE_DIR = /usr/x86_64-w64-mingw32/sys-root/mingw/bin

# Create necessary directories
$(shell mkdir -p $(BUILD_DIR_LINUX)/source/Emulation $(BUILD_DIR_LINUX)/source/SMB $(BUILD_DIR_LINUX)/source/Util \
	$(BUILD_DIR_WIN)/source/Emulation $(BUILD_DIR_WIN)/source/SMB $(BUILD_DIR_WIN)/source/Util \
	$(BUILD_DIR_DOS)/source/Emulation $(BUILD_DIR_DOS)/source/SMB $(BUILD_DIR_DOS)/source/Util \
	$(BUILD_DIR_LINUX_DEBUG)/source/Emulation $(BUILD_DIR_LINUX_DEBUG)/source/SMB $(BUILD_DIR_LINUX_DEBUG)/source/Util \
	$(BUILD_DIR_WIN_DEBUG)/source/Emulation $(BUILD_DIR_WIN_DEBUG)/source/SMB $(BUILD_DIR_WIN_DEBUG)/source/Util \
	$(BUILD_DIR_DOS_DEBUG)/source/Emulation $(BUILD_DIR_DOS_DEBUG)/source/SMB $(BUILD_DIR_DOS_DEBUG)/source/Util)

# Default target - build for Linux
.PHONY: all
all: linux

# OS-specific builds
.PHONY: windows
windows: warpnes-windows

.PHONY: linux
linux: warpnes-linux

.PHONY: dos
dos: warpnes-dos

# Debug targets
.PHONY: debug
debug: warpnes-linux-debug warpnes-windows-debug warpnes-dos-debug

#
# Linux build targets
#
.PHONY: warpnes-linux
warpnes-linux: $(BUILD_DIR_LINUX)/$(TARGET_LINUX)

$(BUILD_DIR_LINUX)/$(TARGET_LINUX): $(addprefix $(BUILD_DIR_LINUX)/,$(OBJS_LINUX))
	@echo "Linking Linux executable..."
	$(CXX_LINUX) $^ -o $@ $(LDFLAGS_LINUX)
	@echo "Linux build complete: $@"

# Generic compilation rules for Linux
$(BUILD_DIR_LINUX)/%.o: %.cpp
	@echo "Compiling $< for Linux..."
	$(CXX_LINUX) $(CXXFLAGS_LINUX) -c $< -o $@

#
# Linux debug targets
#
.PHONY: warpnes-linux-debug
warpnes-linux-debug: $(BUILD_DIR_LINUX_DEBUG)/$(TARGET_LINUX_DEBUG)

$(BUILD_DIR_LINUX_DEBUG)/$(TARGET_LINUX_DEBUG): $(addprefix $(BUILD_DIR_LINUX_DEBUG)/,$(OBJS_LINUX_DEBUG))
	@echo "Linking Linux debug executable..."
	$(CXX_LINUX) $^ -o $@ $(LDFLAGS_LINUX)
	@echo "Linux debug build complete: $@"

# Generic compilation rules for Linux debug
$(BUILD_DIR_LINUX_DEBUG)/%.debug.o: %.cpp
	@echo "Compiling $< for Linux debug..."
	$(CXX_LINUX) $(CXXFLAGS_LINUX_DEBUG) -c $< -o $@

#
# Windows build targets
#
.PHONY: warpnes-windows
warpnes-windows: $(BUILD_DIR_WIN)/$(TARGET_WIN) warpnes-collect-dlls

$(BUILD_DIR_WIN)/$(TARGET_WIN): $(addprefix $(BUILD_DIR_WIN)/,$(OBJS_WIN))
	@echo "Linking Windows executable..."
	$(CXX_WIN) $^ -o $@ $(LDFLAGS_WIN)
	@echo "Windows build complete: $@"

# Generic compilation rules for Windows
$(BUILD_DIR_WIN)/%.win.o: %.cpp
	@echo "Compiling $< for Windows..."
	$(CXX_WIN) $(CXXFLAGS_WIN) -c $< -o $@

#
# Windows debug targets
#
.PHONY: warpnes-windows-debug
warpnes-windows-debug: $(BUILD_DIR_WIN_DEBUG)/$(TARGET_WIN_DEBUG) warpnes-collect-debug-dlls

$(BUILD_DIR_WIN_DEBUG)/$(TARGET_WIN_DEBUG): $(addprefix $(BUILD_DIR_WIN_DEBUG)/,$(OBJS_WIN_DEBUG))
	@echo "Linking Windows debug executable..."
	$(CXX_WIN) $(CXXFLAGS_WIN_DEBUG) $^ -o $@ $(LDFLAGS_WIN)
	@echo "Windows debug build complete: $@"

# Generic compilation rules for Windows debug
$(BUILD_DIR_WIN_DEBUG)/%.win.debug.o: %.cpp
	@echo "Compiling $< for Windows debug..."
	$(CXX_WIN) $(CXXFLAGS_WIN_DEBUG) -c $< -o $@

#
# DOS build targets
#
.PHONY: warpnes-dos
warpnes-dos: $(BUILD_DIR_DOS)/$(TARGET_DOS)

$(BUILD_DIR_DOS)/$(TARGET_DOS): $(addprefix $(BUILD_DIR_DOS)/,$(OBJS_DOS))
	@echo "Linking DOS executable..."
	$(CXX_DOS) $^ -o $@ $(LDFLAGS_DOS)
	@echo "DOS build complete: $@"

# Generic compilation rules for DOS
$(BUILD_DIR_DOS)/%.dos.o: %.cpp
	@echo "Compiling $< for DOS..."
	$(CXX_DOS) $(CXXFLAGS_DOS) -c $< -o $@

#
# DOS debug targets
#
.PHONY: warpnes-dos-debug
warpnes-dos-debug: $(BUILD_DIR_DOS_DEBUG)/$(TARGET_DOS_DEBUG)

$(BUILD_DIR_DOS_DEBUG)/$(TARGET_DOS_DEBUG): $(addprefix $(BUILD_DIR_DOS_DEBUG)/,$(OBJS_DOS_DEBUG))
	@echo "Linking DOS debug executable..."
	$(CXX_DOS) $(CXXFLAGS_DOS_DEBUG) $^ -o $@ $(LDFLAGS_DOS)
	@echo "DOS debug build complete: $@"

# Generic compilation rules for DOS debug
$(BUILD_DIR_DOS_DEBUG)/%.dos.debug.o: %.cpp
	@echo "Compiling $< for DOS debug..."
	$(CXX_DOS) $(CXXFLAGS_DOS_DEBUG) -c $< -o $@

#
# DLL collection for Windows builds
#
.PHONY: warpnes-collect-dlls
warpnes-collect-dlls: $(BUILD_DIR_WIN)/$(TARGET_WIN)
	@echo "Collecting DLLs for Windows build..."
	@build/windows/collect_dlls.sh $(BUILD_DIR_WIN)/$(TARGET_WIN) $(DLL_SOURCE_DIR) $(BUILD_DIR_WIN)

.PHONY: warpnes-collect-debug-dlls
warpnes-collect-debug-dlls: $(BUILD_DIR_WIN_DEBUG)/$(TARGET_WIN_DEBUG)
	@echo "Collecting Debug DLLs for Windows build..."
	@build/windows/collect_dlls.sh $(BUILD_DIR_WIN_DEBUG)/$(TARGET_WIN_DEBUG) $(DLL_SOURCE_DIR) $(BUILD_DIR_WIN_DEBUG)

# Create DLL collection script if it doesn't exist
.PHONY: create-dll-script
create-dll-script:
	@mkdir -p build/windows
	@if [ ! -f "build/windows/collect_dlls.sh" ]; then \
		echo "Creating DLL collection script..."; \
		cat > build/windows/collect_dlls.sh << 'EOF'; \
#!/bin/bash; \
# DLL collection script for Windows builds; \
EXECUTABLE=$$1; \
DLL_SOURCE=$$2; \
TARGET_DIR=$$3; \
; \
echo "Collecting DLLs for $$EXECUTABLE"; \
; \
# Copy common DLLs; \
if [ -d "$$DLL_SOURCE" ]; then; \
    cp "$$DLL_SOURCE"/SDL2.dll "$$TARGET_DIR"/ 2>/dev/null || echo "SDL2.dll not found"; \
    cp "$$DLL_SOURCE"/libgcc_s_seh-1.dll "$$TARGET_DIR"/ 2>/dev/null || echo "libgcc_s_seh-1.dll not found"; \
    cp "$$DLL_SOURCE"/libstdc++-6.dll "$$TARGET_DIR"/ 2>/dev/null || echo "libstdc++-6.dll not found"; \
    cp "$$DLL_SOURCE"/libwinpthread-1.dll "$$TARGET_DIR"/ 2>/dev/null || echo "libwinpthread-1.dll not found"; \
else; \
    echo "DLL source directory not found: $$DLL_SOURCE"; \
fi; \
EOF; \
		chmod +x build/windows/collect_dlls.sh; \
	fi

# Check dependencies target
.PHONY: check-deps
check-deps:
	@echo "Checking dependencies..."
	@echo "Linux Allegro:"; test -f /usr/lib/liballeg.so && echo "  ✓ Allegro 4 found" || echo "  ✗ Allegro 4 not found - install liballegro4-dev"
	@echo "Windows Allegro:"; test -f /usr/x86_64-w64-mingw32/lib/liballeg.a && echo "  ✓ MinGW Allegro 4 found" || echo "  ✗ MinGW Allegro 4 not found - install mingw64-allegro4"
	@echo "DOS Allegro:"; test -f /djgpp/lib/liballeg.a && echo "  ✓ Allegro 4 found" || echo "  ✗ Allegro 4 not found - install Allegro 4 for DJGPP"
	@echo "DOS DJGPP:"; which gxx >/dev/null 2>&1 && echo "  ✓ DJGPP found" || echo "  ✗ DJGPP not found - install DJGPP compiler"

# Clean target
.PHONY: clean
clean:
	@echo "Cleaning build files..."
	find $(BUILD_DIR) -type f -name "*.o" -delete 2>/dev/null || true
	find $(BUILD_DIR) -type f -name "*.dll" -delete 2>/dev/null || true
	find $(BUILD_DIR) -type f -name "*.exe" -delete 2>/dev/null || true
	rm -f $(BUILD_DIR_LINUX)/$(TARGET_LINUX) 2>/dev/null || true
	rm -f $(BUILD_DIR_LINUX_DEBUG)/$(TARGET_LINUX_DEBUG) 2>/dev/null || true
	rm -f $(BUILD_DIR_WIN)/$(TARGET_WIN) 2>/dev/null || true
	rm -f $(BUILD_DIR_WIN_DEBUG)/$(TARGET_WIN_DEBUG) 2>/dev/null || true
	rm -f $(BUILD_DIR_DOS)/$(TARGET_DOS) 2>/dev/null || true
	rm -f $(BUILD_DIR_DOS_DEBUG)/$(TARGET_DOS_DEBUG) 2>/dev/null || true

# Help target
.PHONY: help
help:
	@echo "Available targets:"
	@echo "  make               - Build warpnes for Linux (default)"
	@echo "  make linux         - Build warpnes for Linux"
	@echo "  make windows       - Build warpnes for Windows (requires MinGW)"
	@echo "  make dos           - Build warpnes for DOS (requires DJGPP)"
	@echo ""
	@echo "  make debug         - Build warpnes with debug symbols for all platforms"
	@echo "  make warpnes-linux-debug    - Build warpnes for Linux with debug symbols"
	@echo "  make warpnes-windows-debug  - Build warpnes for Windows with debug symbols"
	@echo "  make warpnes-dos-debug      - Build warpnes for DOS with debug symbols"
	@echo ""
	@echo "  make check-deps    - Check if required dependencies are installed"
	@echo "  make create-dll-script   - Create DLL collection script"
	@echo "  make warpnes-collect-dlls   - Collect DLLs for Windows build"
	@echo ""
	@echo "  make clean         - Remove all build files"
	@echo "  make help          - Show this help message"
	@echo ""
	@echo "Build outputs:"
	@echo "  Linux:   $(BUILD_DIR_LINUX)/$(TARGET_LINUX)"
	@echo "  Windows: $(BUILD_DIR_WIN)/$(TARGET_WIN)"
	@echo "  DOS:     $(BUILD_DIR_DOS)/$(TARGET_DOS)"
	@echo ""
	@echo "Dependencies required:"
	@echo "  Linux:   Allegro 4 (liballegro4-dev)"
	@echo "  Windows: MinGW Allegro 4 (mingw64-allegro4)"
	@echo "  DOS:     DJGPP compiler, Allegro 4 library"
