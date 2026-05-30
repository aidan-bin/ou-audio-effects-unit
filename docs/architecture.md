# Architecture

## Top-Level Modules

- [dsp](../dsp): portable effect and math code
- [firmware](../firmware): STM32 target implementation
- [host](../host): Python demo and ctypes bridge
- [hardware](../hardware): PCB and analog design files

## DSP Path

Core effect interfaces are declared in [dsp/effects/effects.h](../dsp/effects/effects.h).

Implemented effects:

- overdrive
- echo
- compression

Numeric support and fixed-point helpers live in [dsp/math](../dsp/math).

Signal convention in DSP code is unsigned 16-bit with midpoint as the x-axis.

## Firmware Path

The STM32 firmware is generated locally with CubeMX and is not tracked in Git.

The runtime model is FreeRTOS task-based with DMA-backed ADC/DAC audio movement and ping-pong sample buffers.

High-level subsystems in the firmware include:

- effect processing
- button/switch/pot handlers
- LED task
- I2C queue-driven peripheral path
- ROM/display support paths

## Host Demo Path

The host UI in [host/python-demo/main.py](../host/python-demo/main.py) calls the C effects library through [host/python-demo/effects.py](../host/python-demo/effects.py).

The binding layer mirrors the C parameter structs and forwards sample buffers into:

- buf_overdrive
- buf_echo
- buf_compression

## Hardware Path

Board implementation and schematics are in [hardware/STM32AudioEffects](../hardware/STM32AudioEffects).

Analog simulations for selected subcircuits are in [hardware/LTspice](../hardware/LTspice).
