#include "input.h"
#include "framebuffer.h"

input_state_t g_input;
static EFI_SYSTEM_TABLE *g_st;
static EFI_SIMPLE_TEXT_INPUT_PROTOCOL *g_kbd;
static EFI_ABSOLUTE_POINTER_PROTOCOL *g_abs;
static int g_abs_ok;

void input_init(EFI_HANDLE image __attribute__((unused)), EFI_SYSTEM_TABLE *st)
{
    EFI_GUID abs_guid = GUID_ABSOLUTE_POINTER;

    g_st = st;
    g_input.mouse_x = 400;
    g_input.mouse_y = 300;
    g_kbd = g_st->ConIn;

    g_abs_ok = !EFI_ERROR(st->BootServices->LocateProtocol(&abs_guid, 0, (void**)&g_abs));
}

void input_poll(void)
{
    EFI_INPUT_KEY key;

    g_input.key_pressed = 0;
    g_input.last_key = 0;
    g_input.last_unicode = 0;
    g_input.mouse_dx = 0;
    g_input.mouse_dy = 0;
    g_input.mouse_buttons = 0;

    if (g_kbd) {
        if (!EFI_ERROR(g_kbd->ReadKeyStroke(g_kbd, &key))) {
            g_input.key_pressed = 1;
            g_input.last_unicode = key.UnicodeChar;

            if (key.UnicodeChar) {
                g_input.last_key = (uint8_t)(key.UnicodeChar & 0xFF);
            } else {
                switch (key.ScanCode) {
                    case 0x01: g_input.last_key = KEY_UP; break;
                    case 0x02: g_input.last_key = KEY_DOWN; break;
                    case 0x03: g_input.last_key = KEY_RIGHT; break;
                    case 0x04: g_input.last_key = KEY_LEFT; break;
                    case 0x05: g_input.last_key = KEY_HOME; break;
                    case 0x06: g_input.last_key = KEY_END; break;
                    case 0x07: g_input.last_key = KEY_INSERT; break;
                    case 0x08: g_input.last_key = KEY_DELETE; break;
                    case 0x09: g_input.last_key = KEY_PAGEUP; break;
                    case 0x0A: g_input.last_key = KEY_PAGEDOWN; break;
                    case 0x0B: g_input.last_key = KEY_F1; break;
                    case 0x0C: g_input.last_key = KEY_F2; break;
                    case 0x0D: g_input.last_key = KEY_F3; break;
                    case 0x0E: g_input.last_key = KEY_F4; break;
                    case 0x0F: g_input.last_key = KEY_F5; break;
                    case 0x10: g_input.last_key = KEY_F6; break;
                    case 0x17: g_input.last_key = KEY_ESCAPE; break;
                    default: break;
                }
            }
        }
    }

    if (g_abs_ok && g_abs) {
        EFI_ABSOLUTE_POINTER_STATE mstate;
        if (!EFI_ERROR(g_abs->GetState(g_abs, &mstate))) {
            int32_t rx = (int32_t)(mstate.CurrentX - g_abs->Mode->AbsoluteMinX);
            int32_t ry = (int32_t)(mstate.CurrentY - g_abs->Mode->AbsoluteMinY);
            int32_t rw = (int32_t)(g_abs->Mode->AbsoluteMaxX - g_abs->Mode->AbsoluteMinX);
            int32_t rh = (int32_t)(g_abs->Mode->AbsoluteMaxY - g_abs->Mode->AbsoluteMinY);
            int32_t sw = (int32_t)fb_width();
            int32_t sh = (int32_t)fb_height();
            int32_t nx, ny;
            if (rw > 0) nx = rx * sw / rw; else nx = g_input.mouse_x;
            if (rh > 0) ny = ry * sh / rh; else ny = g_input.mouse_y;
            g_input.mouse_dx = (int16_t)(nx - g_input.mouse_x);
            g_input.mouse_dy = (int16_t)(ny - g_input.mouse_y);
            g_input.mouse_x = (int16_t)nx;
            g_input.mouse_y = (int16_t)ny;
            if (mstate.ActiveButtons & 1) g_input.mouse_buttons |= 1;
            if (mstate.ActiveButtons & 2) g_input.mouse_buttons |= 2;
        }
    }

    /* Keyboard mouse emulation */
    if (g_input.key_pressed) {
        int step = 3;
        if (g_input.last_key == KEY_UP && g_input.mouse_y > 0) {
            g_input.mouse_y = (int16_t)(g_input.mouse_y - step < 0 ? 0 : g_input.mouse_y - step);
            g_input.mouse_dy = (int16_t)-step;
        }
        if (g_input.last_key == KEY_DOWN && g_input.mouse_y < (int16_t)fb_height()) {
            g_input.mouse_y = (int16_t)(g_input.mouse_y + step);
            g_input.mouse_dy = (int16_t)step;
        }
        if (g_input.last_key == KEY_LEFT && g_input.mouse_x > 0) {
            g_input.mouse_x = (int16_t)(g_input.mouse_x - step);
            g_input.mouse_dx = (int16_t)-step;
        }
        if (g_input.last_key == KEY_RIGHT && g_input.mouse_x < (int16_t)fb_width()) {
            g_input.mouse_x = (int16_t)(g_input.mouse_x + step);
            g_input.mouse_dx = (int16_t)step;
        }
        if (g_input.last_unicode == ' ' || g_input.last_unicode == 0x0D) {
            g_input.mouse_buttons |= 1;
            g_input.key_pressed = 0;
            g_input.last_key = 0;
            g_input.last_unicode = 0;
        }
    }
}
