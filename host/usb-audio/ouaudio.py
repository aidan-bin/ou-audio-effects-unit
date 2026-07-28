"""Host-side control/streaming tool for the OU audio effects unit.

The device exposes a text CLI over USB CDC ACM and, since the UAC1 rework, a
standard USB Audio Class 1.0 interface for the audio path — no custom driver
or protocol needed on the audio side. This tool drives the CLI over a serial
port and streams audio through the OS's own audio driver (via `sounddevice`),
exactly like any other microphone/speaker the device shows up as.

Usage:
    python ouaudio.py --list
    python ouaudio.py --input in.wav --output out.wav --enable overdrive
    python ouaudio.py --input mic --output speaker --effect echo   # Ctrl-C to stop

Dependencies: pyserial numpy sounddevice

Requires the device's native USB (VID:PID 0483:5740) for the CLI and to be
recognized as the "ou-audio-effects" audio device by the OS. ST-Link VCP
(0483:374B) can be used for CLI-only access when the native cable isn't
connected, but that leaves no audio device to stream through. The device is
the sample clock (~40506 Hz); host audio is resampled accordingly.
"""

from __future__ import annotations

import argparse
import logging
import signal
import sys
import threading
import time
import wave
from collections.abc import Iterator, Mapping
from contextlib import contextmanager
from dataclasses import dataclass
from pathlib import Path

import numpy as np
import serial
from serial.tools import list_ports
from serial.tools.list_ports_common import ListPortInfo

log = logging.getLogger("ouaudio")

# --- Constants and helpers ---

VID, PID = 0x0483, 0x5740  # native composite device (CLI + audio)
STLINK_VID, STLINK_PID = 0x0483, 0x374B  # ST-Link VCP (CLI mirror, fallback)

UAC_DEVICE_NAME = "ou-audio-effects"  # must match USBD_PRODUCT_STRING_FS in usbd_desc.c

DEFAULT_DEVICE_RATE = 40506  # 48 MHz / 1185; fallback if `info` unavailable
FRAME_SAMPLES = 405  # ~10 ms pacing unit at 40506 Hz
CLI_PROMPT = "> "

EFFECT_NAMES = ("overdrive", "echo", "compression")
INT16_MIN, INT16_MAX = -32768, 32767


def to_int16(samples: np.ndarray) -> np.ndarray:
    return np.clip(np.rint(samples), INT16_MIN, INT16_MAX).astype(np.int16)


# --- Resampler ---


class Resampler:
    """Linear interpolation with fractional-phase carry across chunks."""

    def __init__(self, src_rate: float, dst_rate: float) -> None:
        self.src_rate, self.dst_rate = float(src_rate), float(dst_rate)
        self._step = self.src_rate / self.dst_rate
        self._pos = 0.0
        self._prev: float | None = None

    @property
    def passthrough(self) -> bool:
        return self.src_rate == self.dst_rate

    def process(self, samples: np.ndarray) -> np.ndarray:
        x = np.asarray(samples, dtype=np.float64)
        if self.passthrough:
            return x.copy()
        if self._prev is not None:
            x = np.concatenate(([self._prev], x))
        n = x.size
        if n < 2:
            if n == 1:
                self._prev = float(x[0])
            return np.empty(0, dtype=np.float64)

        last = n - 1
        positions = np.arange(self._pos, last, self._step, dtype=np.float64)
        if positions.size == 0:
            self._prev = float(x[-1])
            self._pos -= last
            return np.empty(0, dtype=np.float64)

        left = np.floor(positions).astype(np.intp)
        frac = positions - left
        out = x[left] * (1.0 - frac) + x[left + 1] * frac
        self._pos = positions[-1] + self._step - last
        self._prev = float(x[-1])
        return out

    def flush(self) -> np.ndarray:
        if self.passthrough or self._prev is None or self._pos > 0.0:
            self._pos, self._prev = 0.0, None
            return np.empty(0, dtype=np.float64)
        out = np.array([self._prev], dtype=np.float64)
        self._pos, self._prev = 0.0, None
        return out


# --- CLI control port ---


class DeviceUnsupportedError(RuntimeError):
    pass


class CliError(RuntimeError):
    pass


@dataclass(frozen=True)
class DeviceInfo:
    sample_rate_hz: int
    frame_samples: int
    delay_samples: int


@dataclass(frozen=True)
class Slot:
    pos: int
    effect: str | None
    enabled: bool


class CliPort:
    """Serial CLI port. Paces writes to avoid ST-Link/USART2 byte drops."""

    def __init__(self, port: str, *, char_delay_s: float = 0.01, timeout_s: float = 2.0) -> None:
        self._serial = serial.Serial(port, 115200, timeout=0.05)
        self._char_delay_s = char_delay_s
        self._timeout_s = timeout_s
        self.port = port
        time.sleep(0.1)
        self._serial.reset_input_buffer()

    def close(self) -> None:
        self._serial.close()

    def command(self, text: str, *, timeout_s: float | None = None) -> list[str]:
        self._serial.reset_input_buffer()
        log.debug("CLI[%s] tx: %r", self.port, text)
        for char in text + "\n":
            self._serial.write(char.encode("ascii"))
            self._serial.flush()
            if self._char_delay_s:
                time.sleep(self._char_delay_s)

        start = time.monotonic()
        deadline = start + (timeout_s or self._timeout_s)
        buffer = bytearray()
        while time.monotonic() < deadline:
            chunk = self._serial.read(256)
            if chunk:
                buffer.extend(chunk)
                if buffer.endswith(CLI_PROMPT.encode("ascii")):
                    break
            else:
                time.sleep(0.005)

        text_body = buffer.decode("ascii", "replace").replace("\r", "")
        lines = [ln.strip() for ln in text_body.split("\n")]
        lines = [ln for ln in lines if ln and ln != CLI_PROMPT.strip()]
        if lines and lines[0] == text:
            lines = lines[1:]
        log.debug(
            "CLI[%s] rx (%d lines, %.1f ms)",
            self.port,
            len(lines),
            (time.monotonic() - start) * 1000.0,
        )
        return lines

    @staticmethod
    def _expect_ok(lines: list[str]) -> None:
        for line in lines:
            if line.startswith("err"):
                raise CliError(line)

    # -- queries --
    def ping(self) -> bool:
        return "pong" in self.command("ping", timeout_s=1.0)

    def info(self) -> DeviceInfo:
        for line in self.command("info"):
            if line.startswith("info "):
                f = dict(t.split("=", 1) for t in line.split() if "=" in t)
                return DeviceInfo(int(f["sr"]), int(f["sf"]), int(f["ds"]))
        raise CliError("device did not return an 'info' line")

    def sysinfo(self) -> dict[str, str]:
        return dict(
            ln.split("=", 1)
            for ln in self.command("sysinfo")
            if "=" in ln  # noqa: C416
        )

    def verify_audio_support(self, *, need_input: bool, need_output: bool) -> dict[str, str]:
        info = self.sysinfo()
        tag = f"board={info.get('board', '?')} version={info.get('version', '?')}"
        if info.get("audio_routing") != "uac1":
            raise DeviceUnsupportedError(
                f"audio_routing={info.get('audio_routing')!r} (want uac1); {tag}"
            )
        if need_input and info.get("audio_in") != "1":
            raise DeviceUnsupportedError(f"no USB audio input (audio_in!=1); {tag}")
        if need_output and info.get("audio_out") != "1":
            raise DeviceUnsupportedError(f"no USB audio output (audio_out!=1); {tag}")
        return info

    def audio_status(self) -> dict[str, str]:
        status: dict[str, str] = {}
        for line in self.command("audio status"):
            status.update(t.split("=", 1) for t in line.split() if "=" in t)
        return status

    # -- routing --
    def set_input(self, source: str) -> None:
        self._expect_ok(self.command(f"audio input {source}"))  # adc|usb

    def set_output(self, on: bool) -> None:
        self._expect_ok(self.command(f"audio output {1 if on else 0}"))

    # -- effects chain (slots) --
    def effects_catalog(self) -> dict[str, int]:
        catalog: dict[str, int] = {}
        for line in self.command("effects"):
            for tok in line.split():
                if "@" in tok:
                    name, _, ident = tok.partition("@")
                    catalog[name] = int(ident)
        return catalog

    def get_slots(self) -> list[Slot]:
        import re

        slots: list[Slot] = []
        for line in self.command("slot get"):
            if line.startswith("slots"):
                for pos, name, state in re.findall(r"(\d+)=(\w+)(?:\(([^)]*)\))?", line):
                    if name == "empty":
                        slots.append(Slot(int(pos), None, False))
                    else:
                        slots.append(Slot(int(pos), name, state == "on"))
        return slots

    def _slot_of(self, effect: str) -> int | None:
        return next((s.pos for s in self.get_slots() if s.effect == effect), None)

    def _ensure_slot(self, effect: str) -> int:
        pos = self._slot_of(effect)
        if pos is not None:
            return pos
        if effect not in self.effects_catalog():
            raise ValueError(f"unknown effect: {effect!r}")
        for slot in self.get_slots():
            if slot.effect is None:
                self._expect_ok(self.command(f"slot set {slot.pos} {effect}"))
                return slot.pos
        raise CliError(f"no empty slot for effect {effect!r}")

    def set_active_effect(self, effect: str) -> None:
        self._expect_ok(self.command(f"slot active {self._ensure_slot(effect)}"))

    def set_enabled(self, effect: str, enabled: bool) -> None:
        pos = self._slot_of(effect)
        if pos is None:
            if not enabled:
                return
            pos = self._ensure_slot(effect)
        self._expect_ok(self.command(f"slot enable {pos} {1 if enabled else 0}"))

    def set_param(self, key: str, value: int) -> None:
        self._expect_ok(self.command(f"config set {key} {value}"))


# --- Sources and sinks ---


class Source:
    finished = False

    def start(self) -> None: ...
    def read(self, max_samples: int) -> np.ndarray:
        raise NotImplementedError

    def close(self) -> None: ...


class Sink:
    def start(self) -> None: ...
    def write(self, samples: np.ndarray) -> None:
        raise NotImplementedError

    def close(self) -> None: ...


class FileSource(Source):
    """16-bit PCM WAV, downmixed to mono and resampled to the device rate."""

    def __init__(self, path: str | Path, device_rate: float) -> None:
        with wave.open(str(path), "rb") as wav:
            if wav.getsampwidth() != 2:
                raise ValueError(f"{path}: only 16-bit PCM WAV is supported")
            channels, host_rate = wav.getnchannels(), wav.getframerate()
            data = np.frombuffer(wav.readframes(wav.getnframes()), dtype="<i2")
        if channels > 1:
            data = data.reshape(-1, channels).mean(axis=1)
        r = Resampler(host_rate, device_rate)
        self._buf = to_int16(np.concatenate([r.process(data.astype(np.float64)), r.flush()]))
        self._pos = 0

    @property
    def finished(self) -> bool:
        return self._pos >= self._buf.size

    def read(self, max_samples: int) -> np.ndarray:
        chunk = self._buf[self._pos : self._pos + max_samples]
        self._pos += chunk.size
        return chunk


class FileSink(Sink):
    """Resamples processed audio to host_rate and writes a mono 16-bit WAV."""

    def __init__(self, path: str | Path, device_rate: float, host_rate: int) -> None:
        self._r = Resampler(device_rate, host_rate)
        self._wav = wave.open(str(path), "wb")  # noqa: SIM115
        self._wav.setnchannels(1)
        self._wav.setsampwidth(2)
        self._wav.setframerate(host_rate)

    def write(self, samples: np.ndarray) -> None:
        out = self._r.process(samples.astype(np.float64))
        if out.size:
            self._wav.writeframes(to_int16(out).tobytes())

    def close(self) -> None:
        tail = self._r.flush()
        if tail.size:
            self._wav.writeframes(to_int16(tail).tobytes())
        self._wav.close()


class MicSource(Source):
    """Live microphone, downmixed to mono and resampled to the device rate."""

    finished = False

    def __init__(self, host_rate: float, device_rate: float) -> None:
        import queue

        import sounddevice as sd

        self._r = Resampler(host_rate, device_rate)
        self._queue: queue.Queue[np.ndarray] = queue.Queue()
        self._carry = np.empty(0, dtype=np.int16)
        self._stream = sd.InputStream(
            samplerate=host_rate, channels=1, dtype="int16", callback=self._cb
        )

    def _cb(self, indata, _frames, _time, _status) -> None:
        out = self._r.process(indata[:, 0].astype(np.float64))
        if out.size:
            self._queue.put(to_int16(out))

    def start(self) -> None:
        self._stream.start()

    def read(self, max_samples: int) -> np.ndarray:
        import queue

        parts = [self._carry] if self._carry.size else []
        total = self._carry.size
        while total < max_samples:
            try:
                block = self._queue.get_nowait()
            except queue.Empty:
                break
            parts.append(block)
            total += block.size
        if not parts:
            return np.empty(0, dtype=np.int16)
        merged = np.concatenate(parts)
        out, self._carry = merged[:max_samples], merged[max_samples:]
        return out

    def close(self) -> None:
        self._stream.stop()
        self._stream.close()


class SpeakerSink(Sink):
    """Resamples processed audio to host_rate and plays it on the speaker."""

    def __init__(self, host_rate: float, device_rate: float) -> None:
        import sounddevice as sd

        self._r = Resampler(device_rate, host_rate)
        self._lock = threading.Lock()
        self._buf = np.empty(0, dtype=np.int16)
        self._stream = sd.OutputStream(
            samplerate=host_rate, channels=1, dtype="int16", callback=self._cb
        )

    def _cb(self, outdata, frames, _time, _status) -> None:
        with self._lock:
            n = min(frames, self._buf.size)
            outdata[:n, 0] = self._buf[:n]
            self._buf = self._buf[n:]
        if n < frames:
            outdata[n:, 0] = 0

    def start(self) -> None:
        self._stream.start()

    def write(self, samples: np.ndarray) -> None:
        out = self._r.process(samples.astype(np.float64))
        if out.size:
            with self._lock:
                self._buf = np.concatenate([self._buf, to_int16(out)])

    def close(self) -> None:
        self._stream.stop()
        self._stream.close()


class DeviceAudioSink(Sink):
    """Plays samples on the OU device's UAC1 playback endpoint via the OS audio
    driver. No resampling here — the caller already produced device-rate samples."""

    def __init__(self, device_index: int, device_rate: float) -> None:
        import sounddevice as sd

        self._lock = threading.Lock()
        self._buf = np.empty(0, dtype=np.int16)
        self._stream = sd.OutputStream(
            device=device_index, samplerate=device_rate, channels=1, dtype="int16",
            callback=self._cb,
        )

    def _cb(self, outdata, frames, _time, _status) -> None:
        with self._lock:
            n = min(frames, self._buf.size)
            outdata[:n, 0] = self._buf[:n]
            self._buf = self._buf[n:]
        if n < frames:
            outdata[n:, 0] = 0

    def pending(self) -> int:
        with self._lock:
            return self._buf.size

    def start(self) -> None:
        self._stream.start()

    def write(self, samples: np.ndarray) -> None:
        with self._lock:
            self._buf = np.concatenate([self._buf, samples.astype(np.int16)])

    def close(self) -> None:
        self._stream.stop()
        self._stream.close()


class DeviceAudioSource(Source):
    """Captures samples from the OU device's UAC1 capture endpoint via the OS
    audio driver. No resampling here — output is already at the device rate."""

    finished = False

    def __init__(self, device_index: int, device_rate: float) -> None:
        import queue

        import sounddevice as sd

        self._queue: queue.Queue[np.ndarray] = queue.Queue()
        self._carry = np.empty(0, dtype=np.int16)
        self._stream = sd.InputStream(
            device=device_index, samplerate=device_rate, channels=1, dtype="int16",
            callback=self._cb,
        )

    def _cb(self, indata, _frames, _time, _status) -> None:
        self._queue.put(indata[:, 0].copy())

    def start(self) -> None:
        self._stream.start()

    def read(self, max_samples: int) -> np.ndarray:
        import queue

        parts = [self._carry] if self._carry.size else []
        total = self._carry.size
        while total < max_samples:
            try:
                block = self._queue.get_nowait()
            except queue.Empty:
                break
            parts.append(block)
            total += block.size
        if not parts:
            return np.empty(0, dtype=np.int16)
        merged = np.concatenate(parts)
        out, self._carry = merged[:max_samples], merged[max_samples:]
        return out

    def close(self) -> None:
        self._stream.stop()
        self._stream.close()


class RampSource(Source):
    """Fixed-length int16 ramp generated directly at device rate (no resampling),
    for byte-exact loopback comparison."""

    def __init__(self, length: int) -> None:
        self._buf = (np.arange(length, dtype=np.int64) % 65536 - 32768).astype(np.int16)
        self._pos = 0

    @property
    def buffer(self) -> np.ndarray:
        return self._buf

    @property
    def finished(self) -> bool:
        return self._pos >= self._buf.size

    def read(self, max_samples: int) -> np.ndarray:
        chunk = self._buf[self._pos : self._pos + max_samples]
        self._pos += chunk.size
        return chunk


class InfiniteRampSource(Source):
    """Unbounded int16 ramp at device rate, for sustained throughput testing."""

    finished = False

    def __init__(self) -> None:
        self._next = 0

    def read(self, max_samples: int) -> np.ndarray:
        values = np.arange(self._next, self._next + max_samples, dtype=np.int64) % 65536 - 32768
        self._next += max_samples
        return values.astype(np.int16)


class RecordingSink(Sink):
    """Accumulates every sample written, for byte-exact comparison against a source."""

    def __init__(self) -> None:
        self._chunks: list[np.ndarray] = []

    def write(self, samples: np.ndarray) -> None:
        if samples.size:
            self._chunks.append(samples.copy())

    @property
    def recorded(self) -> np.ndarray:
        if not self._chunks:
            return np.empty(0, dtype=np.int16)
        return np.concatenate(self._chunks)


class DiscardSink(Sink):
    """Tallies sample count without retaining data, for sustained throughput testing."""

    def __init__(self) -> None:
        self.count = 0

    def write(self, samples: np.ndarray) -> None:
        self.count += samples.size


# --- Streaming engine ---


class StreamEngine:
    """Pumps samples between a host Source/Sink pair and the device's UAC1
    endpoints (a DeviceAudioSink/DeviceAudioSource pair). Real-time pacing is
    the OS audio driver's job now (it pulls/pushes via callback at the
    hardware clock) — this just keeps DeviceAudioSink topped up without
    letting it grow unbounded for open-ended sources like InfiniteRampSource."""

    def __init__(
        self,
        device_sink: DeviceAudioSink,
        device_source: DeviceAudioSource,
        source: Source,
        sink: Sink,
        *,
        frame_samples: int = FRAME_SAMPLES,
        tail_s: float = 0.5,
    ) -> None:
        self._device_sink, self._device_source = device_sink, device_source
        self._source, self._sink = source, sink
        self._frame = frame_samples
        self._tail_s = tail_s
        self._stop = threading.Event()
        self._input_done = threading.Event()

    def run(self, stop_event: threading.Event | None = None) -> None:
        self._device_sink.start()
        self._device_source.start()
        self._source.start()
        self._sink.start()
        reader = threading.Thread(target=self._read_loop, daemon=True)
        writer = threading.Thread(target=self._write_loop, daemon=True)
        reader.start()
        writer.start()
        try:
            while not self._stop.is_set():
                if stop_event is not None and stop_event.is_set():
                    break
                if self._input_done.is_set():
                    self._stop.wait(self._tail_s)  # let the reader drain the tail
                    break
                self._stop.wait(0.05)
        except KeyboardInterrupt:
            pass
        finally:
            self._stop.set()
            writer.join(2.0)
            reader.join(2.0)
            self._source.close()
            self._sink.close()
            self._device_sink.close()
            self._device_source.close()

    def _write_loop(self) -> None:
        max_pending = self._frame * 8  # a few callback periods of slack
        while not self._stop.is_set():
            if self._device_sink.pending() >= max_pending:
                self._stop.wait(0.005)
                continue
            samples = self._source.read(self._frame)
            if samples.size:
                self._device_sink.write(samples)
            elif self._source.finished:
                self._input_done.set()
                return
            else:
                self._stop.wait(0.005)

    def _read_loop(self) -> None:
        while not self._stop.is_set():
            data = self._device_source.read(self._frame)
            if data.size:
                self._sink.write(data)
            else:
                self._stop.wait(0.005)
        for _ in range(8):  # final drain
            data = self._device_source.read(self._frame)
            if not data.size:
                break
            self._sink.write(data)


# --- Device session ---


class DiscoveryError(RuntimeError):
    pass


@dataclass
class DeviceSession:
    cli: CliPort
    audio_in_index: int | None
    audio_out_index: int | None
    device_rate: float
    via_stlink: bool = False

    def verify_audio_support(self, *, need_input: bool, need_output: bool) -> dict[str, str]:
        info = self.cli.verify_audio_support(need_input=need_input, need_output=need_output)
        if need_input and self.audio_in_index is None:
            raise DeviceUnsupportedError(
                f"no '{UAC_DEVICE_NAME}' capture device found via the OS audio driver"
            )
        if need_output and self.audio_out_index is None:
            raise DeviceUnsupportedError(
                f"no '{UAC_DEVICE_NAME}' playback device found via the OS audio driver"
            )
        return info

    def configure_effects(
        self,
        *,
        active: str | None = None,
        enable: Mapping[str, bool] | None = None,
        params: Mapping[str, int] | None = None,
    ) -> None:
        if active is not None:
            self.cli.set_active_effect(active)
        for name, state in (enable or {}).items():
            self.cli.set_enabled(name, state)
        for key, value in (params or {}).items():
            self.cli.set_param(key, value)

    @contextmanager
    def usb_routing(self, *, input_usb: bool = True, output_on: bool = True) -> Iterator[None]:
        if input_usb:
            self.cli.set_input("usb")
        if output_on:
            self.cli.set_output(True)
        try:
            yield
        finally:
            try:
                self.cli.set_output(False)
                self.cli.set_input("adc")
            except Exception as exc:  # noqa: BLE001 - best-effort cleanup
                log.warning("failed to restore audio routing: %s", exc)

    def close(self) -> None:
        self.cli.close()


def _ports(vid: int, pid: int) -> list[ListPortInfo]:
    matches = [p for p in list_ports.comports() if p.vid == vid and p.pid == pid]
    return sorted(matches, key=lambda p: (p.location or "", p.device))


def _find_audio_device(
    name_or_index: str | None, *, want_input: bool, want_output: bool
) -> int | None:
    """Resolve a sounddevice device index matching `name_or_index` (or
    UAC_DEVICE_NAME by default) with the required channel direction."""
    import sounddevice as sd

    if name_or_index is not None and name_or_index.lstrip("-").isdigit():
        return int(name_or_index)

    needle = (name_or_index or UAC_DEVICE_NAME).lower()
    for index, info in enumerate(sd.query_devices()):
        if needle not in info["name"].lower():
            continue
        if want_input and info["max_input_channels"] < 1:
            continue
        if want_output and info["max_output_channels"] < 1:
            continue
        return index
    return None


def open_device(
    *,
    cli_port: str | None = None,
    audio_device: str | None = None,
    char_delay_s: float = 0.01,
    query_rate: bool = True,
    open_audio: bool = True,
) -> DeviceSession:
    """Discover/identify the CLI's CDC port and the OS-level UAC1 audio device,
    and open a session (CLI kept open once)."""
    native = [p.device for p in _ports(VID, PID)]
    via_stlink = False

    # Resolve the CLI port: explicit override, else probe native ports with ping.
    cli: CliPort | None = None
    resolved_cli = cli_port
    if resolved_cli is None:
        for device in native:
            candidate = CliPort(device, char_delay_s=char_delay_s)
            if candidate.ping():
                cli, resolved_cli = candidate, device
                break
            candidate.close()
        if cli is None:  # fall back to the ST-Link VCP (CLI only)
            stlink = _ports(STLINK_VID, STLINK_PID)
            if stlink:
                resolved_cli, via_stlink = stlink[0].device, True
    if cli is None:
        if resolved_cli is None:
            raise DiscoveryError(
                f"no OU audio device found (expected CDC {VID:04x}:{PID:04x}). "
                "Connect the native USB cable, or pass --cli-port/--audio-device."
            )
        cli = CliPort(resolved_cli, char_delay_s=char_delay_s)

    audio_in_index = audio_out_index = None
    if open_audio:
        audio_in_index = _find_audio_device(audio_device, want_input=True, want_output=False)
        audio_out_index = _find_audio_device(audio_device, want_input=False, want_output=True)

    device_rate = float(DEFAULT_DEVICE_RATE)
    if query_rate:
        try:
            device_rate = float(cli.info().sample_rate_hz)
        except Exception as exc:  # noqa: BLE001 - best effort
            log.warning("could not query device rate (%s); using %d", exc, DEFAULT_DEVICE_RATE)

    return DeviceSession(cli, audio_in_index, audio_out_index, device_rate, via_stlink)


# --- CLI ---


def _parse_args(argv: list[str] | None) -> argparse.Namespace:
    p = argparse.ArgumentParser(
        prog="ouaudio", description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    p.add_argument("--input", metavar="mic|FILE.wav")
    p.add_argument("--output", metavar="speaker|FILE.wav")
    p.add_argument("--cli-port")
    p.add_argument(
        "--audio-device",
        metavar="NAME|INDEX",
        help=f"override UAC1 device lookup (default: match {UAC_DEVICE_NAME!r} via sounddevice)",
    )
    p.add_argument(
        "--cli-char-delay",
        type=float,
        default=0.01,
        metavar="SEC",
        help="inter-char CLI pacing (default 0.01; set 0 for native USB CLI)",
    )
    p.add_argument("--host-rate", type=int, default=48000)
    p.add_argument("--device-rate", type=int, default=None, help="default: query the device")
    p.add_argument("--tail-ms", type=int, default=500)
    p.add_argument("--effect", choices=EFFECT_NAMES, help="set the active effect")
    p.add_argument("--enable", action="append", default=[], choices=EFFECT_NAMES, metavar="EFFECT")
    p.add_argument("--disable", action="append", default=[], choices=EFFECT_NAMES, metavar="EFFECT")
    p.add_argument(
        "--set",
        action="append",
        default=[],
        dest="set_params",
        metavar="key=value",
        help="effect parameter, e.g. overdrive.mix=256 (repeatable)",
    )
    p.add_argument("--list", action="store_true", help="identify ports, print device info, exit")
    p.add_argument(
        "--debug",
        choices=["probe", "loopback", "stress", "resolve"],
        metavar="MODE",
        help="debug modes: probe, loopback, stress, resolve",
    )
    p.add_argument(
        "--addr",
        action="append",
        default=[],
        metavar="0xADDR",
        help="repeatable, for --debug resolve",
    )
    p.add_argument("--duration", type=float, default=5.0, help="--debug stress duration in seconds")
    p.add_argument("--elf", type=Path, default=None, help="firmware ELF for --debug resolve")
    p.add_argument("-v", "--verbose", action="store_true")
    return p.parse_args(argv)


def _parse_set(items: list[str]) -> dict[str, int]:
    params: dict[str, int] = {}
    for item in items:
        if "=" not in item:
            raise SystemExit(f"--set expects key=value, got {item!r}")
        key, _, value = item.partition("=")
        try:
            params[key.strip()] = int(value)
        except ValueError as exc:
            raise SystemExit(f"--set {item!r}: value must be an integer") from exc
    return params


def _cmd_list(args: argparse.Namespace) -> int:
    import sounddevice as sd

    native, stlink = _ports(VID, PID), _ports(STLINK_VID, STLINK_PID)
    print(f"Native CLI ports ({VID:04x}:{PID:04x}):")
    for port in native or []:
        print(f"  {port.device}  loc={port.location}  {port.product!r}")
    if not native:
        print("  (none -- connect the device's native USB cable)")
    print(f"ST-Link VCP ports ({STLINK_VID:04x}:{STLINK_PID:04x}, CLI fallback):")
    for port in stlink:
        print(f"  {port.device}  loc={port.location}  {port.product!r}")
    print(f"\nOS audio devices matching {(args.audio_device or UAC_DEVICE_NAME)!r}:")
    for index, info in enumerate(sd.query_devices()):
        needle = (args.audio_device or UAC_DEVICE_NAME).lower()
        if needle in info["name"].lower():
            print(
                f"  [{index}] {info['name']!r}  in={info['max_input_channels']} "
                f"out={info['max_output_channels']}  rate={info['default_samplerate']:.0f}"
            )

    dev = open_device(
        cli_port=args.cli_port,
        audio_device=args.audio_device,
        char_delay_s=args.cli_char_delay,
    )
    try:
        print(f"\nResolved CLI       : {dev.cli.port}{' (ST-Link VCP)' if dev.via_stlink else ''}")
        print(f"Resolved audio in  : index {dev.audio_in_index}")
        print(f"Resolved audio out : index {dev.audio_out_index}")
        print("\nsysinfo:", dev.cli.sysinfo())
        info = dev.cli.info()
        print(
            f"info: sample_rate={info.sample_rate_hz} Hz frame={info.frame_samples} "
            f"delay_line={info.delay_samples}"
        )
        print("effects:", dev.cli.effects_catalog())
        print("chain:", [(s.pos, s.effect, s.enabled) for s in dev.cli.get_slots()])
        print("audio status:", dev.cli.audio_status())
    finally:
        dev.close()
    return 0


def _build_source(spec: str, device_rate: float, host_rate: int) -> Source:
    return MicSource(host_rate, device_rate) if spec == "mic" else FileSource(spec, device_rate)


def _build_sink(spec: str, device_rate: float, host_rate: int) -> Sink:
    if spec == "speaker":
        return SpeakerSink(host_rate, device_rate)
    return FileSink(spec, device_rate, host_rate)


def _cmd_stream(args: argparse.Namespace) -> int:
    if not args.input or not args.output:
        raise SystemExit("both --input and --output are required for streaming")
    set_params = _parse_set(args.set_params)
    dev = open_device(
        cli_port=args.cli_port,
        audio_device=args.audio_device,
        char_delay_s=args.cli_char_delay,
        query_rate=args.device_rate is None,
    )
    device_rate = float(args.device_rate) if args.device_rate else dev.device_rate
    try:
        dev.verify_audio_support(need_input=True, need_output=True)
        enable = dict.fromkeys(args.enable, True)
        enable.update(dict.fromkeys(args.disable, False))
        dev.configure_effects(active=args.effect, enable=enable, params=set_params)

        source = _build_source(args.input, device_rate, args.host_rate)
        sink = _build_sink(args.output, device_rate, args.host_rate)
        device_sink = DeviceAudioSink(dev.audio_out_index, device_rate)
        device_source = DeviceAudioSource(dev.audio_in_index, device_rate)
        engine = StreamEngine(
            device_sink, device_source, source, sink, tail_s=args.tail_ms / 1000.0
        )
        stop = threading.Event()
        signal.signal(signal.SIGINT, lambda *_: stop.set())
        live = args.input == "mic" or args.output == "speaker"
        log.info(
            "device %.0f Hz | %s -> device -> %s%s",
            device_rate,
            args.input,
            args.output,
            " (Ctrl-C to stop)" if live else "",
        )
        with dev.usb_routing():
            engine.run(stop_event=stop)
        log.info("done")
    finally:
        dev.close()
    return 0


FIRMWARE_ELF_DEFAULT = (
    Path(__file__).resolve().parents[2]
    / "firmware/stm32f303/nucleo-ou-audio-effects/build/Debug/nucleo-ou-audio-effects.elf"
)


def _cmd_debug_resolve(args: argparse.Namespace) -> int:
    import shutil
    import subprocess

    elf = args.elf or FIRMWARE_ELF_DEFAULT
    addr2line = shutil.which("arm-none-eabi-addr2line")
    if not addr2line or not elf.exists():
        if not addr2line:
            log.warning("arm-none-eabi-addr2line not found on PATH; printing raw addresses")
        else:
            log.warning("ELF not found at %s; printing raw addresses", elf)
        for addr in args.addr:
            print(addr)
        return 0

    for addr in args.addr:
        result = subprocess.run(  # noqa: S603
            [addr2line, "-e", str(elf), "-f", "-C", "-p", addr],
            capture_output=True,
            text=True,
            check=False,
        )
        line = result.stdout.strip() or addr
        print(f"{addr}  {line}")
    return 0


def _cmd_debug_probe(args: argparse.Namespace) -> int:
    start = time.monotonic()

    def elapsed_ms() -> int:
        return int((time.monotonic() - start) * 1000)

    dev = open_device(
        cli_port=args.cli_port,
        audio_device=args.audio_device,
        char_delay_s=args.cli_char_delay,
    )
    try:
        print(
            f"[+{elapsed_ms():4d}ms] open {dev.cli.port}"
            f"{' (ST-Link VCP)' if dev.via_stlink else ''}"
        )
        steps: list[tuple[str, object]] = [
            ("ping", dev.cli.ping),
            ("info", dev.cli.info),
            ("sysinfo", dev.cli.sysinfo),
            ("audio status", dev.cli.audio_status),
            ("effects", dev.cli.effects_catalog),
            ("chain", dev.cli.get_slots),
        ]
        for name, step in steps:
            try:
                result = step()
            except Exception as exc:  # noqa: BLE001 - report and stop on first failure
                print(f"[+{elapsed_ms():4d}ms] {name:<12} -> FAILED: {exc}")
                return 1
            print(f"[+{elapsed_ms():4d}ms] {name:<12} -> {result}")
    finally:
        dev.close()
    print("OK")
    return 0


def _cmd_debug_loopback(args: argparse.Namespace) -> int:
    dev = open_device(
        cli_port=args.cli_port, audio_device=args.audio_device, char_delay_s=args.cli_char_delay
    )
    try:
        dev.verify_audio_support(need_input=True, need_output=True)

        # Query pipeline delay before streaming (CLI is still fast at this
        # point).  Also disable effects and set unity gain on the test input
        # so the ramp passes through unmodified.
        info = dev.cli.info()
        delay = info.delay_samples

        dev.cli.command("slot enable 0 0")
        dev.cli.command("test input mode 1")
        dev.cli.command("test input vector usb")
        dev.cli.command("test input amp 32767")  # INT16_MAX = unity gain
        dev.cli.command("test output mode 0")  # don't replace DAC buffer

        length = 2048
        source = RampSource(length)
        expected = source.buffer.copy()
        sink = RecordingSink()
        device_sink = DeviceAudioSink(dev.audio_out_index, dev.device_rate)
        device_source = DeviceAudioSource(dev.audio_in_index, dev.device_rate)
        with dev.usb_routing(input_usb=True, output_on=True):
            engine = StreamEngine(device_sink, device_source, source, sink)
            engine.run()
            status = dev.cli.audio_status()

        received = sink.recorded
        # Skip pipeline latency plus one frame of pre-fill data.
        skip = delay + FRAME_SAMPLES
        if skip > 0 and received.size > skip:
            received = received[skip:]
        n = min(expected.size, received.size)
        mismatches = np.flatnonzero(expected[:n] != received[:n])
        ok = mismatches.size == 0 and n == expected.size
        print(
            f"compare: {n}/{expected.size} received, "
            f"{mismatches.size} mismatches, delay={delay} skip={skip}"
        )
        if mismatches.size:
            i = int(mismatches[0])
            print(f"first mismatch at offset {i}: expected {expected[i]}, got {received[i]}")
        print("audio status:", status)
        drops = int(status.get("drops_out", 0)) + int(status.get("drops_in", 0))
        if drops:
            ok = False
            print(f"drops detected: {status.get('drops_out')=} {status.get('drops_in')=}")
    finally:
        dev.close()
    print("OK" if ok else "FAILED")
    return 0 if ok else 1


def _cmd_debug_stress(args: argparse.Namespace) -> int:
    dev = open_device(
        cli_port=args.cli_port, audio_device=args.audio_device, char_delay_s=args.cli_char_delay
    )
    try:
        dev.verify_audio_support(need_input=True, need_output=True)

        source = InfiniteRampSource()
        sink = DiscardSink()
        device_sink = DeviceAudioSink(dev.audio_out_index, dev.device_rate)
        device_source = DeviceAudioSource(dev.audio_in_index, dev.device_rate)
        stop = threading.Event()
        timer = threading.Timer(args.duration, stop.set)
        with dev.usb_routing(input_usb=True, output_on=True):
            print(f"streaming {dev.device_rate:.0f} Hz for {args.duration:.0f}s ...")
            engine = StreamEngine(device_sink, device_source, source, sink)
            timer.start()
            engine.run(stop_event=stop)
            timer.cancel()
            status = dev.cli.audio_status()

        print(f"done: {sink.count} samples ({sink.count * 2} bytes)")
        print("audio status:", status)
        drops = int(status.get("drops_out", 0)) + int(status.get("drops_in", 0))
    finally:
        dev.close()
    print("OK" if drops == 0 else "FAILED")
    return 0 if drops == 0 else 1


def _dispatch_debug(args: argparse.Namespace) -> int:
    if args.debug == "resolve":
        return _cmd_debug_resolve(args)
    if args.debug == "probe":
        return _cmd_debug_probe(args)
    if args.debug == "loopback":
        return _cmd_debug_loopback(args)
    if args.debug == "stress":
        return _cmd_debug_stress(args)
    raise SystemExit(f"unknown --debug mode: {args.debug!r}")


def main(argv: list[str] | None = None) -> int:
    args = _parse_args(argv)
    logging.basicConfig(
        level=logging.DEBUG if args.verbose else logging.INFO, format="%(levelname)s %(message)s"
    )
    try:
        if args.debug:
            return _dispatch_debug(args)
        return _cmd_list(args) if args.list else _cmd_stream(args)
    except (DiscoveryError, DeviceUnsupportedError, CliError) as exc:
        log.error("%s", exc)
        return 1


if __name__ == "__main__":
    sys.exit(main())
