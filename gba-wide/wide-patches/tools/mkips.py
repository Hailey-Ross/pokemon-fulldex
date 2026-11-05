#!/usr/bin/env python3
# Builds an IPS patch from a linked widescreen ELF: the code block, plus an 8-byte jump at each game
# function X that has a Wide_X (ldr r3,[pc]; bx r3; .word Wide_X).
# SPDX-License-Identifier: GPL-3.0-or-later
import struct, subprocess, sys

ROM = 0x08000000


def symbols(nm, elf):
    out = {}
    for line in subprocess.run([nm, elf], capture_output=True, text=True, check=True).stdout.splitlines():
        p = line.split()
        if len(p) == 3:
            out[p[2]] = (int(p[0], 16), p[1])
    return out


def record(off, data):
    assert off < 0x1000000 and off != 0x454F46 and len(data) <= 0xFFFF
    return struct.pack('>I', off)[1:] + struct.pack('>H', len(data)) + data


def main():
    nm, objcopy, elf, out = sys.argv[1:5]
    syms = symbols(nm, elf)
    sizes = subprocess.run([objcopy.replace('objcopy', 'size'), '-A', elf], capture_output=True, text=True, check=True).stdout
    for line in sizes.splitlines():
        p = line.split()
        # The patch has no RAM of its own: state lives in gWide
        assert not (p and p[0] in ('.data', '.bss') and int(p[1]) != 0), f'{p[0]} is not empty'
    blob_path = out + '.bin'
    subprocess.run([objcopy, '-O', 'binary', '-j', '.text', elf, blob_path], check=True)
    blob = open(blob_path, 'rb').read()
    start = syms['__start'][0] if '__start' in syms else 0x08F00000
    body = [record(start - ROM + i, blob[i:i + 0xFFFF]) for i in range(0, len(blob), 0xFFFF)]
    hooks = []
    for name, (addr, kind) in sorted(syms.items()):
        if not name.startswith('Wide_') or kind not in 'Tt':
            continue
        game = name[5:]
        assert game in syms, f'{name}: no address for {game} in the version file'
        target = syms[game][0] & ~1
        assert target % 4 == 0, f'{game} at {target:08X} is not word aligned'
        hooks.append(record(target - ROM, bytes.fromhex('004b1847') + struct.pack('<I', addr | 1)))
    with open(out, 'wb') as f:
        f.write(b'PATCH' + b''.join(body + hooks) + b'EOF')
    print(f'{out}: {len(blob)} bytes of code, {len(hooks)} hooks')


main()
