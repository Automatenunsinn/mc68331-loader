#!/usr/bin/env python3
import argparse
from pathlib import Path

import serial


def main() -> None:
    parser = argparse.ArgumentParser(description="Upload a loader over a serial port")
    parser.add_argument("image", type=Path)
    parser.add_argument("--port", default="/dev/ttyUSB0")
    parser.add_argument("--baud", type=int, default=57600)
    args = parser.parse_args()

    data = args.image.read_bytes()
    with serial.Serial(args.port, args.baud, timeout=0.1, write_timeout=2.5) as port:
        port.reset_output_buffer()
        for offset in range(0, len(data), 64):
            port.write(data[offset:offset + 64])
        port.flush()

    print(f"Uploaded {len(data)} bytes from {args.image} to {args.port} at {args.baud} baud")


if __name__ == "__main__":
    main()
