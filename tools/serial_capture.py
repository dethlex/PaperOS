#!/usr/bin/env python3
"""Reset the M5Paper via DTR/RTS and capture serial output.

Used by `make reset-cycle` and `make serial`. PIO's own device monitor
needs a real TTY; this script works in any shell or CI.
"""

import argparse
import glob
import sys
import time

import serial


def find_port() -> str:
    candidates = glob.glob("/dev/cu.usbserial-*")
    if not candidates:
        print("no /dev/cu.usbserial-* device found", file=sys.stderr)
        sys.exit(1)
    return candidates[0]


def reset(s: serial.Serial) -> None:
    # CH9102 wiring on M5Paper: RTS -> EN, DTR -> IO0 (both active-low at chip).
    # pyserial True = line asserted (drives chip pin low).
    s.rts = False
    s.dtr = False
    time.sleep(0.05)
    s.rts = True   # EN low (reset asserted)
    time.sleep(0.15)
    s.rts = False  # EN high (release; IO0 stays HIGH = normal boot)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default=None)
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--seconds", type=float, default=10.0)
    ap.add_argument("--reset", action="store_true", help="pulse reset before reading")
    args = ap.parse_args()

    port = args.port or find_port()
    s = serial.Serial()
    s.port = port
    s.baudrate = args.baud
    s.timeout = 0.3
    s.rtscts = False
    s.dsrdtr = False
    s.open()

    if args.reset:
        reset(s)
        s.reset_input_buffer()

    end = time.time() + args.seconds
    buf = bytearray()
    while time.time() < end:
        chunk = s.read(8192)
        if chunk:
            buf.extend(chunk)
    s.close()

    sys.stdout.buffer.write(bytes(buf))
    print(f"\n--- {len(buf)} bytes from {port} ---", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
