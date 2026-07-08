"""Decode an STM32 fault handler dump into a human-readable report.

Resolves raw hex addresses to source locations using arm-none-eabi-addr2line
and decodes CFSR/HFSR fault register bit patterns.

Usage:
    python3 scripts/lib/decode_fault.py fault_dump.txt
    cat fault_dump.txt | python3 scripts/lib/decode_fault.py
    python3 scripts/lib/decode_fault.py -e path/to/elf fault_dump.txt
"""

import argparse
import os
import re
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path

_REPO_ROOT = Path(__file__).resolve().parent.parent.parent
_DEFAULT_ELF = str(
    _REPO_ROOT
    / "firmware"
    / "stm32f303"
    / "nucleo-ou-audio-effects"
    / "build"
    / "Debug"
    / "nucleo-ou-audio-effects.elf"
)

_CFSR_BITS = (
    (0x00010000, "UNDEFINSTR (undefined instruction)"),
    (0x00020000, "INVSTATE (invalid state / EPSR)"),
    (0x00040000, "INVPC (invalid PC load)"),
    (0x00080000, "NOCP (no coprocessor)"),
    (0x01000000, "UNALIGNED (unaligned access)"),
    (0x02000000, "DIVBYZERO (divide by zero)"),
    (0x00000100, "IBUSERR (instruction bus error)"),
    (0x00000200, "PRECISERR (precise data bus error)"),
    (0x00000400, "IMPRECISERR (imprecise data bus error)"),
    (0x00000800, "UNSTKERR (unstacking error)"),
    (0x00001000, "STKERR (stacking error)"),
    (0x00002000, "LSPERR (floating-point lazy state preservation)"),
    (0x00008000, "BFARVALID (BFAR holds valid address)"),
    (0x00000001, "IACCVIOL (instruction access violation)"),
    (0x00000002, "DACCVIOL (data access violation)"),
    (0x00000008, "MUNSTKERR (unstacking error)"),
    (0x00000010, "MSTKERR (stacking error)"),
    (0x00000020, "MLSPERR (floating-point lazy state preservation)"),
    (0x00000080, "MMARVALID (MMFAR holds valid address)"),
)

_HFSR_BITS = (
    (0x40000000, "FORCED (hard fault escalated from configurable fault)"),
    (0x00000002, "VECTTBL (vector table read error)"),
    (0x80000000, "DEBUGEVT (debug event)"),
)


@dataclass
class CallStackFrame:
    index: int
    address: int


@dataclass
class FaultDump:
    fault_type: str = ""
    regs: dict[str, int] = field(default_factory=dict)
    cfsr: int = 0
    hfsr: int = 0
    cfsr_flags: list[str] = field(default_factory=list)
    hfsr_flags: list[str] = field(default_factory=list)
    call_stack: list[CallStackFrame] = field(default_factory=list)
    raw_lines: list[str] = field(default_factory=list)


def find_addr2line() -> str:
    """Locate arm-none-eabi-addr2line on PATH or under the Arm toolchain."""
    candidates = _path_candidates() + _toolchain_candidates()
    for candidate in candidates:
        if os.path.isfile(candidate) and os.access(candidate, os.X_OK):
            return candidate
    raise FileNotFoundError(
        "arm-none-eabi-addr2line not found on PATH or under /Applications/ArmGNUToolchain"
    )


def _path_candidates() -> list[str]:
    path = os.environ.get("PATH", "")
    candidates: list[str] = []
    for entry in path.split(os.pathsep):
        candidate = os.path.join(entry, "arm-none-eabi-addr2line")
        candidates.append(candidate)
    return candidates


def _toolchain_candidates() -> list[str]:
    base = Path("/Applications/ArmGNUToolchain")
    if not base.is_dir():
        return []
    candidates: list[str] = []
    try:
        for version_dir in sorted(base.iterdir(), reverse=True):
            candidate = version_dir / "arm-none-eabi" / "bin" / "arm-none-eabi-addr2line"
            candidates.append(str(candidate))
    except PermissionError:
        pass
    return candidates


def parse_dumps(text: str) -> list[FaultDump]:
    """Parse one or more fault dumps from raw serial output."""
    dumps: list[FaultDump] = []
    current: FaultDump | None = None
    in_call_stack = False
    in_reasons = False

    for line in text.splitlines():
        if line.startswith("=== ") and line.endswith(" ==="):
            current = FaultDump()
            current.fault_type = line[4:-4].strip()
            dumps.append(current)
            in_call_stack = False
            in_reasons = False
            continue

        if current is None:
            continue

        current.raw_lines.append(line)

        if _try_parse_stacked_regs(line, current):
            continue

        if _try_parse_cfsr_hfsr(line, current):
            in_reasons = True
            in_call_stack = False
            continue

        if in_reasons and line.startswith("  - "):
            current.cfsr_flags.append(line.strip())
            continue

        if "--- Call stack" in line:
            in_call_stack = True
            in_reasons = False
            continue

        if in_call_stack:
            m = re.match(r"\[(\d+)\]\s+([0-9A-Fa-f]+)", line)
            if m:
                frame = CallStackFrame(
                    index=int(m.group(1)),
                    address=int(m.group(2), 16),
                )
                current.call_stack.append(frame)
                continue

    return dumps


def _try_parse_stacked_regs(line: str, dump: FaultDump) -> bool:
    m = re.match(
        r"R0\s*:\s*([0-9A-Fa-f]+)\s+R1\s*:\s*([0-9A-Fa-f]+)\s+"
        r"R2\s*:\s*([0-9A-Fa-f]+)\s+R3\s*:\s*([0-9A-Fa-f]+)",
        line,
    )
    if m:
        dump.regs["r0"] = int(m.group(1), 16)
        dump.regs["r1"] = int(m.group(2), 16)
        dump.regs["r2"] = int(m.group(3), 16)
        dump.regs["r3"] = int(m.group(4), 16)
        return True
    m = re.match(
        r"R12\s*:\s*([0-9A-Fa-f]+)\s+LR\s*:\s*([0-9A-Fa-f]+)\s+"
        r"PC\s*:\s*([0-9A-Fa-f]+)\s+PSR\s*:\s*([0-9A-Fa-f]+)",
        line,
    )
    if m:
        dump.regs["r12"] = int(m.group(1), 16)
        dump.regs["lr"] = int(m.group(2), 16)
        dump.regs["pc"] = int(m.group(3), 16)
        dump.regs["psr"] = int(m.group(4), 16)
        return True
    return False


def _try_parse_cfsr_hfsr(line: str, dump: FaultDump) -> bool:
    m = re.match(r"CFSR:\s*([0-9A-Fa-f]+)\s+HFSR:\s*([0-9A-Fa-f]+)", line)
    if m:
        dump.cfsr = int(m.group(1), 16)
        dump.hfsr = int(m.group(2), 16)
        return True
    return False


def decode_cfsr(cfsr: int) -> list[str]:
    flags: list[str] = []
    for mask, desc in _CFSR_BITS:
        if cfsr & mask:
            flags.append(f"  - {desc}")
    if not flags:
        flags.append("  (none)")
    return flags


def decode_hfsr(hfsr: int) -> list[str]:
    flags: list[str] = []
    for mask, desc in _HFSR_BITS:
        if hfsr & mask:
            flags.append(f"  - {desc}")
    if not flags:
        flags.append("  (none)")
    return flags


def resolve_addresses(addresses: list[int], elf_path: str, addr2line: str) -> dict[int, str]:
    """Resolve a batch of addresses to source locations.

    Returns a dict mapping each address to its resolved string.
    """
    if not addresses:
        return {}

    hex_addrs = [f"0x{a:X}" for a in addresses]
    cmd = [addr2line, "-e", elf_path, "-f", "-p", "-a"] + hex_addrs

    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=10)
    except (subprocess.TimeoutExpired, OSError) as exc:
        raise RuntimeError(f"Failed to run addr2line: {exc}") from exc

    if result.returncode != 0:
        raise RuntimeError(f"addr2line failed (exit {result.returncode}): {result.stderr.strip()}")

    resolved: dict[int, str] = {}
    for line in result.stdout.splitlines():
        m = re.match(r"^(0x[0-9A-Fa-f]+):\s*(.*)", line)
        if m:
            addr = int(m.group(1), 16)
            info = m.group(2).strip() or "??"
            resolved[addr] = info

    return resolved


def needs_addr2line(dumps: list[FaultDump]) -> bool:
    """Return True if any dump has addresses worth resolving."""
    for dump in dumps:
        for name in ("pc", "lr"):
            if name in dump.regs:
                return True
        if dump.call_stack:
            return True
    return False


def collect_addresses(dumps: list[FaultDump]) -> list[int]:
    """Collect all unique addresses from stacked registers and call stacks."""
    seen: set[int] = set()
    ordered: list[int] = []
    for dump in dumps:
        for name in ("pc", "lr", "r0", "r1", "r2", "r3", "r12"):
            if name in dump.regs:
                addr = dump.regs[name]
                if addr not in seen:
                    seen.add(addr)
                    ordered.append(addr)
        for frame in dump.call_stack:
            if frame.address not in seen:
                seen.add(frame.address)
                ordered.append(frame.address)
    return ordered


def print_report(
    dumps: list[FaultDump],
    resolved: dict[int, str] | None,
    file: "object" = sys.stdout,
) -> None:
    """Print a decoded fault report."""
    for i, dump in enumerate(dumps):
        if i > 0:
            print(file=file)

        print(f"=== {dump.fault_type} ===", file=file)
        print(file=file)

        _print_reg("PC", dump.regs.get("pc"), resolved, file=file)
        _print_reg("LR", dump.regs.get("lr"), resolved, file=file)
        print(file=file)

        reg_names = ("r0", "r1", "r2", "r3", "r12", "psr")
        for name in reg_names:
            if name in dump.regs:
                label = name.upper().ljust(3) if name != "psr" else "PSR"
                _print_reg(label, dump.regs[name], resolved, file=file)

        print(file=file)
        print(f"CFSR: 0x{dump.cfsr:08X}  HFSR: 0x{dump.hfsr:08X}", file=file)

        cfsr_flags = decode_cfsr(dump.cfsr)
        for flag in cfsr_flags:
            print(flag, file=file)

        hfsr_flags = decode_hfsr(dump.hfsr)
        for flag in hfsr_flags:
            print(flag, file=file)

        if dump.call_stack:
            print(file=file)
            print("--- Call stack ---", file=file)
            for frame in dump.call_stack:
                label = f"[{frame.index}]"
                _print_reg(label, frame.address, resolved, file=file)

        print(file=file)


def _print_reg(
    label: str,
    value: int | None,
    resolved: dict[int, str] | None,
    file: "object" = sys.stdout,
) -> None:
    if value is None:
        return
    symbol = ""
    if resolved and value in resolved:
        symbol = f"  {resolved[value]}"
    print(f"{label}: 0x{value:08X}{symbol}", file=file)


def read_input(args: argparse.Namespace) -> str:
    if args.input:
        with open(args.input) as f:
            return f.read()
    return sys.stdin.read()


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Decode STM32 fault handler dump with addr2line resolution."
    )
    parser.add_argument(
        "input",
        nargs="?",
        help="Fault dump text file (default: stdin)",
    )
    parser.add_argument(
        "-e",
        "--elf",
        default=_DEFAULT_ELF,
        help=f"ELF file for symbol resolution (default: {_DEFAULT_ELF})",
    )
    parser.add_argument(
        "--addr2line",
        help="Path to arm-none-eabi-addr2line (auto-detected if omitted)",
    )
    args = parser.parse_args()

    text = read_input(args)
    dumps = parse_dumps(text)
    if not dumps:
        print("No fault dump found in input.", file=sys.stderr)
        sys.exit(1)

    resolved = None
    if needs_addr2line(dumps):
        try:
            addr2line_bin = args.addr2line or find_addr2line()
        except FileNotFoundError as exc:
            print(f"Warning: {exc} -- skipping symbol resolution.", file=sys.stderr)
            addr2line_bin = None

        if addr2line_bin and os.path.isfile(args.elf):
            addresses = collect_addresses(dumps)
            if addresses:
                try:
                    resolved = resolve_addresses(addresses, args.elf, addr2line_bin)
                except RuntimeError as exc:
                    print(
                        f"Warning: addr2line failed: {exc}",
                        file=sys.stderr,
                    )
        elif addr2line_bin:
            print(
                f"Warning: ELF not found at {args.elf} -- skipping symbol resolution.",
                file=sys.stderr,
            )

    print_report(dumps, resolved)


if __name__ == "__main__":
    main()
