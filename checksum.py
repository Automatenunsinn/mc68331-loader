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

# Final checksum words from the matching original ADP loaders.  Keep these
# loader-specific values intact and compensate in the preceding checksum slots.
ORIGINAL_CHECKSUMS = {
    "50B_1MB": 0x5676,   # 61640302_5.0b.bin
    "50B_2MB": 0x5400,   # 61640403_5.0b_2MB.bin
    "50A_1MB": 0xB05D,   # 314159XX_5.0a.bin
    "ALT_1MB": 0x1464,   # 616470XX_5.0a.bin
    "UHG_1MB": 0x5743,   # 53746CXX_Ls3.0.bin
    "ROTE_512KB": 0xADF4, # 314159XX_3.0.bin
}

VERSION_VARIANTS = {
    "50B_1MB": "1MB",
    "50B_2MB": "2MB",
    "50A_1MB": "1MB",
    "ALT_1MB": "1MB",
    "UHG_1MB": "1MB",
    "ROTE_512KB": "512KB",
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
    parser.add_argument("loader_version", nargs="?", default="50B_1MB")
    args = parser.parse_args()

    checksum_version = args.loader_version
    if checksum_version not in ORIGINAL_CHECKSUMS:
        checksum_version = "50B_1MB"
    variant = VERSION_VARIANTS.get(args.loader_version, "1MB")

    loader = bytearray(args.image.read_bytes())
    if len(loader) != LOADER_LEN:
        raise SystemExit(f"{args.image}: expected {LOADER_LEN:#x} bytes, got {len(loader):#x}")
    if loader[:8] != b"|load\xfd\xc4\x55":
        raise SystemExit(f"{args.image}: invalid loader signature")

    # Preserve the original loader's final checksum word.  Since alternating
    # words feed separate accumulators, the odd correction lives two words
    # before it and the even correction immediately before it.
    odd_patch = LOADER_LEN - 6
    even_patch = LOADER_LEN - 4
    checksum_word = LOADER_LEN - 2
    loader[odd_patch:checksum_word + 2] = b"\0\0\0\0\0\0"
    loader[checksum_word:checksum_word + 2] = ORIGINAL_CHECKSUMS[checksum_version].to_bytes(2, "big")
    even, odd = checksum(loader, variant)
    loader[even_patch:even_patch + 2] = (-even & 0xFFFF).to_bytes(2, "big")
    loader[odd_patch:odd_patch + 2] = (-odd & 0xFFFF).to_bytes(2, "big")

    even, odd = checksum(loader, variant)
    if even != 0 or (odd & 0xFF) != 0:
        raise SystemExit(f"checksum patch failed: even={even:04x}, odd={odd:04x}")

    args.image.write_bytes(loader)
    print(f"patched {args.image} for {args.loader_version}/{variant}: "
          f"checksum={ORIGINAL_CHECKSUMS[checksum_version]:04x} ({checksum_version}), "
          f"even={even:04x}, odd={odd:04x}")


if __name__ == "__main__":
    main()
