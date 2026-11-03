/** @author southpaw | Project application code. */
#ifndef ILI9341_H
#define ILI9341_H

#include <stdint.h>

typedef enum
{
  ILI9341_ORIENTATION_PORTRAIT = 0,
  ILI9341_ORIENTATION_LANDSCAPE
} ILI9341_Orientation;

#define ILI9341_COLOR_BLACK       0x0000U
#define ILI9341_COLOR_WHITE       0xFFFFU
#define ILI9341_COLOR_NAVY        0x000FU
#define ILI9341_COLOR_DARK_GREY   0x4208U
#define ILI9341_COLOR_LIGHT_GREY  0xC618U
#define ILI9341_COLOR_BLUE        0x041FU
#define ILI9341_COLOR_CYAN        0x07FFU
#define ILI9341_COLOR_GREEN       0x07E0U
#define ILI9341_COLOR_RED         0xF800U
#define ILI9341_COLOR_ORANGE      0xFD20U

void ILI9341_Init(void);
void ILI9341_SetOrientation(ILI9341_Orientation orientation);
uint16_t ILI9341_GetWidth(void);
uint16_t ILI9341_GetHeight(void);
void ILI9341_DrawPixel(uint16_t x, uint16_t y, uint16_t color);
void ILI9341_FillRect(uint16_t x, uint16_t y, uint16_t width,
                      uint16_t height, uint16_t color);
void ILI9341_FillScreen(uint16_t color);

#endif /* ILI9341_H */
