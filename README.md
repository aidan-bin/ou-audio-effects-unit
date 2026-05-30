# STM32 Audio Effects Unit

![Board](hardware/STM32AudioEffects/STM32AudioEffectsUnit.jpg)

Embedded STM32 audio effects project with fixed-point DSP in C, host-side effect demo tooling, and board design files.

Includes overdrive/echo/compression DSP in C, STM32 firmware with FreeRTOS + DMA audio I/O, a Python host demo, and KiCad/LTspice hardware assets.

## Repository Layout

- [dsp](dsp): portable DSP and math code
- [firmware](firmware): STM32 target firmware
- [host](host): host-side demo tooling
- [hardware](hardware): board and analog design assets
- [tests](tests): test assets
- [docs](docs): setup, architecture, bring-up notes
- [scripts](scripts): helper scripts

## Quick Start: Host Demo

From the repository root:

```sh
./scripts/demo.sh
```

The demo launcher builds the shared DSP library, creates/uses a local virtualenv at `.local/python-demo-venv`, installs Python demo dependencies from `host/python-demo/requirements.txt`, and starts the UI.

## Firmware Snapshot

The STM32 firmware project is generated locally with CubeMX and is not checked into this repository.

- FreeRTOS task-based control flow
- ping-pong sample buffering
- DMA-driven ADC/DAC audio transfer
- effect parameter/state handling
- I2C infrastructure for peripherals (EEPROM and display path)

## Tests

Run from repository root:

```sh
./scripts/check.sh
```

`check.sh` runs dependency preflight (`doctor.sh`) before build+test.

Run full verification (format + lint + build + tests):

```sh
./scripts/check-full.sh
```

`check-full.sh` runs dependency preflight (`doctor.sh`) before the full gate.

Or run tests only:

```sh
./scripts/test.sh
```

Check formatting without modifying files:

```sh
./scripts/format-check.sh
```

Clean generated artifacts:

```sh
./scripts/clean.sh
```

Check local tool dependencies:

```sh
./scripts/doctor.sh
```

## Hardware Snapshot

Board assets are under [hardware/STM32AudioEffects](hardware/STM32AudioEffects), with analog simulation files under [hardware/LTspice](hardware/LTspice).

## Why Fixed-Point Here

The DSP path is fixed-point by design: explicit numeric behavior, deterministic control over arithmetic, and portability to targets without strong floating-point support.
