// smb.efi: UEFI loader for the bare-metal Super Mario Bros. kernel.
//
// Reads smb.nes from its own directory, takes the framebuffer from the
// Graphics Output Protocol, copies the embedded position-independent kernel
// below 4 GiB, applies its relocations, exits boot services and jumps to it
// with the same Multiboot-style information GRUB would have provided.
#include <efi.h>
#include <efilib.h>

extern const UINT8 kernel_image[], kernel_image_end[];
#include "kernel_layout.h"      // KERNEL_MEM_SIZE, KERNEL_ENTRY, KERNEL_RELA_START/END

struct __attribute__((packed)) MultibootInfo {
    UINT32 flags, mem_lower, mem_upper, boot_device, cmdline;
    UINT32 mods_count, mods_addr;
    UINT32 syms[4];
    UINT32 mmap_length, mmap_addr, drives_length, drives_addr;
    UINT32 config_table, boot_loader_name, apm_table;
    UINT32 vbe_control_info, vbe_mode_info;
    UINT16 vbe_mode, vbe_interface_seg, vbe_interface_off, vbe_interface_len;
    UINT64 fb_addr;
    UINT32 fb_pitch, fb_width, fb_height;
    UINT8 fb_bpp, fb_type;
};
struct __attribute__((packed)) MultibootModule { UINT32 mod_start, mod_end, string, reserved; };
typedef struct { UINT64 r_offset, r_info; INT64 r_addend; } Elf64_Rela;

static EFI_SYSTEM_TABLE* ST_;

static void fail(CHAR16* msg) {
    Print(L"\r\nsmb.efi: %s\r\nPress any key to return.\r\n", msg);
    UINTN idx;
    uefi_call_wrapper(ST_->BootServices->WaitForEvent, 3, 1, &ST_->ConIn->WaitForKey, &idx);
}

// Allocate pages below 4 GiB (the kernel's DMA structures need 32-bit addresses).
static void* alloc_low(UINTN bytes) {
    EFI_PHYSICAL_ADDRESS addr = 0xFFFFFFFF;
    if (EFI_ERROR(uefi_call_wrapper(ST_->BootServices->AllocatePages, 4, AllocateMaxAddress,
                                    EfiLoaderData, EFI_SIZE_TO_PAGES(bytes), &addr)))
        return NULL;
    return (void*)(UINTN)addr;
}

// Pick a 32-bit BGRX mode: keep the current one if it qualifies, else the
// largest that does.
static EFI_GRAPHICS_OUTPUT_PROTOCOL* setup_gop(void) {
    EFI_GUID guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_GRAPHICS_OUTPUT_PROTOCOL* gop = NULL;
    if (EFI_ERROR(uefi_call_wrapper(ST_->BootServices->LocateProtocol, 3, &guid, NULL, (void**)&gop)) || !gop)
        return NULL;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* cur = gop->Mode->Info;
    if (cur->PixelFormat == PixelBlueGreenRedReserved8BitPerColor && cur->HorizontalResolution >= 256)
        return gop;
    UINT32 best = (UINT32)-1, best_px = 0;
    for (UINT32 m = 0; m < gop->Mode->MaxMode; m++) {
        EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* info;
        UINTN size;
        if (EFI_ERROR(uefi_call_wrapper(gop->QueryMode, 4, gop, m, &size, &info))) continue;
        UINT32 px = info->HorizontalResolution * info->VerticalResolution;
        if (info->PixelFormat == PixelBlueGreenRedReserved8BitPerColor && px > best_px) { best = m; best_px = px; }
    }
    if (best == (UINT32)-1) return NULL;
    uefi_call_wrapper(gop->SetMode, 2, gop, best);
    return gop;
}

// Read a whole file from the volume this image was loaded from.
static void* read_file(EFI_HANDLE image, CHAR16* name, UINTN* size) {
    EFI_GUID lip = LOADED_IMAGE_PROTOCOL, sfs = SIMPLE_FILE_SYSTEM_PROTOCOL;
    EFI_LOADED_IMAGE* li;
    EFI_FILE_IO_INTERFACE* fs;
    EFI_FILE_HANDLE root, f;
    if (EFI_ERROR(uefi_call_wrapper(ST_->BootServices->HandleProtocol, 3, image, &lip, (void**)&li))) return NULL;
    if (EFI_ERROR(uefi_call_wrapper(ST_->BootServices->HandleProtocol, 3, li->DeviceHandle, &sfs, (void**)&fs))) return NULL;
    if (EFI_ERROR(uefi_call_wrapper(fs->OpenVolume, 2, fs, &root))) return NULL;
    if (EFI_ERROR(uefi_call_wrapper(root->Open, 5, root, &f, name, EFI_FILE_MODE_READ, 0))) return NULL;
    EFI_FILE_INFO* info = LibFileInfo(f);
    if (!info) return NULL;
    *size = info->FileSize;
    FreePool(info);
    void* buf = alloc_low(*size + 1);
    if (!buf) return NULL;
    UINTN n = *size;
    if (EFI_ERROR(uefi_call_wrapper(f->Read, 3, f, &n, buf)) || n != *size) return NULL;
    uefi_call_wrapper(f->Close, 1, f);
    return buf;
}

EFI_STATUS efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE* st) {
    InitializeLib(image, st);
    ST_ = st;
    uefi_call_wrapper(st->BootServices->SetWatchdogTimer, 4, 0, 0, 0, NULL);

    // Where are we? The ROM sits next to smb.efi.
    EFI_GUID lip = LOADED_IMAGE_PROTOCOL;
    EFI_LOADED_IMAGE* li;
    uefi_call_wrapper(st->BootServices->HandleProtocol, 3, image, &lip, (void**)&li);
    CHAR16* self = DevicePathToStr(li->FilePath);
    static CHAR16 rom_path[512];
    UINTN len = self ? StrLen(self) : 0, cut = 0;
    for (UINTN i = 0; i < len && i < 400; i++) if (self[i] == L'\\' || self[i] == L'/') cut = i + 1;
    for (UINTN i = 0; i < cut; i++) rom_path[i] = self[i] == L'/' ? L'\\' : self[i];
    StrCpy(rom_path + cut, L"smb.nes");

    UINTN rom_size = 0;
    UINT8* rom = read_file(image, rom_path, &rom_size);
    if (!rom) { fail(L"couldn't read smb.nes next to smb.efi"); return EFI_NOT_FOUND; }

    EFI_GRAPHICS_OUTPUT_PROTOCOL* gop = setup_gop();
    if (!gop) { fail(L"no 32-bit graphics mode available"); return EFI_UNSUPPORTED; }

    // Kernel: copy, zero .bss, relocate.
    UINT8* kbase = alloc_low(KERNEL_MEM_SIZE);
    struct MultibootInfo* mbi = alloc_low(4096);
    if (!kbase || !mbi) { fail(L"out of memory below 4 GiB"); return EFI_OUT_OF_RESOURCES; }
    UINTN image_size = kernel_image_end - kernel_image;
    CopyMem(kbase, (void*)kernel_image, image_size);
    SetMem(kbase + image_size, KERNEL_MEM_SIZE - image_size, 0);
    for (Elf64_Rela* r = (Elf64_Rela*)(kbase + KERNEL_RELA_START); r < (Elf64_Rela*)(kbase + KERNEL_RELA_END); r++) {
        if ((r->r_info & 0xFFFFFFFF) != 8) { fail(L"unexpected relocation type in kernel"); return EFI_LOAD_ERROR; }
        *(UINT64*)(kbase + r->r_offset) = (UINT64)(UINTN)kbase + r->r_addend;
    }

    // Multiboot-style boot information, in the same page.
    SetMem(mbi, 4096, 0);
    struct MultibootModule* mod = (struct MultibootModule*)((UINT8*)mbi + 512);
    char* cmdline = (char*)mbi + 1024;
    mod->mod_start = (UINT32)(UINTN)rom;
    mod->mod_end = (UINT32)(UINTN)rom + (UINT32)rom_size;
    UINTN n = 0;                                       // load options -> ASCII command line
    CHAR16* opts = li->LoadOptions;
    for (UINTN i = 0; opts && i < li->LoadOptionsSize / 2 && n < 1000; i++) {
        CHAR16 c = opts[i];
        if (!c) break;
        cmdline[n++] = (c >= 32 && c < 127) ? (char)c : ' ';
    }
    cmdline[n] = 0;
    mbi->flags = (1 << 2) | (1 << 3) | (1 << 12);
    mbi->cmdline = (UINT32)(UINTN)cmdline;
    mbi->mods_count = 1;
    mbi->mods_addr = (UINT32)(UINTN)mod;
    mbi->fb_addr = gop->Mode->FrameBufferBase;
    mbi->fb_width = gop->Mode->Info->HorizontalResolution;
    mbi->fb_height = gop->Mode->Info->VerticalResolution;
    mbi->fb_pitch = gop->Mode->Info->PixelsPerScanLine * 4;
    mbi->fb_bpp = 32;
    mbi->fb_type = 1;

    // Leave boot services (retry once if the memory map changed under us).
    UINTN map_size = 0, key, desc_size;
    UINT32 desc_ver;
    EFI_MEMORY_DESCRIPTOR* map = NULL;
    for (int attempt = 0; attempt < 4; attempt++) {
        map_size = 0;
        uefi_call_wrapper(st->BootServices->GetMemoryMap, 5, &map_size, NULL, &key, &desc_size, &desc_ver);
        map_size += 8 * desc_size;
        if (map) FreePool(map);
        map = AllocatePool(map_size);
        if (EFI_ERROR(uefi_call_wrapper(st->BootServices->GetMemoryMap, 5, &map_size, map, &key, &desc_size, &desc_ver)))
            continue;
        if (!EFI_ERROR(uefi_call_wrapper(st->BootServices->ExitBootServices, 2, image, key)))
            goto exited;
    }
    fail(L"ExitBootServices failed");
    return EFI_LOAD_ERROR;

exited:
    __asm__ volatile("cli");
    ((void (*)(void*))(kbase + KERNEL_ENTRY))(mbi);   // never returns
    for (;;) __asm__ volatile("hlt");
}
