#include "pong.h"
#include "framebuffer.h"
#include "font.h"
#include "input.h"

typedef struct {
    int left_y;
    int right_y;
    int ball_x;
    int ball_y;
    int ball_dx;
    int ball_dy;
    int left_score;
    int right_score;
    int running;
    int serve_pause;
    int move_wait;
} __attribute__((packed)) pong_state_t;

static pong_state_t *S(window_t *w) { return (pong_state_t *)w->content; }

static uint32_t c(uint8_t r, uint8_t g, uint8_t b) {
    return fb_make_color(r, g, b);
}

void pong_init(window_t *win)
{
    pong_state_t *p = S(win);
    p->left_y = 150;
    p->right_y = 150;
    p->ball_x = 300;
    p->ball_y = 180;
    p->ball_dx = -2;
    p->ball_dy = 1;
    p->left_score = 0;
    p->right_score = 0;
    p->running = 1;
    p->serve_pause = 0;
    p->move_wait = 0;
}

void pong_update(window_t *win)
{
    pong_state_t *p = S(win);
    if (!p->running) return;

    if (p->serve_pause > 0) {
        p->serve_pause--;
        return;
    }

    p->move_wait++;
    if (p->move_wait < 6) return;
    p->move_wait = 0;

    int cw = win->client_w, ch = win->client_h;
    int paddle_h = ch / 4;
    if (paddle_h < 40) paddle_h = 40;
    if (paddle_h > 100) paddle_h = 100;
    int paddle_w = 8;

    int cx = win->client_x, cy = win->client_y;

    p->ball_x += p->ball_dx;
    p->ball_y += p->ball_dy;

    if (p->ball_y <= 0) { p->ball_y = 0; p->ball_dy = -p->ball_dy; }
    if (p->ball_y + 8 >= ch) { p->ball_y = ch - 8; p->ball_dy = -p->ball_dy; }

    if (p->ball_x <= paddle_w) {
        if (p->ball_y + 8 >= p->left_y && p->ball_y <= p->left_y + paddle_h) {
            p->ball_x = paddle_w + 1;
            p->ball_dx = -p->ball_dx;
            int hit_pos = (p->ball_y + 4 - p->left_y) - paddle_h / 2;
            p->ball_dy = hit_pos / 10;
            if (p->ball_dy > 5) p->ball_dy = 5;
            if (p->ball_dy < -5) p->ball_dy = -5;
            if (p->ball_dy == 0) p->ball_dy = (p->ball_x % 2) ? 1 : -1;
        } else {
            p->right_score++;
            p->ball_x = cw / 2;
            p->ball_y = ch / 2;
            int seed = p->right_score * 17 + p->left_score * 31;
            p->ball_dx = (seed % 2) ? -3 : 3;
            p->ball_dy = ((seed / 2) % 5) - 2;
            if (p->ball_dy == 0) p->ball_dy = 1;
            p->serve_pause = 30;
        }
    }

    if (p->ball_x + 8 >= cw - paddle_w) {
        if (p->ball_y + 8 >= p->right_y && p->ball_y <= p->right_y + paddle_h) {
            p->ball_x = cw - paddle_w - 9;
            p->ball_dx = -p->ball_dx;
            int hit_pos = (p->ball_y + 4 - p->right_y) - paddle_h / 2;
            p->ball_dy = hit_pos / 10;
            if (p->ball_dy > 5) p->ball_dy = 5;
            if (p->ball_dy < -5) p->ball_dy = -5;
            if (p->ball_dy == 0) p->ball_dy = (p->ball_x % 2) ? 1 : -1;
        } else {
            p->left_score++;
            p->ball_x = cw / 2;
            p->ball_y = ch / 2;
            int seed = p->right_score * 17 + p->left_score * 31 + 1;
            p->ball_dx = (seed % 2) ? -3 : 3;
            p->ball_dy = ((seed / 2) % 5) - 2;
            if (p->ball_dy == 0) p->ball_dy = 1;
            p->serve_pause = 30;
        }
    }

    int target_y = p->ball_y - paddle_h / 2 + 4;
    if (p->right_y < target_y) p->right_y += 2;
    if (p->right_y > target_y) p->right_y -= 2;

    if (p->right_y < 0) p->right_y = 0;
    if (p->right_y + paddle_h > ch) p->right_y = ch - paddle_h;

    if (p->left_score >= 5 || p->right_score >= 5) {
        p->running = 0;
    }
}

void pong_draw(window_t *win, int is_active)
{
    pong_state_t *p = S(win);
    int cx = win->client_x, cy = win->client_y;
    int cw = win->client_w, ch = win->client_h;

    fb_fill_rect(cx, cy, cw, ch, c(10, 10, 18));

    int paddle_h = ch / 4;
    if (paddle_h < 40) paddle_h = 40;
    if (paddle_h > 100) paddle_h = 100;
    int paddle_w = 8;

    if (p->left_y < 0) p->left_y = 0;
    if (p->left_y + paddle_h > ch) p->left_y = ch - paddle_h;

    p->left_y = g_input.mouse_y - cy - paddle_h / 2;
    if (p->left_y < 0) p->left_y = 0;
    if (p->left_y + paddle_h > ch) p->left_y = ch - paddle_h;

    fb_fill_rect(cx + 2, cy + p->left_y, paddle_w, paddle_h, c(72, 180, 230));
    fb_fill_rect(cx + cw - paddle_w - 2, cy + p->right_y, paddle_w, paddle_h, c(230, 80, 80));

    for (int i = 0; i < ch; i += 20) {
        fb_fill_rect(cx + cw / 2 - 1, cy + i, 2, 10, c(40, 42, 55));
    }

    fb_fill_rect(cx + p->ball_x, cy + p->ball_y, 8, 8, c(240, 240, 245));

    char buf[24];
    int bi = 0;
    int sc = p->left_score;
    char rv[4]; int ri = 0;
    if (sc == 0) rv[ri++] = '0';
    while (sc > 0) { rv[ri++] = '0' + sc % 10; sc /= 10; }
    for (int i = ri-1; i >= 0; i--) buf[bi++] = rv[i];
    buf[bi++] = ' ';
    buf[bi++] = '-';
    buf[bi++] = ' ';
    sc = p->right_score; ri = 0;
    if (sc == 0) rv[ri++] = '0';
    while (sc > 0) { rv[ri++] = '0' + sc % 10; sc /= 10; }
    for (int i = ri-1; i >= 0; i--) buf[bi++] = rv[i];
    buf[bi] = 0;
    int tw = 0; for (int i = 0; buf[i]; i++) tw++;
    fb_draw_string(cx + (cw - tw * 8) / 2, cy + 4, buf, c(100, 110, 125), c(10, 10, 18));

    if (!p->running) {
        const char *m = "PONG!";
        int ml = 0; for (const char *pp = m; *pp; pp++) ml++;
        int tx = cx + (cw - ml * 8) / 2;
        int ty = cy + ch / 2 - 24;
        fb_fill_rect(tx - 6, ty - 2, ml * 8 + 12, 46, c(25, 25, 40));
        fb_draw_string(tx, ty, m, c(72, 180, 230), c(25, 25, 40));
        const char *btn = "  Restart  ";
        int bl = 0; for (const char *p = btn; *p; p++) bl++;
        int bx = cx + (cw - bl * 8) / 2;
        fb_fill_rect(bx - 2, ty + 17, bl * 8 + 4, 20, c(55, 60, 75));
        fb_draw_string(bx, ty + 19, btn, c(200, 210, 220), c(55, 60, 75));
    }
}

void pong_handle_click(window_t *win)
{
    pong_state_t *p = S(win);
    if (!p->running) {
        pong_init(win);
    }
}
