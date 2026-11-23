# Super Mario Bros. for MS-DOS / FreeDOS (DJGPP, 32-bit DPMI, SDL3)
#
#   make            smb.exe with CWSDPMI embedded, plus SMBC.CON next to it:
#                   this folder is then a working game
#   make msdos_original
#                   smb.exe that needs CWSDPMI.EXE next to it (or any
#                   other DPMI host: Windows 9x, HDPMI32, ...)
#   make dist       build/dist/SMB: the game folder, ready for a DOS disk
#   make run        run it in DOSBox
#   make clean
#
# Needs a DJGPP cross compiler on the PATH (i586-pc-msdosdjgpp-gcc/g++;
# prebuilt: https://github.com/andrewwutw/build-djgpp/releases), or Docker,
# which gets the djfdyuruiry/djgpp image instead. Plus make.
# Everything else is in this folder:
#
#   SDL3/         SDL3 source (libsdl.org, commit 4ca1b1c), with its DOS port:
#                 VESA/VGA video, Sound Blaster, keyboard, INT 33h mouse, gameport.
#                 Compiled here into build/libSDL3.a; no CMake. (The same tree
#                 as SDLPoPDOS's msdos/SDL3, with its DOS fixes.)
#   sdl3-config/  the two headers SDL's CMake would generate, for DJGPP.
#   cwsdpmi/      CWSDPMI r7 (the DPMI host; CWSDSTUB.EXE is put in front of the game).
#   compat/       SDL2/SDL.h: the SDL2 names the shared sources use, on SDL3;
#                 boost/property_tree: the bit of Boost Configuration.cpp uses.
#
# The game is the normal source (../source). source/DOSMain.cpp is the DOS
# front end (instead of SDLMain.cpp); source/DOSVideo.cpp puts the NES picture
# on the screen.
#
# Runs on a 486DX or later (FPU needed); a Pentium for full speed. A VESA BIOS
# gives 640x480 (doubled picture); plain VGA gets mode 13h. See README.md.

SRC     := ../source
SDL     := SDL3
BUILD   := build

DOS_TARGET      = smb.exe
COFF_TARGET     = $(BUILD)/smb.coff
SDL3_LIB        = $(BUILD)/libSDL3.a

# The compiler: a local DJGPP cross compiler (i586-pc-msdosdjgpp-gcc on the
# PATH) when there is one, otherwise the djfdyuruiry/djgpp Docker image.
# Force either with DOCKER=1 / DOCKER=0.
DJGPP_IMAGE = djfdyuruiry/djgpp

USER_ID  = $(shell id -u)
GROUP_ID = $(shell id -g)

CROSS ?= i586-pc-msdosdjgpp-
ifeq ($(origin DOCKER),undefined)
DOCKER := $(if $(shell command -v $(CROSS)gcc 2>/dev/null),0,1)
endif

ifeq ($(DOCKER),1)
# The whole tree is mounted, so ../source is reachable. Inside the image the
# DJGPP tools have no prefix; make runs there with DOCKER=0.
RUN      = docker run --rm -v $(abspath ..):/src:z -w /src/msdos_sdl -u $(USER_ID):$(GROUP_ID) $(DJGPP_IMAGE) /bin/sh -c
CC_DOS   = gcc
CXX_DOS  = g++
AR_DOS   = ar
EXE2COFF = exe2coff
else
CC_DOS   = $(CROSS)gcc
CXX_DOS  = $(CROSS)g++
AR_DOS   = $(CROSS)ar
EXE2COFF = $(shell command -v $(CROSS)exe2coff 2>/dev/null || echo $(dir $(shell command -v $(CROSS)gcc))../i586-pc-msdosdjgpp/bin/exe2coff)
endif

all: msdos_embedded

# SDL3/, sdl3-config/ and cwsdpmi/ must be real folders in here (not
# symlinks out of the tree: Docker only sees this repository).
MISSING := $(strip $(foreach d,$(SDL)/include/SDL3/SDL.h sdl3-config/SDL3/SDL_revision.h cwsdpmi/bin/CWSDSTUB.EXE,$(if $(wildcard $(d)),,$(d))))
ifneq ($(MISSING),)
ifeq ($(filter clean,$(MAKECMDGOALS)),)
$(error Missing $(MISSING). Copy SDL3/, sdl3-config/ and cwsdpmi/ from SDLPoPDOS's msdos/ folder into msdos_sdl/ (real copies, not symlinks))
endif
endif

# ---------------------------------------------------------------- SDL3
SDL_CFLAGS = -O2 -DNDEBUG -fno-strict-aliasing -w \
             -DSDL_BUILD_MAJOR_VERSION=3 -DSDL_BUILD_MINOR_VERSION=5 -DSDL_BUILD_MICRO_VERSION=0 \
             -DSDL_STATIC_LIB -DUSING_GENERATED_CONFIG_H \
             -Isdl3-config/build_config -Isdl3-config -I$(SDL)/include -I$(SDL)/src \
             -idirafter $(SDL)/src/video/khronos

# The SDL3 sources for DJGPP (what SDL's own build compiles for this target).
SDL_SRCS = \
  src/SDL.c \
  src/SDL_assert.c \
  src/SDL_error.c \
  src/SDL_guid.c \
  src/SDL_hashtable.c \
  src/SDL_hints.c \
  src/SDL_list.c \
  src/SDL_log.c \
  src/SDL_properties.c \
  src/SDL_utils.c \
  src/atomic/SDL_atomic.c \
  src/atomic/SDL_spinlock.c \
  src/audio/SDL_audio.c \
  src/audio/SDL_audiocvt.c \
  src/audio/SDL_audiodev.c \
  src/audio/SDL_audioqueue.c \
  src/audio/SDL_audioresample.c \
  src/audio/SDL_audiotypecvt.c \
  src/audio/SDL_mixer.c \
  src/audio/SDL_wave.c \
  src/audio/disk/SDL_diskaudio.c \
  src/audio/dos/SDL_dosaudio_sb.c \
  src/audio/dummy/SDL_dummyaudio.c \
  src/camera/SDL_camera.c \
  src/camera/dummy/SDL_camera_dummy.c \
  src/core/SDL_core_unsupported.c \
  src/core/dos/SDL_dos.c \
  src/core/dos/SDL_dos_scheduler.c \
  src/cpuinfo/SDL_cpuinfo.c \
  src/dialog/SDL_dialog.c \
  src/dialog/dummy/SDL_dummydialog.c \
  src/dynapi/SDL_dynapi.c \
  src/events/SDL_categories.c \
  src/events/SDL_clipboardevents.c \
  src/events/SDL_displayevents.c \
  src/events/SDL_dropevents.c \
  src/events/SDL_events.c \
  src/events/SDL_eventwatch.c \
  src/events/SDL_keyboard.c \
  src/events/SDL_keymap.c \
  src/events/SDL_keysym_to_keycode.c \
  src/events/SDL_keysym_to_scancode.c \
  src/events/SDL_mouse.c \
  src/events/SDL_notificationevents.c \
  src/events/SDL_pen.c \
  src/events/SDL_quit.c \
  src/events/SDL_scancode_tables.c \
  src/events/SDL_touch.c \
  src/events/SDL_windowevents.c \
  src/events/imKStoUCS.c \
  src/filesystem/SDL_filesystem.c \
  src/filesystem/dos/SDL_sysfilesystem.c \
  src/filesystem/posix/SDL_sysfsops.c \
  src/gpu/SDL_gpu.c \
  src/gpu/xr/SDL_gpu_openxr.c \
  src/gpu/xr/SDL_openxrdyn.c \
  src/haptic/SDL_haptic.c \
  src/haptic/dummy/SDL_syshaptic.c \
  src/hidapi/SDL_hidapi.c \
  src/io/SDL_asyncio.c \
  src/io/SDL_iostream.c \
  src/io/generic/SDL_asyncio_generic.c \
  src/joystick/SDL_gamepad.c \
  src/joystick/SDL_joystick.c \
  src/joystick/SDL_steam_virtual_gamepad.c \
  src/joystick/controller_type.c \
  src/joystick/dos/SDL_sysjoystick.c \
  src/joystick/virtual/SDL_virtualjoystick.c \
  src/libm/e_atan2.c \
  src/libm/e_exp.c \
  src/libm/e_fmod.c \
  src/libm/e_log.c \
  src/libm/e_log10.c \
  src/libm/e_pow.c \
  src/libm/e_rem_pio2.c \
  src/libm/e_sqrt.c \
  src/libm/k_cos.c \
  src/libm/k_rem_pio2.c \
  src/libm/k_sin.c \
  src/libm/k_tan.c \
  src/libm/s_atan.c \
  src/libm/s_copysign.c \
  src/libm/s_cos.c \
  src/libm/s_fabs.c \
  src/libm/s_floor.c \
  src/libm/s_isinf.c \
  src/libm/s_isinff.c \
  src/libm/s_isnan.c \
  src/libm/s_isnanf.c \
  src/libm/s_modf.c \
  src/libm/s_scalbn.c \
  src/libm/s_sin.c \
  src/libm/s_tan.c \
  src/loadso/dummy/SDL_sysloadso.c \
  src/locale/SDL_locale.c \
  src/locale/dummy/SDL_syslocale.c \
  src/main/SDL_main_callbacks.c \
  src/main/SDL_runapp.c \
  src/main/dos/SDL_sysmain_runapp.c \
  src/main/generic/SDL_sysmain_callbacks.c \
  src/misc/SDL_libusb.c \
  src/misc/SDL_url.c \
  src/misc/dummy/SDL_sysurl.c \
  src/notification/SDL_notification.c \
  src/notification/dummy/SDL_dummynotification.c \
  src/power/SDL_power.c \
  src/process/SDL_process.c \
  src/process/dummy/SDL_dummyprocess.c \
  src/render/SDL_render.c \
  src/render/SDL_render_unsupported.c \
  src/render/SDL_yuv_sw.c \
  src/render/direct3d/SDL_render_d3d.c \
  src/render/direct3d/SDL_shaders_d3d.c \
  src/render/direct3d11/SDL_render_d3d11.c \
  src/render/direct3d11/SDL_shaders_d3d11.c \
  src/render/direct3d12/SDL_render_d3d12.c \
  src/render/direct3d12/SDL_shaders_d3d12.c \
  src/render/gpu/SDL_pipeline_gpu.c \
  src/render/gpu/SDL_render_gpu.c \
  src/render/gpu/SDL_shaders_gpu.c \
  src/render/ngage/SDL_render_ngage.c \
  src/render/opengl/SDL_render_gl.c \
  src/render/opengl/SDL_shaders_gl.c \
  src/render/opengles/SDL_render_gles.c \
  src/render/opengles2/SDL_render_gles2.c \
  src/render/opengles2/SDL_shaders_gles2.c \
  src/render/ps2/SDL_render_ps2.c \
  src/render/psp/SDL_render_psp.c \
  src/render/software/SDL_blendfillrect.c \
  src/render/software/SDL_blendline.c \
  src/render/software/SDL_blendpoint.c \
  src/render/software/SDL_drawline.c \
  src/render/software/SDL_drawpoint.c \
  src/render/software/SDL_render_sw.c \
  src/render/software/SDL_triangle.c \
  src/render/vitagxm/SDL_render_vita_gxm.c \
  src/render/vitagxm/SDL_render_vita_gxm_memory.c \
  src/render/vitagxm/SDL_render_vita_gxm_tools.c \
  src/render/vulkan/SDL_render_vulkan.c \
  src/render/vulkan/SDL_shaders_vulkan.c \
  src/sensor/SDL_sensor.c \
  src/sensor/dummy/SDL_dummysensor.c \
  src/stdlib/SDL_crc16.c \
  src/stdlib/SDL_crc32.c \
  src/stdlib/SDL_getenv.c \
  src/stdlib/SDL_iconv.c \
  src/stdlib/SDL_malloc.c \
  src/stdlib/SDL_memcpy.c \
  src/stdlib/SDL_memmove.c \
  src/stdlib/SDL_memset.c \
  src/stdlib/SDL_mslibc.c \
  src/stdlib/SDL_murmur3.c \
  src/stdlib/SDL_qsort.c \
  src/stdlib/SDL_random.c \
  src/stdlib/SDL_stdlib.c \
  src/stdlib/SDL_string.c \
  src/stdlib/SDL_strtokr.c \
  src/storage/SDL_storage.c \
  src/storage/generic/SDL_genericstorage.c \
  src/thread/SDL_thread.c \
  src/thread/dos/SDL_sysmutex.c \
  src/thread/dos/SDL_syssem.c \
  src/thread/dos/SDL_systhread.c \
  src/thread/dos/SDL_systls.c \
  src/thread/generic/SDL_syscond.c \
  src/thread/generic/SDL_sysrwlock.c \
  src/time/SDL_time.c \
  src/time/unix/SDL_systime.c \
  src/timer/SDL_timer.c \
  src/timer/dos/SDL_systimer.c \
  src/tray/SDL_tray_utils.c \
  src/tray/dummy/SDL_tray.c \
  src/video/SDL_RLEaccel.c \
  src/video/SDL_blit.c \
  src/video/SDL_blit_0.c \
  src/video/SDL_blit_1.c \
  src/video/SDL_blit_A.c \
  src/video/SDL_blit_N.c \
  src/video/SDL_blit_auto.c \
  src/video/SDL_blit_copy.c \
  src/video/SDL_blit_slow.c \
  src/video/SDL_bmp.c \
  src/video/SDL_clipboard.c \
  src/video/SDL_egl.c \
  src/video/SDL_fillrect.c \
  src/video/SDL_pixels.c \
  src/video/SDL_rect.c \
  src/video/SDL_rotate.c \
  src/video/SDL_stb.c \
  src/video/SDL_stretch.c \
  src/video/SDL_surface.c \
  src/video/SDL_video.c \
  src/video/SDL_video_unsupported.c \
  src/video/SDL_vulkan_utils.c \
  src/video/SDL_yuv.c \
  src/video/dos/SDL_dosevents.c \
  src/video/dos/SDL_dosframebuffer.c \
  src/video/dos/SDL_dosmodes.c \
  src/video/dos/SDL_dosmouse.c \
  src/video/dos/SDL_dosvideo.c \
  src/video/dummy/SDL_nullevents.c \
  src/video/dummy/SDL_nullframebuffer.c \
  src/video/dummy/SDL_nullvideo.c \
  src/video/yuv2rgb/yuv_rgb_lsx.c \
  src/video/yuv2rgb/yuv_rgb_sse.c \
  src/video/yuv2rgb/yuv_rgb_std.c

SDL_OBJS = $(addprefix $(BUILD)/sdl/,$(SDL_SRCS:.c=.o))

$(BUILD)/sdl/%.o: $(SDL)/%.c
	@mkdir -p $(dir $@)
	$(CC_DOS) $(SDL_CFLAGS) -MMD -MP -c $< -o $@

-include $(SDL_OBJS:.o=.d)

$(SDL3_LIB): $(SDL_OBJS)
	rm -f $@
	$(AR_DOS) rcs $@ $(SDL_OBJS)

sdl3: $(SDL3_LIB)

# ---------------------------------------------------------------- the game
# compat/ comes first: its SDL2/SDL.h and boost/ stand in for the real ones.
CXXFLAGS = -std=gnu++17 -O2 -march=i486 -mtune=i586 -fpermissive -w \
           -Icompat -I$(SDL)/include -I$(SRC)
LDFLAGS  = -s

GAME_SRCS = DOSMain.cpp DOSVideo.cpp Configuration.cpp SMBRom.cpp \
            Emulation/APU.cpp Emulation/Controller.cpp Emulation/MemoryAccess.cpp Emulation/PPU.cpp \
            SMB/SMB.cpp SMB/SMBData.cpp SMB/SMBEngine.cpp \
            Util/Video.cpp Util/VideoFilters.cpp
OBJS  = $(addprefix $(BUILD)/obj/,$(GAME_SRCS:.cpp=.o))
HDRS  = $(wildcard $(SRC)/*.hpp $(SRC)/*/*.hpp) $(wildcard compat/SDL2/*.h compat/boost/property_tree/*.hpp)

$(BUILD)/obj/%.o: $(SRC)/%.cpp $(HDRS)
	@mkdir -p $(dir $@)
	$(CXX_DOS) $(CXXFLAGS) -c $< -o $@

ifneq ($(DOCKER),1)
$(BUILD)/smb-go32.exe: $(OBJS) $(SDL3_LIB)
	$(CXX_DOS) $(OBJS) $(LDFLAGS) -o $@ $(SDL3_LIB) -lm
endif

ifeq ($(DOCKER),1)
pull-djgpp:
	docker pull $(DJGPP_IMAGE)

# Everything that needs the compiler happens in the container.
$(BUILD)/smb-go32.exe $(COFF_TARGET): FORCE | pull-djgpp
	$(RUN) "make DOCKER=0 CROSS= CC_DOS=gcc CXX_DOS=g++ AR_DOS=ar EXE2COFF=exe2coff $(COFF_TARGET)"

FORCE:
.PHONY: pull-djgpp FORCE
else
$(COFF_TARGET): $(BUILD)/smb-go32.exe
	cp $< $(BUILD)/smb.exe
	cd $(BUILD) && $(EXE2COFF) smb.exe && mv smb smb.coff && rm smb.exe
endif

# Needs CWSDPMI.EXE (or another DPMI host) at run time.
msdos_original: $(BUILD)/smb-go32.exe runtime-files
	cp $(BUILD)/smb-go32.exe $(DOS_TARGET)
	cp cwsdpmi/bin/CWSDPMI.EXE .

# CWSDPMI's stub in front: one file that runs on plain DOS.
msdos_embedded: $(COFF_TARGET) runtime-files
	cat cwsdpmi/bin/CWSDSTUB.EXE $(COFF_TARGET) > $(DOS_TARGET)
	chmod +x $(DOS_TARGET)
	@echo "Created $(DOS_TARGET) with embedded DPMI extender; run it from this folder"

# What smb.exe reads at run time, next to it, so this folder is a working
# game. The settings file is smbc.conf, which is SMBC.CON in 8.3; the ROM is
# built into the EXE.
runtime-files:
	@if [ ! -f SMBC.CON ]; then cp smbc.conf SMBC.CON; fi

msdos: msdos_embedded

# ---------------------------------------------------------------- packaging
# The folder to copy to a DOS machine: SMB.EXE and the settings.
dist: msdos_embedded
	rm -rf $(BUILD)/dist && mkdir -p $(BUILD)/dist/SMB
	cp $(DOS_TARGET) $(BUILD)/dist/SMB/SMB.EXE
	sed 's/$$/\r/' smbc.conf > $(BUILD)/dist/SMB/SMBC.CON
	sed 's/$$/\r/' README.md > $(BUILD)/dist/SMB/README.TXT
	sed 's/$$/\r/' ../LICENSE.md > $(BUILD)/dist/SMB/LICENSE.TXT
	@echo "Created $(BUILD)/dist/SMB"

# DOSBox with this folder as C:.
run: msdos_embedded
	printf '[cpu]\ncore=dynamic\ncputype=pentium_slow\ncycles=max\n[dosbox]\nmachine=svga_s3\nmemsize=64\n[sblaster]\nsbtype=sb16\n[autoexec]\nmount c $(abspath .)\nc:\nSET BLASTER=A220 I7 D1 H5 T6\nSMB.EXE\n' > $(BUILD)/dosbox.conf
	dosbox -conf $(BUILD)/dosbox.conf

# SMBC.CON and save states are left alone.
clean:
	rm -rf $(BUILD) $(DOS_TARGET) CWSDPMI.EXE

.PHONY: all sdl3 msdos msdos_original msdos_embedded runtime-files dist run clean
