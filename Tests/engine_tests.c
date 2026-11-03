/** @author southpaw | Project application code. */
#include "calculator_engine.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static void expect(const char *s, double expected) {
    double result=0;
    assert(CalculatorEngine_Evaluate(s,0,false,CALCULATOR_ENGINE_DEGREES,&result)==CALCULATOR_ENGINE_OK);
    assert(fabs(result-expected)<1e-8);
}
int main(void) {
    expect("2+3*4",14); expect("(2+3)*4",20); expect("2^3^2",512);
    expect("sin(30)",0.5); expect("cos(60)",0.5); expect("sqrt(9)",3);
    expect("5!",120); expect("ln(e)",1); expect("log(100)",2);
    double result=42;
    assert(CalculatorEngine_Evaluate("1/0",0,false,0,&result)==CALCULATOR_ENGINE_MATH_ERROR);
    assert(result==42);
    assert(CalculatorEngine_Evaluate("sqrt(-1)",0,false,0,&result)==CALCULATOR_ENGINE_MATH_ERROR);
    assert(CalculatorEngine_Evaluate("2+",0,false,0,&result)==CALCULATOR_ENGINE_SYNTAX_ERROR);
    assert(CalculatorEngine_Evaluate(NULL,0,false,0,&result)==CALCULATOR_ENGINE_SYNTAX_ERROR);
    assert(CalculatorEngine_Evaluate("ANS",0,false,0,&result)==CALCULATOR_ENGINE_MATH_ERROR);
    assert(CalculatorEngine_Evaluate("ANS+1",2,true,0,&result)==CALCULATOR_ENGINE_OK && result==3);
    assert(CalculatorEngine_Evaluate("ANS",NAN,true,0,&result)==CALCULATOR_ENGINE_MATH_ERROR);
    assert(CalculatorEngine_Evaluate("ANS",INFINITY,true,0,&result)==CALCULATOR_ENGINE_MATH_ERROR);
    char buffer[32];
    assert(CalculatorEngine_FormatFixed4(-1.25,buffer,sizeof buffer)); assert(strcmp(buffer,"-1.2500")==0);
    assert(!CalculatorEngine_FormatFixed4(1,buffer,6));
    assert(!CalculatorEngine_FormatFixed4(NAN,buffer,sizeof buffer));
    puts("PASS: arithmetic, functions, syntax/domain errors, ANS validation and formatting");
}
