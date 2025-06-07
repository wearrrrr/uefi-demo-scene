#include <stdbool.h>
#include "uefi/uefi.h"
#include "img.h"

int main(int argc, char **argv)
{
    efi_runtime_services_t *RS = ST->RuntimeServices;
    ST->ConOut->ClearScreen(ST->ConOut);
    printf("Hello World!\n");

    efi_guid_t gopGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    efi_gop_t *gop = NULL;

    BS->LocateProtocol(&gopGuid, NULL, (void**)&gop);

    uint32_t *fb = (uint32_t*)gop->Mode->FrameBufferBase;
    uint32_t stride = gop->Mode->Information->PixelsPerScanLine;

    for (uint16_t y = 0; y < OTHER_HOUSE_HEIGHT; y++) {
        for (uint16_t x = 0; x < OTHER_HOUSE_WIDTH; x++) {
            uint32_t image_index = y * OTHER_HOUSE_WIDTH + x;
            uint32_t fb_index = y * stride + x;
            fb[fb_index] = other_house[image_index];
        }
    }

    efi_input_key_t key;
    while (ST->ConIn->ReadKeyStroke(ST->ConIn, &key) != EFI_SUCCESS);
    ST->RuntimeServices->ResetSystem(EfiResetShutdown, 0, 0, u"");
    return EFI_SUCCESS;
}
