#include <stdbool.h>
#include "uefi/uefi.h"
#include "bmp.h"

#define CLEAR_MARGIN 2

char *read_esp_file_to_buf(const char *file_path, size_t *out_size) {
    FILE *file = fopen(file_path, "r");
    if (!file) {
        return NULL;
    }
    long int size;
    char *buf;

    fseek(file, 0, SEEK_END);
    size = ftell(file);
    fseek(file, 0, SEEK_SET);
    buf = malloc(size + 1);
    if(!buf) {
        printf("unable to allocate memory!\n");
        return NULL;
    }
    fread(buf, size, 1, file);
    buf[size] = 0;
    fclose(file);
    *out_size = size;
    return buf;
};

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

    size_t size;

    BMP_FILEHEADER *bmp_fh;
    BMP_INFOHEADER *bmp_ih;
    bmp_fh = malloc(sizeof(BMP_FILEHEADER));
    bmp_ih = malloc(sizeof(BMP_INFOHEADER));

    char *buf = read_esp_file_to_buf("\\EFI\\BOOT\\okayu.bmp", &size);
    if (!buf) {
        printf("Failed to load image! Press any key to shutdown...");
        while (ST->ConIn->ReadKeyStroke(ST->ConIn, &key) != EFI_SUCCESS);
        ST->RuntimeServices->ResetSystem(EfiResetShutdown, 0, 0, u"");
    }
    memcpy(bmp_fh, buf, sizeof(BMP_FILEHEADER));
    memcpy(bmp_ih, buf + sizeof(BMP_FILEHEADER), sizeof(BMP_INFOHEADER));

    if (bmp_ih->bitPix != 24) {
        printf("Invalid bit-depth for image! Expected 24, Got %d.\nPress any key to shutdown...", bmp_ih->bitPix);
        while (ST->ConIn->ReadKeyStroke(ST->ConIn, &key) != EFI_SUCCESS);
        ST->RuntimeServices->ResetSystem(EfiResetShutdown, 0, 0, u"");
    }

    int width = bmp_ih->width;
    int height = bmp_ih->height;
    int row_padded = (width * 3 + 3) & (~3);
    uint8_t *pixel_data = (uint8_t*)(buf + bmp_fh->imageDataOffset);
    int *image = malloc(width * height * 3);
    for (int y = 0; y < height; y++) {
        BMP_IMAGE *row = (BMP_IMAGE *)(pixel_data + (height - 1 - y) * row_padded);
        for (int x = 0; x < width; x++) {
            BMP_IMAGE pixel = row[x];
            image[y * width + x] = (pixel.r << 16) | (pixel.g << 8) | pixel.b;
        }
    }
    free(buf);

    while (true) {
        if (ST->ConIn->ReadKeyStroke(ST->ConIn, &key) == EFI_SUCCESS)
            break;

        int32_t clear_x = prev_x - CLEAR_MARGIN;
        int32_t clear_y = prev_y - CLEAR_MARGIN;
        int32_t clear_w = width + 2 * CLEAR_MARGIN;
        int32_t clear_h = height + 2 * CLEAR_MARGIN;

        for (int32_t row = 0; row < clear_h; row++) {
            int32_t fb_y = clear_y + row;
            if (fb_y < 0 || fb_y >= screen_height)
                continue;

            int32_t fb_x = clear_x;
            int32_t clamped_w = clear_w;

            if (fb_x < 0) {
                clamped_w += fb_x;
                fb_x = 0;
            }
            if (fb_x >= screen_width || clamped_w <= 0) continue;

            uint32_t *row_ptr = fb + fb_y * stride + fb_x;
            for (int32_t col = 0; col < clamped_w; col++) {
                row_ptr[col] = 0;
            }
        }

        for (uint32_t row = 0; row < height; row++) {
            int fb_y = y + (int)row;
            if (fb_y < 0 || fb_y >= (int)screen_height)
                continue;

            for (uint32_t col = 0; col < width; col++) {
                int fb_x = x + (int)col;
                if (fb_x < 0 || fb_x >= (int)screen_width)
                    continue;

                fb[fb_y * stride + fb_x] = image[row * width + col];
            }
        }
        prev_x = x;
        prev_y = y;
        x += dx;
        y += dy;

        if (x <= 0 || x + width >= screen_width) dx = -dx;
        if (y <= 0 || y + height >= screen_height) dy = -dy;

        ST->BootServices->Stall(16667); // about 60fps
    }

    // ST->RuntimeServices->ResetSystem(EfiResetShutdown, 0, 0, u"");
    return EFI_SUCCESS;
}
