#!/usr/bin/env python3
"""The fingerprint romhacks.h tells a DS hack from its game by: the CRC-32 of the ARM9 program
(from 0x8000: not the secure area, which copies of one game have differently), the ARM7 program,
the file name and file allocation tables, the overlay tables, the banner (icon and title) and the
ARM9 overlays' files. A hack
changes these (its code, or its files' sizes); copies of the clean game don't. Reads paths (one per
line, optionally after a size and a space) on stdin; prints code, revision, size, fingerprint and
the full CRC-32 (only numbers and names: nothing of the ROMs)."""
import sys, os, zlib, struct

def regions(f, size):
    h = f.read(0x200)
    if len(h) < 0x200: return None, h
    r = lambda o: struct.unpack_from('<I', h, o)[0]
    a9, a9s, a7, a7s = r(0x20), r(0x2C), r(0x30), r(0x3C)
    fnt, fnts, fat, fats = r(0x40), r(0x44), r(0x48), r(0x4C)
    ov9, ov9s, ov7, ov7s = r(0x50), r(0x54), r(0x58), r(0x5C)
    ban = r(0x68)
    out = [(max(a9, 0x8000), a9 + a9s), (a7, a7 + a7s), (fnt, fnt + fnts), (fat, fat + fats), (ov9, ov9 + ov9s), (ov7, ov7 + ov7s), (ban, ban + 0x840 if ban else 0)]
    if ov9s % 32 == 0 and ov9s <= 32 * 1024 and fats % 8 == 0 and fat + fats <= size and ov9 + ov9s <= size:
        f.seek(ov9); table = f.read(ov9s)
        f.seek(fat); fatd = f.read(fats)
        for i in range(0, len(table), 32):
            fid = struct.unpack_from('<I', table, i + 0x18)[0]
            if fid * 8 + 8 <= len(fatd):
                s, e = struct.unpack_from('<II', fatd, fid * 8)
                out.append((s, e))
    return out, h

def fingerprint(path):
    size = os.path.getsize(path)
    with open(path, 'rb') as f:
        regs, h = regions(f, size)
        if regs is None: return None, h
        crc, total = 0, 0
        for s, e in regs:
            e = min(e, size)
            if e <= s: continue
            total += e - s
            if total > (64 << 20): return None, h
            f.seek(s); crc = zlib.crc32(f.read(e - s), crc)
        return crc, h

if __name__ == '__main__':
    seen = set()
    for line in sys.stdin:
        p = line.rstrip('\n')
        if not os.path.exists(p): p = p.split(' ', 1)[-1]
        if not os.path.exists(p): continue
        key = (os.path.basename(p), os.path.getsize(p))
        if key in seen: continue
        seen.add(key)
        fp, h = fingerprint(p)
        if fp is None: continue
        print('%s\t%d\t%d\t%08X\t%s' % (h[0xC:0x10].decode('ascii', 'replace'), h[0x1E], os.path.getsize(p), fp, os.path.basename(p)), flush=True)
