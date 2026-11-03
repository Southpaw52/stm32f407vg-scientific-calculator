/** @author southpaw | Project application code. */
#include "calculator_engine.h"

#include <math.h>
#include <stdint.h>

#define CALCULATOR_MAX_ABSOLUTE_VALUE  999999999999.9999
#define CALCULATOR_DIVISION_EPSILON    0.000000000001
#define CALCULATOR_TANGENT_EPSILON     0.000000000001
#define CALCULATOR_INTEGER_EPSILON     0.000000001
#define CALCULATOR_DECIMAL_SCALE       10000ULL
#define CALCULATOR_PI                  3.14159265358979323846
#define CALCULATOR_E                   2.71828182845904523536

typedef enum
{
  CALCULATOR_FUNCTION_SIN = 0,
  CALCULATOR_FUNCTION_COS,
  CALCULATOR_FUNCTION_TAN,
  CALCULATOR_FUNCTION_ASIN,
  CALCULATOR_FUNCTION_ACOS,
  CALCULATOR_FUNCTION_ATAN,
  CALCULATOR_FUNCTION_LOG10,
  CALCULATOR_FUNCTION_LN,
  CALCULATOR_FUNCTION_SQRT
} CalculatorEngine_Function;

typedef struct
{
  const char *cursor;
  double answer;
  bool answer_valid;
  CalculatorEngine_AngleMode angle_mode;
  CalculatorEngine_Status status;
} CalculatorEngine_Parser;

static void CalculatorEngine_SkipSpaces(CalculatorEngine_Parser *parser)
{
  while (*parser->cursor == ' ')
  {
    ++parser->cursor;
  }
}

static bool CalculatorEngine_IsValidValue(double value)
{
  if (value != value)
  {
    return false;
  }

  return (value <= CALCULATOR_MAX_ABSOLUTE_VALUE) &&
         (value >= -CALCULATOR_MAX_ABSOLUTE_VALUE);
}

static double CalculatorEngine_ParseExpression(CalculatorEngine_Parser *parser);
static double CalculatorEngine_ParseUnary(CalculatorEngine_Parser *parser);

static bool CalculatorEngine_Match(CalculatorEngine_Parser *parser,
                                   const char *token)
{
  const char *cursor = parser->cursor;
  const char *character = token;

  while ((*character != '\0') && (*cursor == *character))
  {
    ++cursor;
    ++character;
  }
  if (*character != '\0')
  {
    return false;
  }

  parser->cursor = cursor;
  return true;
}

static double CalculatorEngine_ToRadians(double value,
                                         CalculatorEngine_AngleMode mode)
{
  return (mode == CALCULATOR_ENGINE_DEGREES) ?
         (value * CALCULATOR_PI / 180.0) : value;
}

static double CalculatorEngine_FromRadians(double value,
                                           CalculatorEngine_AngleMode mode)
{
  return (mode == CALCULATOR_ENGINE_DEGREES) ?
         (value * 180.0 / CALCULATOR_PI) : value;
}

static double CalculatorEngine_ApplyFunction(
  CalculatorEngine_Parser *parser, CalculatorEngine_Function function,
  double argument)
{
  double angle;
  double value = 0.0;

  switch (function)
  {
    case CALCULATOR_FUNCTION_SIN:
      value = sin(CalculatorEngine_ToRadians(argument, parser->angle_mode));
      break;
    case CALCULATOR_FUNCTION_COS:
      value = cos(CalculatorEngine_ToRadians(argument, parser->angle_mode));
      break;
    case CALCULATOR_FUNCTION_TAN:
      angle = CalculatorEngine_ToRadians(argument, parser->angle_mode);
      if (fabs(cos(angle)) < CALCULATOR_TANGENT_EPSILON)
      {
        parser->status = CALCULATOR_ENGINE_MATH_ERROR;
        return 0.0;
      }
      value = tan(angle);
      break;
    case CALCULATOR_FUNCTION_ASIN:
      if ((argument < -1.0) || (argument > 1.0))
      {
        parser->status = CALCULATOR_ENGINE_MATH_ERROR;
        return 0.0;
      }
      value = CalculatorEngine_FromRadians(asin(argument), parser->angle_mode);
      break;
    case CALCULATOR_FUNCTION_ACOS:
      if ((argument < -1.0) || (argument > 1.0))
      {
        parser->status = CALCULATOR_ENGINE_MATH_ERROR;
        return 0.0;
      }
      value = CalculatorEngine_FromRadians(acos(argument), parser->angle_mode);
      break;
    case CALCULATOR_FUNCTION_ATAN:
      value = CalculatorEngine_FromRadians(atan(argument), parser->angle_mode);
      break;
    case CALCULATOR_FUNCTION_LOG10:
      if (argument <= 0.0)
      {
        parser->status = CALCULATOR_ENGINE_MATH_ERROR;
        return 0.0;
      }
      value = log10(argument);
      break;
    case CALCULATOR_FUNCTION_LN:
      if (argument <= 0.0)
      {
        parser->status = CALCULATOR_ENGINE_MATH_ERROR;
        return 0.0;
      }
      value = log(argument);
      break;
    case CALCULATOR_FUNCTION_SQRT:
      if (argument < 0.0)
      {
        parser->status = CALCULATOR_ENGINE_MATH_ERROR;
        return 0.0;
      }
      value = sqrt(argument);
      break;
    default:
      parser->status = CALCULATOR_ENGINE_SYNTAX_ERROR;
      return 0.0;
  }

  if (!CalculatorEngine_IsValidValue(value))
  {
    parser->status = CALCULATOR_ENGINE_MATH_ERROR;
    return 0.0;
  }
  return value;
}

static double CalculatorEngine_ParseFunction(
  CalculatorEngine_Parser *parser, CalculatorEngine_Function function)
{
  double argument;

  CalculatorEngine_SkipSpaces(parser);
  if (*parser->cursor != '(')
  {
    parser->status = CALCULATOR_ENGINE_SYNTAX_ERROR;
    return 0.0;
  }

  ++parser->cursor;
  argument = CalculatorEngine_ParseExpression(parser);
  CalculatorEngine_SkipSpaces(parser);
  if (parser->status != CALCULATOR_ENGINE_OK)
  {
    return 0.0;
  }
  if (*parser->cursor != ')')
  {
    parser->status = CALCULATOR_ENGINE_SYNTAX_ERROR;
    return 0.0;
  }
  ++parser->cursor;
  return CalculatorEngine_ApplyFunction(parser, function, argument);
}

static double CalculatorEngine_ParseNumber(CalculatorEngine_Parser *parser)
{
  double value = 0.0;
  double decimal_place = 0.1;
  bool has_digit = false;

  while ((*parser->cursor >= '0') && (*parser->cursor <= '9'))
  {
    has_digit = true;
    value = (value * 10.0) + (double)(*parser->cursor - '0');
    ++parser->cursor;
  }

  if (*parser->cursor == '.')
  {
    ++parser->cursor;
    while ((*parser->cursor >= '0') && (*parser->cursor <= '9'))
    {
      has_digit = true;
      value += (double)(*parser->cursor - '0') * decimal_place;
      decimal_place *= 0.1;
      ++parser->cursor;
    }
  }

  if (!has_digit)
  {
    parser->status = CALCULATOR_ENGINE_SYNTAX_ERROR;
    return 0.0;
  }

  if (!CalculatorEngine_IsValidValue(value))
  {
    parser->status = CALCULATOR_ENGINE_MATH_ERROR;
    return 0.0;
  }

  return value;
}

static double CalculatorEngine_ParsePrimary(CalculatorEngine_Parser *parser)
{
  double value;

  CalculatorEngine_SkipSpaces(parser);
  if (*parser->cursor == '(')
  {
    ++parser->cursor;
    value = CalculatorEngine_ParseExpression(parser);
    CalculatorEngine_SkipSpaces(parser);
    if ((parser->status == CALCULATOR_ENGINE_OK) &&
        (*parser->cursor == ')'))
    {
      ++parser->cursor;
      return value;
    }

    parser->status = CALCULATOR_ENGINE_SYNTAX_ERROR;
    return 0.0;
  }

  if (CalculatorEngine_Match(parser, "ANS"))
  {
    if (!parser->answer_valid || !CalculatorEngine_IsValidValue(parser->answer))
    {
      parser->status = CALCULATOR_ENGINE_MATH_ERROR;
      return 0.0;
    }
    return parser->answer;
  }

  if (CalculatorEngine_Match(parser, "pi"))
  {
    return CALCULATOR_PI;
  }
  if (CalculatorEngine_Match(parser, "e"))
  {
    return CALCULATOR_E;
  }
  if (CalculatorEngine_Match(parser, "asin"))
  {
    return CalculatorEngine_ParseFunction(parser, CALCULATOR_FUNCTION_ASIN);
  }
  if (CalculatorEngine_Match(parser, "acos"))
  {
    return CalculatorEngine_ParseFunction(parser, CALCULATOR_FUNCTION_ACOS);
  }
  if (CalculatorEngine_Match(parser, "atan"))
  {
    return CalculatorEngine_ParseFunction(parser, CALCULATOR_FUNCTION_ATAN);
  }
  if (CalculatorEngine_Match(parser, "sin"))
  {
    return CalculatorEngine_ParseFunction(parser, CALCULATOR_FUNCTION_SIN);
  }
  if (CalculatorEngine_Match(parser, "cos"))
  {
    return CalculatorEngine_ParseFunction(parser, CALCULATOR_FUNCTION_COS);
  }
  if (CalculatorEngine_Match(parser, "tan"))
  {
    return CalculatorEngine_ParseFunction(parser, CALCULATOR_FUNCTION_TAN);
  }
  if (CalculatorEngine_Match(parser, "log"))
  {
    return CalculatorEngine_ParseFunction(parser, CALCULATOR_FUNCTION_LOG10);
  }
  if (CalculatorEngine_Match(parser, "ln"))
  {
    return CalculatorEngine_ParseFunction(parser, CALCULATOR_FUNCTION_LN);
  }
  if (CalculatorEngine_Match(parser, "sqrt"))
  {
    return CalculatorEngine_ParseFunction(parser, CALCULATOR_FUNCTION_SQRT);
  }

  return CalculatorEngine_ParseNumber(parser);
}

static double CalculatorEngine_ApplyFactorial(
  CalculatorEngine_Parser *parser, double value)
{
  double rounded;
  double result = 1.0;
  uint32_t factor;

  if (value < 0.0)
  {
    parser->status = CALCULATOR_ENGINE_MATH_ERROR;
    return 0.0;
  }
  rounded = floor(value + 0.5);
  if ((fabs(value - rounded) > CALCULATOR_INTEGER_EPSILON) ||
      (rounded > 170.0))
  {
    parser->status = CALCULATOR_ENGINE_MATH_ERROR;
    return 0.0;
  }

  for (factor = 2U; factor <= (uint32_t)rounded; ++factor)
  {
    result *= (double)factor;
    if (!CalculatorEngine_IsValidValue(result))
    {
      parser->status = CALCULATOR_ENGINE_MATH_ERROR;
      return 0.0;
    }
  }
  return result;
}

static double CalculatorEngine_ParsePostfix(CalculatorEngine_Parser *parser)
{
  double value = CalculatorEngine_ParsePrimary(parser);

  CalculatorEngine_SkipSpaces(parser);
  while ((parser->status == CALCULATOR_ENGINE_OK) &&
         (*parser->cursor == '!'))
  {
    ++parser->cursor;
    value = CalculatorEngine_ApplyFactorial(parser, value);
    CalculatorEngine_SkipSpaces(parser);
  }
  return value;
}

static double CalculatorEngine_ParsePower(CalculatorEngine_Parser *parser)
{
  double left = CalculatorEngine_ParsePostfix(parser);

  CalculatorEngine_SkipSpaces(parser);
  if ((parser->status == CALCULATOR_ENGINE_OK) && (*parser->cursor == '^'))
  {
    double right;

    ++parser->cursor;
    right = CalculatorEngine_ParseUnary(parser);
    if (parser->status != CALCULATOR_ENGINE_OK)
    {
      return 0.0;
    }
    left = pow(left, right);
    if (!CalculatorEngine_IsValidValue(left))
    {
      parser->status = CALCULATOR_ENGINE_MATH_ERROR;
      return 0.0;
    }
  }
  return left;
}

static double CalculatorEngine_ParseUnary(CalculatorEngine_Parser *parser)
{
  CalculatorEngine_SkipSpaces(parser);
  if (*parser->cursor == '+')
  {
    ++parser->cursor;
    return CalculatorEngine_ParseUnary(parser);
  }
  if (*parser->cursor == '-')
  {
    ++parser->cursor;
    return -CalculatorEngine_ParseUnary(parser);
  }

  return CalculatorEngine_ParsePower(parser);
}

static double CalculatorEngine_ParseTerm(CalculatorEngine_Parser *parser)
{
  double left = CalculatorEngine_ParseUnary(parser);

  while (parser->status == CALCULATOR_ENGINE_OK)
  {
    char operation;
    double right;

    CalculatorEngine_SkipSpaces(parser);
    operation = *parser->cursor;
    if ((operation != '*') && (operation != '/'))
    {
      break;
    }

    ++parser->cursor;
    right = CalculatorEngine_ParseUnary(parser);
    if (parser->status != CALCULATOR_ENGINE_OK)
    {
      return 0.0;
    }

    if (operation == '*')
    {
      left *= right;
    }
    else
    {
      if ((right < CALCULATOR_DIVISION_EPSILON) &&
          (right > -CALCULATOR_DIVISION_EPSILON))
      {
        parser->status = CALCULATOR_ENGINE_MATH_ERROR;
        return 0.0;
      }
      left /= right;
    }

    if (!CalculatorEngine_IsValidValue(left))
    {
      parser->status = CALCULATOR_ENGINE_MATH_ERROR;
      return 0.0;
    }
  }

  return left;
}

static double CalculatorEngine_ParseExpression(CalculatorEngine_Parser *parser)
{
  double left = CalculatorEngine_ParseTerm(parser);

  while (parser->status == CALCULATOR_ENGINE_OK)
  {
    char operation;
    double right;

    CalculatorEngine_SkipSpaces(parser);
    operation = *parser->cursor;
    if ((operation != '+') && (operation != '-'))
    {
      break;
    }

    ++parser->cursor;
    right = CalculatorEngine_ParseTerm(parser);
    if (parser->status != CALCULATOR_ENGINE_OK)
    {
      return 0.0;
    }

    left = (operation == '+') ? (left + right) : (left - right);
    if (!CalculatorEngine_IsValidValue(left))
    {
      parser->status = CALCULATOR_ENGINE_MATH_ERROR;
      return 0.0;
    }
  }

  return left;
}

CalculatorEngine_Status CalculatorEngine_Evaluate(const char *expression,
                                                   double answer,
                                                   bool answer_valid,
                                                   CalculatorEngine_AngleMode angle_mode,
                                                   double *result)
{
  CalculatorEngine_Parser parser;
  double value;

  if ((expression == NULL) || (result == NULL) || (*expression == '\0'))
  {
    return CALCULATOR_ENGINE_SYNTAX_ERROR;
  }

  parser.cursor = expression;
  parser.answer = answer;
  parser.answer_valid = answer_valid;
  parser.angle_mode = angle_mode;
  parser.status = CALCULATOR_ENGINE_OK;

  value = CalculatorEngine_ParseExpression(&parser);
  CalculatorEngine_SkipSpaces(&parser);
  if ((parser.status == CALCULATOR_ENGINE_OK) && (*parser.cursor != '\0'))
  {
    parser.status = CALCULATOR_ENGINE_SYNTAX_ERROR;
  }

  if (parser.status == CALCULATOR_ENGINE_OK)
  {
    *result = value;
  }
  return parser.status;
}

bool CalculatorEngine_FormatFixed4(double value, char *text,
                                   size_t capacity)
{
  char reverse_digits[16];
  uint64_t scaled;
  uint64_t integer_part;
  uint16_t fraction_part;
  size_t digit_count = 0U;
  size_t required;
  size_t output_index = 0U;
  bool negative = value < 0.0;

  if ((text == NULL) || !CalculatorEngine_IsValidValue(value))
  {
    return false;
  }

  if ((value > -0.00005) && (value < 0.00005))
  {
    value = 0.0;
    negative = false;
  }
  if (negative)
  {
    value = -value;
  }

  scaled = (uint64_t)((value * (double)CALCULATOR_DECIMAL_SCALE) + 0.5);
  integer_part = scaled / CALCULATOR_DECIMAL_SCALE;
  fraction_part = (uint16_t)(scaled % CALCULATOR_DECIMAL_SCALE);

  do
  {
    reverse_digits[digit_count] = (char)('0' + (integer_part % 10ULL));
    integer_part /= 10ULL;
    ++digit_count;
  } while (integer_part != 0ULL);

  required = digit_count + 5U + (negative ? 1U : 0U);
  if (capacity <= required)
  {
    return false;
  }

  if (negative)
  {
    text[output_index++] = '-';
  }
  while (digit_count > 0U)
  {
    text[output_index++] = reverse_digits[--digit_count];
  }
  text[output_index++] = '.';
  text[output_index++] = (char)('0' + ((fraction_part / 1000U) % 10U));
  text[output_index++] = (char)('0' + ((fraction_part / 100U) % 10U));
  text[output_index++] = (char)('0' + ((fraction_part / 10U) % 10U));
  text[output_index++] = (char)('0' + (fraction_part % 10U));
  text[output_index] = '\0';
  return true;
}

const char *CalculatorEngine_GetErrorText(CalculatorEngine_Status status)
{
  if (status == CALCULATOR_ENGINE_SYNTAX_ERROR)
  {
    return "SYNTAX ERROR";
  }
  if (status == CALCULATOR_ENGINE_MATH_ERROR)
  {
    return "MATH ERROR";
  }

  return "";
}
