#ifndef SNAKE_H
#define SNAKE_H

#include "desktop.h"

#define SNAKE_GRID_W 20
#define SNAKE_GRID_H 16
#define MAX_SNAKE_BODY 100

typedef struct {
    uint8_t dir;
    uint8_t next_dir;
    uint8_t food_x;
    uint8_t food_y;
    uint8_t score;
    uint8_t flags;
    uint8_t move_wait;
    uint8_t snake_len;
    uint8_t body[200];
} snake_state_t;

void snake_init(window_t *win);
void snake_update(window_t *win);
void snake_draw(window_t *win, int is_active);
void snake_handle_key(window_t *win, uint8_t key);

#endif
