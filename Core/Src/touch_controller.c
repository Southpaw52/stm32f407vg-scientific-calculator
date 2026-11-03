/** @author southpaw | Project application code. */
#include "touch_controller.h"

#include "main.h"

#include <stddef.h>

#define TOUCH_COMMAND_X          0xD0U
#define TOUCH_COMMAND_Y          0x90U
#define TOUCH_SAMPLE_COUNT       5U
#define TOUCH_SPI_TIMEOUT_MS     5U

#define TOUCH_RAW_X_LEFT         3586U
#define TOUCH_RAW_X_CENTER       2085U
#define TOUCH_RAW_X_RIGHT        541U
#define TOUCH_RAW_Y_TOP          342U
#define TOUCH_RAW_Y_CENTER       2174U
#define TOUCH_RAW_Y_BOTTOM       3588U

#define TOUCH_PORTRAIT_WIDTH     240U
#define TOUCH_PORTRAIT_HEIGHT    320U
#define TOUCH_PORTRAIT_CENTER_X  (TOUCH_PORTRAIT_WIDTH / 2U)
#define TOUCH_PORTRAIT_CENTER_Y  (TOUCH_PORTRAIT_HEIGHT / 2U)

#define TOUCH_LANDSCAPE_RAW_X_LEFT     319U
#define TOUCH_LANDSCAPE_RAW_X_CENTER   1928U
#define TOUCH_LANDSCAPE_RAW_X_RIGHT    3625U
#define TOUCH_LANDSCAPE_RAW_Y_TOP      459U
#define TOUCH_LANDSCAPE_RAW_Y_CENTER   2085U
#define TOUCH_LANDSCAPE_RAW_Y_BOTTOM   3706U

#define TOUCH_LANDSCAPE_WIDTH          320U
#define TOUCH_LANDSCAPE_HEIGHT         240U
#define TOUCH_LANDSCAPE_CENTER_X       (TOUCH_LANDSCAPE_WIDTH / 2U)
#define TOUCH_LANDSCAPE_CENTER_Y       (TOUCH_LANDSCAPE_HEIGHT / 2U)

static SPI_HandleTypeDef *touch_spi;

static bool TouchController_ReadChannel(uint8_t command, uint16_t *value)
{
  uint8_t transmit_data[3] = {command, 0U, 0U};
  uint8_t receive_data[3] = {0U, 0U, 0U};
  HAL_StatusTypeDef status;

  HAL_GPIO_WritePin(TOUCH_CS_GPIO_Port, TOUCH_CS_Pin, GPIO_PIN_RESET);
  status = HAL_SPI_TransmitReceive(touch_spi, transmit_data, receive_data,
                                   sizeof(transmit_data),
                                   TOUCH_SPI_TIMEOUT_MS);
  HAL_GPIO_WritePin(TOUCH_CS_GPIO_Port, TOUCH_CS_Pin, GPIO_PIN_SET);

  if (status != HAL_OK)
  {
    return false;
  }

  *value = (uint16_t)((((uint16_t)receive_data[1] << 8U) |
                       receive_data[2]) >> 3U);
  *value &= 0x0FFFU;
  return true;
}

static uint16_t TouchController_GetMedian(uint16_t values[TOUCH_SAMPLE_COUNT])
{
  uint8_t index;
  uint8_t compare_index;

  for (index = 1U; index < TOUCH_SAMPLE_COUNT; ++index)
  {
    uint16_t selected = values[index];
    compare_index = index;

    while ((compare_index > 0U) &&
           (values[(uint8_t)(compare_index - 1U)] > selected))
    {
      values[compare_index] = values[(uint8_t)(compare_index - 1U)];
      --compare_index;
    }
    values[compare_index] = selected;
  }

  return values[TOUCH_SAMPLE_COUNT / 2U];
}

void TouchController_Init(SPI_HandleTypeDef *spi)
{
  touch_spi = spi;
  HAL_GPIO_WritePin(TOUCH_CS_GPIO_Port, TOUCH_CS_Pin, GPIO_PIN_SET);
}

bool TouchController_IsPressed(void)
{
  return HAL_GPIO_ReadPin(TOUCH_PENIRQ_GPIO_Port,
                          TOUCH_PENIRQ_Pin) == GPIO_PIN_RESET;
}

bool TouchController_ReadRaw(TouchController_RawPoint *point)
{
  uint16_t x_samples[TOUCH_SAMPLE_COUNT];
  uint16_t y_samples[TOUCH_SAMPLE_COUNT];
  uint8_t index;

  if ((touch_spi == NULL) || (point == NULL) ||
      !TouchController_IsPressed())
  {
    return false;
  }

  for (index = 0U; index < TOUCH_SAMPLE_COUNT; ++index)
  {
    if (!TouchController_ReadChannel(TOUCH_COMMAND_X, &x_samples[index]) ||
        !TouchController_ReadChannel(TOUCH_COMMAND_Y, &y_samples[index]))
    {
      return false;
    }
  }

  if (!TouchController_IsPressed())
  {
    return false;
  }

  point->x = TouchController_GetMedian(x_samples);
  point->y = TouchController_GetMedian(y_samples);
  return true;
}

void TouchController_MapRawToPortrait(
  const TouchController_RawPoint *raw_point,
  TouchController_ScreenPoint *screen_point)
{
  uint32_t mapped_value;

  if ((raw_point == NULL) || (screen_point == NULL))
  {
    return;
  }

  if (raw_point->x >= TOUCH_RAW_X_LEFT)
  {
    screen_point->x = 0U;
  }
  else if (raw_point->x <= TOUCH_RAW_X_RIGHT)
  {
    screen_point->x = TOUCH_PORTRAIT_WIDTH - 1U;
  }
  else if (raw_point->x >= TOUCH_RAW_X_CENTER)
  {
    mapped_value =
      ((uint32_t)(TOUCH_RAW_X_LEFT - raw_point->x) *
       TOUCH_PORTRAIT_CENTER_X) /
      (TOUCH_RAW_X_LEFT - TOUCH_RAW_X_CENTER);
    screen_point->x = (uint16_t)mapped_value;
  }
  else
  {
    mapped_value = TOUCH_PORTRAIT_CENTER_X +
      (((uint32_t)(TOUCH_RAW_X_CENTER - raw_point->x) *
        ((TOUCH_PORTRAIT_WIDTH - 1U) - TOUCH_PORTRAIT_CENTER_X)) /
       (TOUCH_RAW_X_CENTER - TOUCH_RAW_X_RIGHT));
    screen_point->x = (uint16_t)mapped_value;
  }

  if (raw_point->y <= TOUCH_RAW_Y_TOP)
  {
    screen_point->y = 0U;
  }
  else if (raw_point->y >= TOUCH_RAW_Y_BOTTOM)
  {
    screen_point->y = TOUCH_PORTRAIT_HEIGHT - 1U;
  }
  else if (raw_point->y <= TOUCH_RAW_Y_CENTER)
  {
    mapped_value =
      ((uint32_t)(raw_point->y - TOUCH_RAW_Y_TOP) *
       TOUCH_PORTRAIT_CENTER_Y) /
      (TOUCH_RAW_Y_CENTER - TOUCH_RAW_Y_TOP);
    screen_point->y = (uint16_t)mapped_value;
  }
  else
  {
    mapped_value = TOUCH_PORTRAIT_CENTER_Y +
      (((uint32_t)(raw_point->y - TOUCH_RAW_Y_CENTER) *
        ((TOUCH_PORTRAIT_HEIGHT - 1U) - TOUCH_PORTRAIT_CENTER_Y)) /
       (TOUCH_RAW_Y_BOTTOM - TOUCH_RAW_Y_CENTER));
    screen_point->y = (uint16_t)mapped_value;
  }
}

void TouchController_MapRawToLandscape(
  const TouchController_RawPoint *raw_point,
  TouchController_ScreenPoint *screen_point)
{
  uint32_t mapped_value;

  if ((raw_point == NULL) || (screen_point == NULL))
  {
    return;
  }

  if (raw_point->y <= TOUCH_LANDSCAPE_RAW_X_LEFT)
  {
    screen_point->x = 0U;
  }
  else if (raw_point->y >= TOUCH_LANDSCAPE_RAW_X_RIGHT)
  {
    screen_point->x = TOUCH_LANDSCAPE_WIDTH - 1U;
  }
  else if (raw_point->y <= TOUCH_LANDSCAPE_RAW_X_CENTER)
  {
    mapped_value =
      ((uint32_t)(raw_point->y - TOUCH_LANDSCAPE_RAW_X_LEFT) *
       TOUCH_LANDSCAPE_CENTER_X) /
      (TOUCH_LANDSCAPE_RAW_X_CENTER - TOUCH_LANDSCAPE_RAW_X_LEFT);
    screen_point->x = (uint16_t)mapped_value;
  }
  else
  {
    mapped_value = TOUCH_LANDSCAPE_CENTER_X +
      (((uint32_t)(raw_point->y - TOUCH_LANDSCAPE_RAW_X_CENTER) *
        ((TOUCH_LANDSCAPE_WIDTH - 1U) - TOUCH_LANDSCAPE_CENTER_X)) /
       (TOUCH_LANDSCAPE_RAW_X_RIGHT - TOUCH_LANDSCAPE_RAW_X_CENTER));
    screen_point->x = (uint16_t)mapped_value;
  }

  if (raw_point->x <= TOUCH_LANDSCAPE_RAW_Y_TOP)
  {
    screen_point->y = 0U;
  }
  else if (raw_point->x >= TOUCH_LANDSCAPE_RAW_Y_BOTTOM)
  {
    screen_point->y = TOUCH_LANDSCAPE_HEIGHT - 1U;
  }
  else if (raw_point->x <= TOUCH_LANDSCAPE_RAW_Y_CENTER)
  {
    mapped_value =
      ((uint32_t)(raw_point->x - TOUCH_LANDSCAPE_RAW_Y_TOP) *
       TOUCH_LANDSCAPE_CENTER_Y) /
      (TOUCH_LANDSCAPE_RAW_Y_CENTER - TOUCH_LANDSCAPE_RAW_Y_TOP);
    screen_point->y = (uint16_t)mapped_value;
  }
  else
  {
    mapped_value = TOUCH_LANDSCAPE_CENTER_Y +
      (((uint32_t)(raw_point->x - TOUCH_LANDSCAPE_RAW_Y_CENTER) *
        ((TOUCH_LANDSCAPE_HEIGHT - 1U) - TOUCH_LANDSCAPE_CENTER_Y)) /
       (TOUCH_LANDSCAPE_RAW_Y_BOTTOM - TOUCH_LANDSCAPE_RAW_Y_CENTER));
    screen_point->y = (uint16_t)mapped_value;
  }
}
