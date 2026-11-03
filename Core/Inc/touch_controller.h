/** @author southpaw | Project application code. */
#ifndef TOUCH_CONTROLLER_H
#define TOUCH_CONTROLLER_H

#include "stm32f4xx_hal.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
  uint16_t x;
  uint16_t y;
} TouchController_RawPoint;

typedef struct
{
  uint16_t x;
  uint16_t y;
} TouchController_ScreenPoint;

void TouchController_Init(SPI_HandleTypeDef *spi);
bool TouchController_IsPressed(void);
bool TouchController_ReadRaw(TouchController_RawPoint *point);
void TouchController_MapRawToPortrait(
  const TouchController_RawPoint *raw_point,
  TouchController_ScreenPoint *screen_point);
void TouchController_MapRawToLandscape(
  const TouchController_RawPoint *raw_point,
  TouchController_ScreenPoint *screen_point);

#endif /* TOUCH_CONTROLLER_H */
