/** @author southpaw | Project application code. */
#ifndef DISPLAY_TEXT_H
#define DISPLAY_TEXT_H

#include <stdint.h>

uint16_t DisplayText_GetWidth(const char *text, uint8_t scale);
void DisplayText_DrawString(uint16_t x, uint16_t y, const char *text,
                            uint8_t scale, uint16_t foreground,
                            uint16_t background);

#endif /* DISPLAY_TEXT_H */
