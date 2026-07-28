# CLI Reference

## Commands

### info

Query audio system information.

- `info` — prints `info sr=<Hz> sf=<samples> sp=<us> ds=<samples> sl=<ms>`

- `sr` - sample rate in Hz
- `sf` - audio frame size (samples per buffer half)
- `sp` - sampling period in microseconds
- `ds` - delay line length in samples
- `sl` - processing deadline slack in ms

### help, ping

- `help [command]` — list all commands, or show usage for one.
- `ping` — responds `pong`.

### override

Override hardware inputs for testing.

- `override pot set <index> <value>` — set pot override (index 0–3, value = raw ADC)
- `override pot clear <index>`
- `override switch set <index> <0|1>`
- `override switch clear <index>`
- `override clear-all`

### config

Get/set runtime configuration. Values parsed as `int32_t`.

`config get <key>`, `config set <key> <value>`

#### Effect Parameters

Q8 fixed-point where noted: `256 = 1.0`.

- `overdrive.gain` — output gain (Q8), range 0–256, default **256**
- `overdrive.level` — wet signal saturation amplitude, range 0–32767, default **32767**
- `overdrive.tone` — tone filter cutoff (Q8), range 256–1280, default **256** (below 256 inverts signal)
- `overdrive.mix` — wet/dry ratio (Q8), range 0–256, default **0** (0 = dry, 256 = fully wet)
- `echo.delay_samples` — buffer depth for echo, range 0–2207, default **1**
- `echo.pre_delay` — samples before first echo, range 1–2207, default **1**
- `echo.density` — echo density (Q8), range 0–256, default **256**
- `echo.attack` — gain on first echo (Q8), range 0–256, default **256**
- `echo.decay` — gain reduction per subsequent echo (Q8), range 0–256, default **256**
- `compression.threshold` — amplitude threshold, range 0–32767, default **32767**
- `compression.ratio` — gain reduction amount (Q8; 0 = off, 256 = hard clip), range 0–256, default **0**

#### Runtime

- `heartbeat.period_ms` — LED toggle period ms, range 50–5000, default **500**
- `system.heartbeat_ms` — alias for `heartbeat.period_ms`

### slot

The effects chain is a set of slots (`NUM_SLOTS`, default 4) that is separate from the catalog of effect *types*. Each slot is empty or holds one effect (one instance per effect type); the chain runs the assigned, enabled slots in order.

- `slot [get]` — list slots, e.g. `slots 0=overdrive(on) 1=echo(off) 2=compression(off) 3=empty`
- `slot set <pos> <effect>` — assign an effect to a slot; if it is already in another slot it is moved (one instance per effect)
- `slot clear <pos>` — empty a slot
- `slot swap <a> <b>` — swap two slots (assignment and enable move together)
- `slot enable <pos> <0|1>` — enable/disable a slot from the CLI; authoritative for slots with no physical switch (switch-backed slots 0–2 are re-driven by the switch each poll — use `override switch` to force those)
- `slot active [<pos>]` — get or set the active slot (the one the pots edit); prints `active slot <pos> <effect|empty>`

Slot assignment and enables are persisted state, so `rom save-state` (and the periodic auto-save) retain them across reboot.

The control mapping is *positional*: switches bind to slot positions, and the pots edit the effect in the active slot (`slot active`), not a fixed effect. Re-ordering/re-assigning therefore changes which effect each switch and pot controls.

### effects

- `effects` — list the catalog of effect types and where each is assigned, e.g. `effects overdrive@0 echo(unassigned) compression@2`

To enable an effect: assign it to a slot (`slot set <pos> <effect>`), then enable it — a switch for slots 0–2, or `slot enable <pos> 1` for a CLI-only slot.

### rom

EEPROM state persistence.

- `rom save-state` — save current state to EEPROM
- `rom load-state` — load state from EEPROM
- `rom read <addr> <len>` — raw hex dump (max 64 bytes)
- `rom write <addr> <hex>` — raw hex write

Note that the EEPROM tracks version/layout and rejects mismatches on load (falling back to defaults).

### log

- `log enable <0|1>` — enable/disable logging
- `log level <0–255>` — verbosity (0=off, 1=error, 2=warn, 3=info, 4=debug, 5=trace)
- `log stream [batch <N>]` — live profiling stream; press `q` to stop (see [log stream](#log-stream))
- `log stats` — show counters (enabled, stream, level, frames, failures, stepfail, streak)
- `log stats reset` — reset all counters
- `log stats timing` — show timing stats (min, max, avg frame time in us, overruns, drops, batch size, queue depth)

### audio

Control audio routing between analog I/O and USB streaming.

- `audio input adc` — effects pipeline fed from ADC (standalone / analog)
- `audio input usb` — effects pipeline fed from USB CDC audio port (host provides audio)
- `audio output on` — processed output streamed to host on USB CDC audio port
- `audio output off` — processed output goes to DAC only (not streamed over USB)
- `audio status` — show current routing state (`input=adc|usb`, `output=on|off`)

Default: `audio input adc`, `audio output off` (standalone analog mode).

### reboot

- `reboot` — confirms then resets the device.

### sysinfo

Compile-time system information. One `key=value` per line.

- `sysinfo` — prints board, MCU, feature flags, and protocol hints.

Includes:

- `audio_in` / `audio_out` — USB audio streaming support flags
- `audio_routing` — USB audio protocol hint (`uac1`)
- `version` — firmware/protocol version string
- `board` / `mcu` — hardware identifiers set at build time

### test

Inject test signals in place of ADC input / route to DAC output.

- `test input|output mode <0|1>` — enable/disable
- `test input|output vector <sine|lut|sweep|wav|impulse|usb>` — waveform type
    - `sine` — sine wave LUT with linear interpolation
    - `lut` — user-defined fixed waveform LUT
    - `sweep` — frequency sweep from 20 Hz to fs/2
    - `wav` — sequential playback of a compile-time WAV LUT at native rate; `freq` acts as per-mille pitch multiplier (1000 = 1.0x)
    - `impulse` — single impulse when `freq=0`, or periodic at Hz when `freq>0`; amplitude set via `amp`
    - `usb` — samples pulled from USB audio input stream ring buffer
- `test input|output freq <hz>` — min 20
- `test input|output amp <value>` — max 32767
- `test input|output status` — show config
- `test status` — show both input and output config

## log stream

Provides a live stream of profiling lines for each processed audio frame, with optional batching to reduce overhead (default 1, max 255).

### Profiling line

```
prof f=<N> t=<avg_us> mn=<min_us> mx=<max_us> ov=<overruns> dr=<drops>
```

- `f` — frame count in this batch (`batchFrameCount`)
- `t` — average frame time in us (`batchFrameTimeTotalUs / batchFrameCount`)
- `mn` — minimum frame time in us in batch (`batchFrameTimeMinUs`)
- `mx` — maximum frame time in us in batch (`batchFrameTimeMaxUs`)
- `ov` — overrun count in batch (`batchOverrunCount`)
- `dr` — cumulative stream drops (`streamDropCount`, persists across batches)

An overrun is declared when `frame_time_us > samplingPeriodUs * sampleBufLen + processingSlackMs * 1000`.

### Timing stats line

```
log timing mn=<min> mx=<max> avg=<avg> meas=<count> over=<overruns> drop=<drops> bs=<batchSize> q=<queueDepth>
```

