#include <stdbool.h>
#include "uefi/uefi.h"
#include "img.h"

int main(int argc, char **argv)
{
    ST->ConOut->ClearScreen(ST->ConOut);
    efi_runtime_services_t *RS = ST->RuntimeServices;

    efi_guid_t gopGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    efi_gop_t *gop = NULL;

    BS->LocateProtocol(&gopGuid, NULL, (void**)&gop);

    uint32_t *fb = (uint32_t*)gop->Mode->FrameBufferBase;
    uint32_t screen_width = gop->Mode->Information->HorizontalResolution;
    uint32_t screen_height = gop->Mode->Information->VerticalResolution;
    uint32_t stride = gop->Mode->Information->PixelsPerScanLine;
    efi_input_key_t key;

    int32_t x = 0, y = 0, dx = 4, dy = 3;
    int32_t prev_x = 0, prev_y = 0;

    while (true) {
        if (ST->ConIn->ReadKeyStroke(ST->ConIn, &key) == EFI_SUCCESS)
            break;

        uint32_t erase_w = OKAYU_WIDTH;
        uint32_t erase_h = OKAYU_HEIGHT;

        if (prev_x + erase_w > screen_width)
            erase_w = screen_width - prev_x;

        if (prev_y + erase_h > screen_height)
            erase_h = screen_height - prev_y;

        for (uint32_t row = 0; row < erase_h; row++) {
            uint32_t fb_y = prev_y + row;
            if (fb_y >= screen_height) continue;

            uint32_t *row_ptr = fb + fb_y * stride + prev_x;
            memset(row_ptr, 0, erase_w * sizeof(uint32_t));
        }

        for (uint32_t row = 0; row < OKAYU_HEIGHT; row++) {
            int fb_y = y + (int)row;
            if (fb_y < 0 || fb_y >= screen_height)
                continue;

            for (uint32_t col = 0; col < OKAYU_WIDTH; col++) {
                int fb_x = x + (int)col;
                if (fb_x < 0 || fb_x >= screen_width)
                    continue;

                fb[fb_y * stride + fb_x] = okayu[row * OKAYU_WIDTH + col];
            }
        }

        prev_x = x;
        prev_y = y;
        x += dx;
        y += dy;

        if (x <= 0 || x + OKAYU_WIDTH >= screen_width) dx = -dx;
        if (y <= 0 || y + OKAYU_HEIGHT >= screen_height) dy = -dy;

        ST->BootServices->Stall(16667); // about 60fps
    }

    ST->RuntimeServices->ResetSystem(EfiResetShutdown, 0, 0, u"");
    return EFI_SUCCESS;
}
