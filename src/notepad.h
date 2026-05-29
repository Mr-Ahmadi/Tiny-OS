#ifndef NOTEPAD_H
#define NOTEPAD_H

#include "desktop.h"

#define NOTEPAD_TEXT_MAX 2048

void notepad_init(window_t *win);
void notepad_draw(window_t *win, int is_active);
void notepad_key(window_t *win, uint16_t unicode, uint8_t key);
void notepad_tick(window_t *win);
void notepad_load_file(window_t *win, const char *filename);

#endif
