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
9. Confirm USB/USART CLI or serial logs if configured.

## Nucleo-Specific Notes

### Input Configuration

The ADC is internally connected to OPAMP3 in follower mode to buffer the input signal. No external OPAMP-ADC wiring is required.

To bias the input signal, you can create a mid-rail reference using a resistor divider (e.g., two 10k resistors) between Vdda and GND, with the input signal AC-coupled to the junction with a 10uF capacitor.

For anti-aliasing, you can use an RC low-pass filter on the input. For example, a 1k resistor in series with the biased input and a 2.2nF capacitor from the OPAMP3 input (PB13) to ground; this has a cutoff frequency of around 16kHz.

### Output Configuration

If you have a piezo, you can connect one terminal to the DAC output (PA4) and the other to GND. It is recommended to add a series resistor (~1k) for protection, a series capacitor (~1uF) for AC coupling (since the DAC output is DC biased), and a parallel resistor (~1M) for discharge.

OPAMP2 is configured in follower mode to buffer the DAC output (useful since the DAC has limited drive capability). You can connect the DAC output (PA4) to the OPAMP2 VINP (PA7) and the OPAMP2 output (PA6) to the piezo.

## Calibration and Tuning Notes

Run these checks during bring-up and whenever board-level analog behavior changes.

1. Confirm audio sample rate from TIM2 settings (PSC/ARR), not ADC sample-time cycles.
2. Tune ADC sample time for stability margin:
   - Start from a moderate value (for example 7.5 or 19.5 cycles for buffered sources).
   - Increase if you observe noise, distortion, or transient settling artifacts.
3. Verify ADC input bias/midpoint and headroom:
   - Confirm idle signal sits near expected midpoint.
   - Confirm loud input does not clip at ADC full scale.
4. Verify DAC output headroom and offset:
   - Confirm no unexpected DC offset at output.
   - Confirm output amplitude is within analog stage limits.
5. Calibrate analog gain staging end-to-end:
   - Input gain into OPAMP/ADC path.
   - Output level from DAC/analog output path.
6. Validate timing and buffering health:
   - No DMA overruns/underruns.
   - No audible glitches at buffer boundaries.
7. Validate controls with real hardware travel:
   - Pot range mapping and endpoint behavior.
   - Switch/button debounce and response latency.
8. Re-check effect defaults after calibration:
   - Overdrive, echo, and compression should be musically usable at default values.
