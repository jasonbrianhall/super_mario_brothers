# Bare-metal build of Super Mario Bros. Virtualized.
#
#   make                         build smb.elf (the kernel)
#   make run ROM=path/to/smb.nes boot it in QEMU/KVM
#   make iso ROM=path/to/smb.nes build a bootable GRUB ISO (smb.iso)
#   make floppies ROM=...        all three boot floppies: smb-floppy.img (x86_64),
#                                smb-i586-floppy.img and smb-i386-floppy.img
#
# ARCH=x86_64 (default) builds the 64-bit kernel, with UEFI support (make efi).
# ARCH=i586 builds a 32-bit kernel for a Pentium or later; its outputs are
# named smb-i586.elf, smb-i586.iso and smb-i586-floppy.img. CPU=pentium-mmx
# tunes it for a Pentium MMX. ARCH=i386 builds one that runs on a 386 (with
# or without a 387) and asks for 1024x768 at 8 bits per pixel first, which
# 1 MB VESA cards of the time can do. The 32-bit builds need g++-multilib.
#
# The ROM is never compiled in. It's loaded at boot as a Multiboot module.

ARCH     ?= x86_64
USB_OBJ  := usb
CXX      ?= g++
ROM      ?= smb.nes
SRC      := ../source
BUILD    := build/$(ARCH)
MIRROR   := $(BUILD)/src

# Fedora names the GRUB tools grub2-*; Debian/Ubuntu use grub-*.
GRUB_MKIMAGE  ?= $(shell command -v grub2-mkimage || command -v grub-mkimage)
GRUB_MKRESCUE ?= $(shell command -v grub2-mkrescue || command -v grub-mkrescue)
GRUB_I386     ?= $(firstword $(wildcard /usr/lib/grub/i386-pc /usr/lib/grub2/i386-pc))

ifeq ($(ARCH),x86_64)
MFLAG    := -m64
SUFFIX   :=
GFXMODE  := 1024x768x32
BOOT_S   := boot.S
# PIE=1 builds the position-independent kernel that smb.efi embeds.
ifeq ($(PIE),1)
CODEGEN  := -fpie
BOOTDEFS := -DEFI
else
CODEGEN  := -fno-pic -fno-pie -mcmodel=kernel
BOOTDEFS :=
endif
ARCHFLAGS := $(CODEGEN) -mno-red-zone
FPFLAGS  := -msse2
LDEMU    :=
else ifeq ($(ARCH),i586)
CPU      ?= pentium
MFLAG    := -m32
SUFFIX   := -i586
GFXMODE  := 640x480x32
BOOT_S   := boot32.S
BOOTDEFS := -march=$(CPU) -DFB_W=640 -DFB_H=480 -DFB_BPP=32
# No SSE on a Pentium: floats go through the x87 FPU.
ARCHFLAGS := -fno-pic -fno-pie -march=$(CPU) -mtune=$(CPU) -DFB_W=640 -DFB_H=480 -DFB_BPP=32
FPFLAGS  := -mfpmath=387 -mno-sse
LDEMU    := -m elf_i386
else ifeq ($(ARCH),i386)
CPU      ?= i386
MFLAG    := -m32
SUFFIX   := -i386
# 8 bits loses nothing (the NES has 64 colours). GRUB actually goes by the
# kernel's Multiboot header (FB_* below): 1024x768x8, else 1024x768 at any
# depth the card has, else whatever it can do; the kernel handles 8-32 bits.
GFXMODE  := 1024x768x8
BOOT_S   := boot32.S
USB_OBJ  := usb_none
FBDEFS   := -DFB_W=1024 -DFB_H=768 -DFB_BPP=8
BOOTDEFS := -march=$(CPU) -DCPU_386 $(FBDEFS)
ARCHFLAGS := -fno-pic -fno-pie -march=$(CPU) -mtune=i486 $(FBDEFS)
# x87 when there is one; without a 387 the kernel keeps sound (the only
# floating-point code) switched off.
FPFLAGS  := -mfpmath=387 -mno-sse
LDEMU    := -m elf_i386
else
$(error ARCH must be x86_64, i586 or i386)
endif

GCCINC   := $(shell $(CXX) $(MFLAG) -print-file-name=include)
LIBGCC   := $(shell $(CXX) $(MFLAG) -print-libgcc-file-name)
COMMON   := $(MFLAG) -ffreestanding $(ARCHFLAGS) \
            -fno-stack-protector -fno-exceptions -fno-rtti -fno-threadsafe-statics \
            -fno-asynchronous-unwind-tables -O2 -nostdinc -isystem $(GCCINC) \
            -Iinclude -I$(MIRROR) -Wno-write-strings -Wno-narrowing
CXXFLAGS := $(COMMON) -std=gnu++17 $(FPFLAGS)
IRQFLAGS := $(COMMON) -std=gnu++17 -mgeneral-regs-only
LDFLAGS  := $(LDEMU) -nostdlib -static -no-pie -T linker.ld -z max-page-size=0x1000 --no-warn-rwx-segments

KERNEL   := smb$(SUFFIX).elf
ISO      := smb$(SUFFIX).iso
FLOPPY   := smb$(SUFFIX)-floppy.img

# Engine sources taken unchanged from ../source
CORE := SMB/SMB.cpp SMB/SMBData.cpp SMB/SMBEngine.cpp Emulation/PPU.cpp Emulation/MemoryAccess.cpp \
        Emulation/APU.cpp
HDRS := SMB/SMB.hpp SMB/SMBConstants.hpp SMB/SMBDataPointers.hpp SMB/SMBEngine.hpp \
        Emulation/PPU.hpp Emulation/MemoryAccess.hpp Emulation/APU.hpp
# Platform pieces replaced for bare metal
OVERRIDES := Configuration.hpp Util/Video.hpp Emulation/Controller.hpp \
             Emulation/MemoryAccess.hpp Emulation/MemoryAccess.cpp \
             Emulation/Controller.cpp

OBJS := $(BUILD)/boot.o $(BUILD)/kernel.o $(BUILD)/runtime.o $(BUILD)/irq.o $(BUILD)/audio.o $(BUILD)/pci.o $(BUILD)/$(USB_OBJ).o \
        $(BUILD)/floppy.o $(BUILD)/fat12.o $(BUILD)/storage.o \
        $(patsubst %.cpp,$(BUILD)/core/%.o,$(CORE)) \
        $(BUILD)/core/Emulation/Controller.o

all: $(KERNEL)

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

$(BUILD)/boot.o: $(BOOT_S)
	@mkdir -p $(BUILD)
	$(CXX) $(MFLAG) $(BOOTDEFS) -c $< -o $@

$(BUILD)/irq.o: irq.cpp hw.hpp $(MIRROR)/.stamp
	$(CXX) $(IRQFLAGS) -c $< -o $@

$(BUILD)/%.o: %.cpp $(wildcard *.hpp include/*) font.h $(MIRROR)/.stamp
	$(CXX) $(CXXFLAGS) -c $< -o $@

ifeq ($(ARCH),x86_64)
# Link as ELF64, then wrap as ELF32 so QEMU's -kernel and GRUB's
# multiboot loader both accept it. The 32-bit entry stub switches to long mode.
$(KERNEL): $(OBJS) linker.ld
	ld $(LDFLAGS) $(OBJS) $(LIBGCC) -o $(BUILD)/smb64.elf
	objcopy -O elf32-i386 $(BUILD)/smb64.elf $@
else
# Already ELF32; the entry stub stays in 32-bit protected mode.
$(KERNEL): $(OBJS) linker.ld
	ld $(LDFLAGS) $(OBJS) $(LIBGCC) -o $@
endif

# ---------------------------------------------------------------- UEFI
# smb.efi = a small UEFI loader (gnu-efi) with the kernel embedded. It loads
# smb.nes from its own directory. gnu-efi's files live in different places on
# different distros, so find them. x86_64 only.
EFI_INC  ?= $(firstword $(wildcard /usr/include/efi))
EFI_CRT0 ?= $(firstword $(shell find /usr/lib /usr/lib64 \( -name 'crt0-efi-x86_64.o' -o -path '*gnuefi*x64*crt0.o' \) 2>/dev/null))
EFI_LDS  ?= $(firstword $(shell find /usr/lib /usr/lib64 \( -name 'elf_x86_64_efi.lds' -o -name 'elf_x64_efi.lds' \) 2>/dev/null))
EFI_LIBS ?= $(shell find /usr/lib /usr/lib64 \( -name libgnuefi.a -o -name libefi.a \) 2>/dev/null | sort -u)
EFI_BUILD := build/efi
EFI_CFLAGS := -I$(EFI_INC) -I$(EFI_INC)/x86_64 -I$(EFI_BUILD) -fpic -ffreestanding -fno-stack-protector \
              -fno-stack-check -fshort-wchar -mno-red-zone -maccumulate-outgoing-args -O2 -Wall

efi: smb.efi

ifeq ($(ARCH),x86_64)
# The kernel again, position-independent, built in its own directory.
$(EFI_BUILD)/kernel.elf: FORCE
	$(MAKE) --no-print-directory ARCH=x86_64 PIE=1 BUILD=$(EFI_BUILD)/k $(EFI_BUILD)/k/kernel-pie.elf
	cp $(EFI_BUILD)/k/kernel-pie.elf $@
FORCE:

$(BUILD)/kernel-pie.elf: $(OBJS) efi/kernel.ld
	ld -nostdlib -pie --no-dynamic-linker -z notext -z max-page-size=0x1000 --no-warn-rwx-segments \
	   -T efi/kernel.ld $(OBJS) $(LIBGCC) -o $@

$(EFI_BUILD)/kernel.bin $(EFI_BUILD)/kernel_layout.h: $(EFI_BUILD)/kernel.elf
	objcopy -O binary $< $(EFI_BUILD)/kernel.bin
	@nm $< | awk '/ __kernel_end$$/ {print "#define KERNEL_MEM_SIZE 0x" $$1} \
	              / efi_entry64$$/  {print "#define KERNEL_ENTRY 0x" $$1} \
	              / __rela_start$$/ {print "#define KERNEL_RELA_START 0x" $$1} \
	              / __rela_end$$/   {print "#define KERNEL_RELA_END 0x" $$1}' > $(EFI_BUILD)/kernel_layout.h
	@test $$(grep -c define $(EFI_BUILD)/kernel_layout.h) = 4 || { echo "kernel layout symbols missing"; exit 1; }

smb.efi: efi/loader.c efi/image.S $(EFI_BUILD)/kernel.bin $(EFI_BUILD)/kernel_layout.h
	@test -n "$(EFI_CRT0)" -a -n "$(EFI_LDS)" || { echo "gnu-efi not found (Debian: gnu-efi, Fedora: gnu-efi-devel)"; exit 1; }
	gcc $(EFI_CFLAGS) -c efi/loader.c -o $(EFI_BUILD)/loader.o
	gcc -c -DKERNEL_BIN='"$(EFI_BUILD)/kernel.bin"' efi/image.S -o $(EFI_BUILD)/image.o
	ld -nostdlib -znocombreloc -shared -Bsymbolic -T $(EFI_LDS) $(EFI_CRT0) \
	   $(EFI_BUILD)/loader.o $(EFI_BUILD)/image.o $(EFI_LIBS) $(EFI_LIBS) -o $(EFI_BUILD)/smb.so
	objcopy -j .text -j .sdata -j .data -j .rodata -j .dynamic -j .dynsym -j .rel -j .rela \
	        -j '.rel.*' -j '.rela.*' -j .reloc -O efi-app-x86_64 --subsystem=10 $(EFI_BUILD)/smb.so $@
else
smb.efi run-efi:
	@echo "UEFI is x86_64 only; build it without ARCH=i586"; exit 1
.PHONY: smb.efi
endif

# grub.cfg carries the x86_64 video mode; the i586 images get their own.
iso: $(ISO)
$(ISO): $(KERNEL) grub.cfg $(ROM)
	@rm -rf $(BUILD)/iso && mkdir -p $(BUILD)/iso/boot/grub
	cp $(KERNEL) $(BUILD)/iso/boot/smb.elf
	cp "$(ROM)" $(BUILD)/iso/boot/smb.nes
	sed 's/1024x768x32/$(GFXMODE)/' grub.cfg > $(BUILD)/iso/boot/grub/grub.cfg
	$(GRUB_MKRESCUE) -o $@ $(BUILD)/iso 2>/dev/null

# Sound card for QEMU: SOUND=hda or ac97 (the default for each ARCH is below)
# or none. ARGS adds kernel options, e.g. ARGS=debug for a once-a-second heartbeat.
AUDIODEV ?= $(if $(filter Darwin,$(shell uname)),coreaudio,pa)
QSOUND_ac97 := -audiodev $(AUDIODEV),id=snd0 -device AC97,audiodev=snd0
QSOUND_hda  := -audiodev $(AUDIODEV),id=snd0 -device intel-hda -device hda-output,audiodev=snd0
QSOUND_none :=
QUSB_xhci   := -device qemu-xhci -device usb-kbd
QUSB_none   :=
ifeq ($(ARCH),x86_64)
SOUND    ?= hda
USB      ?= xhci
QEMU     := qemu-system-x86_64 -enable-kvm -m 256 -serial stdio $(QSOUND_$(SOUND)) $(QUSB_$(USB))
else ifeq ($(ARCH),i386)
# The oldest CPU QEMU models is a 486; 8 MB, ISA-era settings, no sound card.
SOUND    ?= none
USB      ?= none
QEMU     := qemu-system-i386 -cpu 486 -m 8 -serial stdio $(QSOUND_$(SOUND)) $(QUSB_$(USB))
else
# A period machine: Pentium CPU, 64 MB, PS/2 keyboard, no KVM.
# USB=xhci adds a USB keyboard on an xHCI controller.
SOUND    ?= ac97
USB      ?= none
QEMU     := qemu-system-i386 -cpu $(CPU) -m 64 -serial stdio $(QSOUND_$(SOUND)) $(QUSB_$(USB))
endif

# 1.44 MB FAT12 boot floppy: GRUB's boot sector (keeping the FAT header) in
# sector 0, GRUB's core image in the reserved sectors after it, and the
# kernel (gzipped; GRUB unpacks it), ROM and grub.cfg as files. Save states
# go in /smb on the same disk (storage.cpp).
MKFS_FAT ?= $(shell command -v mkfs.fat || command -v mkfs.vfat || echo /usr/sbin/mkfs.fat)
floppy: $(FLOPPY)
$(FLOPPY): $(KERNEL) grub.cfg $(ROM)
	$(GRUB_MKIMAGE) -O i386-pc -o $(BUILD)/core.img -p /boot/grub \
	    biosdisk fat gzio normal configfile multiboot vbe video video_fb boot
	gzip -9 -n -c $(KERNEL) > $(BUILD)/kernel.gz
	sed 's/1024x768x32/$(GFXMODE)/' grub.cfg > $(BUILD)/grub.cfg
	rm -f $@
	$(MKFS_FAT) -C -F 12 -f 2 -r 224 -R $$(( $$(stat -c %s $(BUILD)/core.img) / 512 + 2 )) -n SMB $@ 1440 >/dev/null
	mmd -i $@ ::/boot ::/boot/grub ::/smb
	mcopy -i $@ $(BUILD)/kernel.gz ::/boot/smb.elf
	mcopy -i $@ "$(ROM)" ::/boot/smb.nes
	mcopy -i $@ $(BUILD)/grub.cfg ::/boot/grub/grub.cfg
	cp $(GRUB_I386)/boot.img $(BUILD)/boot.img
	dd if=$@ of=$(BUILD)/boot.img bs=1 skip=3 seek=3 count=59 conv=notrunc status=none
	dd if=$(BUILD)/boot.img of=$@ bs=512 count=1 conv=notrunc status=none
	dd if=$(BUILD)/core.img of=$@ bs=512 seek=1 conv=notrunc status=none
	@echo "$@: GRUB $$(( $$(stat -c %s $(BUILD)/core.img) / 1024 )) KB + kernel $$(( $$(stat -c %s $(BUILD)/kernel.gz) / 1024 )) KB (gzipped) + ROM; $$(( $$(mdir -i $@ :: | sed -n 's/ *\([0-9 ]*\) bytes free/\1/p' | tr -d ' ') / 1024 )) KB free for saves"

# Every architecture's floppy, each in its own build directory.
floppies:
	$(MAKE) --no-print-directory ARCH=x86_64 floppy
	$(MAKE) --no-print-directory ARCH=i586 floppy
	$(MAKE) --no-print-directory ARCH=i386 floppy

run: $(KERNEL)
	$(QEMU) -kernel $(KERNEL) -initrd "$(ROM)" -append "audio=$(SOUND) $(ARGS)"

run-iso: $(ISO)
	$(QEMU) -cdrom $(ISO)

ifeq ($(ARCH),x86_64)
# Boot smb.efi under UEFI firmware (OVMF) from a folder QEMU presents as a FAT disk.
# OVMF needs its code image plus a writable copy of the matching variable store.
OVMF ?= $(firstword $(wildcard /usr/share/OVMF/OVMF_CODE_4M.fd /usr/share/OVMF/OVMF_CODE.fd \
          /usr/share/edk2/ovmf/OVMF_CODE.fd /usr/share/edk2/x64/OVMF_CODE.4m.fd /usr/share/qemu/ovmf-x86_64.bin))
OVMF_VARS ?= $(firstword $(wildcard $(subst CODE,VARS,$(OVMF))))
run-efi: smb.efi
	@test -n "$(OVMF)" -a -n "$(OVMF_VARS)" || { echo "OVMF firmware not found (Debian: ovmf, Fedora: edk2-ovmf); set OVMF= and OVMF_VARS="; exit 1; }
	@rm -rf $(BUILD)/esp && mkdir -p $(BUILD)/esp/EFI/BOOT
	cp smb.efi $(BUILD)/esp/EFI/BOOT/BOOTX64.EFI
	cp "$(ROM)" $(BUILD)/esp/EFI/BOOT/smb.nes
	cp $(OVMF_VARS) $(BUILD)/ovmf_vars.fd
	$(QEMU) -drive if=pflash,format=raw,readonly=on,file=$(OVMF) \
	        -drive if=pflash,format=raw,file=$(BUILD)/ovmf_vars.fd -drive format=raw,file=fat:rw:$(BUILD)/esp
endif

run-floppy: $(FLOPPY)
	$(QEMU) -fda $(FLOPPY) -boot a

clean:
	rm -rf build smb.elf smb.iso smb-floppy.img smb.efi smb-i586.elf smb-i586.iso smb-i586-floppy.img \
	       smb-i386.elf smb-i386.iso smb-i386-floppy.img

.PHONY: all iso floppy floppies efi run run-iso run-floppy run-efi clean FORCE
