/** @author southpaw | Project application code. */
#ifndef CALCULATOR_ENGINE_H
#define CALCULATOR_ENGINE_H

#include <stdbool.h>
#include <stddef.h>

typedef enum
{
  CALCULATOR_ENGINE_OK = 0,
  CALCULATOR_ENGINE_SYNTAX_ERROR,
  CALCULATOR_ENGINE_MATH_ERROR
} CalculatorEngine_Status;

typedef enum
{
  CALCULATOR_ENGINE_DEGREES = 0,
  CALCULATOR_ENGINE_RADIANS
} CalculatorEngine_AngleMode;

CalculatorEngine_Status CalculatorEngine_Evaluate(const char *expression,
                                                   double answer,
                                                   bool answer_valid,
                                                   CalculatorEngine_AngleMode angle_mode,
                                                   double *result);
bool CalculatorEngine_FormatFixed4(double value, char *text,
                                   size_t capacity);
const char *CalculatorEngine_GetErrorText(CalculatorEngine_Status status);

#endif /* CALCULATOR_ENGINE_H */
