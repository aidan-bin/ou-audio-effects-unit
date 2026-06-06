# Audio ADC/DAC/Timer Quick Reference

## Model

- ADC3: free-running continuous conversion into circular DMA.
- TIM2: sample-rate master clock.
- DAC1 CH1: one output sample per TIM2 update (TRGO).
- DSP: processes on ADC DMA half/complete callbacks.

Key point: TIM2 sets audio sample rate. ADC sampling-time cycles do not set output sample rate.

## Trigger Path

- TIM2 master trigger: `TIM_TRGO_UPDATE`
- DAC trigger: `DAC_TRIGGER_T2_TRGO`
- ADC trigger: software start + continuous mode

## Formulas

Timer-controlled sample rate:

$$
F_s = \frac{F_{TIM2}}{(PSC + 1)(ARR + 1)}
$$

Sample period:

$$
T_s = \frac{1}{F_s}, \quad T_{s,\mu s} = \frac{1{,}000{,}000}{F_s}
$$

ADC conversion time (12-bit regular conversion):

$$
T_{conv} \approx \frac{N_{sample} + 12.5}{F_{ADC}}
$$

`12.5` is the fixed F3 SAR conversion phase (cycles).

## CubeMX Values For This Project

- `F_TIM2 = 48 MHz`
- `F_ADC34 = 48 MHz`
- ADC3 clock prescaler: `ADC_CLOCK_ASYNC_DIV1`
- ADC3 mode: `ContinuousConvMode = ENABLE`
- ADC3 DMA: `DMA2_Channel5`, circular, halfword
- DAC DMA: `DMA1_Channel3`, circular, halfword

## Set 40.5 kHz (recommended settings)

In TIM2:

- `Prescaler = 0`
- `Period = 1184`
- `Master Output Trigger = Update Event (TRGO)`

In DAC1 CH1:

- `Trigger = TIM2 TRGO`

Result:

$$
F_s = \frac{48{,}000{,}000}{(0+1)(1184+1)} = 40{,}506\,\text{Hz}
$$

For near 44.1 kHz: `PSC = 0`, `ARR = 1088` (about `44,077 Hz`).

## ADC Sampling-Time Guidance

- Start at 7.5 or 19.5 cycles for margin.
- Use higher sampling-time cycles if source impedance is high or noise/jitter appears.
- Keep conversion throughput comfortably above target audio rate.

Example at `F_ADC = 48 MHz`:

- `1.5 cycles`: $(1.5 + 12.5)/48e6 = 0.292\,\mu s$ (about `3.43 MSPS` max)
- `19.5 cycles`: $(19.5 + 12.5)/48e6 = 0.667\,\mu s$ (about `1.50 MSPS` max)

Both are far above `40.5 kSPS`.
