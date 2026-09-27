# Bare-metal build of Super Mario Bros. Virtualized.
#
#   make                         build smb.elf (the kernel)
#   make run ROM=path/to/smb.nes boot it in QEMU/KVM
#   make iso ROM=path/to/smb.nes build a bootable GRUB ISO (smb.iso)
#
# The ROM is never compiled in. It's loaded at boot as a Multiboot module.

CXX      ?= g++
ROM      ?= smb.nes
SRC      := ../source
BUILD    := build
MIRROR   := $(BUILD)/src

GCCINC   := $(shell $(CXX) -print-file-name=include)
COMMON   := -m64 -ffreestanding -fno-pic -fno-pie -mno-red-zone -mcmodel=kernel \
            -fno-stack-protector -fno-exceptions -fno-rtti -fno-threadsafe-statics \
            -fno-asynchronous-unwind-tables -O2 -nostdinc -isystem $(GCCINC) \
            -Iinclude -I$(MIRROR) -Wno-write-strings -Wno-narrowing
CXXFLAGS := $(COMMON) -std=gnu++17 -msse2
IRQFLAGS := $(COMMON) -std=gnu++17 -mgeneral-regs-only
LDFLAGS  := -nostdlib -static -no-pie -T linker.ld -z max-page-size=0x1000 --no-warn-rwx-segments

# Engine sources taken unchanged from ../source
CORE := SMB/SMB.cpp SMB/SMBData.cpp SMB/SMBEngine.cpp Emulation/PPU.cpp Emulation/MemoryAccess.cpp \
        Emulation/APU.cpp
HDRS := SMB/SMB.hpp SMB/SMBConstants.hpp SMB/SMBDataPointers.hpp SMB/SMBEngine.hpp \
        Emulation/PPU.hpp Emulation/MemoryAccess.hpp Emulation/APU.hpp
# Platform pieces replaced for bare metal
OVERRIDES := Configuration.hpp Util/Video.hpp Emulation/Controller.hpp \
             Emulation/MemoryAccess.hpp Emulation/MemoryAccess.cpp \
             Emulation/Controller.cpp

OBJS := $(BUILD)/boot.o $(BUILD)/kernel.o $(BUILD)/runtime.o $(BUILD)/irq.o $(BUILD)/audio.o \
        $(patsubst %.cpp,$(BUILD)/core/%.o,$(CORE)) \
        $(BUILD)/core/Emulation/Controller.o

all: smb.elf

# APU.cpp relies on -fpermissive (as in the main build) and wraps its
# allocations in try/catch; with exceptions off, turn those into plain blocks.
$(BUILD)/core/Emulation/APU.o: CXXFLAGS += -fpermissive -w "-Dtry=if (true)" "-Dcatch(x)=if (false)"

$(MIRROR)/.stamp: $(addprefix $(SRC)/,$(CORE) $(HDRS)) $(addprefix overrides/,$(OVERRIDES))
	@mkdir -p $(MIRROR)/SMB $(MIRROR)/Emulation $(MIRROR)/Util
	@for f in $(CORE) $(HDRS); do cp $(SRC)/$$f $(MIRROR)/$$f; done
	@for f in $(OVERRIDES); do cp overrides/$$f $(MIRROR)/$$f; done
	@touch $@

$(BUILD)/core/%.o: $(MIRROR)/.stamp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $(MIRROR)/$*.cpp -o $@

$(BUILD)/boot.o: boot.S
	@mkdir -p $(BUILD)
	$(CXX) -m64 -c $< -o $@

$(BUILD)/irq.o: irq.cpp hw.hpp $(MIRROR)/.stamp
	$(CXX) $(IRQFLAGS) -c $< -o $@

$(BUILD)/%.o: %.cpp $(wildcard *.hpp include/*) font.h $(MIRROR)/.stamp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Link as ELF64, then wrap as ELF32 so QEMU's -kernel and GRUB's
# multiboot loader both accept it. The 32-bit entry stub switches to long mode.
smb.elf: $(OBJS) linker.ld
	ld $(LDFLAGS) $(OBJS) $(shell $(CXX) -print-libgcc-file-name) -o $(BUILD)/smb64.elf
	objcopy -O elf32-i386 $(BUILD)/smb64.elf $@

iso: smb.iso
smb.iso: smb.elf grub.cfg $(ROM)
	@rm -rf $(BUILD)/iso && mkdir -p $(BUILD)/iso/boot/grub
	cp smb.elf $(BUILD)/iso/boot/smb.elf
	cp "$(ROM)" $(BUILD)/iso/boot/smb.nes
	cp grub.cfg $(BUILD)/iso/boot/grub/grub.cfg
	grub-mkrescue -o $@ $(BUILD)/iso 2>/dev/null

# Sound card for QEMU: SOUND=ac97 (default), SOUND=sb16 or SOUND=none
SOUND    ?= ac97
AUDIODEV ?= $(if $(filter Darwin,$(shell uname)),coreaudio,pa)
QSOUND_ac97 := -audiodev $(AUDIODEV),id=snd0 -device AC97,audiodev=snd0
QSOUND_sb16 := -audiodev $(AUDIODEV),id=snd0 -device sb16,audiodev=snd0
QSOUND_none :=
QEMU     := qemu-system-x86_64 -enable-kvm -m 256 -serial stdio $(QSOUND_$(SOUND))

run: smb.elf
	$(QEMU) -kernel smb.elf -initrd "$(ROM)" -append "audio=$(SOUND)"

run-iso: smb.iso
	$(QEMU) -cdrom smb.iso

clean:
	rm -rf $(BUILD) smb.elf smb.iso

.PHONY: all iso run run-iso clean
