#!/usr/bin/env python3
import json
import re
import sys
from pathlib import Path

BEGIN_RE = re.compile(r"USER\s+CODE\s+BEGIN\s+([^*]+)\*/")
END_RE = re.compile(r"USER\s+CODE\s+END\s+([^*]+)\*/")
SKIP_RE = re.compile(r"^\s*$|^\s*[{}]\s*$")
USAGE = "Usage: user_code_regions.py <ranges|clang-format-args|clang-tidy-line-filter> <file>"


def parse_ranges(file_path: Path, lines: list[str]) -> list[tuple[int, int]]:
    ranges: list[tuple[int, int]] = []
    open_line = 0
    open_label = ""

    for i, line in enumerate(lines, start=1):
        begin = BEGIN_RE.search(line)
        if begin:
            if open_line:
                raise ValueError(
                    f"{file_path}:{i}: nested USER CODE BEGIN before USER CODE END for section \"{open_label}\""
                )
            open_line = i
            open_label = begin.group(1).strip()
            continue

        end = END_RE.search(line)
        if not end:
            continue

        end_label = end.group(1).strip()
        if not open_line:
            raise ValueError(f"{file_path}:{i}: USER CODE END without matching USER CODE BEGIN")
        if end_label != open_label:
            raise ValueError(
                f"{file_path}:{i}: USER CODE marker mismatch: BEGIN \"{open_label}\" vs END \"{end_label}\""
            )

        start, stop = open_line + 1, i - 1
        if start <= stop:
            ranges.append((start, stop))
        open_line = 0
        open_label = ""

    if open_line:
        raise ValueError(f"{file_path}:{open_line}: USER CODE BEGIN \"{open_label}\" missing USER CODE END")

    return ranges


def trimmed_ranges(lines: list[str], ranges: list[tuple[int, int]]) -> list[tuple[int, int]]:
    out: list[tuple[int, int]] = []

    for start, stop in ranges:
        while start <= stop and SKIP_RE.match(lines[start - 1]):
            start += 1
        while stop >= start and SKIP_RE.match(lines[stop - 1]):
            stop -= 1
        if start <= stop:
            out.append((start, stop))

    return out


def main() -> int:
    if len(sys.argv) != 3:
        print(USAGE, file=sys.stderr)
        return 2

    mode, file_arg = sys.argv[1], sys.argv[2]
    file_path = Path(file_arg)
    lines = file_path.read_text(encoding="utf-8", errors="replace").splitlines()

    try:
        ranges = parse_ranges(file_path, lines)
    except ValueError as err:
        print(err, file=sys.stderr)
        return 1

    if mode == "ranges":
        for start, stop in ranges:
            print(f"{start}:{stop}")
        return 0

    if mode == "clang-format-args":
        for start, stop in trimmed_ranges(lines, ranges):
            print(f"-lines={start}:{stop}")
        return 0

    if mode == "clang-tidy-line-filter":
        if ranges:
            payload = [{"name": str(file_path), "lines": [[s, e] for s, e in ranges]}]
            print(json.dumps(payload, separators=(",", ":")))
        return 0

    print(f"Unknown mode: {mode}", file=sys.stderr)
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
