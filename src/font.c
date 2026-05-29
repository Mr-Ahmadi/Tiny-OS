#include "font.h"
#include "font_data.h"

const uint8_t *font_get(unsigned char c)
{
    if (c < 32 || c > 127) {
        return &font_data[0][0];
    }
    return &font_data[c][0];
}
