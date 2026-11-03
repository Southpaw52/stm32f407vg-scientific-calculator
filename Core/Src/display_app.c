/** @author southpaw | Project application code. */
#include "display_app.h"

#include "calculator_engine.h"
#include "display_text.h"
#include "ili9341.h"
#include "main.h"
#include "touch_controller.h"

#if defined(DISPLAY_BUS_DIAGNOSTIC)

#define DISPLAY_DIAGNOSTIC_INTERVAL_MS  1000U

static uint8_t diagnostic_color_index;
static uint32_t diagnostic_color_tick;

static void DisplayApp_DrawDiagnosticColor(void)
{
  static const uint16_t diagnostic_colors[] = {
    ILI9341_COLOR_RED,
    ILI9341_COLOR_GREEN,
    ILI9341_COLOR_BLUE
  };

  ILI9341_FillScreen(diagnostic_colors[diagnostic_color_index]);
}

void DisplayApp_Init(void)
{
  ILI9341_Init();
  HAL_GPIO_WritePin(LCD_BL_GPIO_Port, LCD_BL_Pin, GPIO_PIN_SET);
  diagnostic_color_index = 0U;
  diagnostic_color_tick = HAL_GetTick();
  DisplayApp_DrawDiagnosticColor();
}

void DisplayApp_Process(void)
{
  uint32_t current_tick = HAL_GetTick();

  if ((uint32_t)(current_tick - diagnostic_color_tick) >=
      DISPLAY_DIAGNOSTIC_INTERVAL_MS)
  {
    diagnostic_color_tick = current_tick;
    diagnostic_color_index = (uint8_t)((diagnostic_color_index + 1U) % 3U);
    DisplayApp_DrawDiagnosticColor();
  }
}

#else

#define K0_DEBOUNCE_MS           40U
#define TOUCH_POLL_INTERVAL_MS    40U
#define EXPRESSION_CAPACITY       49U
#define RESULT_CAPACITY           24U
#define DISPLAY_EXPRESSION_CAPACITY  ((EXPRESSION_CAPACITY * 3U) + 1U)
#define DISPLAY_PI_GLYPH          '@'

extern SPI_HandleTypeDef hspi2;

static ILI9341_Orientation current_orientation = ILI9341_ORIENTATION_PORTRAIT;
static GPIO_PinState stable_button_state = GPIO_PIN_SET;
static GPIO_PinState candidate_button_state = GPIO_PIN_SET;
static uint32_t candidate_since_tick;
static uint32_t last_touch_poll_tick;
static TouchController_RawPoint last_touch_point;
static bool touch_is_down;

typedef struct
{
  bool valid;
  uint16_t x;
  uint16_t y;
  uint16_t width;
  uint16_t height;
  const char *label;
  uint8_t scale;
  uint16_t default_color;
} DisplayApp_KeyHit;

static const char *const portrait_main_labels[] = {
  "7", "8", "9", "/",
  "4", "5", "6", "*",
  "1", "2", "3", "-",
  "0", ".", "=", "+"
};

static const char *const portrait_bottom_labels[] = {
  "(", ")", "C", "DEL", "ANS"
};

static const char *const scientific_labels[] = {
  "SIN", "COS", "TAN", "LOG", "LN",
  "ASIN", "ACOS", "ATAN", "~", "POW",
  "PI", "e", "X2", "1/X", "N!",
  "DEG", "RAD", "(", ")", "C"
};

static const char *const landscape_utility_labels[] = {
  "DEL", "ANS"
};

static const char *const numeric_labels[] = {
  "7", "8", "9", "/",
  "4", "5", "6", "*",
  "1", "2", "3", "-",
  "0", ".", "=", "+"
};

static DisplayApp_KeyHit active_key;
static char calculator_expression[EXPRESSION_CAPACITY];
static size_t calculator_expression_length;
static char calculator_result[RESULT_CAPACITY];
static double calculator_answer;
static bool calculator_answer_valid;
static bool calculator_evaluation_complete;
static CalculatorEngine_Status calculator_status;
static CalculatorEngine_AngleMode calculator_angle_mode;

static void DisplayApp_CopyText(char *destination, size_t capacity,
                                const char *source)
{
  size_t index = 0U;

  if ((destination == NULL) || (capacity == 0U))
  {
    return;
  }

  while ((source[index] != '\0') && ((index + 1U) < capacity))
  {
    destination[index] = source[index];
    ++index;
  }
  destination[index] = '\0';
}

static bool DisplayApp_LabelEquals(const char *label, const char *expected)
{
  while ((*label != '\0') && (*label == *expected))
  {
    ++label;
    ++expected;
  }
  return (*label == '\0') && (*expected == '\0');
}

static void DisplayApp_BuildExpressionText(char *destination,
                                           size_t capacity)
{
  size_t source_index = 0U;
  size_t destination_index = 0U;

  if (capacity == 0U)
  {
    return;
  }
  while ((calculator_expression[source_index] != '\0') &&
         ((destination_index + 1U) < capacity))
  {
    if ((calculator_expression[source_index] == 'p') &&
        (calculator_expression[source_index + 1U] == 'i'))
    {
      destination[destination_index++] = DISPLAY_PI_GLYPH;
      source_index += 2U;
      continue;
    }
    if (calculator_expression[source_index] == '^')
    {
      const char power_text[] = "POW";
      size_t power_index = 0U;

      while ((power_text[power_index] != '\0') &&
             ((destination_index + 1U) < capacity))
      {
        destination[destination_index++] = power_text[power_index++];
      }
    }
    else
    {
      destination[destination_index++] =
        calculator_expression[source_index];
    }
    ++source_index;
  }
  destination[destination_index] = '\0';
}

static void DisplayApp_ClearExpression(void)
{
  calculator_expression[0] = '\0';
  calculator_expression_length = 0U;
}

static bool DisplayApp_AppendExpression(const char *text)
{
  size_t index = 0U;
  size_t text_length = 0U;

  while (text[text_length] != '\0')
  {
    ++text_length;
  }
  if ((calculator_expression_length + text_length) >= EXPRESSION_CAPACITY)
  {
    return false;
  }

  while (index < text_length)
  {
    calculator_expression[calculator_expression_length++] = text[index++];
  }
  calculator_expression[calculator_expression_length] = '\0';
  return true;
}

static bool DisplayApp_IsOperator(const char *label)
{
  return (((label[0] == '+') || (label[0] == '-') ||
           (label[0] == '*') || (label[0] == '/')) &&
          (label[1] == '\0'));
}

static bool DisplayApp_IsBasicInput(const char *label)
{
  if (((label[0] >= '0') && (label[0] <= '9') &&
       (label[1] == '\0')) ||
      (((label[0] == '.') || (label[0] == '(') || (label[0] == ')')) &&
       (label[1] == '\0')) || DisplayApp_IsOperator(label))
  {
    return true;
  }

  return (label[0] == 'A') && (label[1] == 'N') &&
         (label[2] == 'S') && (label[3] == '\0');
}

static bool DisplayApp_GetScientificInput(const char *label,
                                          const char **input,
                                          bool *continue_from_result)
{
  *continue_from_result = false;
  if (DisplayApp_LabelEquals(label, "SIN"))
  {
    *input = "sin(";
  }
  else if (DisplayApp_LabelEquals(label, "COS"))
  {
    *input = "cos(";
  }
  else if (DisplayApp_LabelEquals(label, "TAN"))
  {
    *input = "tan(";
  }
  else if (DisplayApp_LabelEquals(label, "ASIN"))
  {
    *input = "asin(";
  }
  else if (DisplayApp_LabelEquals(label, "ACOS"))
  {
    *input = "acos(";
  }
  else if (DisplayApp_LabelEquals(label, "ATAN"))
  {
    *input = "atan(";
  }
  else if (DisplayApp_LabelEquals(label, "LOG"))
  {
    *input = "log(";
  }
  else if (DisplayApp_LabelEquals(label, "LN"))
  {
    *input = "ln(";
  }
  else if (DisplayApp_LabelEquals(label, "~"))
  {
    *input = "sqrt(";
  }
  else if (DisplayApp_LabelEquals(label, "POW"))
  {
    *input = "^";
    *continue_from_result = true;
  }
  else if (DisplayApp_LabelEquals(label, "PI"))
  {
    *input = "pi";
  }
  else if (DisplayApp_LabelEquals(label, "e"))
  {
    *input = "e";
  }
  else if (DisplayApp_LabelEquals(label, "X2"))
  {
    *input = "^2";
    *continue_from_result = true;
  }
  else if (DisplayApp_LabelEquals(label, "1/X"))
  {
    *input = "^(-1)";
    *continue_from_result = true;
  }
  else if (DisplayApp_LabelEquals(label, "N!"))
  {
    *input = "!";
    *continue_from_result = true;
  }
  else
  {
    return false;
  }
  return true;
}

static uint16_t DisplayApp_GetKeyColor(const char *label,
                                       uint16_t default_color)
{
  if ((label[0] == '=') && (label[1] == '\0'))
  {
    return ILI9341_COLOR_ORANGE;
  }
  if (((label[0] == '+') || (label[0] == '-') || (label[0] == '*') ||
       (label[0] == '/')) && (label[1] == '\0'))
  {
    return ILI9341_COLOR_BLUE;
  }
  if ((label[0] == 'C') && (label[1] == '\0'))
  {
    return ILI9341_COLOR_ORANGE;
  }

  return default_color;
}

static void DisplayApp_DrawKeyState(uint16_t x, uint16_t y, uint16_t width,
                                    uint16_t height, const char *label,
                                    uint8_t scale, uint16_t default_color,
                                    bool pressed)
{
  uint16_t color = pressed ? ILI9341_COLOR_LIGHT_GREY :
                             DisplayApp_GetKeyColor(label, default_color);
  uint16_t foreground = pressed ? ILI9341_COLOR_BLACK : ILI9341_COLOR_WHITE;
  uint16_t text_width = DisplayText_GetWidth(label, scale);
  uint16_t text_height = (uint16_t)(7U * scale);
  uint16_t text_x = (uint16_t)(x + ((width - text_width) / 2U));
  uint16_t text_y = (uint16_t)(y + ((height - text_height) / 2U));

  ILI9341_FillRect((uint16_t)(x + 2U), (uint16_t)(y + 2U),
                   (uint16_t)(width - 4U), (uint16_t)(height - 4U), color);

  if ((label[0] == 'X') && (label[1] == '2') && (label[2] == '\0'))
  {
    uint16_t symbol_x = (uint16_t)(x + ((width - 11U) / 2U));
    uint16_t symbol_y = (uint16_t)(y + ((height - 7U) / 2U) + 3U);

    DisplayText_DrawString(symbol_x, symbol_y, "X", 1U,
                           foreground, color);
    DisplayText_DrawString((uint16_t)(symbol_x + 6U),
                           (uint16_t)(symbol_y - 4U), "2", 1U,
                           foreground, color);
    return;
  }

  if ((label[0] == 'P') && (label[1] == 'I') && (label[2] == '\0'))
  {
    uint16_t symbol_x = (uint16_t)(x + ((width - 9U) / 2U));
    uint16_t symbol_y = (uint16_t)(y + ((height - 8U) / 2U));

    ILI9341_FillRect(symbol_x, symbol_y, 9U, 1U, foreground);
    ILI9341_FillRect((uint16_t)(symbol_x + 1U),
                     (uint16_t)(symbol_y + 1U), 1U, 7U,
                     foreground);
    ILI9341_FillRect((uint16_t)(symbol_x + 6U),
                     (uint16_t)(symbol_y + 1U), 1U, 7U,
                     foreground);
    ILI9341_FillRect(symbol_x, (uint16_t)(symbol_y + 7U), 2U, 1U,
                     foreground);
    ILI9341_FillRect((uint16_t)(symbol_x + 6U),
                     (uint16_t)(symbol_y + 7U), 2U, 1U,
                     foreground);
    return;
  }

  if ((label[0] == 'P') && (label[1] == 'O') &&
      (label[2] == 'W') && (label[3] == '\0'))
  {
    uint16_t symbol_x = (uint16_t)(x + ((width - 11U) / 2U));
    uint16_t symbol_y = (uint16_t)(y + ((height - 7U) / 2U) + 3U);

    DisplayText_DrawString(symbol_x, symbol_y, "X", 1U,
                           foreground, color);
    DisplayText_DrawString((uint16_t)(symbol_x + 6U),
                           (uint16_t)(symbol_y - 4U), "Y", 1U,
                           foreground, color);
    return;
  }

  DisplayText_DrawString(text_x, text_y, label, scale,
                         foreground, color);
}

static void DisplayApp_DrawKey(uint16_t x, uint16_t y, uint16_t width,
                               uint16_t height, const char *label,
                               uint8_t scale, uint16_t default_color)
{
  DisplayApp_DrawKeyState(x, y, width, height, label, scale,
                          default_color, false);
}

static void DisplayApp_DrawKeyGrid(uint16_t x, uint16_t y, uint16_t width,
                                   uint16_t height, uint8_t columns,
                                   uint8_t rows, const char *const *labels,
                                   uint8_t scale, uint16_t default_color)
{
  uint8_t column;
  uint8_t row;
  uint16_t cell_width = (uint16_t)(width / columns);
  uint16_t cell_height = (uint16_t)(height / rows);

  for (row = 0U; row < rows; ++row)
  {
    for (column = 0U; column < columns; ++column)
    {
      uint8_t index = (uint8_t)(((uint16_t)row * columns) + column);
      DisplayApp_DrawKey((uint16_t)(x + ((uint16_t)column * cell_width)),
                         (uint16_t)(y + ((uint16_t)row * cell_height)),
                         cell_width, cell_height, labels[index], scale,
                         default_color);
    }
  }
}

static void DisplayApp_DrawPortraitLayout(void)
{
  ILI9341_SetOrientation(ILI9341_ORIENTATION_PORTRAIT);
  ILI9341_FillScreen(ILI9341_COLOR_BLACK);
  ILI9341_FillRect(8U, 8U, 224U, 72U, ILI9341_COLOR_NAVY);
  DisplayApp_DrawKeyGrid(6U, 88U, 228U, 180U, 4U, 4U,
                         portrait_main_labels, 2U,
                         ILI9341_COLOR_DARK_GREY);
  DisplayApp_DrawKeyGrid(7U, 268U, 225U, 46U, 5U, 1U,
                         portrait_bottom_labels, 2U,
                         ILI9341_COLOR_DARK_GREY);
}

static void DisplayApp_DrawLandscapeLayout(void)
{
  ILI9341_SetOrientation(ILI9341_ORIENTATION_LANDSCAPE);
  ILI9341_FillScreen(ILI9341_COLOR_BLACK);
  ILI9341_FillRect(8U, 8U, 304U, 54U, ILI9341_COLOR_NAVY);
  DisplayApp_DrawKeyGrid(6U, 68U, 150U, 132U, 5U, 4U,
                         scientific_labels, 1U, ILI9341_COLOR_BLUE);
  DisplayApp_DrawKeyGrid(6U, 200U, 150U, 34U, 2U, 1U,
                         landscape_utility_labels, 2U,
                         ILI9341_COLOR_DARK_GREY);
  DisplayApp_DrawKeyGrid(160U, 68U, 154U, 166U, 4U, 4U,
                         numeric_labels, 2U, ILI9341_COLOR_DARK_GREY);
}

static void DisplayApp_GetTouchPosition(
  TouchController_ScreenPoint *display_point)
{
  if (current_orientation == ILI9341_ORIENTATION_LANDSCAPE)
  {
    TouchController_MapRawToLandscape(&last_touch_point, display_point);
  }
  else
  {
    TouchController_MapRawToPortrait(&last_touch_point, display_point);
  }
}

static size_t DisplayApp_GetTextLength(const char *text)
{
  size_t length = 0U;

  while (text[length] != '\0')
  {
    ++length;
  }
  return length;
}

static void DisplayApp_DrawRightAlignedText(const char *text, uint16_t y,
                                            uint8_t scale, uint16_t color)
{
  uint16_t right =
    (current_orientation == ILI9341_ORIENTATION_LANDSCAPE) ? 306U : 226U;
  uint16_t available_width = (uint16_t)(right - 14U);
  size_t length = DisplayApp_GetTextLength(text);
  size_t maximum_characters =
    (size_t)((available_width + scale) / ((uint16_t)scale * 6U));
  const char *visible_text = text;
  uint16_t text_width;

  if (length > maximum_characters)
  {
    visible_text = &text[length - maximum_characters];
  }
  text_width = DisplayText_GetWidth(visible_text, scale);
  DisplayText_DrawString((uint16_t)(right - text_width), y, visible_text,
                         scale, color, ILI9341_COLOR_NAVY);
}

static void DisplayApp_DrawCalculatorDisplay(void)
{
  char display_expression[DISPLAY_EXPRESSION_CAPACITY];
  const char *primary_text;
  uint16_t primary_color = ILI9341_COLOR_WHITE;
  uint16_t header_width =
    (current_orientation == ILI9341_ORIENTATION_LANDSCAPE) ? 300U : 220U;
  uint16_t header_height =
    (current_orientation == ILI9341_ORIENTATION_LANDSCAPE) ? 50U : 68U;
  uint16_t primary_y =
    (current_orientation == ILI9341_ORIENTATION_LANDSCAPE) ? 42U : 48U;

  ILI9341_FillRect(10U, 10U, header_width, header_height,
                   ILI9341_COLOR_NAVY);
  if (current_orientation == ILI9341_ORIENTATION_LANDSCAPE)
  {
    const char *title =
      (calculator_angle_mode == CALCULATOR_ENGINE_DEGREES) ?
      "DEG" : "RAD";
    DisplayText_DrawString(14U, 12U, title, 1U,
                           ILI9341_COLOR_CYAN, ILI9341_COLOR_NAVY);
  }

  DisplayApp_BuildExpressionText(display_expression,
                                 DISPLAY_EXPRESSION_CAPACITY);

  if (calculator_evaluation_complete)
  {
    DisplayApp_DrawRightAlignedText(display_expression, 27U, 1U,
                                    ILI9341_COLOR_LIGHT_GREY);
    primary_text = calculator_result;
    if (calculator_status != CALCULATOR_ENGINE_OK)
    {
      primary_color = ILI9341_COLOR_ORANGE;
    }
  }
  else
  {
    primary_text = display_expression;
  }

  DisplayApp_DrawRightAlignedText(primary_text, primary_y, 2U,
                                  primary_color);
}

static void DisplayApp_HandleKey(const char *label)
{
  const char *scientific_input;
  bool continue_from_result;

  if ((label[0] == 'C') && (label[1] == '\0'))
  {
    DisplayApp_ClearExpression();
    calculator_result[0] = '\0';
    calculator_evaluation_complete = false;
    calculator_status = CALCULATOR_ENGINE_OK;
    DisplayApp_DrawCalculatorDisplay();
    return;
  }

  if (DisplayApp_LabelEquals(label, "DEG") ||
      DisplayApp_LabelEquals(label, "RAD"))
  {
    calculator_angle_mode = DisplayApp_LabelEquals(label, "DEG") ?
      CALCULATOR_ENGINE_DEGREES : CALCULATOR_ENGINE_RADIANS;
    DisplayApp_DrawCalculatorDisplay();
    return;
  }

  if ((label[0] == 'D') && (label[1] == 'E') && (label[2] == 'L') &&
      (label[3] == '\0'))
  {
    if (calculator_evaluation_complete)
    {
      calculator_result[0] = '\0';
      calculator_evaluation_complete = false;
      calculator_status = CALCULATOR_ENGINE_OK;
    }
    if (calculator_expression_length > 0U)
    {
      calculator_expression_length--;
      calculator_expression[calculator_expression_length] = '\0';
    }
    DisplayApp_DrawCalculatorDisplay();
    return;
  }

  if ((label[0] == '=') && (label[1] == '\0'))
  {
    double result;

    calculator_status = CalculatorEngine_Evaluate(
      calculator_expression, calculator_answer, calculator_answer_valid,
      calculator_angle_mode,
      &result);
    if (calculator_status == CALCULATOR_ENGINE_OK)
    {
      if (CalculatorEngine_FormatFixed4(result, calculator_result,
                                        RESULT_CAPACITY))
      {
        calculator_answer = result;
        calculator_answer_valid = true;
      }
      else
      {
        calculator_status = CALCULATOR_ENGINE_MATH_ERROR;
      }
    }
    if (calculator_status != CALCULATOR_ENGINE_OK)
    {
      DisplayApp_CopyText(calculator_result, RESULT_CAPACITY,
                          CalculatorEngine_GetErrorText(calculator_status));
    }
    calculator_evaluation_complete = true;
    DisplayApp_DrawCalculatorDisplay();
    return;
  }

  if (DisplayApp_GetScientificInput(label, &scientific_input,
                                    &continue_from_result))
  {
    if (calculator_evaluation_complete)
    {
      if ((calculator_status == CALCULATOR_ENGINE_OK) &&
          continue_from_result)
      {
        DisplayApp_ClearExpression();
        (void)DisplayApp_AppendExpression("(");
        (void)DisplayApp_AppendExpression(calculator_result);
        (void)DisplayApp_AppendExpression(")");
      }
      else
      {
        DisplayApp_ClearExpression();
      }
      calculator_evaluation_complete = false;
      calculator_status = CALCULATOR_ENGINE_OK;
    }
    (void)DisplayApp_AppendExpression(scientific_input);
    DisplayApp_DrawCalculatorDisplay();
    return;
  }

  if (!DisplayApp_IsBasicInput(label))
  {
    return;
  }

  if (calculator_evaluation_complete)
  {
    if ((calculator_status == CALCULATOR_ENGINE_OK) &&
        DisplayApp_IsOperator(label))
    {
      DisplayApp_ClearExpression();
      (void)DisplayApp_AppendExpression(calculator_result);
    }
    else
    {
      DisplayApp_ClearExpression();
    }
    calculator_evaluation_complete = false;
    calculator_status = CALCULATOR_ENGINE_OK;
  }

  (void)DisplayApp_AppendExpression(label);
  DisplayApp_DrawCalculatorDisplay();
}

static bool DisplayApp_HitTestGrid(
  const TouchController_ScreenPoint *point, uint16_t grid_x,
  uint16_t grid_y, uint16_t grid_width, uint16_t grid_height,
  uint8_t columns, uint8_t rows, const char *const *labels,
  uint8_t scale, uint16_t default_color, DisplayApp_KeyHit *key)
{
  uint16_t cell_width = (uint16_t)(grid_width / columns);
  uint16_t cell_height = (uint16_t)(grid_height / rows);
  uint16_t used_width = (uint16_t)((uint16_t)columns * cell_width);
  uint16_t used_height = (uint16_t)((uint16_t)rows * cell_height);
  uint8_t column;
  uint8_t row;

  if ((point->x < grid_x) || (point->x >= (grid_x + used_width)) ||
      (point->y < grid_y) || (point->y >= (grid_y + used_height)))
  {
    return false;
  }

  column = (uint8_t)((point->x - grid_x) / cell_width);
  row = (uint8_t)((point->y - grid_y) / cell_height);
  key->valid = true;
  key->x = (uint16_t)(grid_x + ((uint16_t)column * cell_width));
  key->y = (uint16_t)(grid_y + ((uint16_t)row * cell_height));
  key->width = cell_width;
  key->height = cell_height;
  key->label = labels[(uint8_t)(((uint16_t)row * columns) + column)];
  key->scale = scale;
  key->default_color = default_color;
  return true;
}

static bool DisplayApp_FindKey(const TouchController_ScreenPoint *point,
                               DisplayApp_KeyHit *key)
{
  key->valid = false;
  if (current_orientation == ILI9341_ORIENTATION_PORTRAIT)
  {
    if (DisplayApp_HitTestGrid(point, 6U, 88U, 228U, 180U, 4U, 4U,
                               portrait_main_labels, 2U,
                               ILI9341_COLOR_DARK_GREY, key))
    {
      return true;
    }
    return DisplayApp_HitTestGrid(point, 7U, 268U, 225U, 46U, 5U, 1U,
                                  portrait_bottom_labels, 2U,
                                  ILI9341_COLOR_DARK_GREY, key);
  }

  if (DisplayApp_HitTestGrid(point, 6U, 68U, 150U, 132U, 5U, 4U,
                             scientific_labels, 1U,
                             ILI9341_COLOR_BLUE, key))
  {
    return true;
  }

  if (DisplayApp_HitTestGrid(point, 6U, 200U, 150U, 34U, 2U, 1U,
                             landscape_utility_labels, 2U,
                             ILI9341_COLOR_DARK_GREY, key))
  {
    return true;
  }

  return DisplayApp_HitTestGrid(point, 160U, 68U, 154U, 166U, 4U, 4U,
                                numeric_labels, 2U,
                                ILI9341_COLOR_DARK_GREY, key);
}

static void DisplayApp_DrawCurrentLayout(void)
{
  if (current_orientation == ILI9341_ORIENTATION_LANDSCAPE)
  {
    DisplayApp_DrawLandscapeLayout();
  }
  else
  {
    DisplayApp_DrawPortraitLayout();
  }

  DisplayApp_DrawCalculatorDisplay();
}

void DisplayApp_Init(void)
{
  ILI9341_Init();
  HAL_GPIO_WritePin(LCD_BL_GPIO_Port, LCD_BL_Pin, GPIO_PIN_SET);

  stable_button_state = HAL_GPIO_ReadPin(K0_GPIO_Port, K0_Pin);
  candidate_button_state = stable_button_state;
  candidate_since_tick = HAL_GetTick();
  last_touch_poll_tick = candidate_since_tick;
  touch_is_down = false;
  active_key.valid = false;
  DisplayApp_ClearExpression();
  calculator_result[0] = '\0';
  calculator_answer = 0.0;
  calculator_answer_valid = false;
  calculator_evaluation_complete = false;
  calculator_status = CALCULATOR_ENGINE_OK;
  calculator_angle_mode = CALCULATOR_ENGINE_DEGREES;
  TouchController_Init(&hspi2);
  DisplayApp_DrawCurrentLayout();
}

void DisplayApp_Process(void)
{
  uint32_t current_tick = HAL_GetTick();
  GPIO_PinState raw_button_state = HAL_GPIO_ReadPin(K0_GPIO_Port, K0_Pin);

  if (raw_button_state != candidate_button_state)
  {
    candidate_button_state = raw_button_state;
    candidate_since_tick = current_tick;
  }

  if ((candidate_button_state != stable_button_state) &&
      ((uint32_t)(current_tick - candidate_since_tick) >= K0_DEBOUNCE_MS))
  {
    stable_button_state = candidate_button_state;
    if (stable_button_state == GPIO_PIN_RESET)
    {
      current_orientation =
        (current_orientation == ILI9341_ORIENTATION_PORTRAIT) ?
        ILI9341_ORIENTATION_LANDSCAPE : ILI9341_ORIENTATION_PORTRAIT;
      active_key.valid = false;
      touch_is_down = TouchController_IsPressed();
      DisplayApp_DrawCurrentLayout();
    }
  }

  if ((uint32_t)(current_tick - last_touch_poll_tick) >=
      TOUCH_POLL_INTERVAL_MS)
  {
    last_touch_poll_tick = current_tick;
    if (TouchController_ReadRaw(&last_touch_point))
    {
      TouchController_ScreenPoint touch_position;

      if (!touch_is_down)
      {
        touch_is_down = true;
        DisplayApp_GetTouchPosition(&touch_position);
        if (DisplayApp_FindKey(&touch_position, &active_key))
        {
          DisplayApp_HandleKey(active_key.label);
          DisplayApp_DrawKeyState(active_key.x, active_key.y,
                                  active_key.width, active_key.height,
                                  active_key.label, active_key.scale,
                                  active_key.default_color, true);
        }
      }
    }
    else if (!TouchController_IsPressed() && touch_is_down)
    {
      touch_is_down = false;
      if (active_key.valid)
      {
        DisplayApp_DrawKey(active_key.x, active_key.y,
                           active_key.width, active_key.height,
                           active_key.label, active_key.scale,
                           active_key.default_color);
        active_key.valid = false;
      }
    }
  }
}

#endif /* DISPLAY_BUS_DIAGNOSTIC */
