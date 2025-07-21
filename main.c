#include <stdbool.h>
#include "uefi/uefi.h"
#include "bmp.h"
#include "font8x8.h"


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

uint32_t *fb;
uint32_t fb_width, fb_height;
uint32_t fb_stride;

static inline void draw_pixel(int x, int y, uint32_t color) {
    if (x < 0 || x >= (int)fb_width || y < 0 || y >= (int)fb_height)
        return;
    fb[y * fb_stride + x] = color;
}

void draw_char(int x, int y, char c, uint32_t fg_color, uint32_t bg_color) {
    if ((unsigned char)c >= 128)
        return;

    for (int row = 0; row < 8; row++) {
        uint8_t bits = font8x8_basic[(unsigned char)c][row];
        for (int col = 0; col < 8; col++) {
            uint32_t color = (bits & (1 << col)) ? fg_color : bg_color;
            draw_pixel(x + col, y + row, color);
        }
    }
}

void draw_string(int x, int y, const char *str) {
    while (*str) {
        draw_char(x, y, *str++, 0xFFFFFFFF, 0x00000000);
        x += 8;
    }
}

int main(int argc, char **argv) {
    ST->ConOut->ClearScreen(ST->ConOut);

    efi_runtime_services_t *RS = ST->RuntimeServices;

    efi_guid_t gopGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    efi_gop_t *gop = NULL;

    BS->LocateProtocol(&gopGuid, NULL, (void**)&gop);

    fb = (uint32_t*)gop->Mode->FrameBufferBase;
    fb_width = gop->Mode->Information->HorizontalResolution;
    fb_height = gop->Mode->Information->VerticalResolution;
    fb_stride = gop->Mode->Information->PixelsPerScanLine;
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
    uint32_t *image = malloc(width * height * 3);
    for (int y = 0; y < height; y++) {
        BMP_IMAGE *row = (BMP_IMAGE *)(pixel_data + (height - 1 - y) * row_padded);
        for (int x = 0; x < width; x++) {
            BMP_IMAGE pixel = row[x];
            image[y * width + x] = (pixel.r << 16) | (pixel.g << 8) | pixel.b;
        }
    }
    free(buf);

    #define BLT_DELTA_TIGHTLY_PACKED 0

    efi_gop_pixel_bitmask_t black = {
        .BlueMask = 0,
        .GreenMask = 0,
        .RedMask = 0,
        .ReservedMask = 0
    };

    while (true) {
        if (ST->ConIn->ReadKeyStroke(ST->ConIn, &key) == EFI_SUCCESS)
            break;

        uint64_t start_cycles = __rdtsc();

        int32_t clear_x = prev_x - CLEAR_MARGIN;
        int32_t clear_y = prev_y - CLEAR_MARGIN;
        int32_t clear_w = width + 2 * CLEAR_MARGIN;
        int32_t clear_h = height + 2 * CLEAR_MARGIN;

        if (clear_x < 0) {
            clear_w += clear_x;
            clear_x = 0;
        }
        if (clear_y < 0) {
            clear_h += clear_y;
            clear_y = 0;
        }
        if (clear_x + clear_w > fb_width) {
            clear_w = fb_width - clear_x;
        }
        if (clear_y + clear_h > fb_height) {
            clear_h = fb_height - clear_y;
        }

        gop->Blt(
            gop,
            (uint32_t*)&black,
            EfiBltVideoFill,
            0, 0,
            clear_x, clear_y,
            clear_w, clear_h,
            BLT_DELTA_TIGHTLY_PACKED
        );


        efi_status_t status = gop->Blt(
            gop,
            image,
            EfiBltBufferToVideo,
            0, 0,
            x, y,
            width, height,
            BLT_DELTA_TIGHTLY_PACKED
        );
        prev_x = x;
        prev_y = y;
        x += dx;
        y += dy;

        if (x <= 0 || x + width >= fb_width) dx = -dx;
        if (y <= 0 || y + height >= fb_height) dy = -dy;

        char buf[64];
        uint64_t end_cycles = __rdtsc();
        uint64_t frame_cycles = end_cycles - start_cycles;
        int len = snprintf(buf, sizeof(buf), "Cycles / Frame: %lu", frame_cycles);

        draw_string(10, 10, buf);

        ST->BootServices->Stall(16667); // about 60fps
    }

    // ST->RuntimeServices->ResetSystem(EfiResetShutdown, 0, 0, u"");
    return EFI_SUCCESS;
}
