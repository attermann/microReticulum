#!/usr/bin/env python3
"""
Encode a ResourceAdvertisement with known inputs using Python RNS and print
the packed bytes as a hex literal suitable for pasting into the C++ unit
test at test/test_resource_advertisement/test_resource_advertisement.cpp
(constant PYTHON_GOLDEN_HEX).

Use the local Reticulum fork rather than the system-installed one so the
wire format we validate against matches the upstream version the C++ port
is targeting:

    PYTHONPATH=/Users/chad/python/Reticulum-latest python3 encode_advertisement.py

Outputs a single string (no whitespace) and the unpacked field values for
reference.
"""

import sys

try:
    import RNS
    from RNS.Resource import ResourceAdvertisement
except ImportError as e:
    print(f"Failed to import RNS — set PYTHONPATH or install the package: {e}",
          file=sys.stderr)
    sys.exit(1)


def main():
    # These inputs must stay in sync with the C++ test's expected field values.
    adv = ResourceAdvertisement()
    adv.t = 12345
    adv.d = 67890
    adv.n = 7
    adv.i = 1
    adv.l = 3
    adv.h = bytes(range(16))                       # 16 bytes 0x00..0x0f
    adv.r = bytes([0x10, 0x11, 0x12, 0x13])        # 4 bytes
    adv.o = bytes([0x20 + i for i in range(16)])   # 16 bytes 0x20..0x2f
    adv.q = bytes([0x30 + i for i in range(8)])    # 8 bytes 0x30..0x37
    adv.m = bytes([0x40 + i for i in range(28)])   # 28 bytes (7 * MAPHASH_LEN)
    adv.e = True
    adv.c = False
    adv.s = True
    adv.u = True
    adv.p = False
    adv.x = True
    adv.f = ((adv.x << 5) | (adv.p << 4) | (adv.u << 3)
             | (adv.s << 2) | (adv.c << 1) | adv.e)

    packed = adv.pack(0)
    print(f"Python RNS {RNS.__version__}: packed {len(packed)} bytes")
    print(f"Hex (copy into PYTHON_GOLDEN_HEX in the C++ test):")
    print(packed.hex())

    # Round-trip via Python to confirm the encoded bytes decode to the same
    # values the C++ test expects.
    back = ResourceAdvertisement.unpack(packed)
    print()
    print("Decoded field values:")
    for field in ("t", "d", "n", "i", "l", "f"):
        print(f"  {field} = {getattr(back, field)}")
    for field in ("h", "r", "o", "q", "m"):
        v = getattr(back, field)
        print(f"  {field} = {v.hex()} ({len(v)} bytes)")
    print(f"  flags: e={back.e} c={back.c} s={back.s} u={back.u} p={back.p} x={back.x}")


if __name__ == "__main__":
    main()
