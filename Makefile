# Makefile for smbc with Windows and Linux Support
# Based on Tetrimone Makefile structure
# Last modification: $(date +%m/%d/%Y)

# Compiler settings
CXX_LINUX = g++
CXX_WIN = x86_64-w64-mingw32-g++
CXXFLAGS_COMMON = -s -std=c++11 -Wno-c++11-narrowing -fpermissive

# Debug flags
DEBUG_FLAGS = -g -DDEBUG

# SDL flags for Linux
SDL_CFLAGS_LINUX := $(shell pkg-config --cflags sdl2)
SDL_LIBS_LINUX := $(shell pkg-config --libs sdl2)

# SDL flags for Windows
SDL_CFLAGS_WIN := $(shell mingw64-pkg-config --cflags sdl2 2>/dev/null || echo "-I/usr/x86_64-w64-mingw32/include/SDL2")
SDL_LIBS_WIN := $(shell mingw64-pkg-config --libs sdl2 2>/dev/null || echo "-lSDL2 -lSDL2main")

# Boost flags (header-only, no linking needed)
BOOST_CFLAGS_LINUX := $(shell pkg-config --cflags boost 2>/dev/null || echo "")
BOOST_CFLAGS_WIN := $(shell mingw64-pkg-config --cflags boost 2>/dev/null || echo "")

# Platform-specific settings
CXXFLAGS_LINUX = $(CXXFLAGS_COMMON) $(SDL_CFLAGS_LINUX) $(BOOST_CFLAGS_LINUX) -DLINUX
CXXFLAGS_WIN = $(CXXFLAGS_COMMON) $(SDL_CFLAGS_WIN) $(BOOST_CFLAGS_WIN) -DWIN32

# Debug-specific flags
CXXFLAGS_LINUX_DEBUG = $(CXXFLAGS_LINUX) $(DEBUG_FLAGS)
CXXFLAGS_WIN_DEBUG = $(CXXFLAGS_WIN) $(DEBUG_FLAGS)

# Linker flags
LDFLAGS_LINUX = $(SDL_LIBS_LINUX)
LDFLAGS_WIN = $(SDL_LIBS_WIN) -lwinmm -static-libgcc -static-libstdc++

# Source files (from CMakeLists.txt)
SOURCE_FILES = \
    source/Configuration.cpp \
    source/Main.cpp \
    source/Emulation/APU.cpp \
    source/Emulation/Controller.cpp \
    source/Emulation/MemoryAccess.cpp \
    source/Emulation/PPU.cpp \
    source/SMB/SMB.cpp \
    source/SMB/SMBData.cpp \
    source/SMB/SMBEngine.cpp \
    source/Util/Video.cpp \
    source/Util/VideoFilters.cpp \
    source/SMBRom.cpp

# Object files
OBJS_LINUX = $(SOURCE_FILES:.cpp=.o)
OBJS_WIN = $(SOURCE_FILES:.cpp=.win.o)
OBJS_LINUX_DEBUG = $(SOURCE_FILES:.cpp=.debug.o)
OBJS_WIN_DEBUG = $(SOURCE_FILES:.cpp=.win.debug.o)

# Target executables
TARGET_LINUX = smbc
TARGET_WIN = smbc.exe
TARGET_LINUX_DEBUG = smbc_debug
TARGET_WIN_DEBUG = smbc_debug.exe

# Build directories
BUILD_DIR = build
BUILD_DIR_LINUX = $(BUILD_DIR)/linux
BUILD_DIR_WIN = $(BUILD_DIR)/windows
BUILD_DIR_LINUX_DEBUG = $(BUILD_DIR)/linux_debug
BUILD_DIR_WIN_DEBUG = $(BUILD_DIR)/windows_debug

# Windows DLL settings
DLL_SOURCE_DIR = /usr/x86_64-w64-mingw32/sys-root/mingw/bin

# Create necessary directories
$(shell mkdir -p $(BUILD_DIR_LINUX)/source/Emulation $(BUILD_DIR_LINUX)/source/SMB $(BUILD_DIR_LINUX)/source/Util \
	$(BUILD_DIR_WIN)/source/Emulation $(BUILD_DIR_WIN)/source/SMB $(BUILD_DIR_WIN)/source/Util \
	$(BUILD_DIR_LINUX_DEBUG)/source/Emulation $(BUILD_DIR_LINUX_DEBUG)/source/SMB $(BUILD_DIR_LINUX_DEBUG)/source/Util \
	$(BUILD_DIR_WIN_DEBUG)/source/Emulation $(BUILD_DIR_WIN_DEBUG)/source/SMB $(BUILD_DIR_WIN_DEBUG)/source/Util)

# Default target - build for Linux
.PHONY: all
all: linux

# OS-specific builds
.PHONY: windows
windows: smbc-windows

.PHONY: linux
linux: smbc-linux

# Debug targets
.PHONY: debug
debug: smbc-linux-debug smbc-windows-debug

#
# Linux build targets
#
.PHONY: smbc-linux
smbc-linux: $(BUILD_DIR_LINUX)/$(TARGET_LINUX)

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
.PHONY: smbc-linux-debug
smbc-linux-debug: $(BUILD_DIR_LINUX_DEBUG)/$(TARGET_LINUX_DEBUG)

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
.PHONY: smbc-windows
smbc-windows: $(BUILD_DIR_WIN)/$(TARGET_WIN) smbc-collect-dlls

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
.PHONY: smbc-windows-debug
smbc-windows-debug: $(BUILD_DIR_WIN_DEBUG)/$(TARGET_WIN_DEBUG) smbc-collect-debug-dlls

$(BUILD_DIR_WIN_DEBUG)/$(TARGET_WIN_DEBUG): $(addprefix $(BUILD_DIR_WIN_DEBUG)/,$(OBJS_WIN_DEBUG))
	@echo "Linking Windows debug executable..."
	$(CXX_WIN) $(CXXFLAGS_WIN_DEBUG) $^ -o $@ $(LDFLAGS_WIN)
	@echo "Windows debug build complete: $@"

# Generic compilation rules for Windows debug
$(BUILD_DIR_WIN_DEBUG)/%.win.debug.o: %.cpp
	@echo "Compiling $< for Windows debug..."
	$(CXX_WIN) $(CXXFLAGS_WIN_DEBUG) -c $< -o $@

#
# DLL collection for Windows builds
#
.PHONY: smbc-collect-dlls
smbc-collect-dlls: $(BUILD_DIR_WIN)/$(TARGET_WIN)
	@echo "Collecting DLLs for Windows build..."
	@if [ -f "build/windows/collect_dlls.sh" ]; then \
		build/windows/collect_dlls.sh $(BUILD_DIR_WIN)/$(TARGET_WIN) $(DLL_SOURCE_DIR) $(BUILD_DIR_WIN); \
	else \
		echo "Creating simple DLL collection..."; \
		if [ -d "$(DLL_SOURCE_DIR)" ]; then \
			cp $(DLL_SOURCE_DIR)/SDL2.dll $(BUILD_DIR_WIN)/ 2>/dev/null || echo "SDL2.dll not found"; \
			cp $(DLL_SOURCE_DIR)/libgcc_s_seh-1.dll $(BUILD_DIR_WIN)/ 2>/dev/null || echo "libgcc_s_seh-1.dll not found"; \
			cp $(DLL_SOURCE_DIR)/libstdc++-6.dll $(BUILD_DIR_WIN)/ 2>/dev/null || echo "libstdc++-6.dll not found"; \
			cp $(DLL_SOURCE_DIR)/libwinpthread-1.dll $(BUILD_DIR_WIN)/ 2>/dev/null || echo "libwinpthread-1.dll not found"; \
		else \
			echo "DLL source directory not found: $(DLL_SOURCE_DIR)"; \
		fi; \
	fi

.PHONY: smbc-collect-debug-dlls
smbc-collect-debug-dlls: $(BUILD_DIR_WIN_DEBUG)/$(TARGET_WIN_DEBUG)
	@echo "Collecting Debug DLLs for Windows build..."
	@if [ -f "build/windows/collect_dlls.sh" ]; then \
		build/windows/collect_dlls.sh $(BUILD_DIR_WIN_DEBUG)/$(TARGET_WIN_DEBUG) $(DLL_SOURCE_DIR) $(BUILD_DIR_WIN_DEBUG); \
	else \
		echo "Creating simple DLL collection..."; \
		if [ -d "$(DLL_SOURCE_DIR)" ]; then \
			cp $(DLL_SOURCE_DIR)/SDL2.dll $(BUILD_DIR_WIN_DEBUG)/ 2>/dev/null || echo "SDL2.dll not found"; \
			cp $(DLL_SOURCE_DIR)/libgcc_s_seh-1.dll $(BUILD_DIR_WIN_DEBUG)/ 2>/dev/null || echo "libgcc_s_seh-1.dll not found"; \
			cp $(DLL_SOURCE_DIR)/libstdc++-6.dll $(BUILD_DIR_WIN_DEBUG)/ 2>/dev/null || echo "libstdc++-6.dll not found"; \
			cp $(DLL_SOURCE_DIR)/libwinpthread-1.dll $(BUILD_DIR_WIN_DEBUG)/ 2>/dev/null || echo "libwinpthread-1.dll not found"; \
		else \
			echo "DLL source directory not found: $(DLL_SOURCE_DIR)"; \
		fi; \
	fi

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

# Help target
.PHONY: help
help:
	@echo "Available targets:"
	@echo "  make               - Build smbc for Linux (default)"
	@echo "  make linux         - Build smbc for Linux"
	@echo "  make windows       - Build smbc for Windows (requires MinGW)"
	@echo ""
	@echo "  make debug         - Build smbc with debug symbols for both platforms"
	@echo "  make smbc-linux-debug    - Build smbc for Linux with debug symbols"
	@echo "  make smbc-windows-debug  - Build smbc for Windows with debug symbols"
	@echo ""
	@echo "  make create-dll-script   - Create DLL collection script"
	@echo "  make smbc-collect-dlls   - Collect DLLs for Windows build"
	@echo ""
	@echo "  make clean         - Remove all build files"
	@echo "  make help          - Show this help message"
	@echo ""
	@echo "Build outputs:"
	@echo "  Linux:   $(BUILD_DIR_LINUX)/$(TARGET_LINUX)"
	@echo "  Windows: $(BUILD_DIR_WIN)/$(TARGET_WIN)"
