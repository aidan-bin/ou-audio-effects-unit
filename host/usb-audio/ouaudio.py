"""Host-side USB audio streaming tool for the OU audio effects unit.

Dual USB-CDC: text CLI on one port, raw int16 mono PCM on the other. Controls
effects/routing over the CLI and streams audio with file or live (mic/speaker) I/O.

Usage:
    python ouaudio.py --list
    python ouaudio.py --input in.wav --output out.wav --enable overdrive
    python ouaudio.py --input mic --output speaker --effect echo   # Ctrl-C to stop

Dependencies: pyserial numpy sounddevice

Requires the device's native USB (VID:PID 0483:5740) for audio. ST-Link VCP
(0483:374B) can be used for CLI only. The device is the sample clock (~40506 Hz);
host audio is resampled accordingly.
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

SAMPLE_DTYPE = np.dtype("<i2")  # little-endian signed 16-bit, mono
DEFAULT_DEVICE_RATE = 40506  # 48 MHz / 1185; fallback if `info` unavailable
FRAME_SAMPLES = 405  # ~10 ms pacing unit at 40506 Hz
CLI_PROMPT = "> "

EFFECT_NAMES = ("overdrive", "echo", "compression")
INT16_MIN, INT16_MAX = -32768, 32767


def samples_to_bytes(samples: np.ndarray) -> bytes:
    return np.ascontiguousarray(samples, dtype=SAMPLE_DTYPE).tobytes()


def bytes_to_samples(raw: bytes) -> np.ndarray:
    usable = len(raw) - (len(raw) % 2)
    if usable <= 0:
        return np.empty(0, dtype=SAMPLE_DTYPE)
    return np.frombuffer(raw[:usable], dtype=SAMPLE_DTYPE)


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
        for char in text + "\n":
            self._serial.write(char.encode("ascii"))
            self._serial.flush()
            if self._char_delay_s:
                time.sleep(self._char_delay_s)

        deadline = time.monotonic() + (timeout_s or self._timeout_s)
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
        if info.get("audio_routing") != "dual-cdc":
            raise DeviceUnsupportedError(
                f"audio_routing={info.get('audio_routing')!r} (want dual-cdc); {tag}"
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


# --- Audio transport ---


class AudioPort:
    def __init__(self, port: str, *, read_timeout_s: float = 0.02) -> None:
        self._serial = serial.Serial(port, 115200, timeout=read_timeout_s, write_timeout=2.0)
        self.port = port

    def write(self, data: bytes) -> None:
        self._serial.write(data)

    def read(self, max_bytes: int) -> bytes:
        return self._serial.read(max_bytes)

    def reset(self) -> None:
        self._serial.reset_input_buffer()
        self._serial.reset_output_buffer()

    def close(self) -> None:
        self._serial.close()


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


# --- Streaming engine ---


class StreamEngine:
    def __init__(
        self,
        audio: AudioPort,
        source: Source,
        sink: Sink,
        *,
        device_rate: float,
        frame_samples: int = FRAME_SAMPLES,
        tail_s: float = 0.5,
    ) -> None:
        self._port, self._source, self._sink = audio, source, sink
        self._frame = frame_samples
        self._period = frame_samples / device_rate
        self._tail_s = tail_s
        self._stop = threading.Event()
        self._input_done = threading.Event()

    def run(self, stop_event: threading.Event | None = None) -> None:
        self._port.reset()
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

    def _write_loop(self) -> None:
        next_send = time.monotonic()
        while not self._stop.is_set():
            samples = self._source.read(self._frame)
            if samples.size:
                self._port.write(samples_to_bytes(samples))
            elif self._source.finished:
                self._input_done.set()
                return
            next_send += self._period
            delay = next_send - time.monotonic()
            if delay > 0:
                self._stop.wait(delay)
            else:
                next_send = time.monotonic()

    def _read_loop(self) -> None:
        read_size = self._frame * 8
        while not self._stop.is_set():
            data = self._port.read(read_size)
            if data:
                self._sink.write(bytes_to_samples(data))
        for _ in range(8):  # final drain
            data = self._port.read(read_size)
            if not data:
                break
            self._sink.write(bytes_to_samples(data))


# --- Device session ---


class DiscoveryError(RuntimeError):
    pass


@dataclass
class DeviceSession:
    cli: CliPort
    audio: AudioPort | None
    device_rate: float
    via_stlink: bool = False
    audio_port_name: str | None = None

    def verify_audio_support(self, *, need_input: bool, need_output: bool) -> dict[str, str]:
        return self.cli.verify_audio_support(need_input=need_input, need_output=need_output)

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
        if self.audio is not None:
            self.audio.close()
        self.cli.close()


def _ports(vid: int, pid: int) -> list[ListPortInfo]:
    matches = [p for p in list_ports.comports() if p.vid == vid and p.pid == pid]
    return sorted(matches, key=lambda p: (p.location or "", p.device))


def open_device(
    *,
    cli_port: str | None = None,
    audio_port: str | None = None,
    char_delay_s: float = 0.01,
    query_rate: bool = True,
    open_audio: bool = True,
) -> DeviceSession:
    """Discover/identify the CDC ports and open a session (CLI kept open once)."""
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
                "Connect the native USB cable, or pass --cli-port/--audio-port."
            )
        cli = CliPort(resolved_cli, char_delay_s=char_delay_s)

    # Audio port: explicit override, else the native port that is not the CLI.
    resolved_audio = audio_port
    if resolved_audio is None and not via_stlink:
        resolved_audio = next((d for d in native if d != resolved_cli), None)
    audio = AudioPort(resolved_audio) if (open_audio and resolved_audio) else None

    device_rate = float(DEFAULT_DEVICE_RATE)
    if query_rate:
        try:
            device_rate = float(cli.info().sample_rate_hz)
        except Exception as exc:  # noqa: BLE001 - best effort
            log.warning("could not query device rate (%s); using %d", exc, DEFAULT_DEVICE_RATE)

    return DeviceSession(cli, audio, device_rate, via_stlink, audio_port_name=resolved_audio)


# --- CLI ---


def _parse_args(argv: list[str] | None) -> argparse.Namespace:
    p = argparse.ArgumentParser(
        prog="ouaudio", description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    p.add_argument("--input", metavar="mic|FILE.wav")
    p.add_argument("--output", metavar="speaker|FILE.wav")
    p.add_argument("--cli-port")
    p.add_argument("--audio-port")
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
    native, stlink = _ports(VID, PID), _ports(STLINK_VID, STLINK_PID)
    print(f"Native device ports ({VID:04x}:{PID:04x}):")
    for port in native or []:
        print(f"  {port.device}  loc={port.location}  {port.product!r}")
    if not native:
        print("  (none -- connect the device's native USB cable)")
    print(f"ST-Link VCP ports ({STLINK_VID:04x}:{STLINK_PID:04x}, CLI fallback):")
    for port in stlink:
        print(f"  {port.device}  loc={port.location}  {port.product!r}")

    dev = open_device(
        cli_port=args.cli_port,
        audio_port=args.audio_port,
        char_delay_s=args.cli_char_delay,
        open_audio=False,
    )
    try:
        print(f"\nResolved CLI  : {dev.cli.port}{' (ST-Link VCP)' if dev.via_stlink else ''}")
        print(f"Resolved audio: {dev.audio_port_name or '(unavailable -- native USB unreachable)'}")
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
        audio_port=args.audio_port,
        char_delay_s=args.cli_char_delay,
        query_rate=args.device_rate is None,
    )
    device_rate = float(args.device_rate) if args.device_rate else dev.device_rate
    try:
        if dev.audio is None:
            raise SystemExit(
                f"audio port unavailable: the native USB ({VID:04x}:{PID:04x}) is not "
                "enumerated (only the ST-Link VCP is present). Connect the native cable."
            )
        dev.verify_audio_support(need_input=True, need_output=True)
        enable = dict.fromkeys(args.enable, True)
        enable.update(dict.fromkeys(args.disable, False))
        dev.configure_effects(active=args.effect, enable=enable, params=set_params)

        source = _build_source(args.input, device_rate, args.host_rate)
        sink = _build_sink(args.output, device_rate, args.host_rate)
        engine = StreamEngine(
            dev.audio, source, sink, device_rate=device_rate, tail_s=args.tail_ms / 1000.0
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


def main(argv: list[str] | None = None) -> int:
    args = _parse_args(argv)
    logging.basicConfig(
        level=logging.DEBUG if args.verbose else logging.INFO, format="%(levelname)s %(message)s"
    )
    try:
        return _cmd_list(args) if args.list else _cmd_stream(args)
    except (DiscoveryError, DeviceUnsupportedError, CliError) as exc:
        log.error("%s", exc)
        return 1


if __name__ == "__main__":
    sys.exit(main())
