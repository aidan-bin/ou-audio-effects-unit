# Board Bring-Up

First power-on and audio-path validation for the target.

## Prerequisites

- A build/flash environment that can open the CubeMX project
- Nucleo board or custom board assembled from [hardware/STM32AudioEffects](../hardware/STM32AudioEffects)
- Audio source and audio output chain
- Debug probe and USB connection for firmware flashing

## Bring-Up Checklist

1. Program firmware generated locally from the CubeMX project.
2. Confirm stable board power.
3. Confirm ADC input produces valid samples.
4. Confirm DAC output is audible.
5. Confirm baseline passthrough behavior.
6. Enable each effect one at a time and confirm control response:
   - overdrive
   - echo
   - compression
7. Confirm user input paths (switches, buttons, pots) influence state as expected.
8. Confirm EEPROM path is functional if configured.
