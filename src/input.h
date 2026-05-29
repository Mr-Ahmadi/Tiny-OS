#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>
#include "uefi.h"

#define KEY_UP       0x01
#define KEY_DOWN     0x02
#define KEY_RIGHT    0x03
#define KEY_LEFT     0x04
#define KEY_ESCAPE   0x17
#define KEY_TAB      0x09
#define KEY_RETURN   0x0D
#define KEY_BACKSPACE 0x08
#define KEY_HOME     0x10
#define KEY_END      0x11
#define KEY_PAGEUP   0x12
#define KEY_PAGEDOWN 0x13
#define KEY_INSERT   0x14
#define KEY_LCTRL    0xE0
#define KEY_RCTRL    0xE1
#define KEY_F1       0x1B
#define KEY_F2       0x1C
#define KEY_F3       0x1D
#define KEY_F4       0x1E
#define KEY_F5       0x1F
#define KEY_F6       0x20
#define KEY_DELETE   0x7F

typedef struct {
    int16_t mouse_x;
    int16_t mouse_y;
    int16_t mouse_dx;
    int16_t mouse_dy;
    uint8_t mouse_buttons;
    uint8_t last_key;
    uint16_t last_unicode;
    uint8_t key_pressed;
    uint8_t shift;
    uint8_t alt;
    uint8_t ctrl;
} input_state_t;

extern input_state_t g_input;

void input_init(EFI_HANDLE image, EFI_SYSTEM_TABLE *st);
void input_poll(void);

#endif
