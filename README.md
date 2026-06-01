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
./scripts/run.sh demo
```

The demo launcher builds the shared DSP library, creates/uses a local virtualenv at `.local/python-demo-venv`, installs Python demo dependencies from `host/python-demo/requirements.txt`, and starts the UI.

## Firmware Snapshot

The STM32 firmware project is generated locally with CubeMX and is not checked into this repository.

- FreeRTOS task-based control flow
- ping-pong sample buffering
- DMA-driven ADC/DAC audio transfer
- merged effect parameter/state/control handling in the firmware app
- I2C infrastructure for peripherals (EEPROM and display path)

## Tests

Run from repository root:

```sh
./scripts/run.sh
```

`run.sh` defaults to the `check` workflow, which runs dependency preflight before build+test.

Run full verification (format + lint + build + tests):

```sh
./scripts/run.sh check-full
```

`check-full` runs dependency preflight before the full gate.

Or run tests only:

```sh
./scripts/run.sh test
```

Check formatting without modifying files:

```sh
./scripts/run.sh format-check
```

Clean generated artifacts:

```sh
./scripts/run.sh clean
```

Check local tool dependencies:

```sh
./scripts/run.sh doctor
```

## Hardware Snapshot

Board assets are under [hardware/STM32AudioEffects](hardware/STM32AudioEffects), with analog simulation files under [hardware/LTspice](hardware/LTspice).

## Why Fixed-Point Here

The DSP path is fixed-point by design: explicit numeric behavior, deterministic control over arithmetic, and portability to targets without strong floating-point support.
