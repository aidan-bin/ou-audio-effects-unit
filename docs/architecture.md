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

Firmware runs as FreeRTOS tasks over a DMA ADC/DAC audio path with ping-pong buffers, enabling continuous fixed-rate processing.

Audio trigger/timing details and CubeMX settings are documented in [audio-sampling-design.md](audio-sampling-design.md).

Project-specific logic lives in [firmware/stm32f303/app](../firmware/stm32f303/app), which extends the CubeMX-generated firmware through user sections and app code.

Subsystems include:

- effects model/pipeline and audio task
- control tasks for pots, switches, and buttons
- peripheral services for I2C dispatch and ROM support/handlers

Testing uses a host-mockable HAL/RTOS abstraction layer, with tests in [tests/firmware](../tests/firmware).

## Host Demo Path

The host UI in [host/python-demo/main.py](../host/python-demo/main.py) calls the C effects library through [host/python-demo/effects.py](../host/python-demo/effects.py).

The binding mirrors C parameter structs and forwards sample buffers into the effects library.

## Hardware Path

Board implementation and schematics are in [hardware/STM32AudioEffects](../hardware/STM32AudioEffects).

Analog simulations for selected subcircuits are in [hardware/LTspice](../hardware/LTspice).
