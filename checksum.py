#!/usr/bin/env python3
import argparse
from pathlib import Path

LOADER_LEN = 0xBC0
BGND_ADDR = 0x0FFE

# Initial accumulator values
VARIANTS = {
    "512KB": (0x00080000, 0xFFDA, 0xF7CC),
    "1MB": (0x00100000, 0xFFD2, 0xF7CC),
    "2MB": (0x00200000, 0xFFC2, 0xF7CC),
    "4MB": (0x00400000, 0xFFA2, 0xF7CC),
}

# Alternating 16-bit sums of the fixed 64-byte EEPROM preloader at 0xFC0.
PRELOADER_EVEN_SUM = 0x5116
PRELOADER_ODD_SUM = 0xC086


def loader_sums(loader):
    words = [int.from_bytes(loader[i:i + 2], "big") for i in range(0, len(loader), 2)]
    return sum(words[0::2]) & 0xFFFF, sum(words[1::2]) & 0xFFFF


def checksum(loader, variant):
    sram_size, initial_even, initial_odd = VARIANTS[variant]
    loader_even, loader_odd = loader_sums(loader)

    # Vector 0 contains SRAM size, vector 1 contains 0x408, and vectors 2..255
    # point to the preloader's BGND instruction at 0xFFE.
    vector_even = sram_size >> 16
    vector_odd = 0x0408 + (254 * BGND_ADDR)
    even = (initial_even + vector_even + loader_even + PRELOADER_EVEN_SUM) & 0xFFFF
    odd = (initial_odd + vector_odd + loader_odd + PRELOADER_ODD_SUM) & 0xFFFF
    return even, odd


def main():
    parser = argparse.ArgumentParser(description="Patch a loader for the fixed supervisor checksum")
    parser.add_argument("image", type=Path)
    parser.add_argument("variant", choices=VARIANTS)
    args = parser.parse_args()

    loader = bytearray(args.image.read_bytes())
    if len(loader) != LOADER_LEN:
        raise SystemExit(f"{args.image}: expected {LOADER_LEN:#x} bytes, got {len(loader):#x}")
    if loader[:8] != b"|load\xfd\xc4\x55":
        raise SystemExit(f"{args.image}: invalid loader signature")

    # The last two loader words are padding and feed different accumulators.
    even_patch = LOADER_LEN - 4
    odd_patch = LOADER_LEN - 2
    loader[even_patch:odd_patch + 2] = b"\0\0\0\0"
    even, odd = checksum(loader, args.variant)
    loader[even_patch:even_patch + 2] = (-even & 0xFFFF).to_bytes(2, "big")
    loader[odd_patch:odd_patch + 2] = (-odd & 0xFFFF).to_bytes(2, "big")

    even, odd = checksum(loader, args.variant)
    if even != 0 or (odd & 0xFF) != 0:
        raise SystemExit(f"checksum patch failed: even={even:04x}, odd={odd:04x}")

    args.image.write_bytes(loader)
    print(f"patched {args.image} for {args.variant}: even={even:04x}, odd={odd:04x}")


if __name__ == "__main__":
    main()
