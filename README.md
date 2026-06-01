# STM32 Audio Effects Unit

![Board](hardware/STM32AudioEffects/STM32AudioEffectsUnit.jpg)

STM32 audio effects project with fixed-point DSP in C, STM32 firmware, a Python demo UI, and hardware design files.

## Repository Layout

- [dsp](dsp): portable DSP and math code
- [firmware](firmware): STM32 target firmware
- [host](host): host-side demo tooling
- [hardware](hardware): board and analog design assets
- [tests](tests): test assets
- [docs](docs): setup, architecture, bring-up notes
- [scripts](scripts): helper scripts

## Quick Start: Host Demo

```sh
./scripts/run.sh demo
```

Builds the DSP library, prepares a local demo virtualenv, installs demo deps, and starts the UI.

## Firmware Snapshot

The STM32 firmware project is generated locally with CubeMX and is not checked into this repository.

- FreeRTOS task-based control flow
- DMA-driven ADC/DAC audio transfer
- ping-pong sample buffering
- effect model/state/control and processing tasks
- I2C path for peripherals (EEPROM/display)

## Tests

```sh
./scripts/run.sh
```

Default check flow (preflight + build + tests).

Full gate (format + lint + build + tests):

```sh
./scripts/run.sh check-full
```

Tests only:

```sh
./scripts/run.sh test
```

Format check only:

```sh
./scripts/run.sh format-check
```

Clean artifacts:

```sh
./scripts/run.sh clean
```

Dependency check:

```sh
./scripts/run.sh doctor
```

## Hardware Snapshot

Board assets are under [hardware/STM32AudioEffects](hardware/STM32AudioEffects), with analog simulation files under [hardware/LTspice](hardware/LTspice).

## Why Fixed-Point Here

Fixed-point is used for deterministic arithmetic and portability on constrained targets.
