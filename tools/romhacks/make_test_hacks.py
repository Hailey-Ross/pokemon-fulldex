#!/usr/bin/env python3
"""Test ROM hacks for Fulldex's ROM hack support, made from the player's own ROMs (nothing of the
games leaves the PC): patch files in UPS, BPS and VCDIFF (xdelta's format, uncompressed), a
pre-patched DS ROM, and a damaged patch. Usage: make_test_hacks.py ROMS_DIR"""
import os, sys, struct, zlib, re

def num(v):  # UPS/BPS numbers
    o = bytearray()
    while True:
        x = v & 0x7f; v >>= 7
        if v == 0: o.append(0x80 | x); break
        o.append(x); v -= 1
    return bytes(o)

def foot(p, a, b):
    p += struct.pack('<II', zlib.crc32(a), zlib.crc32(b)); p += struct.pack('<I', zlib.crc32(p)); return bytes(p)

def spans(a, b):
    """(start, end, same) runs over b."""
    i, L = 0, len(b)
    # fast: compare in blocks
    out = []
    blk = 1 << 16
    start = 0; same = True
    while i < L:
        j = min(L, i + blk)
        if j <= len(a) and a[i:j] == b[i:j]:
            if not same: out.append((start, i, False)); start = i; same = True
            i = j; continue
        for k in range(i, j):
            s = k < len(a) and a[k] == b[k]
            if s != same: out.append((start, k, same)); start = k; same = s
        i = j
    out.append((start, L, same))
    return [x for x in out if x[1] > x[0]]

def bps(a, b):
    p = bytearray(b'BPS1') + num(len(a)) + num(len(b)) + num(0)
    for s, e, same in spans(a, b):
        if same: p += num(((e - s - 1) << 2) | 0)
        else: p += num(((e - s - 1) << 2) | 1) + b[s:e]
    return foot(p, a, b)

def ups(a, b):
    p = bytearray(b'UPS1') + num(len(a)) + num(len(b))
    last = 0
    for s, e, same in spans(a, b):
        if same: continue
        p += num(s - last)
        p += bytes(b[k] ^ (a[k] if k < len(a) else 0) for k in range(s, e)) + b'\0'
        last = e + 1
    return foot(p, a, b)

def vnum(v):  # VCDIFF numbers (big-endian base 128)
    out = [v & 0x7f]; v >>= 7
    while v: out.append(0x80 | (v & 0x7f)); v >>= 7
    return bytes(reversed(out))

def adler32(d): return zlib.adler32(d) & 0xffffffff

def vcdiff(a, b, window=8 << 20):
    out = bytearray(b'\xd6\xc3\xc4\x00\x04') + vnum(len(b'test.nds//base.nds/')) + b'test.nds//base.nds/'
    for w0 in range(0, len(b), window):
        w1 = min(len(b), w0 + window)
        seg = min(len(a), w1) - w0
        data = bytearray(); inst = bytearray(); addr = bytearray()
        for s, e, same in spans(a[w0:w0 + seg], b[w0:w1]):
            n = e - s
            if same:
                inst += bytes([19]) + vnum(n)       # COPY size 0 (next), mode 0 (SELF)
                addr += vnum(s)
            else:
                inst += bytes([1]) + vnum(n)        # ADD size 0 (next)
                data += b[w0 + s:w0 + e]
        tgt = bytes(b[w0:w1])
        body = vnum(len(tgt)) + b'\x00' + vnum(len(data)) + vnum(len(inst)) + vnum(len(addr)) + struct.pack('>I', adler32(tgt)) + data + inst + addr
        out += bytes([0x01 | 0x04]) + vnum(seg) + vnum(w0) + vnum(len(body)) + body
    return bytes(out)

def retitle(rom, title):
    b = bytearray(rom)
    ban = struct.unpack_from('<I', b, 0x68)[0]
    t = title.encode('utf-16-le')[:0x100].ljust(0x100, b'\0')
    for lang in range(6): b[ban + 0x240 + lang * 0x100: ban + 0x340 + lang * 0x100] = t
    return b

roms = sys.argv[1]
def find(pattern, folder=roms):
    for n in sorted(os.listdir(folder)):
        if re.search(pattern, n, re.I): return os.path.join(folder, n)
    sys.exit('no ROM matching ' + pattern)

w2 = open(find(r'white version 2'), 'rb').read()
open(os.path.join(roms, 'White 2 Test Hack.bps'), 'wb').write(bps(w2, retitle(w2, 'White 2 Test Hack\nBPS patch')))
del w2
pt = open(find(r'platinum'), 'rb').read()
open(os.path.join(roms, 'Platinum Test Hack (patched).nds'), 'wb').write(retitle(pt, 'Platinum Test Hack\nPre-patched ROM'))
open(os.path.join(roms, 'Platinum Xdelta Test.xdelta'), 'wb').write(vcdiff(pt, retitle(pt, 'Platinum Xdelta Test\nVCDIFF patch')))
del pt
open(os.path.join(roms, 'Broken Hack.xdelta'), 'wb').write(b'\xd6\xc3\xc4\x00\x00garbage-garbage')
gba = os.path.join(roms, 'GBA')
em = open(find(r'emerald', gba), 'rb').read()
e2 = bytearray(em); e2[0xA00000:0xA00004] = b'TEST'
open(os.path.join(gba, 'Emerald Test Hack.ups'), 'wb').write(ups(em, e2))
fr = open(find(r'firered', gba), 'rb').read()
patches = sys.argv[2]
safety = os.path.join(patches, 'firered11-wide.safety' if fr[0xBC] == 1 else 'firered-wide.safety')
f2 = bytearray(fr)
off = None
if os.path.exists(safety):
    for line in open(safety):
        parts = line.split()
        if len(parts) >= 4 and parts[0] != 'free':
            off = int(parts[1], 16) & 0x1FFFFFF; break
off = off if off is not None else 0x3000
f2[off] ^= 0xFF
open(os.path.join(gba, 'FireRed Forced Test.bps'), 'wb').write(bps(fr, f2))
print('made; FireRed byte changed at %#x' % off)
