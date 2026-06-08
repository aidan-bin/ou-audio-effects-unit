# CLI Reference

## Commands

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

#### State

- `state.active_effect` — 0=overdrive, 1=echo, 2=compression, default **0**
- `state.enable.overdrive` — 0/1, default **1**
- `state.enable.echo` — 0/1, default **0**
- `state.enable.compression` — 0/1, default **0**

#### Runtime

- `heartbeat.period_ms` — LED toggle period ms, range 50–5000, default **500**
- `system.heartbeat_ms` — alias for `heartbeat.period_ms`

### rom

EEPROM state persistence.

- `rom save-state` — save current state to EEPROM
- `rom load-state` — load state from EEPROM
- `rom read <addr> <len>` — raw hex dump (max 64 bytes)
- `rom write <addr> <hex>` — raw hex write

### log

- `log enable <0|1>` — enable/disable logging
- `log level <0–255>` — verbosity (0=off, 1=error, 2=warn, 3=info, 4=debug, 5=trace)
- `log stream [0|1] [batch <N>]` — live profiling stream (see [log stream](#log-stream))
- `log stats` — show counters (enabled, stream, level, frames, failures, stepfail, streak)
- `log stats reset` — reset all counters
- `log stats timing` — show timing stats (min, max, avg frame time in us, overruns, drops, batch size, queue depth)

### test

Inject test signals in place of ADC input / route to DAC output.

- `test input|output mode <0|1>` — enable/disable
- `test input|output vector <sine|lut|sweep>` — waveform type
- `test input|output freq <hz>` — range 20–8000
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

