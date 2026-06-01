# Architecture

## Top-Level Modules

- [dsp](../dsp): portable effect and math code
- [firmware](../firmware): STM32 target implementation
- [host](../host): Python demo and ctypes bridge
- [hardware](../hardware): PCB and analog design files

## DSP Path

Core interfaces are in [dsp/effects/effects.h](../dsp/effects/effects.h).

Implemented effects:

- overdrive
- echo
- compression

Fixed-point helpers live in [dsp/math](../dsp/math). Signal convention is unsigned 16-bit with midpoint-centered audio.

## Firmware Path

The STM32 firmware is generated locally with CubeMX and is not tracked in Git.

Runtime model: FreeRTOS tasks, DMA-backed ADC/DAC transfer, and ping-pong sample buffers.

Main subsystems:

- effect model/state/control logic
- effect processing
- button/switch/pot handlers
- LED task
- I2C queue-driven peripheral path
- ROM/display support paths

Host-side firmware tests use op-vector style mocks for task boundaries. The test runtime stays host-only; HAL and FreeRTOS symbol mocks are reserved for modules that link those APIs directly.

## Host Demo Path

The host UI in [host/python-demo/main.py](../host/python-demo/main.py) calls the C effects library through [host/python-demo/effects.py](../host/python-demo/effects.py).

The binding mirrors C parameter structs and forwards sample buffers into:

- buf_overdrive
- buf_echo
- buf_compression

## Hardware Path

Board implementation and schematics are in [hardware/STM32AudioEffects](../hardware/STM32AudioEffects).

Analog simulations for selected subcircuits are in [hardware/LTspice](../hardware/LTspice).
