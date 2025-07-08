# Makefile for smbc with Windows and Linux Support
# Enhanced version with GTK and SDL build variants
# Last modification: $(date +%m/%d/%Y)

# Compiler settings
CXX_LINUX = g++
CXX_WIN = x86_64-w64-mingw32-g++
CXXFLAGS_COMMON = -s -fpermissive

# Debug flags
DEBUG_FLAGS = -g -DDEBUG

# SDL flags for Linux
SDL_CFLAGS_LINUX := $(shell pkg-config --cflags sdl2)
SDL_LIBS_LINUX := $(shell pkg-config --libs sdl2)

# GTK3 flags for Linux
GTK_CFLAGS_LINUX := $(shell pkg-config --cflags gtk+-3.0)
GTK_LIBS_LINUX := $(shell pkg-config --libs gtk+-3.0)

# SDL flags for Windows
SDL_CFLAGS_WIN := $(shell mingw64-pkg-config --cflags sdl2 2>/dev/null || echo "-I/usr/x86_64-w64-mingw32/include/SDL2")
SDL_LIBS_WIN := $(shell mingw64-pkg-config --libs sdl2 2>/dev/null || echo "-lSDL2 -lSDL2main")

# GTK3 flags for Windows
GTK_CFLAGS_WIN := $(shell mingw64-pkg-config --cflags gtk+-3.0 2>/dev/null || echo "-I/usr/x86_64-w64-mingw32/include/gtk-3.0")
GTK_LIBS_WIN := $(shell mingw64-pkg-config --libs gtk+-3.0 2>/dev/null || echo "-lgtk-3 -lgdk-3 -lgobject-2.0 -lglib-2.0")

# Boost flags (header-only, no linking needed)
BOOST_CFLAGS_LINUX := $(shell pkg-config --cflags boost 2>/dev/null || echo "")
BOOST_CFLAGS_WIN := $(shell mingw64-pkg-config --cflags boost 2>/dev/null || echo "")

# Platform-specific settings for GTK builds
CXXFLAGS_LINUX_GTK = $(CXXFLAGS_COMMON) $(SDL_CFLAGS_LINUX) $(GTK_CFLAGS_LINUX) $(BOOST_CFLAGS_LINUX) -DLINUX -DGTK_BUILD
CXXFLAGS_WIN_GTK = $(CXXFLAGS_COMMON) $(SDL_CFLAGS_WIN) $(GTK_CFLAGS_WIN) $(BOOST_CFLAGS_WIN) -DWIN32 -DGTK_BUILD

# Platform-specific settings for SDL builds
CXXFLAGS_LINUX_SDL = $(CXXFLAGS_COMMON) $(SDL_CFLAGS_LINUX) $(BOOST_CFLAGS_LINUX) -DLINUX -DSDL_BUILD
CXXFLAGS_WIN_SDL = $(CXXFLAGS_COMMON) $(SDL_CFLAGS_WIN) $(BOOST_CFLAGS_WIN) -DWIN32 -DSDL_BUILD

# Debug-specific flags
CXXFLAGS_LINUX_GTK_DEBUG = $(CXXFLAGS_LINUX_GTK) $(DEBUG_FLAGS)
CXXFLAGS_WIN_GTK_DEBUG = $(CXXFLAGS_WIN_GTK) $(DEBUG_FLAGS)
CXXFLAGS_LINUX_SDL_DEBUG = $(CXXFLAGS_LINUX_SDL) $(DEBUG_FLAGS)
CXXFLAGS_WIN_SDL_DEBUG = $(CXXFLAGS_WIN_SDL) $(DEBUG_FLAGS)

# Linker flags
LDFLAGS_LINUX_GTK = $(SDL_LIBS_LINUX) $(GTK_LIBS_LINUX)
LDFLAGS_WIN_GTK = $(SDL_LIBS_WIN) $(GTK_LIBS_WIN) -lwinmm -static-libgcc -static-libstdc++
LDFLAGS_LINUX_SDL = $(SDL_LIBS_LINUX)
LDFLAGS_WIN_SDL = $(SDL_LIBS_WIN) -lwinmm -static-libgcc -static-libstdc++

# Base source files (common to both versions)
BASE_SOURCE_FILES = \
    source/Configuration.cpp \
    source/Emulation/APU.cpp \
    source/Emulation/Controller.cpp \
    source/Emulation/MemoryAccess.cpp \
    source/Emulation/PPU.cpp \
    source/SMB/SMB.cpp \
    source/SMB/SMBData.cpp \
    source/SMB/SMBEngine.cpp \
    source/Util/Video.cpp \
    source/Util/VideoFilters.cpp \
    source/SMBRom.cpp \
    source/WindowsAudio.cpp

# GTK version source files
GTK_SOURCE_FILES = $(BASE_SOURCE_FILES) source/GTKMainWindow.cpp source/SMB/SMBCheatConstants.cpp

# SDL version source files
SDL_SOURCE_FILES = $(BASE_SOURCE_FILES) source/SDLMain.cpp source/SDLCacheScaling.cpp

# Object files for different variants
OBJS_LINUX_GTK = $(GTK_SOURCE_FILES:.cpp=.gtk.o)
OBJS_WIN_GTK = $(GTK_SOURCE_FILES:.cpp=.gtk.win.o)
OBJS_LINUX_SDL = $(SDL_SOURCE_FILES:.cpp=.sdl.o)
OBJS_WIN_SDL = $(SDL_SOURCE_FILES:.cpp=.sdl.win.o)

# Debug object files
OBJS_LINUX_GTK_DEBUG = $(GTK_SOURCE_FILES:.cpp=.gtk.debug.o)
OBJS_WIN_GTK_DEBUG = $(GTK_SOURCE_FILES:.cpp=.gtk.win.debug.o)
OBJS_LINUX_SDL_DEBUG = $(SDL_SOURCE_FILES:.cpp=.sdl.debug.o)
OBJS_WIN_SDL_DEBUG = $(SDL_SOURCE_FILES:.cpp=.sdl.win.debug.o)

# Target executables
TARGET_LINUX_GTK = smbc-gtk
TARGET_WIN_GTK = smbc-gtk.exe
TARGET_LINUX_SDL = smbc-sdl
TARGET_WIN_SDL = smbc-sdl.exe

# Debug targets
TARGET_LINUX_GTK_DEBUG = smbc-gtk_debug
TARGET_WIN_GTK_DEBUG = smbc-gtk_debug.exe
TARGET_LINUX_SDL_DEBUG = smbc-sdl_debug
TARGET_WIN_SDL_DEBUG = smbc-sdl_debug.exe

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

# Default target - build GTK version for Linux (maintaining backward compatibility)
.PHONY: all
all: linux-gtk

# Backward compatibility targets
.PHONY: windows linux
windows: windows-gtk
linux: linux-gtk

# Main build targets
.PHONY: linux-gtk linux-sdl windows-gtk windows-sdl
linux-gtk: $(BUILD_DIR_LINUX)/$(TARGET_LINUX_GTK)
linux-sdl: $(BUILD_DIR_LINUX)/$(TARGET_LINUX_SDL)
windows-gtk: $(BUILD_DIR_WIN)/$(TARGET_WIN_GTK) collect-gtk-dlls
windows-sdl: $(BUILD_DIR_WIN)/$(TARGET_WIN_SDL) collect-sdl-dlls

# Debug targets
.PHONY: debug debug-gtk debug-sdl
debug: debug-gtk debug-sdl
debug-gtk: linux-gtk-debug windows-gtk-debug
debug-sdl: linux-sdl-debug windows-sdl-debug

.PHONY: linux-gtk-debug linux-sdl-debug windows-gtk-debug windows-sdl-debug
linux-gtk-debug: $(BUILD_DIR_LINUX_DEBUG)/$(TARGET_LINUX_GTK_DEBUG)
linux-sdl-debug: $(BUILD_DIR_LINUX_DEBUG)/$(TARGET_LINUX_SDL_DEBUG)
windows-gtk-debug: $(BUILD_DIR_WIN_DEBUG)/$(TARGET_WIN_GTK_DEBUG) collect-gtk-debug-dlls
windows-sdl-debug: $(BUILD_DIR_WIN_DEBUG)/$(TARGET_WIN_SDL_DEBUG) collect-sdl-debug-dlls

#
# Linux GTK build targets
#
$(BUILD_DIR_LINUX)/$(TARGET_LINUX_GTK): $(addprefix $(BUILD_DIR_LINUX)/,$(OBJS_LINUX_GTK))
	@echo "Linking Linux GTK executable..."
	$(CXX_LINUX) $^ -o $@ $(LDFLAGS_LINUX_GTK)
	@echo "Linux GTK build complete: $@"

$(BUILD_DIR_LINUX)/%.gtk.o: %.cpp
	@echo "Compiling $< for Linux GTK..."
	$(CXX_LINUX) $(CXXFLAGS_LINUX_GTK) -c $< -o $@

#
# Linux SDL build targets
#
$(BUILD_DIR_LINUX)/$(TARGET_LINUX_SDL): $(addprefix $(BUILD_DIR_LINUX)/,$(OBJS_LINUX_SDL))
	@echo "Linking Linux SDL executable..."
	$(CXX_LINUX) $^ -o $@ $(LDFLAGS_LINUX_SDL)
	@echo "Linux SDL build complete: $@"

$(BUILD_DIR_LINUX)/%.sdl.o: %.cpp
	@echo "Compiling $< for Linux SDL..."
	$(CXX_LINUX) $(CXXFLAGS_LINUX_SDL) -c $< -o $@

#
# Windows GTK build targets
#
$(BUILD_DIR_WIN)/$(TARGET_WIN_GTK): $(addprefix $(BUILD_DIR_WIN)/,$(OBJS_WIN_GTK))
	@echo "Linking Windows GTK executable..."
	$(CXX_WIN) $^ -o $@ $(LDFLAGS_WIN_GTK)
	@echo "Windows GTK build complete: $@"

$(BUILD_DIR_WIN)/%.gtk.win.o: %.cpp
	@echo "Compiling $< for Windows GTK..."
	$(CXX_WIN) $(CXXFLAGS_WIN_GTK) -c $< -o $@

#
# Windows SDL build targets
#
$(BUILD_DIR_WIN)/$(TARGET_WIN_SDL): $(addprefix $(BUILD_DIR_WIN)/,$(OBJS_WIN_SDL))
	@echo "Linking Windows SDL executable..."
	$(CXX_WIN) $^ -o $@ $(LDFLAGS_WIN_SDL)
	@echo "Windows SDL build complete: $@"

$(BUILD_DIR_WIN)/%.sdl.win.o: %.cpp
	@echo "Compiling $< for Windows SDL..."
	$(CXX_WIN) $(CXXFLAGS_WIN_SDL) -c $< -o $@

#
# Linux GTK debug build targets
#
$(BUILD_DIR_LINUX_DEBUG)/$(TARGET_LINUX_GTK_DEBUG): $(addprefix $(BUILD_DIR_LINUX_DEBUG)/,$(OBJS_LINUX_GTK_DEBUG))
	@echo "Linking Linux GTK debug executable..."
	$(CXX_LINUX) $^ -o $@ $(LDFLAGS_LINUX_GTK)
	@echo "Linux GTK debug build complete: $@"

$(BUILD_DIR_LINUX_DEBUG)/%.gtk.debug.o: %.cpp
	@echo "Compiling $< for Linux GTK debug..."
	$(CXX_LINUX) $(CXXFLAGS_LINUX_GTK_DEBUG) -c $< -o $@

#
# Linux SDL debug build targets
#
$(BUILD_DIR_LINUX_DEBUG)/$(TARGET_LINUX_SDL_DEBUG): $(addprefix $(BUILD_DIR_LINUX_DEBUG)/,$(OBJS_LINUX_SDL_DEBUG))
	@echo "Linking Linux SDL debug executable..."
	$(CXX_LINUX) $^ -o $@ $(LDFLAGS_LINUX_SDL)
	@echo "Linux SDL debug build complete: $@"

$(BUILD_DIR_LINUX_DEBUG)/%.sdl.debug.o: %.cpp
	@echo "Compiling $< for Linux SDL debug..."
	$(CXX_LINUX) $(CXXFLAGS_LINUX_SDL_DEBUG) -c $< -o $@

#
# Windows GTK debug build targets
#
$(BUILD_DIR_WIN_DEBUG)/$(TARGET_WIN_GTK_DEBUG): $(addprefix $(BUILD_DIR_WIN_DEBUG)/,$(OBJS_WIN_GTK_DEBUG))
	@echo "Linking Windows GTK debug executable..."
	$(CXX_WIN) $^ -o $@ $(LDFLAGS_WIN_GTK)
	@echo "Windows GTK debug build complete: $@"

$(BUILD_DIR_WIN_DEBUG)/%.gtk.win.debug.o: %.cpp
	@echo "Compiling $< for Windows GTK debug..."
	$(CXX_WIN) $(CXXFLAGS_WIN_GTK_DEBUG) -c $< -o $@

#
# Windows SDL debug build targets
#
$(BUILD_DIR_WIN_DEBUG)/$(TARGET_WIN_SDL_DEBUG): $(addprefix $(BUILD_DIR_WIN_DEBUG)/,$(OBJS_WIN_SDL_DEBUG))
	@echo "Linking Windows SDL debug executable..."
	$(CXX_WIN) $^ -o $@ $(LDFLAGS_WIN_SDL)
	@echo "Windows SDL debug build complete: $@"

$(BUILD_DIR_WIN_DEBUG)/%.sdl.win.debug.o: %.cpp
	@echo "Compiling $< for Windows SDL debug..."
	$(CXX_WIN) $(CXXFLAGS_WIN_SDL_DEBUG) -c $< -o $@

#
# DLL collection for Windows builds
#
.PHONY: collect-gtk-dlls collect-sdl-dlls collect-gtk-debug-dlls collect-sdl-debug-dlls
collect-gtk-dlls: $(BUILD_DIR_WIN)/$(TARGET_WIN_GTK) create-dll-script
	@echo "Collecting GTK DLLs for Windows build..."
	@build/windows/collect_dlls.sh $(BUILD_DIR_WIN)/$(TARGET_WIN_GTK) $(DLL_SOURCE_DIR) $(BUILD_DIR_WIN) gtk

collect-sdl-dlls: $(BUILD_DIR_WIN)/$(TARGET_WIN_SDL) create-dll-script
	@echo "Collecting SDL DLLs for Windows build..."
	@build/windows/collect_dlls.sh $(BUILD_DIR_WIN)/$(TARGET_WIN_SDL) $(DLL_SOURCE_DIR) $(BUILD_DIR_WIN) sdl

collect-gtk-debug-dlls: $(BUILD_DIR_WIN_DEBUG)/$(TARGET_WIN_GTK_DEBUG) create-dll-script
	@echo "Collecting GTK Debug DLLs for Windows build..."
	@build/windows/collect_dlls.sh $(BUILD_DIR_WIN_DEBUG)/$(TARGET_WIN_GTK_DEBUG) $(DLL_SOURCE_DIR) $(BUILD_DIR_WIN_DEBUG) gtk

collect-sdl-debug-dlls: $(BUILD_DIR_WIN_DEBUG)/$(TARGET_WIN_SDL_DEBUG) create-dll-script
	@echo "Collecting SDL Debug DLLs for Windows build..."
	@build/windows/collect_dlls.sh $(BUILD_DIR_WIN_DEBUG)/$(TARGET_WIN_SDL_DEBUG) $(DLL_SOURCE_DIR) $(BUILD_DIR_WIN_DEBUG) sdl

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
BUILD_TYPE=$$4; \
; \
echo "Collecting DLLs for $$EXECUTABLE ($$BUILD_TYPE build)"; \
; \
# Copy common DLLs; \
if [ -d "$$DLL_SOURCE" ]; then; \
    cp "$$DLL_SOURCE"/SDL2.dll "$$TARGET_DIR"/ 2>/dev/null || echo "SDL2.dll not found"; \
    cp "$$DLL_SOURCE"/libgcc_s_seh-1.dll "$$TARGET_DIR"/ 2>/dev/null || echo "libgcc_s_seh-1.dll not found"; \
    cp "$$DLL_SOURCE"/libstdc++-6.dll "$$TARGET_DIR"/ 2>/dev/null || echo "libstdc++-6.dll not found"; \
    cp "$$DLL_SOURCE"/libwinpthread-1.dll "$$TARGET_DIR"/ 2>/dev/null || echo "libwinpthread-1.dll not found"; \
    ; \
    # Copy GTK3 DLLs if this is a GTK build; \
    if [ "$$BUILD_TYPE" = "gtk" ]; then; \
        cp "$$DLL_SOURCE"/libgtk-3-0.dll "$$TARGET_DIR"/ 2>/dev/null || echo "libgtk-3-0.dll not found"; \
        cp "$$DLL_SOURCE"/libgdk-3-0.dll "$$TARGET_DIR"/ 2>/dev/null || echo "libgdk-3-0.dll not found"; \
        cp "$$DLL_SOURCE"/libgobject-2.0-0.dll "$$TARGET_DIR"/ 2>/dev/null || echo "libgobject-2.0-0.dll not found"; \
        cp "$$DLL_SOURCE"/libglib-2.0-0.dll "$$TARGET_DIR"/ 2>/dev/null || echo "libglib-2.0-0.dll not found"; \
        cp "$$DLL_SOURCE"/libgdk_pixbuf-2.0-0.dll "$$TARGET_DIR"/ 2>/dev/null || echo "libgdk_pixbuf-2.0-0.dll not found"; \
        cp "$$DLL_SOURCE"/libgio-2.0-0.dll "$$TARGET_DIR"/ 2>/dev/null || echo "libgio-2.0-0.dll not found"; \
        cp "$$DLL_SOURCE"/libcairo-2.dll "$$TARGET_DIR"/ 2>/dev/null || echo "libcairo-2.dll not found"; \
        cp "$$DLL_SOURCE"/libpango-1.0-0.dll "$$TARGET_DIR"/ 2>/dev/null || echo "libpango-1.0-0.dll not found"; \
        cp "$$DLL_SOURCE"/libpangocairo-1.0-0.dll "$$TARGET_DIR"/ 2>/dev/null || echo "libpangocairo-1.0-0.dll not found"; \
        cp "$$DLL_SOURCE"/libatk-1.0-0.dll "$$TARGET_DIR"/ 2>/dev/null || echo "libatk-1.0-0.dll not found"; \
        echo "GTK DLLs copied"; \
    else; \
        echo "SDL-only build - no GTK DLLs needed"; \
    fi; \
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
	@echo "Linux SDL2:"; pkg-config --exists sdl2 && echo "  ✓ SDL2 found" || echo "  ✗ SDL2 not found - install libsdl2-dev"
	@echo "Linux GTK3:"; pkg-config --exists gtk+-3.0 && echo "  ✓ GTK+3 found" || echo "  ✗ GTK+3 not found - install libgtk-3-dev"
	@echo "Windows SDL2:"; mingw64-pkg-config --exists sdl2 2>/dev/null && echo "  ✓ MinGW SDL2 found" || echo "  ✗ MinGW SDL2 not found - install mingw64-SDL2-devel"
	@echo "Windows GTK3:"; mingw64-pkg-config --exists gtk+-3.0 2>/dev/null && echo "  ✓ MinGW GTK+3 found" || echo "  ✗ MinGW GTK+3 not found - install mingw64-gtk3-devel"

# Clean target
.PHONY: clean
clean:
	@echo "Cleaning build files..."
	find $(BUILD_DIR) -type f -name "*.o" -delete 2>/dev/null || true
	find $(BUILD_DIR) -type f -name "*.dll" -delete 2>/dev/null || true
	find $(BUILD_DIR) -type f -name "*.exe" -delete 2>/dev/null || true
	rm -f $(BUILD_DIR_LINUX)/$(TARGET_LINUX_GTK) 2>/dev/null || true
	rm -f $(BUILD_DIR_LINUX)/$(TARGET_LINUX_SDL) 2>/dev/null || true
	rm -f $(BUILD_DIR_WIN)/$(TARGET_WIN_GTK) 2>/dev/null || true
	rm -f $(BUILD_DIR_WIN)/$(TARGET_WIN_SDL) 2>/dev/null || true
	rm -f $(BUILD_DIR_LINUX_DEBUG)/$(TARGET_LINUX_GTK_DEBUG) 2>/dev/null || true
	rm -f $(BUILD_DIR_LINUX_DEBUG)/$(TARGET_LINUX_SDL_DEBUG) 2>/dev/null || true
	rm -f $(BUILD_DIR_WIN_DEBUG)/$(TARGET_WIN_GTK_DEBUG) 2>/dev/null || true
	rm -f $(BUILD_DIR_WIN_DEBUG)/$(TARGET_WIN_SDL_DEBUG) 2>/dev/null || true

# Help target
.PHONY: help
help:
	@echo "Available targets:"
	@echo "  make                    - Build GTK version for Linux (default)"
	@echo "  make linux              - Build GTK version for Linux (backward compatibility)"
	@echo "  make windows            - Build GTK version for Windows (backward compatibility)"
	@echo ""
	@echo "GTK Version builds:"
	@echo "  make linux-gtk          - Build GTK version for Linux"
	@echo "  make windows-gtk        - Build GTK version for Windows"
	@echo ""
	@echo "SDL Version builds:"
	@echo "  make linux-sdl          - Build SDL version for Linux"
	@echo "  make windows-sdl        - Build SDL version for Windows"
	@echo ""
	@echo "Debug builds:"
	@echo "  make debug              - Build debug versions for both GTK and SDL"
	@echo "  make debug-gtk          - Build debug versions for GTK (Linux + Windows)"
	@echo "  make debug-sdl          - Build debug versions for SDL (Linux + Windows)"
	@echo "  make linux-gtk-debug    - Build GTK version for Linux with debug symbols"
	@echo "  make linux-sdl-debug    - Build SDL version for Linux with debug symbols"
	@echo "  make windows-gtk-debug  - Build GTK version for Windows with debug symbols"
	@echo "  make windows-sdl-debug  - Build SDL version for Windows with debug symbols"
	@echo ""
	@echo "Utility targets:"
	@echo "  make check-deps         - Check if required dependencies are installed"
	@echo "  make create-dll-script  - Create DLL collection script"
	@echo "  make clean              - Remove all build files"
	@echo "  make help               - Show this help message"
	@echo ""
	@echo "Build outputs:"
	@echo "  Linux GTK:   $(BUILD_DIR_LINUX)/$(TARGET_LINUX_GTK)"
	@echo "  Linux SDL:   $(BUILD_DIR_LINUX)/$(TARGET_LINUX_SDL)"
	@echo "  Windows GTK: $(BUILD_DIR_WIN)/$(TARGET_WIN_GTK)"
	@echo "  Windows SDL: $(BUILD_DIR_WIN)/$(TARGET_WIN_SDL)"
	@echo ""
	@echo "Dependencies required:"
	@echo "  Linux:   SDL2 (libsdl2-dev), GTK+3 (libgtk-3-dev) for GTK builds"
	@echo "  Windows: MinGW SDL2 (mingw64-SDL2-devel), MinGW GTK+3 (mingw64-gtk3-devel) for GTK builds"
