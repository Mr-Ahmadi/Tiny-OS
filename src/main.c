#include "uefi.h"
#include "framebuffer.h"
#include "input.h"
#include "desktop.h"
#include "fs.h"

static void center_rect(int w, int h, int *x, int *y)
{
    int sw = (int)g_fb.width;
    int sh = (int)g_fb.height - TASKBAR_HEIGHT;
    if (w > sw - 40) w = sw - 40;
    if (h > sh - 20) h = sh - 20;
    *x = (sw - w) / 2;
    *y = (sh - h) / 2;
}

EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    EFI_GUID gop_guid = GUID_GRAPHICS_OUTPUT;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = 0;
    EFI_STATUS s = SystemTable->BootServices->LocateProtocol(&gop_guid, 0, (void**)&gop);
    if (EFI_ERROR(s) || !gop)
        return EFI_UNSUPPORTED;

    gop->SetMode(gop, 0);

    UINT32 fb_w = gop->Mode->Info->HorizontalResolution;
    UINT32 fb_h = gop->Mode->Info->VerticalResolution;
    UINT32 fb_psl = gop->Mode->Info->PixelsPerScanLine;
    UINT32 fb_pitch = fb_psl * 4;

    UINTN fb_size = (UINTN)fb_h * fb_pitch;
    UINTN pages = EFI_SIZE_TO_PAGES(fb_size);
    EFI_PHYSICAL_ADDRESS fb_phys = 0;
    s = SystemTable->BootServices->AllocatePages(AllocateAnyPages, EfiBootServicesData, pages, &fb_phys);
    if (EFI_ERROR(s) || !fb_phys)
        return EFI_OUT_OF_RESOURCES;

    fb_init(gop, fb_w, fb_h, fb_pitch);
    fb_set_buffer((void*)(uintptr_t)fb_phys);
    if (gop->Mode->Info->PixelFormat == PixelRedGreenBlueReserved8BitPerColor)
        fb_pixel_format(0, 8, 16);
    else
        fb_pixel_format(16, 8, 0);
    fb_clear(fb_make_color(35, 45, 60));

    input_init(ImageHandle, SystemTable);
    fs_init(SystemTable);
    desktop_init();
    {
        int ax, ay;
        center_rect(420, 220, &ax, &ay);
        desktop_create_window(ax, ay, 420, 220, "About TinyOS", WT_ABOUT);
        center_rect(500, 360, &ax, &ay);
        desktop_create_window(ax, ay, 500, 360, "Notepad", WT_NOTEPAD);
    }

    while (1) {
        input_poll();
        if (g_input.key_pressed && g_input.last_key == KEY_ESCAPE)
            break;
        desktop_tick();
        desktop_handle_input();
        desktop_draw();
        fb_present();
    }

    SystemTable->BootServices->Stall(500000);
    SystemTable->RuntimeServices->ResetSystem(EfiResetShutdown, EFI_SUCCESS, 0, 0);
    return EFI_SUCCESS;
}
