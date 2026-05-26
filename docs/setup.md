# Setup

See [Documentation Index](README.md) for all project docs.

## Build Baseline

Run from repository root:

```sh
./scripts/check.sh
```

Optional tooling:

```sh
./scripts/build.sh
./scripts/format.sh
./scripts/lint.sh
./scripts/test.sh
./scripts/clean.sh
```

## Host Demo

Run from repository root:

```sh
./scripts/demo.sh
```

Equivalent explicit commands:

```sh
./scripts/build.sh
python3 -m pip install -r host/python-demo/requirements.txt
python3 host/python-demo/main.py
```

Notes:

- The Python binding in [host/python-demo/effects.py](../host/python-demo/effects.py) resolves the shared library relative to the module location.
- The loader supports libeffects.dll, libeffects.so, and libeffects.dylib.

## Firmware Source Location

- STM32 firmware entrypoint: [firmware/stm32f303/cubemx/main.c](../firmware/stm32f303/cubemx/main.c)
- CubeMX configuration: [firmware/stm32f303/cubemx/STM32F303RETx-Audio-Effects-Unit.ioc](../firmware/stm32f303/cubemx/STM32F303RETx-Audio-Effects-Unit.ioc)

## Hardware Source Location

- KiCad project and schematics: [hardware/STM32AudioEffects](../hardware/STM32AudioEffects)
- LTspice files: [hardware/LTspice](../hardware/LTspice)
