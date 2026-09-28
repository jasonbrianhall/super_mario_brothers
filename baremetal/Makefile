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
# Fedora names the GRUB tools grub2-*; Debian/Ubuntu use grub-*.
GRUB_MKIMAGE  ?= $(shell command -v grub2-mkimage || command -v grub-mkimage)
GRUB_MKRESCUE ?= $(shell command -v grub2-mkrescue || command -v grub-mkrescue)
# PIE=1 builds the position-independent kernel that smb.efi embeds.
ifeq ($(PIE),1)
CODEGEN  := -fpie
BOOTDEFS := -DEFI
else
CODEGEN  := -fno-pic -fno-pie -mcmodel=kernel
BOOTDEFS :=
endif
COMMON   := -m64 -ffreestanding $(CODEGEN) -mno-red-zone \
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

OBJS := $(BUILD)/boot.o $(BUILD)/kernel.o $(BUILD)/runtime.o $(BUILD)/irq.o $(BUILD)/audio.o $(BUILD)/pci.o $(BUILD)/usb.o \
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
	$(CXX) -m64 $(BOOTDEFS) -c $< -o $@

$(BUILD)/irq.o: irq.cpp hw.hpp $(MIRROR)/.stamp
	$(CXX) $(IRQFLAGS) -c $< -o $@

$(BUILD)/%.o: %.cpp $(wildcard *.hpp include/*) font.h $(MIRROR)/.stamp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Link as ELF64, then wrap as ELF32 so QEMU's -kernel and GRUB's
# multiboot loader both accept it. The 32-bit entry stub switches to long mode.
smb.elf: $(OBJS) linker.ld
	ld $(LDFLAGS) $(OBJS) $(shell $(CXX) -print-libgcc-file-name) -o $(BUILD)/smb64.elf
	objcopy -O elf32-i386 $(BUILD)/smb64.elf $@

# ---------------------------------------------------------------- UEFI
# smb.efi = a small UEFI loader (gnu-efi) with the kernel embedded. It loads
# smb.nes from its own directory. gnu-efi's files live in different places on
# different distros, so find them.
EFI_INC  ?= $(firstword $(wildcard /usr/include/efi))
EFI_CRT0 ?= $(firstword $(shell find /usr/lib /usr/lib64 \( -name 'crt0-efi-x86_64.o' -o -path '*gnuefi*x64*crt0.o' \) 2>/dev/null))
EFI_LDS  ?= $(firstword $(shell find /usr/lib /usr/lib64 \( -name 'elf_x86_64_efi.lds' -o -name 'elf_x64_efi.lds' \) 2>/dev/null))
EFI_LIBS ?= $(shell find /usr/lib /usr/lib64 \( -name libgnuefi.a -o -name libefi.a \) 2>/dev/null | sort -u)
EFI_BUILD := $(BUILD)/efi
EFI_CFLAGS := -I$(EFI_INC) -I$(EFI_INC)/x86_64 -I$(EFI_BUILD) -fpic -ffreestanding -fno-stack-protector \
              -fno-stack-check -fshort-wchar -mno-red-zone -maccumulate-outgoing-args -O2 -Wall

efi: smb.efi

# The kernel again, position-independent, built in its own directory.
$(EFI_BUILD)/kernel.elf: FORCE
	$(MAKE) --no-print-directory PIE=1 BUILD=$(EFI_BUILD)/k $(EFI_BUILD)/k/kernel-pie.elf
	cp $(EFI_BUILD)/k/kernel-pie.elf $@
FORCE:

$(BUILD)/kernel-pie.elf: $(OBJS) efi/kernel.ld
	ld -nostdlib -pie --no-dynamic-linker -z notext -z max-page-size=0x1000 --no-warn-rwx-segments \
	   -T efi/kernel.ld $(OBJS) $(shell $(CXX) -print-libgcc-file-name) -o $@

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
	        -j '.rel.*' -j '.rela.*' -j .reloc --target efi-app-x86_64 --subsystem=10 $(EFI_BUILD)/smb.so $@

iso: smb.iso
smb.iso: smb.elf grub.cfg $(ROM)
	@rm -rf $(BUILD)/iso && mkdir -p $(BUILD)/iso/boot/grub
	cp smb.elf $(BUILD)/iso/boot/smb.elf
	cp "$(ROM)" $(BUILD)/iso/boot/smb.nes
	cp grub.cfg $(BUILD)/iso/boot/grub/grub.cfg
	$(GRUB_MKRESCUE) -o $@ $(BUILD)/iso 2>/dev/null

# Sound card for QEMU: SOUND=hda (default), SOUND=ac97 or SOUND=none.
# ARGS adds kernel options, e.g. ARGS=debug for a once-a-second heartbeat.
SOUND    ?= hda
AUDIODEV ?= $(if $(filter Darwin,$(shell uname)),coreaudio,pa)
QSOUND_ac97 := -audiodev $(AUDIODEV),id=snd0 -device AC97,audiodev=snd0
QSOUND_hda  := -audiodev $(AUDIODEV),id=snd0 -device intel-hda -device hda-output,audiodev=snd0
QSOUND_none :=
QEMU     := qemu-system-x86_64 -enable-kvm -m 256 -serial stdio $(QSOUND_$(SOUND)) \
            -device qemu-xhci -device usb-kbd

# 1.44 MB boot floppy: GRUB's core image with the kernel, ROM and grub.cfg
# packed inside it (as a compressed memdisk), behind GRUB's boot sector.
floppy: smb-floppy.img
smb-floppy.img: smb.elf grub.cfg $(ROM)
	@rm -rf $(BUILD)/fd && mkdir -p $(BUILD)/fd/boot/grub
	cp smb.elf $(BUILD)/fd/boot/smb.elf
	cp "$(ROM)" $(BUILD)/fd/boot/smb.nes
	cp grub.cfg $(BUILD)/fd/boot/grub/grub.cfg
	tar -C $(BUILD)/fd -cf $(BUILD)/memdisk.tar boot
	$(GRUB_MKIMAGE) -O i386-pc -o $(BUILD)/core.img -p '(memdisk)/boot/grub' \
	    -m $(BUILD)/memdisk.tar memdisk tar normal configfile multiboot vbe video video_fb boot
	cat /usr/lib/grub/i386-pc/boot.img $(BUILD)/core.img > $@
	truncate -s 1474560 $@
	@echo "$@: $$(( ($$(stat -c %s $(BUILD)/core.img) + 512) * 100 / 1474560 ))% of a 1.44 MB floppy used"

run: smb.elf
	$(QEMU) -kernel smb.elf -initrd "$(ROM)" -append "audio=$(SOUND) $(ARGS)"

run-iso: smb.iso
	$(QEMU) -cdrom smb.iso

# Boot smb.efi under UEFI firmware (OVMF) from a folder QEMU presents as a FAT disk.
OVMF ?= $(firstword $(wildcard /usr/share/OVMF/OVMF_CODE_4M.fd /usr/share/OVMF/OVMF_CODE.fd \
          /usr/share/edk2/ovmf/OVMF_CODE.fd /usr/share/edk2/x64/OVMF_CODE.4m.fd /usr/share/qemu/ovmf-x86_64.bin))
run-efi: smb.efi
	@rm -rf $(BUILD)/esp && mkdir -p $(BUILD)/esp/EFI/BOOT
	cp smb.efi $(BUILD)/esp/EFI/BOOT/BOOTX64.EFI
	cp "$(ROM)" $(BUILD)/esp/EFI/BOOT/smb.nes
	$(QEMU) -drive if=pflash,format=raw,readonly=on,file=$(OVMF) -drive format=raw,file=fat:rw:$(BUILD)/esp

run-floppy: smb-floppy.img
	$(QEMU) -fda smb-floppy.img -boot a

clean:
	rm -rf $(BUILD) smb.elf smb.iso smb-floppy.img smb.efi

.PHONY: all iso floppy efi run run-iso run-floppy run-efi clean FORCE
