#ifndef DESKTOP_H
#define DESKTOP_H

#include <stdint.h>

#define TASKBAR_HEIGHT 40
#define TOPBAR_HEIGHT  0
#define TITLE_BAR_HEIGHT 24
#define DESKTOP_BG_COLOR   0x2d2d2d
#define TASKBAR_BG_COLOR   0x1a1a1a
#define WINDOW_BG_COLOR    0x3c3c3c
#define TITLE_BG_COLOR     0x2b2b2b
#define TEXT_COLOR         0xcccccc
#define TEXT_COLOR_BRIGHT  0xffffff
#define ACCENT_COLOR       0x5294e2

#define MAX_WINDOWS 8

typedef enum {
    WT_TERMINAL,
    WT_ABOUT,
    WT_FILE_VIEWER,
    WT_NOTEPAD,
    WT_SNAKE,
    WT_PONG
} window_type_t;

typedef struct {
    int32_t x, y, w, h;
    int32_t client_x, client_y, client_w, client_h;
    uint8_t visible;
    uint8_t has_close;
    uint8_t drag;
    int32_t drag_off_x, drag_off_y;
    window_type_t type;
    char title[64];
    char content[256];
} window_t;

extern window_t windows[MAX_WINDOWS];

void desktop_init(void);
void desktop_draw(void);
void desktop_tick(void);
void desktop_handle_input(void);
window_t *desktop_create_window(int x, int y, int w, int h, const char *title, window_type_t type);
void desktop_draw_window(window_t *win);
void desktop_draw_taskbar(void);
void desktop_draw_background(void);
int desktop_has_game_overlay(window_t *win);

#endif
