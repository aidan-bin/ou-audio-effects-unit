# STM32 Audio Effects Unit

![Board v1](hardware/STM32AudioEffects/STM32AudioEffectsUnit.jpg)

Embedded STM32 audio effects project with fixed-point DSP in C, host-side effect demo tooling, and board design files.

This repository is being modernized. The goal is to keep it developer-usable while making quality signals obvious: clean structure, reproducible setup, tests, and CI.

## What Is Here

- DSP effects in C: overdrive, echo, compression
- STM32 firmware using FreeRTOS + ADC/DAC DMA audio pipeline
- Host demo (Python + ctypes) for quick effect validation
- Hardware design assets (KiCad + LTspice)

## Repository Layout

- [dsp](dsp): portable DSP and math code
- [firmware](firmware): STM32 target firmware
- [host](host): host-side demo tooling
- [hardware](hardware): board and analog design assets
- [tests](tests): transitional tests (full suite in progress)
- [docs](docs): setup, architecture, bring-up docs (expanding)
- [scripts](scripts): helper scripts

## Quick Start: Host Demo (Current Baseline)

The current Python binding expects a shared library named `libeffects.dll` in [host/python-demo](host/python-demo).

From the repository root:

```sh
cd host/python-demo
python3 -m pip install PyQt5 matplotlib numpy sounddevice
cc -Wall -I../../dsp/effects -I../../dsp/math -c ../../dsp/math/fast_math.c ../../dsp/effects/effects.c
cc -shared -o libeffects.dll fast_math.o effects.o
python3 main.py
```

Notes:

- The current Python binding hardcodes `libeffects.dll`.
- Demo packaging/tooling cleanup is part of V1 modernization.

## Firmware Snapshot

Current STM32 firmware lives under [firmware/stm32f303/cubemx](firmware/stm32f303/cubemx).

Current implementation includes:

- FreeRTOS task-based control flow
- ping-pong sample buffering
- DMA-driven ADC/DAC audio transfer
- effect parameter/state handling
- I2C infrastructure for peripherals (EEPROM and display path)

## Hardware Snapshot

Board assets are under [hardware/STM32AudioEffects](hardware/STM32AudioEffects), with analog simulation files under [hardware/LTspice](hardware/LTspice).

## Modernization Status

Completed:

- repo layout refactor (DSP / firmware / host / hardware split)
- README pass for the new layout

In progress for V1:

- reproducible build/tooling baseline
- test framework + deterministic regression tests
- CI checks
- board bring-up documentation

## Why Fixed-Point Here

The DSP path is fixed-point by design in V1: explicit numeric behavior, deterministic control over arithmetic, and portability to targets without strong floating-point support.
