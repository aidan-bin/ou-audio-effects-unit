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
- CLI for live parameter editing and test mode
- peripheral services for I2C dispatch and ROM support/handlers
- USB audio streaming

Testing uses a host-mockable HAL/RTOS abstraction layer, with tests in [tests/firmware](../tests/firmware).

### USB

Dual CDC ACM composite with a custom class driver in [usbd_cdc_dual.c](../firmware/stm32f303/app/src/usbd_cdc_dual.c) presenting two virtual COM ports:

- CDC0 (interfaces 0–1, EP1 Bulk OUT/IN, EP2 Interrupt IN) — text CLI
- CDC1 (interfaces 2–3, EP3 Bulk OUT/IN, EP4 Interrupt IN) — raw int16_t LE audio samples

CLI TX is mirrored to both CDC0 and USART2; CLI RX is merged from both sources into one `CliSession`.

Audio samples pass through ring buffers in [usb_audio_stream.c](../firmware/stm32f303/app/src/usb_audio_stream.c) (2048 samples each) with no host-side framing.

Host discovery: open CDC0, issue `sysinfo` to read `version`/`routing`/`board`/`mcu`, then stream audio on CDC1.

## Host Demo Path

The host UI in [host/python-demo/main.py](../host/python-demo/main.py) calls the C effects library through [host/python-demo/effects.py](../host/python-demo/effects.py).

The binding mirrors C parameter structs and forwards sample buffers into the effects library.

## Hardware Path

Board implementation and schematics are in [hardware/STM32AudioEffects](../hardware/STM32AudioEffects).

Analog simulations for selected subcircuits are in [hardware/LTspice](../hardware/LTspice).
