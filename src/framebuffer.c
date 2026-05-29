#include "framebuffer.h"
#include "font.h"

framebuffer_t g_fb;

void fb_init(void *gop, uint32_t width, uint32_t height, uint32_t pitch)
{
    g_fb.gop     = gop;
    g_fb.width   = width;
    g_fb.height  = height;
    g_fb.pitch   = pitch;
    g_fb.bpp     = 32;
    g_fb.red_pos   = 16;
    g_fb.green_pos = 8;
    g_fb.blue_pos  = 0;
}

void fb_set_buffer(void *buf)
{
    g_fb.base = buf;
    g_fb.back_buffer = buf;
}

uint32_t fb_make_color(uint8_t r, uint8_t g, uint8_t b)
{
    return ((uint32_t)r << g_fb.red_pos) |
           ((uint32_t)g << g_fb.green_pos) |
           ((uint32_t)b << g_fb.blue_pos);
}

void fb_pixel(uint32_t x, uint32_t y, uint32_t color)
{
    if (x >= g_fb.width || y >= g_fb.height) return;
    volatile uint32_t *ptr = (volatile uint32_t *)(g_fb.base + y * g_fb.pitch);
    ptr[x] = color;
}

void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color)
{
    if (x >= g_fb.width || y >= g_fb.height) return;
    if (x + w > g_fb.width)  w = g_fb.width - x;
    if (y + h > g_fb.height) h = g_fb.height - y;

    for (uint32_t row = 0; row < h; row++) {
        volatile uint32_t *ptr = (volatile uint32_t *)(g_fb.base + (y + row) * g_fb.pitch);
        for (uint32_t col = 0; col < w; col++) {
            ptr[x + col] = color;
        }
    }
}

uint32_t fb_width(void) { return g_fb.width; }
uint32_t fb_height(void) { return g_fb.height; }

void fb_pixel_format(uint8_t r, uint8_t g, uint8_t b)
{
    g_fb.red_pos = r;
    g_fb.green_pos = g;
    g_fb.blue_pos = b;
}

void fb_clear(uint32_t color)
{
    for (uint32_t y = 0; y < g_fb.height; y++) {
        volatile uint32_t *ptr = (volatile uint32_t *)(g_fb.base + y * g_fb.pitch);
        for (uint32_t x = 0; x < g_fb.width; x++) {
            ptr[x] = color;
        }
    }
}

void fb_draw_char(uint32_t x, uint32_t y, unsigned char c, uint32_t fg, uint32_t bg)
{
    if (x + FONT_WIDTH > g_fb.width || y + FONT_HEIGHT > g_fb.height) return;
    const uint8_t *glyph = font_get(c);
    if (!glyph) return;

    for (int row = 0; row < FONT_HEIGHT; row++) {
        volatile uint32_t *ptr = (volatile uint32_t *)(g_fb.base + (y + row) * g_fb.pitch);
        uint8_t bits = glyph[row];
        for (int col = 0; col < FONT_WIDTH; col++) {
            if (x + col >= g_fb.width) break;
            ptr[x + col] = (bits & (0x80 >> col)) ? fg : bg;
        }
    }
}

void fb_draw_string(uint32_t x, uint32_t y, const char *str, uint32_t fg, uint32_t bg)
{
    while (*str) {
        fb_draw_char(x, y, (unsigned char)*str, fg, bg);
        x += FONT_WIDTH;
        if (x + FONT_WIDTH > g_fb.width) break;
        str++;
    }
}

void fb_present(void)
{
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = (EFI_GRAPHICS_OUTPUT_PROTOCOL *)g_fb.gop;
    if (!gop || !gop->Blt) return;

    gop->Blt(gop, (EFI_GRAPHICS_OUTPUT_BLT_PIXEL *)g_fb.back_buffer,
             BltBufferToVideo, 0, 0, 0, 0,
             g_fb.width, g_fb.height, 0);
}
