#include "desktop.h"
#include "framebuffer.h"
#include "input.h"
#include "font.h"
#include "snake.h"
#include "pong.h"
#include "notepad.h"
#include "fs.h"

#define MAX_WINDOWS 8
#define MAX_Z 8

window_t windows[MAX_WINDOWS];
static int num_windows;
static dir_list_t g_file_list;
static int g_file_listed;
static int active_window = -1;
static int drag_window = -1;
static uint8_t z_order[MAX_Z];
static int z_count;
static int start_open;

static uint32_t c(uint8_t r, uint8_t g, uint8_t b) {
    return fb_make_color(r, g, b);
}

static void fit_window(window_t *win)
{
    int max_x = (int)g_fb.width - 40;
    int max_y = (int)g_fb.height - TASKBAR_HEIGHT - 20;
    if (win->x + win->w > max_x) win->x = (max_x - win->w > 0) ? max_x - win->w : 0;
    if (win->y + win->h > max_y) win->y = (max_y - win->h > 0) ? max_y - win->h : 0;
    if (win->x < 0) win->x = 0;
    if (win->y < 0) win->y = 0;
}

static int z_find(int wi) {
    for (int i = 0; i < z_count; i++) if (z_order[i] == wi) return i;
    return -1;
}

static void z_raise(int wi) {
    int idx = z_find(wi);
    if (idx < 0) return;
    for (int i = idx; i < z_count - 1; i++) z_order[i] = z_order[i + 1];
    z_order[z_count - 1] = (uint8_t)wi;
    active_window = wi;
}

static void z_add(int wi) {
    if (z_count < MAX_Z) z_order[z_count++] = (uint8_t)wi;
    active_window = wi;
}

static void z_remove(int wi) {
    int idx = z_find(wi);
    if (idx >= 0) {
        for (int i = idx; i < z_count - 1; i++) z_order[i] = z_order[i + 1];
        z_count--;
    }
    if (active_window == wi) active_window = (z_count > 0) ? z_order[z_count - 1] : -1;
}

void desktop_init(void)
{
    num_windows = 0;
    active_window = -1;
    z_count = 0;
    start_open = 0;
    for (int i = 0; i < MAX_WINDOWS; i++) windows[i].visible = 0;
}

window_t *desktop_create_window(int x, int y, int w, int h,
                                 const char *title, window_type_t type)
{
    if (num_windows >= MAX_WINDOWS) return 0;
    window_t *win = &windows[num_windows];
    win->x = x; win->y = y; win->w = w; win->h = h;
    win->type = type;
    win->visible = 1;
    win->has_close = 1;
    win->drag = 0;
    fit_window(win);
    win->client_x = win->x + 2;
    win->client_y = win->y + TITLE_BAR_HEIGHT + 2;
    win->client_w = win->w - 4;
    win->client_h = win->h - TITLE_BAR_HEIGHT - 4;
    for (int i = 0; i < 63 && title[i]; i++) {
        win->title[i] = title[i];
        win->title[i+1] = 0;
    }
    for (int i = 0; i < 256; i++) win->content[i] = 0;
    z_add(num_windows);
    num_windows++;
    return win;
}

void desktop_draw_background(void)
{
    uint32_t colors[] = {
        c(25, 30, 42), c(30, 38, 50), c(35, 45, 60),
        c(30, 42, 55), c(25, 35, 48)
    };
    int n = sizeof(colors) / sizeof(colors[0]);
    for (uint32_t y = 0; y < g_fb.height; y++) {
        float t = (float)y / g_fb.height;
        int idx = (int)(t * (n - 1));
        float f = t * (n - 1) - idx;
        if (idx >= n - 1) { idx = n - 2; f = 1; }
        uint32_t c1 = colors[idx], c2 = colors[idx + 1];
        uint8_t r = (uint8_t)(((c1 >> 16) & 0xFF) * (1-f) + ((c2 >> 16) & 0xFF) * f);
        uint8_t g = (uint8_t)(((c1 >> 8) & 0xFF) * (1-f) + ((c2 >> 8) & 0xFF) * f);
        uint8_t b = (uint8_t)((c1 & 0xFF) * (1-f) + (c2 & 0xFF) * f);
        uint32_t color = fb_make_color(r, g, b);
        volatile uint32_t *row = (volatile uint32_t *)(g_fb.base + y * g_fb.pitch);
        for (uint32_t x = 0; x < g_fb.width; x++) row[x] = color;
    }
}

static void draw_icon(uint32_t x, uint32_t y, const char *label,
                       uint8_t active, window_type_t type)
{
    uint32_t icon_color;
    switch (type) {
        case WT_ABOUT:  icon_color = c(82, 148, 226); break;
        case WT_TERMINAL: icon_color = c(152, 195, 121); break;
        case WT_NOTEPAD: icon_color = c(230, 200, 120); break;
        case WT_FILE_VIEWER: icon_color = c(200, 170, 110); break;
        case WT_SNAKE:  icon_color = c(72, 210, 72); break;
        case WT_PONG:   icon_color = c(72, 180, 230); break;
        default: icon_color = c(92, 99, 112); break;
    }

    if (active) fb_fill_rect(x, y, 48, 36, c(45, 50, 62));

    switch (type) {
        case WT_ABOUT:
            fb_fill_rect(x + 20, y + 4, 8, 16, icon_color);
            fb_fill_rect(x + 16, y + 20, 16, 4, icon_color);
            break;
        case WT_TERMINAL:
            fb_fill_rect(x + 12, y + 6, 24, 18, c(20, 22, 30));
            fb_draw_string(x + 17, y + 10, ">_", icon_color, c(20, 22, 30));
            break;
        case WT_NOTEPAD:
            fb_fill_rect(x + 14, y + 4, 20, 22, c(60, 55, 42));
            fb_fill_rect(x + 16, y + 6, 16, 2, icon_color);
            fb_fill_rect(x + 16, y + 10, 16, 2, icon_color);
            fb_fill_rect(x + 16, y + 14, 10, 2, icon_color);
            break;
        case WT_FILE_VIEWER:
            fb_fill_rect(x + 14, y + 4, 20, 22, icon_color);
            fb_fill_rect(x + 18, y + 10, 12, 2, c(30, 33, 42));
            fb_fill_rect(x + 18, y + 14, 12, 2, c(30, 33, 42));
            fb_fill_rect(x + 18, y + 18, 8, 2, c(30, 33, 42));
            break;
        case WT_SNAKE:
            fb_fill_rect(x + 16, y + 6, 6, 6, icon_color);
            fb_fill_rect(x + 24, y + 6, 6, 6, c(50, 140, 50));
            fb_fill_rect(x + 20, y + 12, 6, 6, c(45, 120, 45));
            fb_fill_rect(x + 18, y + 18, 14, 4, icon_color);
            fb_pixel(x + 18, y + 6, c(255, 255, 255));
            break;
        case WT_PONG:
            fb_fill_rect(x + 12, y + 6, 4, 18, icon_color);
            fb_fill_rect(x + 32, y + 6, 4, 18, c(230, 80, 80));
            fb_fill_rect(x + 22, y + 12, 4, 4, c(240, 240, 245));
            break;
        default:
            fb_fill_rect(x + 20, y + 4, 8, 16, icon_color);
            fb_fill_rect(x + 16, y + 20, 16, 4, icon_color);
            break;
    }

    int lw = 0;
    for (const char *p = label; *p; p++) lw++;
    int tx = x + (48 - lw * FONT_WIDTH) / 2;
    fb_draw_string(tx, y + 24, label, c(171, 178, 191), active ? c(45, 50, 62) : 0);
}

static void draw_start_menu(void)
{
    struct { const char *name; window_type_t type; } apps[] = {
        {"About TinyOS", WT_ABOUT},
        {"Terminal", WT_TERMINAL},
        {"Notepad", WT_NOTEPAD},
        {"File Viewer", WT_FILE_VIEWER},
        {"Snake", WT_SNAKE},
        {"Pong", WT_PONG}
    };
    int napps = sizeof(apps) / sizeof(apps[0]);
    int mx = 0, my = g_fb.height - TASKBAR_HEIGHT - 36 * napps - 4;
    int mw = 160, mh = 36 * napps + 4;
    fb_fill_rect(mx, my, mw, mh, c(30, 33, 42));
    fb_fill_rect(mx + 1, my + 1, mw - 2, mh - 2, c(38, 42, 52));
    for (int i = 0; i < napps; i++) {
        int iy = my + 4 + i * 36;
        uint32_t hc = c(45, 50, 62);
        int mx_cur = g_input.mouse_x;
        int my_cur = g_input.mouse_y;
        int hover = (mx_cur >= 2 && mx_cur < mw - 2 && my_cur >= iy && my_cur < iy + 36);
        if (hover) fb_fill_rect(2, iy, mw - 4, 36, hc);
        draw_icon(6, iy + 2, apps[i].name, hover, apps[i].type);
    }
}

static void draw_window_title(window_t *win, int is_active)
{
    uint32_t tb = is_active ? c(38, 44, 58) : c(32, 36, 48);
    fb_fill_rect(win->x, win->y, win->w, TITLE_BAR_HEIGHT, tb);

    fb_draw_string(win->x + 8, win->y + 4, win->title,
                   is_active ? c(220, 222, 228) : c(140, 145, 155), tb);

    if (win->has_close) {
        int bx = win->x + win->w - 24;
        int by = win->y + 4;
        int bz = 16;
        fb_fill_rect(bx, by, bz, bz, c(60, 40, 40));
        fb_fill_rect(bx + 1, by + 1, bz - 2, bz - 2, c(80, 50, 50));
        uint32_t xc = c(220, 100, 100);
        int cx = bx + 4, cy = by + 4;
        fb_fill_rect(cx, cy, 1, 8, xc);
        fb_fill_rect(cx + 7, cy, 1, 8, xc);
        fb_fill_rect(cx, cy, 8, 1, xc);
        fb_fill_rect(cx, cy + 7, 8, 1, xc);
        fb_pixel(cx + 1, cy + 1, xc);
        fb_pixel(cx + 6, cy + 1, xc);
        fb_pixel(cx + 1, cy + 6, xc);
        fb_pixel(cx + 6, cy + 6, xc);
    }
}

void desktop_draw_window(window_t *win)
{
    if (!win->visible) return;
    int is_active = (active_window >= 0 && &windows[active_window] == win);

    fb_fill_rect(win->x, win->y, win->w, win->h, c(50, 54, 65));
    fb_fill_rect(win->x + 1, win->y + TITLE_BAR_HEIGHT + 1,
                 win->w - 2, win->h - TITLE_BAR_HEIGHT - 2, c(42, 46, 56));

    draw_window_title(win, is_active);

    switch (win->type) {
        case WT_ABOUT: {
            int cx = win->x + 20, cy = win->y + TITLE_BAR_HEIGHT + 16;
            fb_draw_string(cx, cy, "TinyOS v0.3", c(171, 178, 191), c(42, 46, 56));
            fb_draw_string(cx, cy + 20, "A hobby OS for ARM64 UTM", c(130, 140, 155), c(42, 46, 56));
            fb_draw_string(cx, cy + 40, "Games: Snake + Pong", c(130, 140, 155), c(42, 46, 56));
            fb_draw_string(cx, cy + 60, "1-5 or F1-F5 to launch apps", c(100, 110, 125), c(42, 46, 56));
            fb_draw_string(cx, cy + 80, "Arrow keys move mouse", c(100, 110, 125), c(42, 46, 56));
            fb_draw_string(cx, cy + 100, "Space/Enter = click  ESC = quit", c(100, 110, 125), c(42, 46, 56));
            break;
        }
        case WT_TERMINAL: {
            int cx = win->client_x + 8, cy = win->client_y + 8;
            fb_draw_string(cx, cy, "TinyOS Terminal", c(82, 148, 226), c(42, 46, 56));
            fb_draw_string(cx, cy + 20, "> ", c(152, 195, 121), c(42, 46, 56));
            if (win->content[0]) {
                char buf[72]; int len = 0;
                for (int i = 0; win->content[i] && i < 70; i++) len++;
                for (int i = 0; i < len; i++) buf[i] = win->content[i];
                buf[len] = 0;
                fb_draw_string(cx + 16, cy + 20, buf, c(152, 195, 121), c(42, 46, 56));
            }
            fb_draw_string(cx, cy + 42, "1:About 2:Term 3:Files",
                           c(100, 110, 125), c(42, 46, 56));
            fb_draw_string(cx, cy + 58, "4:Snake 5:Pong",
                           c(100, 110, 125), c(42, 46, 56));
            break;
        }
        case WT_NOTEPAD: {
            notepad_draw(win, is_active);
            break;
        }
        case WT_FILE_VIEWER: {
            int cx = win->client_x + 8, cy = win->client_y + 8;
            int cw = win->client_w, ch = win->client_h;
            fb_draw_string(cx, cy, "File Viewer", c(200, 170, 110), c(42, 46, 56));
            if (!g_file_listed) {
                g_file_listed = 1;
                fs_list_root(&g_file_list);
            }
            int yy = cy + 22;
            for (int i = 0; i < g_file_list.count && i < 12; i++) {
                fb_draw_string(cx + 8, yy, g_file_list.names[i], c(171, 178, 191), c(42, 46, 56));
                yy += FONT_HEIGHT + 2;
            }
            if (g_file_list.count <= 0) {
                fb_draw_string(cx + 8, yy, "(no files found)", c(100, 110, 125), c(42, 46, 56));
                yy += FONT_HEIGHT + 2;
            }
            fb_draw_string(cx + 8, yy + 8, "Click a file to open in Notepad", c(100, 110, 125), c(42, 46, 56));
            break;
        }
        case WT_SNAKE:
            snake_draw(win, is_active);
            break;
        case WT_PONG:
            pong_draw(win, is_active);
            break;
    }
}

void desktop_draw_taskbar(void)
{
    uint32_t bar_color = c(24, 27, 35);
    fb_fill_rect(0, g_fb.height - TASKBAR_HEIGHT,
                 g_fb.width, TASKBAR_HEIGHT, bar_color);
    fb_fill_rect(0, g_fb.height - TASKBAR_HEIGHT - 1,
                 g_fb.width, 1, c(45, 50, 62));

    draw_icon(8,   g_fb.height - TASKBAR_HEIGHT + 2, "App",   0, WT_ABOUT);
    draw_icon(64,  g_fb.height - TASKBAR_HEIGHT + 2, "Term",  0, WT_TERMINAL);
    draw_icon(120, g_fb.height - TASKBAR_HEIGHT + 2, "Note",  0, WT_NOTEPAD);
    draw_icon(176, g_fb.height - TASKBAR_HEIGHT + 2, "Files", 0, WT_FILE_VIEWER);
    draw_icon(232, g_fb.height - TASKBAR_HEIGHT + 2, "Snake", 0, WT_SNAKE);
    draw_icon(288, g_fb.height - TASKBAR_HEIGHT + 2, "Pong",  0, WT_PONG);

    int x = g_fb.width - 130, y = g_fb.height - TASKBAR_HEIGHT + 4;
    fb_draw_string(x, y, "TinyOS v0.3", c(120, 128, 142), c(24, 27, 35));

    char res[24]; int pos = 0;
    uint32_t tmp = g_fb.width;
    char rev[8]; int ri = 0;
    do { rev[ri++] = '0' + tmp % 10; tmp /= 10; } while (tmp > 0);
    for (int i = ri-1; i >= 0; i--) res[pos++] = rev[i];
    res[pos++] = 'x';
    tmp = g_fb.height; ri = 0;
    do { rev[ri++] = '0' + tmp % 10; tmp /= 10; } while (tmp > 0);
    for (int i = ri-1; i >= 0; i--) res[pos++] = rev[i];
    res[pos] = 0;
    fb_draw_string(x, y + 16, res, c(90, 98, 112), c(24, 27, 35));

    if (z_count > 0) {
        int tw = 0;
        char list[64]; int lp = 0;
        for (int i = 0; i < z_count && lp < 60; i++) {
            int wi = z_order[i];
            const char *t = windows[wi].title;
            list[lp++] = (wi == active_window) ? '*' : ' ';
            while (*t && lp < 60) list[lp++] = *(t++);
            if (i < z_count - 1 && lp < 60) list[lp++] = ' ';
        }
        list[lp] = 0;
        fb_draw_string(344, g_fb.height - TASKBAR_HEIGHT + 6, list,
                       c(100, 108, 122), c(24, 27, 35));
    }
}

static void draw_arrow_cursor(void)
{
    int mx = g_input.mouse_x;
    int my = g_input.mouse_y;
    if (mx < 0 || mx >= (int)g_fb.width || my < 0 || my >= (int)g_fb.height)
        return;

    static const int8_t arrow[12][2] = {
        {0,0},{1,0},{2,0},{3,0},{4,0},{5,0},{6,0},{7,0},
        {0,1},{1,1},{2,1},{3,1}
    };
    static const int8_t shaft[16][2] = {
        {0,2},{1,2},{2,2},{3,2},{0,3},{1,3},{2,3},{3,3},
        {0,4},{1,4},{2,4},{3,4},{0,5},{1,5},{2,5},{3,5}
    };

    for (int i = 0; i < 12; i++) {
        int px = mx + arrow[i][0];
        int py = my + arrow[i][1];
        if (px >= 0 && px < (int)g_fb.width && py >= 0 && py < (int)g_fb.height)
            fb_pixel(px, py, 0xFFFFFF);
    }
    for (int i = 0; i < 16; i++) {
        int px = mx + shaft[i][0];
        int py = my + shaft[i][1];
        if (px >= 0 && px < (int)g_fb.width && py >= 0 && py < (int)g_fb.height)
            fb_pixel(px, py, 0xFFFFFF);
    }
    fb_pixel(mx, my, 0x000000);
    fb_pixel(mx + 1, my + 1, 0x000000);
    fb_pixel(mx, my + 1, 0x000000);
    fb_pixel(mx + 1, my, 0x000000);
}

void desktop_draw(void)
{
    desktop_draw_background();
    for (int i = 0; i < z_count; i++) {
        int wi = z_order[i];
        if (windows[wi].visible) desktop_draw_window(&windows[wi]);
    }
    desktop_draw_taskbar();
    if (start_open) draw_start_menu();
    draw_arrow_cursor();
}

void desktop_tick(void)
{
    for (int i = 0; i < num_windows; i++) {
        if (!windows[i].visible) continue;
        switch (windows[i].type) {
            case WT_SNAKE: snake_update(&windows[i]); break;
            case WT_PONG:  pong_update(&windows[i]); break;
            case WT_NOTEPAD: notepad_tick(&windows[i]); break;
            default: break;
        }
    }
}

static void get_app_rect(window_type_t type, int *x, int *y, int *w, int *h)
{
    int sw = (int)g_fb.width;
    int sh = (int)g_fb.height - TASKBAR_HEIGHT;
    switch (type) {
        case WT_ABOUT:       *w = 400; *h = 220; break;
        case WT_TERMINAL:    *w = 500; *h = 280; break;
        case WT_NOTEPAD:     *w = 500; *h = 360; break;
        case WT_FILE_VIEWER: *w = 380; *h = 300; break;
        case WT_SNAKE:       *w = 540; *h = 440; break;
        case WT_PONG:        *w = 560; *h = 400; break;
        default:             *w = 400; *h = 220; break;
    }
    if (*w > sw - 40) *w = sw - 40;
    if (*h > sh - 20) *h = sh - 20;
    *x = (sw - *w) / 2;
    *y = (sh - *h) / 2;
}

static window_t *find_or_create(window_type_t type, int x, int y, int w, int h,
                                  const char *title)
{
    for (int i = 0; i < num_windows; i++) {
        if (windows[i].type == type) {
            if (!windows[i].visible) {
                windows[i].visible = 1;
                z_add(i);
                switch (type) {
                    case WT_SNAKE: snake_init(&windows[i]); break;
                    case WT_PONG:  pong_init(&windows[i]); break;
                    default: break;
                }
            } else {
                z_raise(i);
            }
            return &windows[i];
        }
    }
    window_t *win = desktop_create_window(x, y, w, h, title, type);
    if (win) {
        switch (type) {
            case WT_SNAKE: snake_init(win); break;
            case WT_PONG:  pong_init(win); break;
            case WT_NOTEPAD: notepad_init(win); break;
            default: break;
        }
    }
    return win;
}

static void check_close_click(window_t *win, int mx, int my)
{
    if (!win->has_close || !win->visible) return;
    int bx = win->x + win->w - 24;
    int by = win->y + 4;
    if (mx >= bx && mx < bx + 16 && my >= by && my < by + 16) {
        win->visible = 0;
        z_remove((int)(win - windows));
    }
}

static void check_window_drag(int mx, int my)
{
    for (int i = z_count - 1; i >= 0; i--) {
        int wi = z_order[i];
        if (wi < 0 || wi >= num_windows) continue;
        window_t *win = &windows[wi];
        if (!win->visible) continue;
        if (mx >= win->x && mx < win->x + win->w &&
            my >= win->y && my < win->y + TITLE_BAR_HEIGHT)
        {
            check_close_click(win, mx, my);
            if (win->visible) {
                z_raise(wi);
                drag_window = wi;
                win->drag = 1;
                win->drag_off_x = mx - win->x;
                win->drag_off_y = my - win->y;
            }
            return;
        }
    }
}

static void check_game_click(int mx, int my)
{
    for (int i = z_count - 1; i >= 0; i--) {
        int wi = z_order[i];
        if (wi < 0 || wi >= num_windows) continue;
        window_t *win = &windows[wi];
        if (!win->visible) continue;
        if (win->type != WT_SNAKE && win->type != WT_PONG) continue;
        if (mx >= win->client_x && mx < win->client_x + win->client_w &&
            my >= win->client_y && my < win->client_y + win->client_h)
        {
            z_raise(wi);
            if (win->type == WT_SNAKE) {
                snake_state_t *ss = (snake_state_t *)win->content;
                if (ss->snake_len > 0 && (ss->flags & 3))
                    snake_handle_key(win, ' ');
            }
            if (win->type == WT_PONG) pong_handle_click(win);
            return;
        }
    }
}

static void check_taskbar_click(int mx, int my)
{
    int ty = g_fb.height - TASKBAR_HEIGHT;
    if (my < ty || my >= (int)g_fb.height) return;

    int idx = -1;
    if (mx >= 8 && mx < 56) idx = 0;
    else if (mx >= 64 && mx < 112) idx = 1;
    else if (mx >= 120 && mx < 168) idx = 2;
    else if (mx >= 176 && mx < 224) idx = 3;
    else if (mx >= 232 && mx < 280) idx = 4;
    else if (mx >= 288 && mx < 336) idx = 5;

    if (idx >= 0) {
        window_type_t types[] = {WT_ABOUT, WT_TERMINAL, WT_NOTEPAD, WT_FILE_VIEWER, WT_SNAKE, WT_PONG};
        const char *titles[] = {"About TinyOS", "Terminal", "Notepad", "File Viewer", "Snake", "Pong"};
        int ax, ay, aw, ah;
        get_app_rect(types[idx], &ax, &ay, &aw, &ah);
        find_or_create(types[idx], ax, ay, aw, ah, titles[idx]);
        return;
    }
    if (mx >= 0 && mx < 60) start_open = !start_open;
    else start_open = 0;
}

static void check_start_menu_click(int mx, int my)
{
    if (!start_open) return;
    window_type_t types[] = {WT_ABOUT, WT_TERMINAL, WT_NOTEPAD, WT_FILE_VIEWER, WT_SNAKE, WT_PONG};
    const char *titles[] = {"About TinyOS", "Terminal", "Notepad", "File Viewer", "Snake", "Pong"};
    int napps = sizeof(types) / sizeof(types[0]);
    int my0 = g_fb.height - TASKBAR_HEIGHT - 36 * napps - 4;
    if (mx < 2 || mx >= 158 || my < my0 + 2 || my >= my0 + 2 + 36 * napps) {
        start_open = 0;
        return;
    }
    int idx = (my - my0 - 2) / 36;
    if (idx >= 0 && idx < napps) {
        int ax, ay, aw, ah;
        get_app_rect(types[idx], &ax, &ay, &aw, &ah);
        find_or_create(types[idx], ax, ay, aw, ah, titles[idx]);
        start_open = 0;
    }
}

static void check_file_viewer_click(int mx, int my)
{
    for (int i = z_count - 1; i >= 0; i--) {
        int wi = z_order[i];
        if (wi < 0 || wi >= num_windows) continue;
        window_t *win = &windows[wi];
        if (!win->visible || win->type != WT_FILE_VIEWER) continue;
        if (mx < win->client_x || mx >= win->client_x + win->client_w) continue;
        if (my < win->client_y || my >= win->client_y + win->client_h) continue;
        z_raise(wi);

        int cy = win->client_y + 22;
        if (my < cy) return;
        int idx = (my - cy) / (FONT_HEIGHT + 2);
        if (idx < 0 || idx >= g_file_list.count) return;

        int ax, ay, aw, ah;
        get_app_rect(WT_NOTEPAD, &ax, &ay, &aw, &ah);
        window_t *np = find_or_create(WT_NOTEPAD, ax, ay, aw, ah, "Notepad");
        if (np) {
            notepad_load_file(np, g_file_list.names[idx]);
        }
        return;
    }
}

void desktop_handle_input(void)
{
    if (g_input.mouse_buttons & 1) {
        if (g_input.mouse_dx || g_input.mouse_dy) {
            if (drag_window >= 0) {
                window_t *win = &windows[drag_window];
                win->x = g_input.mouse_x - win->drag_off_x;
                win->y = g_input.mouse_y - win->drag_off_y;
                fit_window(win);
                win->client_x = win->x + 2;
                win->client_y = win->y + TITLE_BAR_HEIGHT + 2;
                win->client_w = win->w - 4;
                win->client_h = win->h - TITLE_BAR_HEIGHT - 4;
            }
        }
    } else {
        drag_window = -1;
        for (int i = 0; i < num_windows; i++) windows[i].drag = 0;
    }

    {
        static int prev_btn = 0;
        if (g_input.mouse_buttons & 1) {
            if (!prev_btn) {
                check_start_menu_click(g_input.mouse_x, g_input.mouse_y);
                check_window_drag(g_input.mouse_x, g_input.mouse_y);
                if (drag_window < 0)
                    check_game_click(g_input.mouse_x, g_input.mouse_y);
                if (drag_window < 0)
                    check_file_viewer_click(g_input.mouse_x, g_input.mouse_y);
                if (drag_window < 0)
                    check_taskbar_click(g_input.mouse_x, g_input.mouse_y);
            }
            prev_btn = 1;
        } else {
            prev_btn = 0;
        }
    }

    if (g_input.key_pressed) {
        uint16_t uc = g_input.last_unicode;
        if (uc == 0x11 || uc == '1' || g_input.last_key == KEY_F1) {
            int ax, ay, aw, ah; get_app_rect(WT_ABOUT, &ax, &ay, &aw, &ah);
            find_or_create(WT_ABOUT, ax, ay, aw, ah, "About TinyOS");
        } else if (uc == 0x12 || uc == '2' || g_input.last_key == KEY_F2) {
            int ax, ay, aw, ah; get_app_rect(WT_TERMINAL, &ax, &ay, &aw, &ah);
            find_or_create(WT_TERMINAL, ax, ay, aw, ah, "Terminal");
        } else if (uc == 0x13 || uc == '3' || g_input.last_key == KEY_F3) {
            int ax, ay, aw, ah; get_app_rect(WT_NOTEPAD, &ax, &ay, &aw, &ah);
            find_or_create(WT_NOTEPAD, ax, ay, aw, ah, "Notepad");
        } else if (uc == 0x14 || uc == '4' || g_input.last_key == KEY_F4) {
            int ax, ay, aw, ah; get_app_rect(WT_FILE_VIEWER, &ax, &ay, &aw, &ah);
            find_or_create(WT_FILE_VIEWER, ax, ay, aw, ah, "File Viewer");
        } else if (uc == 0x15 || uc == '5' || g_input.last_key == KEY_F5) {
            int ax, ay, aw, ah; get_app_rect(WT_SNAKE, &ax, &ay, &aw, &ah);
            window_t *w = find_or_create(WT_SNAKE, ax, ay, aw, ah, "Snake");
            if (w && w->type == WT_SNAKE) snake_handle_key(w, g_input.last_key);
        } else if (uc == 0x16 || uc == '6' || g_input.last_key == KEY_F6) {
            int ax, ay, aw, ah; get_app_rect(WT_PONG, &ax, &ay, &aw, &ah);
            find_or_create(WT_PONG, ax, ay, aw, ah, "Pong");
        }

        if (active_window >= 0 && active_window < num_windows) {
            window_t *aw = &windows[active_window];
            if (aw->type == WT_TERMINAL && aw->visible) {
                if (g_input.last_unicode >= 0x20 && g_input.last_unicode <= 0x7E) {
                    int len = 0;
                    while (aw->content[len] && len < 254) len++;
                    if (len < 254) {
                        aw->content[len] = (char)g_input.last_unicode;
                        aw->content[len+1] = 0;
                    }
                } else if (g_input.last_key == KEY_BACKSPACE || g_input.last_unicode == 0x08) {
                    int len = 0;
                    while (aw->content[len] && len < 255) len++;
                    if (len > 0) aw->content[len-1] = 0;
                } else if (g_input.last_key == KEY_RETURN || g_input.last_unicode == 0x0D) {
                    int len = 0;
                    while (aw->content[len] && len < 254) len++;
                    if (len + 2 < 255) { aw->content[len] = '\n'; aw->content[len+1] = 0; }
                }
            } else if (aw->type == WT_NOTEPAD && aw->visible) {
                notepad_key(aw, g_input.last_unicode, g_input.last_key);
            } else if (aw->type == WT_SNAKE && aw->visible) {
                snake_handle_key(aw, g_input.last_key);
            }
        }
        g_input.key_pressed = 0;
    }
}
