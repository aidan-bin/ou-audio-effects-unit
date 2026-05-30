# Board Bring-Up

## Scope

This checklist is for first power-on and first audio-path validation on the STM32 target using the local CubeMX-generated firmware project.

## Prerequisites

- A build/flash environment that can open the CubeMX project
- Target board assembled from [hardware/STM32AudioEffects](../hardware/STM32AudioEffects)
- Audio source and audio output chain
- Debug probe and USB connection for firmware flashing

## Bring-Up Checklist

1. Program firmware generated locally from the CubeMX project.
2. Confirm board powers correctly and remains stable.
3. Confirm ADC input path produces valid samples.
4. Confirm DAC output path produces audible output.
5. Confirm baseline passthrough audio behavior.
6. Enable each effect one at a time and confirm control response:
   - overdrive
   - echo
   - compression
7. Confirm user input paths (switches, buttons, pots) influence state as expected.
8. Confirm EEPROM path is functional if configured.

## Known Current Limit

Display/OLED support path exists in firmware, but display behavior should be treated as a separate validation item from core audio bring-up.

## Failure Triage

- No audio out: verify power rails, codec analog path, and DAC DMA activity.
- Distorted passthrough: check input biasing and output filter network against [hardware/LTspice](../hardware/LTspice) assumptions.
- Controls unresponsive: verify GPIO and task scheduling in the generated firmware source.
- Peripheral issues: verify I2C addressing and queue flow in firmware I2C task path.
