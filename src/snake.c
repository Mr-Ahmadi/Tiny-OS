#include "snake.h"
#include "framebuffer.h"
#include "font.h"
#include "input.h"

#define S_DIR(d)  ((d) & 0x03)
#define S_HEAD_X(s)  ((s)->body[(s)->snake_len * 2 - 2])
#define S_HEAD_Y(s)  ((s)->body[(s)->snake_len * 2 - 1])



static snake_state_t *S(window_t *w) { return (snake_state_t *)w->content; }

static uint32_t c(uint8_t r, uint8_t g, uint8_t b) {
    return fb_make_color(r, g, b);
}

static void place_food(snake_state_t *s)
{
    int total = SNAKE_GRID_W * SNAKE_GRID_H;
    int free_count = 0;

    for (int i = 0; i < total; i++) {
        int occ = 0;
        int px = i % SNAKE_GRID_W;
        int py = i / SNAKE_GRID_W;
        for (int j = 0; j < s->snake_len; j++) {
            if (s->body[j*2] == px && s->body[j*2+1] == py) {
                occ = 1; break;
            }
        }
        if (!occ) free_count++;
    }

    if (free_count == 0) { s->flags |= 2; return; }

    int pick = (s->food_x * 7 + s->food_y * 31 + s->snake_len * 13) % total;
    for (int i = 0; i < total; i++) {
        int idx = (pick + i) % total;
        int occ = 0;
        for (int j = 0; j < s->snake_len; j++) {
            if (s->body[j*2] == idx % SNAKE_GRID_W && s->body[j*2+1] == idx / SNAKE_GRID_W) {
                occ = 1; break;
            }
        }
        if (!occ) {
            s->food_x = idx % SNAKE_GRID_W;
            s->food_y = idx / SNAKE_GRID_W;
            return;
        }
    }
}

void snake_init(window_t *win)
{
    snake_state_t *s = S(win);
    s->dir = 2;
    s->next_dir = 2;
    s->score = 0;
    s->flags = 0;
    s->move_wait = 0;
    s->snake_len = 3;

    s->body[0] = SNAKE_GRID_W / 2;
    s->body[1] = SNAKE_GRID_H / 2;
    s->body[2] = s->body[0] - 1;
    s->body[3] = s->body[1];
    s->body[4] = s->body[0] - 2;
    s->body[5] = s->body[1];

    place_food(s);
}

static void move_snake(snake_state_t *s)
{
    if (s->flags & 3) return;

    s->dir = S_DIR(s->next_dir);
    int new_x = s->body[0];
    int new_y = s->body[1];
    switch (s->dir) {
        case 0: new_y = (new_y == 0) ? -1 : new_y - 1; break;
        case 1: new_x++; break;
        case 2: new_y++; break;
        case 3: new_x = (new_x == 0) ? -1 : new_x - 1; break;
    }

    if (new_x < 0 || new_x >= SNAKE_GRID_W || new_y < 0 || new_y >= SNAKE_GRID_H) {
        s->flags |= 1;
        return;
    }

    for (int i = 1; i < s->snake_len; i++) {
        if (s->body[i*2] == new_x && s->body[i*2+1] == new_y) {
            s->flags |= 1;
            return;
        }
    }

    int eating = (new_x == s->food_x && new_y == s->food_y);

    for (int i = s->snake_len - 1; i > 0; i--) {
        s->body[i*2] = s->body[(i-1)*2];
        s->body[i*2+1] = s->body[(i-1)*2+1];
    }
    s->body[0] = new_x;
    s->body[1] = new_y;

    if (eating) {
        s->snake_len++;
        s->score += 10;
        if (s->snake_len >= MAX_SNAKE_BODY) {
            s->flags |= 2;
            return;
        }
        s->food_x = 0; s->food_y = 0;
        place_food(s);
    }
}

void snake_update(window_t *win)
{
    snake_state_t *s = S(win);
    if (s->snake_len == 0) { snake_init(win); return; }
    if (s->move_wait == 0) return;
    s->move_wait++;
    if (s->move_wait >= 20) {
        s->move_wait = 0;
        move_snake(s);
    }
}

void snake_draw(window_t *win, int is_active)
{
    snake_state_t *s = S(win);
    if (s->snake_len == 0) return;

    int cx = win->client_x, cy = win->client_y;
    int cw = win->client_w, ch = win->client_h;

    fb_fill_rect(cx, cy, cw, ch, c(18, 20, 26));

    int cell = (cw < ch ? cw : ch) / (SNAKE_GRID_W > SNAKE_GRID_H ? SNAKE_GRID_W : SNAKE_GRID_H);
    if (cell < 6) cell = 6;
    if (cell > 24) cell = 24;

    int gpw = SNAKE_GRID_W * cell;
    int gph = SNAKE_GRID_H * cell;
    int ox = cx + (cw - gpw) / 2;
    int oy = cy + (ch - gph) / 2;

    for (int gy = 0; gy < SNAKE_GRID_H; gy++) {
        for (int gx = 0; gx < SNAKE_GRID_W; gx++) {
            uint32_t col = ((gx + gy) & 1) ? c(24, 27, 34) : c(18, 20, 26);
            fb_fill_rect(ox + gx * cell, oy + gy * cell, cell, cell, col);
        }
    }

    for (int i = 0; i < s->snake_len; i++) {
        int px = ox + s->body[i*2] * cell;
        int py = oy + s->body[i*2+1] * cell;
        uint32_t col;
        if (i == 0) {
            col = is_active ? c(72, 210, 72) : c(50, 150, 50);
        } else {
            uint8_t g = (uint8_t)(190 - (i * 90 / s->snake_len));
            col = c(35, g, 50);
        }
        fb_fill_rect(px + 1, py + 1, cell - 2, cell - 2, col);
        if (i == 0) {
            uint32_t ec = c(255, 255, 255);
            fb_pixel(px + cell/3, py + cell/3, ec);
            fb_pixel(px + 2*cell/3, py + cell/3, ec);
            fb_pixel(px + cell/3, py + 2*cell/3 - 1, c(30, 160, 30));
            fb_pixel(px + 2*cell/3, py + 2*cell/3 - 1, c(30, 160, 30));
        }
    }

    int fx = ox + s->food_x * cell;
    int fy = oy + s->food_y * cell;
    fb_fill_rect(fx + 2, fy + 2, cell - 4, cell - 4, c(230, 50, 50));
    fb_fill_rect(fx + cell/4, fy + 1, cell/2, cell/4, c(250, 120, 120));

    char buf[24]; int bi = 0;
    const char *pre = "Score: ";
    for (int i = 0; pre[i]; i++) buf[bi++] = pre[i];
    int sc = s->score;
    char rv[6]; int ri = 0;
    if (sc == 0) rv[ri++] = '0';
    while (sc > 0) { rv[ri++] = '0' + sc % 10; sc /= 10; }
    for (int i = ri-1; i >= 0; i--) buf[bi++] = rv[i];
    buf[bi] = 0;
    fb_draw_string(cx + 6, cy + 2, buf, c(150, 158, 170), c(18, 20, 26));

    if (s->move_wait == 0) {
        const char *m = "Press a direction key to start";
        int ml = 0; for (const char *p = m; *p; p++) ml++;
        int tx = cx + (cw - ml * 8) / 2;
        int ty = cy + ch / 2 - 8;
        fb_fill_rect(tx - 6, ty - 2, ml * 8 + 12, 18, c(30, 35, 50));
        fb_draw_string(tx, ty, m, c(150, 200, 255), c(30, 35, 50));
        return;
    }

    {
        const char *m;
        int ml;
        uint32_t bg;
        uint32_t fg;
        if (s->flags & 1) { m = "GAME OVER"; bg = c(50, 20, 20); fg = c(240, 80, 80); }
        else if (s->flags & 2) { m = "YOU WIN!"; bg = c(20, 60, 20); fg = c(80, 230, 80); }
        else m = 0;
        if (m) {
            ml = 0; for (const char *p = m; *p; p++) ml++;
            int tx = cx + (cw - ml * 8) / 2;
            int ty = cy + ch / 2 - 24;
            fb_fill_rect(tx - 6, ty - 2, ml * 8 + 12, 46, bg);
            fb_draw_string(tx, ty, m, fg, bg);
            const char *btn = "  Restart  ";
            int bl = 0; for (const char *p = btn; *p; p++) bl++;
            int bx = cx + (cw - bl * 8) / 2;
            fb_fill_rect(bx - 2, ty + 17, bl * 8 + 4, 20, c(55, 60, 75));
            fb_draw_string(bx, ty + 19, btn, c(200, 210, 220), c(55, 60, 75));
        }
    }
}

void snake_handle_key(window_t *win, uint8_t key)
{
    snake_state_t *s = S(win);
    if (s->snake_len == 0) return;

    if (s->move_wait == 0) {
        switch (key) {
            case KEY_UP:    case 'w': case 'W': s->dir = 0; s->next_dir = 0; s->move_wait = 1; break;
            case KEY_RIGHT: case 'd': case 'D': s->dir = 1; s->next_dir = 1; s->move_wait = 1; break;
            case KEY_DOWN:  case 's': case 'S': s->dir = 2; s->next_dir = 2; s->move_wait = 1; break;
            case KEY_LEFT:  case 'a': case 'A': s->dir = 3; s->next_dir = 3; s->move_wait = 1; break;
        }
        return;
    }

    if (s->flags & 3) {
        if (key == KEY_RETURN || key == ' ' || key == 'r') {
            snake_init(win);
        }
        return;
    }

    switch (key) {
        case KEY_UP:
        case 'w': case 'W':
            if (s->dir != 2) { s->next_dir = 0; } break;
        case KEY_RIGHT:
        case 'd': case 'D':
            if (s->dir != 3) { s->next_dir = 1; } break;
        case KEY_DOWN:
        case 's': case 'S':
            if (s->dir != 0) { s->next_dir = 2; } break;
        case KEY_LEFT:
        case 'a': case 'A':
            if (s->dir != 1) { s->next_dir = 3; } break;
    }
}
