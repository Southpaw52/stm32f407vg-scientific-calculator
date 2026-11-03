# STM32F407VG scientific calculator

[![CI](https://github.com/Southpaw52/stm32f407vg-scientific-calculator/actions/workflows/ci.yml/badge.svg)](https://github.com/Southpaw52/stm32f407vg-scientific-calculator/actions/workflows/ci.yml)

Touch-operated calculator firmware with portrait and landscape layouts,
an ILI9341 display over FSMC, XPT2046 touch input and K0 orientation switching.

This target was migrated from an STM32F407VE board. Existing pin assignments
and five-point touch calibration are historical configuration, **not proof
of compatibility with the current VG board**. Current VG hardware behavior,
wiring and calibration remain unverified.

## Implemented behavior

- Portable expression engine: decimal arithmetic, unary signs, parentheses,
  powers, factorial, trigonometric/inverse functions, logarithms and square root.
- Degree/radian modes, pi/e constants and previous-answer token ANS.
- Four-decimal formatting with explicit syntax/domain errors.
- Touch filtering and separate portrait/landscape coordinate mapping.
- ILI9341 drawing and compact text rendering.

Core/Src/calculator_engine.c is independent of HAL.
Core/Src/display_app.c integrates the UI; touch_controller.c and ili9341.c
contain board-specific behavior. The default display register-select
configuration is FSMC A18 on PD13; verify wiring before hardware use.

## Firmware build

Requires CMake, Ninja and GNU Arm Embedded GCC:

```sh
cmake --preset Debug
cmake --build --preset Debug --parallel
```

Output: build/VG-Debug/stm32f407vg-scientific-calculator.elf.

## Host tests

With Python 3 and host GCC:

```sh
python Tests/run.py
```

Tests cover arithmetic precedence, powers, selected scientific functions,
invalid domains, syntax errors, invalid ANS values and bounded formatting.
A regression reproducing acceptance of NaN as ANS failed before the fix and
passed afterwards. Firmware was rebuilt on 2026-11-01. These tests do not
validate display, touch, electrical wiring or exhaustive parser behavior.

## Limits and licensing

Input nesting and stack use need further review before treating the expression
engine as a general untrusted-input parser. It is currently a bounded UI
component, not a network-facing calculator service.

Private agent notes and previous Git history are excluded from this candidate.
Application code is MIT-licensed (see LICENSE); vendor components keep their own licenses in Drivers.


## Author

**southpaw** — application development and embedded integration.
See [AUTHORS.md](AUTHORS.md) for third-party attribution.
