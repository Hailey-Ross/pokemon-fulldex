#!/usr/bin/env python3
# Applies a widescreen IPS patch to a ROM copy: applyips.py <rom in> <patch> <rom out> [<version>.check]
# With a check list, refuses a ROM whose game code differs (a hack that changed it, or another version).
# Always refuses when the code block would land on bytes that are not free (0xFF).
# SPDX-License-Identifier: GPL-3.0-or-later
import struct, sys, zlib


def records(data):
    assert data[:5] == b'PATCH'
    i = 5
    while data[i:i + 3] != b'EOF':
        off = int.from_bytes(data[i:i + 3], 'big')
        n = struct.unpack('>H', data[i + 3:i + 5])[0]
        i += 5
        if n == 0:
            count = struct.unpack('>H', data[i:i + 2])[0]
            yield off, data[i + 2:i + 3] * count
            i += 3
        else:
            yield off, data[i:i + n]
            i += n


def mismatches(rom, check):
    for line in open(check):
        if line.startswith('#') or not line.strip():
            continue
        off, size, crc, name = line.split()
        off, size = int(off, 16), int(size, 16)
        if zlib.crc32(rom[off:off + size]) != int(crc, 16):
            yield name


def main():
    rom = bytearray(open(sys.argv[1], 'rb').read())
    if len(sys.argv) > 4:
        bad = list(mismatches(rom, sys.argv[4]))
        if bad:
            sys.exit(f'the widescreen patch does not fit this ROM ({len(bad)} differences, first {bad[0]})')
    for off, chunk in records(open(sys.argv[2], 'rb').read()):
        if len(chunk) > 8 and rom[off:off + len(chunk)] != b'\xff' * len(chunk):
            sys.exit(f'{off:06X}: not free space')
        rom[off:off + len(chunk)] = chunk
    open(sys.argv[3], 'wb').write(rom)


main()
