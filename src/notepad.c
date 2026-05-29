#include "notepad.h"
#include "framebuffer.h"
#include "font.h"
#include "input.h"
#include "fs.h"
#include "desktop.h"
#include <stdint.h>
#include <stddef.h>

#define MAX_NOTEPADS 2
#define VISIBLE_COLS 40
#define VISIBLE_ROWS 12

typedef struct {
    char filename[64];
    char text[NOTEPAD_TEXT_MAX];
    int cursor_pos;
    int text_len;
    int scroll_y;
    int dirty;
    int status_msg;
} notepad_state_t;

static notepad_state_t g_np[MAX_NOTEPADS];

static uint32_t c(uint8_t r, uint8_t g, uint8_t b) {
    return fb_make_color(r, g, b);
}

static int np_alloc(window_t *win)
{
    for (int i = 0; i < MAX_NOTEPADS; i++) {
        int used = 0;
        for (int j = 0; j < MAX_WINDOWS; j++) {
            extern window_t windows[MAX_WINDOWS];
            if (windows[j].visible && windows[j].type == WT_NOTEPAD &&
                (int)(windows[j].content[0]) == i)
                used = 1;
        }
        if (!used) {
            win->content[0] = (char)i;
            g_np[i].filename[0] = 0;
            g_np[i].text[0] = 0;
            g_np[i].cursor_pos = 0;
            g_np[i].text_len = 0;
            g_np[i].scroll_y = 0;
            g_np[i].dirty = 0;
            g_np[i].status_msg = 0;
            return i;
        }
    }
    return -1;
}

static notepad_state_t *NP(window_t *w)
{
    int idx = (int)(w->content[0]);
    if (idx < 0 || idx >= MAX_NOTEPADS) return 0;
    return &g_np[idx];
}

void notepad_init(window_t *win)
{
    np_alloc(win);
}

static void status_set(notepad_state_t *np, int msg)
{
    np->status_msg = msg;
    np->dirty = 1;
}
static const char *status_text(int msg)
{
    switch (msg) {
        case 1: return "Saved";
        case 2: return "Loaded";
        case 3: return "File list updated";
        case 4: return "Save failed";
        case 5: return "Load failed";
        default: return 0;
    }
}

void notepad_draw(window_t *win, int is_active)
{
    notepad_state_t *np = NP(win);
    if (!np) return;

    int cx = win->client_x, cy = win->client_y;
    int cw = win->client_w, ch = win->client_h;

    fb_fill_rect(cx, cy, cw, ch, c(28, 30, 38));

    if (np->filename[0]) {
        char hdr[80];
        int i = 0;
        hdr[i++] = '[';
        for (int j = 0; np->filename[j] && i < 74; j++)
            hdr[i++] = np->filename[j];
        if (np->dirty) { hdr[i++] = ' '; hdr[i++] = '*'; }
        hdr[i++] = ']';
        hdr[i] = 0;
        fb_draw_string(cx + 4, cy + 2, hdr, c(171, 178, 191), c(28, 30, 38));
    } else {
        fb_draw_string(cx + 4, cy + 2, "[untitled]", np->dirty ? c(230, 180, 80) : c(130, 140, 155), c(28, 30, 38));
    }

    int line_y = cy + 16;
    int line = 0;
    int col = 0;
    int draw_pos = 0;
    int visible_rows = (ch - 20) / FONT_HEIGHT;
    if (visible_rows < 1) visible_rows = 1;
    int visible_cols = (cw - 8) / FONT_WIDTH;
    if (visible_cols < 1) visible_cols = 1;

    while (draw_pos < np->text_len && line - np->scroll_y < visible_rows) {
        if (line >= np->scroll_y) {
            int row_y = line_y + (line - np->scroll_y) * FONT_HEIGHT;
            if (row_y + FONT_HEIGHT > cy + ch) break;

            char row_buf[64];
            int ri = 0;

            while (draw_pos < np->text_len && np->text[draw_pos] != '\n' && ri < visible_cols - 1) {
                row_buf[ri++] = np->text[draw_pos];
                draw_pos++;
            }
            row_buf[ri] = 0;

            fb_draw_string(cx + 4, row_y, row_buf, c(200, 205, 212), c(28, 30, 38));

            int cur_on_line = (np->cursor_pos >= draw_pos - ri && np->cursor_pos <= draw_pos)
                              ? (np->cursor_pos - (draw_pos - ri)) : -1;
            if (cur_on_line >= 0 && np->cursor_pos <= draw_pos) {
                int cur_abs = np->cursor_pos;
                int check_pos = 0;
                int tmp_line = 0;
                while (check_pos < np->text_len && tmp_line < line) {
                    if (np->text[check_pos] == '\n') tmp_line++;
                    check_pos++;
                }
                int cur_col = np->cursor_pos - check_pos;
                if (cur_col < visible_cols) {
                    int x = cx + 4 + cur_col * FONT_WIDTH;
                    fb_fill_rect(x, row_y, 2, FONT_HEIGHT, c(180, 190, 210));
                }
            }

            if (np->text[draw_pos] == '\n') draw_pos++;
        } else {
            while (draw_pos < np->text_len && np->text[draw_pos] != '\n') draw_pos++;
            if (np->text[draw_pos] == '\n') draw_pos++;
        }
        line++;
    }

    if (np->status_msg) {
        const char *m = status_text(np->status_msg);
        if (m) {
            int ml = 0;
            for (const char *p = m; *p; p++) ml++;
            int tx = cx + (cw - ml * FONT_WIDTH) / 2;
            int ty = cy + ch - 20;
            fb_fill_rect(tx - 4, ty - 2, ml * FONT_WIDTH + 8, FONT_HEIGHT + 4, c(40, 80, 55));
            fb_draw_string(tx, ty, m, c(180, 240, 190), c(40, 80, 55));
        }
    }
}

static void np_insert_char(notepad_state_t *np, unsigned char ch)
{
    if (np->text_len >= NOTEPAD_TEXT_MAX - 1) return;
    for (int i = np->text_len; i > np->cursor_pos; i--)
        np->text[i] = np->text[i - 1];
    np->text[np->cursor_pos] = ch;
    np->cursor_pos++;
    np->text_len++;
    np->text[np->text_len] = 0;
    np->dirty = 1;
}

static void np_backspace(notepad_state_t *np)
{
    if (np->cursor_pos <= 0) return;
    for (int i = np->cursor_pos - 1; i < np->text_len; i++)
        np->text[i] = np->text[i + 1];
    np->cursor_pos--;
    np->text_len--;
    np->text[np->text_len] = 0;
    np->dirty = 1;
}

static void np_delete(notepad_state_t *np)
{
    if (np->cursor_pos >= np->text_len) return;
    for (int i = np->cursor_pos; i < np->text_len; i++)
        np->text[i] = np->text[i + 1];
    np->text_len--;
    np->text[np->text_len] = 0;
    np->dirty = 1;
}

static void np_cursor_up(notepad_state_t *np)
{
    int pos = 0;
    int target_line = 0;
    int target_col = 0;

    int cur_line = 0;
    int cur_col = 0;
    int p = 0;
    while (p < np->cursor_pos) {
        if (np->text[p] == '\n') { cur_line++; cur_col = 0; }
        else cur_col++;
        p++;
    }
    target_line = cur_line - 1;
    target_col = cur_col;
    if (target_line < 0) { np->cursor_pos = 0; return; }

    int line_s = 0, line_c = 0;
    p = 0;
    while (p < np->text_len) {
        if (line_s == target_line) {
            line_c = 0;
            while (p < np->text_len && np->text[p] != '\n' && line_c < target_col) {
                p++; line_c++;
            }
            np->cursor_pos = p;
            return;
        }
        if (np->text[p] == '\n') line_s++;
        p++;
    }
    np->cursor_pos = 0;
}

static void np_cursor_down(notepad_state_t *np)
{
    int p = 0;
    int cur_line = 0;
    int cur_col = 0;
    while (p < np->cursor_pos) {
        if (np->text[p] == '\n') { cur_line++; cur_col = 0; }
        else cur_col++;
        p++;
    }

    int target_line = cur_line + 1;
    int target_col = cur_col;
    int line_s = 0;
    p = 0;
    while (p < np->text_len) {
        if (line_s == target_line) {
            int col = 0;
            while (p < np->text_len && np->text[p] != '\n' && col < target_col) {
                p++; col++;
            }
            np->cursor_pos = p;
            if (np->cursor_pos > np->text_len) np->cursor_pos = np->text_len;
            return;
        }
        if (np->text[p] == '\n') line_s++;
        p++;
    }
    np->cursor_pos = np->text_len;
}

static void np_cursor_left(notepad_state_t *np)
{
    if (np->cursor_pos > 0) np->cursor_pos--;
}

static void np_cursor_right(notepad_state_t *np)
{
    if (np->cursor_pos < np->text_len) np->cursor_pos++;
}

static void np_cursor_home(notepad_state_t *np)
{
    int p = np->cursor_pos - 1;
    while (p >= 0 && np->text[p] != '\n') p--;
    np->cursor_pos = p + 1;
}

static void np_cursor_end(notepad_state_t *np)
{
    int p = np->cursor_pos;
    while (p < np->text_len && np->text[p] != '\n') p++;
    np->cursor_pos = p;
}

static void np_newline(notepad_state_t *np)
{
    np_insert_char(np, '\n');
}

static void np_scroll_to_cursor(notepad_state_t *np, int visible_rows)
{
    int line = 0;
    int p = 0;
    while (p < np->cursor_pos) {
        if (np->text[p] == '\n') line++;
        p++;
    }
    if (line < np->scroll_y)
        np->scroll_y = line;
    else if (line >= np->scroll_y + visible_rows)
        np->scroll_y = line - visible_rows + 1;
    if (np->scroll_y < 0) np->scroll_y = 0;
}

static void notepad_save(notepad_state_t *np)
{
    const char *fname = np->filename;
    if (fname[0] == 0) return;
    int r = fs_write(fname, np->text, np->text_len);
    if (r > 0) {
        np->dirty = 0;
        status_set(np, 1);
    } else {
        status_set(np, 4);
    }
}

static void notepad_load(notepad_state_t *np, const char *fname)
{
    char buf[NOTEPAD_TEXT_MAX];
    int r = fs_read(fname, buf, NOTEPAD_TEXT_MAX);
    if (r > 0) {
        int i;
        for (i = 0; fname[i] && i < 63; i++)
            np->filename[i] = fname[i];
        np->filename[i] = 0;
        for (i = 0; i < r; i++)
            np->text[i] = buf[i];
        np->text_len = r;
        np->text[r] = 0;
        np->cursor_pos = 0;
        np->scroll_y = 0;
        np->dirty = 0;
        status_set(np, 2);
    } else {
        status_set(np, 5);
    }
}

void notepad_key(window_t *win, uint16_t unicode, uint8_t key)
{
    notepad_state_t *np = NP(win);
    if (!np) return;

    if (np->status_msg) {
        np->status_msg = 0;
    }

    /* Ctrl+S (0x13) = Save */
    if (unicode == 0x13) {
        notepad_save(np);
        return;
    }

    /* Ctrl+O (0x0F) = Open next demo file */
    if (unicode == 0x0F) {
        static int load_idx;
        const char *demo_files[] = {"readme.txt", "notes.txt", "hello.txt", "test.txt"};
        load_idx = (load_idx + 1) % 4;
        notepad_load(np, demo_files[load_idx]);
        return;
    }

    if (key == KEY_UP) { np_cursor_up(np); return; }
    if (key == KEY_DOWN) { np_cursor_down(np); return; }
    if (key == KEY_LEFT) { np_cursor_left(np); return; }
    if (key == KEY_RIGHT) { np_cursor_right(np); return; }
    if (key == KEY_HOME) { np_cursor_home(np); return; }
    if (key == KEY_END) { np_cursor_end(np); return; }
    if (key == KEY_BACKSPACE || unicode == 0x08) { np_backspace(np); return; }
    if (key == KEY_DELETE) { np_delete(np); return; }
    if (key == KEY_RETURN || unicode == 0x0D) { np_newline(np); return; }
    if (unicode >= 0x20 && unicode <= 0x7E) {
        np_insert_char(np, (unsigned char)unicode);
        return;
    }
}

void notepad_load_file(window_t *win, const char *filename)
{
    notepad_state_t *np = NP(win);
    if (!np) return;
    notepad_load(np, filename);
}

void notepad_tick(window_t *win)
{
    notepad_state_t *np = NP(win);
    if (!np) return;
    int visible_rows = (win->client_h - 20) / FONT_HEIGHT;
    if (visible_rows < 1) visible_rows = 1;
    np_scroll_to_cursor(np, visible_rows);
}
