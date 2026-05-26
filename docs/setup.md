# Setup

## Build Baseline

Run from repository root:

```sh
./scripts/build.sh
```

Optional tooling:

```sh
./scripts/format.sh
./scripts/lint.sh
./scripts/test.sh
```

## Host Demo

Run from repository root:

```sh
./scripts/build.sh
cd host/python-demo
python3 -m pip install -r requirements.txt
python3 main.py
```

Notes:

- The Python binding in [host/python-demo/effects.py](host/python-demo/effects.py) loads a library named libeffects.dll from the current working directory.
- On non-Windows hosts the output name is still libeffects.dll because the binding currently hardcodes that filename.

## Firmware Source Location

- STM32 firmware entrypoint: [firmware/stm32f303/cubemx/main.c](firmware/stm32f303/cubemx/main.c)
- CubeMX configuration: [firmware/stm32f303/cubemx/STM32F303RETx-Audio-Effects-Unit.ioc](firmware/stm32f303/cubemx/STM32F303RETx-Audio-Effects-Unit.ioc)

## Hardware Source Location

- KiCad project and schematics: [hardware/STM32AudioEffects](hardware/STM32AudioEffects)
- LTspice files: [hardware/LTspice](hardware/LTspice)
