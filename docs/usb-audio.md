# USB Audio Architecture

## Overview

The device is a Full-Speed composite USB peripheral that combines:

- **CDC ACM** — virtual COM port carrying the text CLI.
- **UAC1** — mono 16-bit PCM playback (host → device) and capture (device → host) at 48 kHz.

The ST USBD core only supports one registered class per device, so a dispatcher in [composite.c](../firmware/stm32f303/app/src/usb/composite.c) owns the combined configuration descriptor and forwards each core callback to the CDC or UAC sub-driver by interface / endpoint number.

| Iface | Class       | Purpose                    | Endpoints                         |
| :---- | :---------- | :------------------------- | :-------------------------------- |
| 0–1   | CDC         | CLI text                   | EP1 bulk OUT/IN, EP2 interrupt IN |
| 2     | UAC1 AC     | Audio control (topology)   | none                              |
| 3     | UAC1 AS-OUT | Host → device (playback)   | EP5 isoc OUT, sync                |
| 4     | UAC1 AS-IN  | Device → host (capture)    | EP3 isoc IN, async                |

## Sample Path

Two SPSC ring buffers in `UsbAudioStream` ([audio_stream.h](../firmware/stm32f303/app/include/usb/audio_stream.h)) sit between the USB ISR and the effects task (2048 samples each, ~42 ms @ 48 kHz):

- `in_ring`  — AS-OUT ISR writes, effects task reads.
- `out_ring` — effects task writes, AS-IN ISR reads.

**Playback**: EP5 delivers 48-sample packets every SOF; `usbd_uac_data_out` pushes them into `in_ring`. With `audio input usb`, the effects task's input hook drains `in_ring` in place of the ADC buffer.

**Capture**: with `audio output on`, the effects task pushes processed samples into `out_ring`; `usbd_uac_transmit_as_in_packet` pops one packet's worth into EP3 IN. Scheduling is **data-in-driven** — the next packet is armed from the `DataIn` callback, not SOF — so the PMA slot is never overwritten while a packet is still pending.

**Routing** is a runtime CLI toggle, no USB reconfiguration:

- `audio input adc|usb` — swap the effects task's input source.
- `audio output on|off` — gate the push into `out_ring`.

## Notable Design Choices

Most of these look odd in isolation. Each fixes a specific host-interop or hardware quirk.

- **AS-OUT sync, no feedback endpoint.** macOS's `AUALockDelay` check on the feedback endpoint kept failing and cycling the audio session. Declaring sync sidesteps it; the feedback endpoint and its rate-control code have been removed.
- **AS-IN async.** Device is the sample-clock source; declaring sync here made macOS cycle both streams.
- **AS-OUT on EP5, not EP3.** F303 double-buffered OUT reuses the BTABLE `ADDR_TX` slot, which collides with any IN endpoint on the same EP number. EP5 keeps EP3's slot free for AS-IN.
- **Full-length isoc IN packets, zero-padded on underrun.** macOS gives up on AS-IN after a few empty packets and tears down AS-OUT too. Underruns are counted as `as_in_underrun`.
- **Silence on `in_ring` underrun, not sample-hold.** Repeating the last sample turns starvation into a loud beep at the DSP frame rate (~100 Hz). Counted as `effects_zoh_holds`.
- **Single 48 kHz rate.** `SAMPLING_FREQ_CONTROL` `SET_*` requests are accepted and discarded; `GET_*` all return 48000. `UAC_SAMPLE_RATE_HZ` must match `main.c`'s TIM2 divider.

## USB CLI

CDC0 is the primary CLI transport. The same session is also mirrored to USART2 on the ST-Link VCP, so you can drive it from either — RX from either source feeds one `CliSession`, TX is written to both.

Host tool: [host/usb-audio/ouaudio.py](../host/usb-audio/ouaudio.py).

- Auto-discovers the CDC port by VID/PID (`0483:5740`), falling back to ST-Link VCP.
- `--list` prints ports, sounddevice matches, and CLI-derived status.
- `--input {mic|FILE.wav} --output {speaker|FILE.wav}` streams through the effects chain, toggling `audio input usb` / `audio output on` for the run.
- `--effect / --enable / --disable / --set key=val` configure the chain over CLI before streaming.
- `--diag` resets and prints `audio diag` counters around the run.

Device-side CLI grammar: [cli-reference.md](cli-reference.md).

## Diagnostics

`audio diag` (and `audio diag reset`) exposes two counter sets:

- **Stream-level** (`UsbAudioStream`): `as_out_packets`, `in_ring_fill_peak/current`, `effects_frames`, `effects_zoh_holds`, `drops_out`, `drops_in`.
- **UAC-driver level** (`UsbdUacDiag`, [uac_diag.h](../firmware/stm32f303/app/include/usb/uac_diag.h)): AS-OUT ISR / push counts (`uac_isr_total`, `uac_pushed`), `SET_INTERFACE` alt transitions (`setalt_out`, `setalt_in`), AS-IN tx / underrun counts.

Host-only tests: [test_firmware_usb_audio_stream.c](../tests/test_firmware_usb_audio_stream.c) (ring behavior), [test_firmware_usbd_uac_feedback.c](../tests/test_firmware_usbd_uac_feedback.c) (feedback encoding). End-to-end validation is manual via `ouaudio.py`.

## Fault Debugging

On a hard fault the handler dumps CPU registers plus USB peripheral state (`CNTR`, `ISTR`, `FNR`, `DADDR`, every `EPnR`) over USART2 with no interrupt or DMA dependency. Capture and decode as described in [setup.md](setup.md#monitoring-the-st-link-serial-port-usart2). The register snapshot plus call stack usually pinpoints what the USB core was doing at the moment of the fault.
