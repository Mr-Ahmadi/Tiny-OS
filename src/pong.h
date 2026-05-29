#ifndef PONG_H
#define PONG_H

#include "desktop.h"

void pong_init(window_t *win);
void pong_update(window_t *win);
void pong_draw(window_t *win, int is_active);
void pong_handle_click(window_t *win);

#endif
