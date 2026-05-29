#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <stdint.h>
#include "uefi.h"

typedef struct {
    volatile void *base;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bpp;
    uint8_t  red_pos;
    uint8_t  green_pos;
    uint8_t  blue_pos;
    void *gop;
    void *back_buffer;
} framebuffer_t;

extern framebuffer_t g_fb;

void fb_init(void *gop, uint32_t width, uint32_t height, uint32_t pitch);
void fb_set_buffer(void *buf);
uint32_t fb_make_color(uint8_t r, uint8_t g, uint8_t b);
void fb_pixel(uint32_t x, uint32_t y, uint32_t color);
void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void fb_draw_char(uint32_t x, uint32_t y, unsigned char c, uint32_t fg, uint32_t bg);
void fb_draw_string(uint32_t x, uint32_t y, const char *str, uint32_t fg, uint32_t bg);
void fb_clear(uint32_t color);
void fb_present(void);
void fb_pixel_format(uint8_t r, uint8_t g, uint8_t b);
uint32_t fb_width(void);
uint32_t fb_height(void);

#endif
