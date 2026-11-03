/** @author southpaw | Project application code. */
#include "ili9341.h"

#include "main.h"

#define ILI9341_COMMAND_ADDRESS  0x60000000UL

#ifndef ILI9341_RS_ADDRESS_LINE
#define ILI9341_RS_ADDRESS_LINE  16
#endif

#if ILI9341_RS_ADDRESS_LINE == 16
#define ILI9341_DATA_ADDRESS     0x60020000UL
#elif ILI9341_RS_ADDRESS_LINE == 18
#define ILI9341_DATA_ADDRESS     0x60080000UL
#else
#error "ILI9341_RS_ADDRESS_LINE must be 16 or 18"
#endif

#define ILI9341_SWRESET          0x01U
#define ILI9341_SLPOUT           0x11U
#define ILI9341_DISPON           0x29U
#define ILI9341_CASET            0x2AU
#define ILI9341_PASET            0x2BU
#define ILI9341_RAMWR            0x2CU
#define ILI9341_MADCTL           0x36U
#define ILI9341_PIXFMT           0x3AU

#define ILI9341_MADCTL_MY        0x80U
#define ILI9341_MADCTL_MX        0x40U
#define ILI9341_MADCTL_MV        0x20U
#define ILI9341_MADCTL_BGR       0x08U

#define ILI9341_PORTRAIT_WIDTH   240U
#define ILI9341_PORTRAIT_HEIGHT  320U

static uint16_t display_width = ILI9341_PORTRAIT_WIDTH;
static uint16_t display_height = ILI9341_PORTRAIT_HEIGHT;

static void ILI9341_ConfigureRegisterSelectPin(void)
{
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_GPIOD_CLK_ENABLE();
#if ILI9341_RS_ADDRESS_LINE == 16
  gpio.Pin = GPIO_PIN_11;
#else
  gpio.Pin = GPIO_PIN_13;
#endif
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  gpio.Alternate = GPIO_AF12_FSMC;
  HAL_GPIO_Init(GPIOD, &gpio);
}

static void ILI9341_WriteCommand(uint8_t command)
{
  *(volatile uint16_t *)ILI9341_COMMAND_ADDRESS = command;
}

static void ILI9341_WriteData(uint8_t data)
{
  *(volatile uint16_t *)ILI9341_DATA_ADDRESS = data;
}

static void ILI9341_WriteColor(uint16_t color)
{
  *(volatile uint16_t *)ILI9341_DATA_ADDRESS = color;
}

#if !defined(DISPLAY_BUS_DIAGNOSTIC)
static void ILI9341_WriteDataBuffer(const uint8_t *data, uint32_t length)
{
  uint32_t index;

  for (index = 0U; index < length; ++index)
  {
    ILI9341_WriteData(data[index]);
  }
}
#endif

static void ILI9341_SetAddressWindow(uint16_t x_start, uint16_t y_start,
                                     uint16_t x_end, uint16_t y_end)
{
  ILI9341_WriteCommand(ILI9341_CASET);
  ILI9341_WriteData((uint8_t)(x_start >> 8));
  ILI9341_WriteData((uint8_t)x_start);
  ILI9341_WriteData((uint8_t)(x_end >> 8));
  ILI9341_WriteData((uint8_t)x_end);

  ILI9341_WriteCommand(ILI9341_PASET);
  ILI9341_WriteData((uint8_t)(y_start >> 8));
  ILI9341_WriteData((uint8_t)y_start);
  ILI9341_WriteData((uint8_t)(y_end >> 8));
  ILI9341_WriteData((uint8_t)y_end);

  ILI9341_WriteCommand(ILI9341_RAMWR);
}

void ILI9341_Init(void)
{
#if !defined(DISPLAY_BUS_DIAGNOSTIC)
  static const uint8_t power_control_b[] = {0x00U, 0xC1U, 0x30U};
  static const uint8_t power_on_sequence[] = {0x64U, 0x03U, 0x12U, 0x81U};
  static const uint8_t driver_timing_a[] = {0x85U, 0x00U, 0x78U};
  static const uint8_t power_control_a[] = {0x39U, 0x2CU, 0x00U, 0x34U, 0x02U};
  static const uint8_t driver_timing_b[] = {0x00U, 0x00U};
  static const uint8_t display_function[] = {0x08U, 0x82U, 0x27U};
  static const uint8_t positive_gamma[] = {
    0x0FU, 0x31U, 0x2BU, 0x0CU, 0x0EU, 0x08U, 0x4EU, 0xF1U,
    0x37U, 0x07U, 0x10U, 0x03U, 0x0EU, 0x09U, 0x00U
  };
  static const uint8_t negative_gamma[] = {
    0x00U, 0x0EU, 0x14U, 0x03U, 0x11U, 0x07U, 0x31U, 0xC1U,
    0x48U, 0x08U, 0x0FU, 0x0CU, 0x31U, 0x36U, 0x0FU
  };
#endif

  ILI9341_ConfigureRegisterSelectPin();

  ILI9341_WriteCommand(ILI9341_SWRESET);
  HAL_Delay(150U);

#if !defined(DISPLAY_BUS_DIAGNOSTIC)
  ILI9341_WriteCommand(0xCFU);
  ILI9341_WriteDataBuffer(power_control_b, sizeof(power_control_b));
  ILI9341_WriteCommand(0xEDU);
  ILI9341_WriteDataBuffer(power_on_sequence, sizeof(power_on_sequence));
  ILI9341_WriteCommand(0xE8U);
  ILI9341_WriteDataBuffer(driver_timing_a, sizeof(driver_timing_a));
  ILI9341_WriteCommand(0xCBU);
  ILI9341_WriteDataBuffer(power_control_a, sizeof(power_control_a));
  ILI9341_WriteCommand(0xF7U);
  ILI9341_WriteData(0x20U);
  ILI9341_WriteCommand(0xEAU);
  ILI9341_WriteDataBuffer(driver_timing_b, sizeof(driver_timing_b));

  ILI9341_WriteCommand(0xC0U);
  ILI9341_WriteData(0x23U);
  ILI9341_WriteCommand(0xC1U);
  ILI9341_WriteData(0x10U);
  ILI9341_WriteCommand(0xC5U);
  ILI9341_WriteData(0x3EU);
  ILI9341_WriteData(0x28U);
  ILI9341_WriteCommand(0xC7U);
  ILI9341_WriteData(0x86U);

  ILI9341_WriteCommand(ILI9341_PIXFMT);
  ILI9341_WriteData(0x55U);
  ILI9341_WriteCommand(0xB1U);
  ILI9341_WriteData(0x00U);
  ILI9341_WriteData(0x18U);
  ILI9341_WriteCommand(0xB6U);
  ILI9341_WriteDataBuffer(display_function, sizeof(display_function));
  ILI9341_WriteCommand(0xF2U);
  ILI9341_WriteData(0x00U);
  ILI9341_WriteCommand(0x26U);
  ILI9341_WriteData(0x01U);
  ILI9341_WriteCommand(0xE0U);
  ILI9341_WriteDataBuffer(positive_gamma, sizeof(positive_gamma));
  ILI9341_WriteCommand(0xE1U);
  ILI9341_WriteDataBuffer(negative_gamma, sizeof(negative_gamma));
#endif

  ILI9341_SetOrientation(ILI9341_ORIENTATION_PORTRAIT);
#if defined(DISPLAY_BUS_DIAGNOSTIC)
  ILI9341_WriteCommand(ILI9341_PIXFMT);
  ILI9341_WriteData(0x55U);
#endif
  ILI9341_WriteCommand(ILI9341_SLPOUT);
  HAL_Delay(120U);
  ILI9341_WriteCommand(ILI9341_DISPON);
  HAL_Delay(20U);
}

void ILI9341_SetOrientation(ILI9341_Orientation orientation)
{
  uint8_t madctl;

  if (orientation == ILI9341_ORIENTATION_LANDSCAPE)
  {
    madctl = ILI9341_MADCTL_MV | ILI9341_MADCTL_BGR;
    display_width = ILI9341_PORTRAIT_HEIGHT;
    display_height = ILI9341_PORTRAIT_WIDTH;
  }
  else
  {
    madctl = ILI9341_MADCTL_MX | ILI9341_MADCTL_BGR;
    display_width = ILI9341_PORTRAIT_WIDTH;
    display_height = ILI9341_PORTRAIT_HEIGHT;
  }

  ILI9341_WriteCommand(ILI9341_MADCTL);
  ILI9341_WriteData(madctl);
}

uint16_t ILI9341_GetWidth(void)
{
  return display_width;
}

uint16_t ILI9341_GetHeight(void)
{
  return display_height;
}

void ILI9341_DrawPixel(uint16_t x, uint16_t y, uint16_t color)
{
  if ((x >= display_width) || (y >= display_height))
  {
    return;
  }

  ILI9341_SetAddressWindow(x, y, x, y);
  ILI9341_WriteColor(color);
}

void ILI9341_FillRect(uint16_t x, uint16_t y, uint16_t width,
                      uint16_t height, uint16_t color)
{
  uint32_t pixel_count;

  if ((width == 0U) || (height == 0U) ||
      (x >= display_width) || (y >= display_height))
  {
    return;
  }

  if (width > (uint16_t)(display_width - x))
  {
    width = (uint16_t)(display_width - x);
  }
  if (height > (uint16_t)(display_height - y))
  {
    height = (uint16_t)(display_height - y);
  }

  ILI9341_SetAddressWindow(x, y, (uint16_t)(x + width - 1U),
                          (uint16_t)(y + height - 1U));
  pixel_count = (uint32_t)width * (uint32_t)height;
  while (pixel_count > 0U)
  {
    ILI9341_WriteColor(color);
    --pixel_count;
  }
}

void ILI9341_FillScreen(uint16_t color)
{
  ILI9341_FillRect(0U, 0U, display_width, display_height, color);
}
