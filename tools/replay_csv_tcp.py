#!/usr/bin/env python3
"""Replay a 3-column CSV as binary frames matching configurable_engine.json."""

import argparse
import csv
import socket
import struct
import time
from pathlib import Path


def encode_frame(row):
    if len(row) != 3:
        raise ValueError(f"expected 3 columns, got {len(row)}")
    temperature = float(row[0])
    pressure = int(float(row[1]))
    voltage = float(row[2])
    temperature_raw = round(temperature / 0.01)
    if not -32768 <= temperature_raw <= 32767:
        raise ValueError(f"temperature out of int16 range: {temperature}")
    if not 0 <= pressure <= 0xFFFFFFFF:
        raise ValueError(f"pressure out of uint32 range: {pressure}")
    return b"\xAA\x55" + struct.pack("<hIf", temperature_raw, pressure, voltage) + b"\x0D\x0A"


def rows_from_csv(path):
    with path.open("r", encoding="utf-8-sig", newline="") as stream:
        for line_number, row in enumerate(csv.reader(stream), 1):
            if not row or all(not cell.strip() for cell in row):
                continue
            try:
                yield line_number, encode_frame(row)
            except ValueError as error:
                raise ValueError(f"CSV line {line_number}: {error}") from error


def main():
    parser = argparse.ArgumentParser()
    default_csv = Path(__file__).resolve().parent.parent / "examples" / "original_config_vofa_source.csv"
    parser.add_argument(
        "csv_file",
        type=Path,
        nargs="?",
        default=default_csv,
        help=f"3-column CSV file (default: {default_csv})",
    )
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=1347)
    parser.add_argument("--interval-ms", type=float, default=20.0)
    parser.add_argument("--loop", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    if not args.csv_file.is_file():
        raise SystemExit(f"CSV file not found: {args.csv_file}")
    frames = list(rows_from_csv(args.csv_file))
    if not frames:
        raise SystemExit("CSV contains no sample rows")
    if args.dry_run:
        print(f"validated {len(frames)} frames")
        print(frames[0][1].hex(" ").upper())
        print(frames[-1][1].hex(" ").upper())
        return

    delay = max(args.interval_ms, 0.0) / 1000.0
    with socket.create_connection((args.host, args.port), timeout=5.0) as connection:
        while True:
            for _, frame in frames:
                connection.sendall(frame)
                if delay:
                    time.sleep(delay)
            if not args.loop:
                break


if __name__ == "__main__":
    main()
