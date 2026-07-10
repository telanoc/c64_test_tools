#!/usr/bin/env python3
"""Assert the maximum interval between /RAS refreshes of each DRAM row."""

from __future__ import annotations

import argparse
import re
import sys
from collections import defaultdict
from pathlib import Path


VAR_RE = re.compile(r"^\$var\s+wire\s+(\d+)\s+(\S+)\s+(\S+)\s+\$end$")
PHASE_NAMES = {
    1: "1111 write",
    2: "0000 write",
    3: "0101 write",
    4: "1010 write",
    5: "pattern verify",
    6: "random write",
    7: "random verify",
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("trace", type=Path)
    parser.add_argument("--clock-hz", type=int, required=True)
    parser.add_argument("--limit-us", type=float, required=True)
    parser.add_argument("--minimum-refreshes", type=int, default=2)
    return parser.parse_args()


def read_refreshes(
    trace: Path,
) -> tuple[dict[int, list[int]], dict[int, dict[int, list[int]]]]:
    signal_ids: dict[str, str] = {}
    values: dict[str, int] = {}
    refreshes: dict[int, list[int]] = defaultdict(list)
    phase_refreshes: dict[int, dict[int, list[int]]] = defaultdict(
        lambda: defaultdict(list)
    )
    timestamp_ns = 0
    definitions_done = False

    with trace.open(encoding="ascii") as lines:
        for raw_line in lines:
            line = raw_line.strip()
            if not line:
                continue

            if not definitions_done:
                match = VAR_RE.match(line)
                if match:
                    signal_ids[match.group(3)] = match.group(2)
                elif line == "$enddefinitions $end":
                    definitions_done = True
                    missing = {"PORTA", "GPIOR1", "RAS"} - signal_ids.keys()
                    if missing:
                        raise ValueError(
                            "trace is missing signal(s): " + ", ".join(sorted(missing))
                        )
                continue

            if line.startswith("#"):
                timestamp_ns = int(line[1:])
                continue

            if line.startswith("b"):
                bits, signal_id = line[1:].split()
                if "x" not in bits.lower() and "z" not in bits.lower():
                    values[signal_id] = int(bits, 2)
                continue

            if line[0] not in "01":
                continue

            signal_id = line[1:]
            value = int(line[0])
            previous = values.get(signal_id)
            values[signal_id] = value

            if (
                signal_id == signal_ids["RAS"]
                and value == 0
                and previous != 0
                and signal_ids["PORTA"] in values
            ):
                row = values[signal_ids["PORTA"]]
                refreshes[row].append(timestamp_ns)
                phase_id = values.get(signal_ids["GPIOR1"])
                if phase_id in PHASE_NAMES:
                    phase_refreshes[phase_id][row].append(timestamp_ns)

    return refreshes, phase_refreshes


def maximum_gap(refreshes: dict[int, list[int]]) -> tuple[int, int]:
    max_gap_ns = 0
    max_gap_row = 0
    for row, timestamps in refreshes.items():
        for previous, current in zip(timestamps, timestamps[1:]):
            gap_ns = current - previous
            if gap_ns > max_gap_ns:
                max_gap_ns = gap_ns
                max_gap_row = row
    return max_gap_ns, max_gap_row


def main() -> int:
    args = parse_args()
    if not args.trace.is_file():
        print(f"FAIL: trace was not generated: {args.trace}", file=sys.stderr)
        return 1

    try:
        refreshes, phase_refreshes = read_refreshes(args.trace)
    except (OSError, ValueError) as error:
        print(f"FAIL: cannot analyze trace: {error}", file=sys.stderr)
        return 1

    failed = False
    for phase_id, phase_name in PHASE_NAMES.items():
        phase_data = phase_refreshes[phase_id]
        missing_rows = [
            row for row in range(256) if len(phase_data[row]) < args.minimum_refreshes
        ]
        if missing_rows:
            print(
                f"FAIL: {phase_name}: {len(missing_rows)} rows had fewer than "
                f"{args.minimum_refreshes} refreshes",
                file=sys.stderr,
            )
            failed = True
            continue

        max_gap_ns, max_gap_row = maximum_gap(phase_data)
        max_gap_us = max_gap_ns / 1000
        max_gap_cycles = round(max_gap_ns * args.clock_hz / 1_000_000_000)
        status = "PASS" if max_gap_us <= args.limit_us else "FAIL"
        output = sys.stdout if status == "PASS" else sys.stderr
        print(
            f"{status}: {phase_name:<14} {max_gap_us:8.3f} us "
            f"({max_gap_cycles:5d} cycles), row {max_gap_row}",
            file=output,
        )
        failed |= status == "FAIL"

    max_gap_ns, max_gap_row = maximum_gap(refreshes)
    max_gap_us = max_gap_ns / 1000
    max_gap_cycles = round(max_gap_ns * args.clock_hz / 1_000_000_000)
    refresh_count = sum(len(timestamps) for timestamps in refreshes.values())
    status = "PASS" if max_gap_us <= args.limit_us else "FAIL"
    output = sys.stdout if status == "PASS" else sys.stderr
    print(
        f"{status}: overall        {max_gap_us:8.3f} us "
        f"({max_gap_cycles:5d} cycles), row {max_gap_row}; "
        f"{refresh_count} refreshes, limit {args.limit_us:.3f} us",
        file=output,
    )
    failed |= status == "FAIL"
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
