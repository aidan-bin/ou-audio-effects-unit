# STM32 Audio Effects Unit

![Board v1](hardware/STM32AudioEffects/STM32AudioEffectsUnit.jpg)

Embedded audio effects platform built around STM32, with portable DSP code in C, a host-side demo path, and full board design assets.

This project is being modernized as a portfolio-quality embedded systems repo. The current codebase already demonstrates:

- real-time audio processing on STM32 with ADC/DAC DMA and FreeRTOS tasking
- fixed-point DSP implementation in C for effect processing and numeric control
- portable host-side effect execution through a Python UI and C shared library
- mixed hardware and firmware ownership: KiCad PCB, LTspice analog validation, MCU firmware, and DSP code

## Highlights

- Effects implemented in C: overdrive, echo, compression
- Firmware pipeline built around low-latency sample buffering and DMA-driven I/O
- Fixed-point parameterization and effect math designed for explicit numeric control
- Board-level design includes analog front-end, output filtering, user I/O, EEPROM, and power stages
- Host demo provides a fast way to inspect and audition effect behavior without flashing hardware

## Skills Demonstrated

- embedded C
- low-level firmware design
- real-time audio pipelines
- DMA and interrupt-driven I/O
- fixed-point DSP
- hardware and firmware co-design
- board design in KiCad
- analog verification in LTspice

## Repository Layout

- `dsp/` portable DSP and numeric code
- `firmware/` target-specific firmware and CubeMX assets
- `hardware/` KiCad project and LTspice schematics
- `host/` host-side demo application
- `tests/` transitional test assets; full automated coverage is part of the modernization work
- `docs/` project documentation, setup, architecture, and bring-up notes
- `scripts/` helper scripts and workflow tooling

## Current Structure

### DSP

- `dsp/effects/` effect implementations and public effect interfaces
- `dsp/math/` fixed-point helpers and math support code

### Firmware

- `firmware/stm32f303/cubemx/` current STM32 firmware entrypoint and MCU configuration assets

### Host Demo

- `host/python-demo/` PyQt-based demo UI, Python bindings, and sample media

### Hardware

- `hardware/STM32AudioEffects/` KiCad project, schematics, PCB, and custom footprints
- `hardware/LTspice/` analog simulation files for selected subcircuits

## Firmware Snapshot

Current firmware work includes:

- FreeRTOS-based task structure
- ADC and DAC with DMA for block-based audio processing
- ping-pong sample buffering
- effect parameter handling for user controls
- I2C integration for EEPROM and display-facing infrastructure

The firmware is currently centered around the CubeMX-generated STM32F303 target under `firmware/stm32f303/cubemx/`.

## Host Demo: Current Run Path

The host demo is still in pre-tooling-refresh form. Today it expects a shared library named `libeffects.dll` in `host/python-demo/`.

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
- The demo is useful for host-side effect inspection, but it has not been packaged yet.
- Live host audio is planned as part of the modernization track, but it is not the current baseline.

## Hardware Snapshot

Board-level features in the current design:

- 1/4 in audio input and output
- OLED display
- EEPROM for stored configuration
- user controls via potentiometers, buttons, and switches
- analog input/output conditioning and filtering
- support for wall adapter, battery, and USB-derived power paths

## Modernization Status

The repository is in active V1 cleanup and modernization.

Completed so far:

- top-level layout refactor to separate DSP, firmware, hardware, host, tests, and docs

Planned in V1:

- README and setup overhaul
- portable build system
- linting and formatting
- unit and integration tests
- CI validation
- board bring-up documentation

## Why Fixed-Point Here

This codebase uses fixed-point arithmetic intentionally. The goal is not just raw performance; it is to make numeric behavior explicit, keep the DSP core portable, and demonstrate control over embedded arithmetic tradeoffs.

## Status

This is not yet the final polished V1 state. The structure is now cleaner, but build tooling, tests, and setup ergonomics are still being upgraded.
