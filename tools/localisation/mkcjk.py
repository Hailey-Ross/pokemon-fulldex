#!/usr/bin/env python3
# Makes the launcher's Japanese and Korean fallback fonts: Noto Sans JP / KR (SIL OFL 1.1) cut down
# to the kana, the common kanji (JIS X 0208 level 1), the hangul of KS X 1001 and whatever the
# translations use. Needs fonttools. Usage: mkcjk.py <JP.otf> <KR.otf> <lang dir or .txt files...> <out.cpp>
import sys, glob, os
from fontTools import subset
from fontTools.ttLib import TTFont
jp, kr, outcpp = sys.argv[1], sys.argv[2], sys.argv[-1]
texts = sys.argv[3:-1]
used = set()
for t in texts:
    for p in (glob.glob(t) if not os.path.isfile(t) else [t]):
        for ch in open(p, encoding="utf-8").read():
            if ord(ch) >= 0x2000: used.add(ord(ch))
def rows(codec, r1, r2):
    s = set()
    for a in range(r1, r2 + 1):
        for b in range(0xA1, 0xFF):
            try: s.add(ord(bytes([a, b]).decode(codec)))
            except Exception: pass
    return s
jpset = set(range(0x3000, 0x3100)) | set(range(0xFF01, 0xFF5F)) | set(range(0xFF61, 0xFFA0)) | set(range(0x2010, 0x2050)) | {0x3000} | rows("euc_jp", 0xB0, 0xCF) | used
krset = set(range(0x3131, 0x3164)) | rows("euc_kr", 0xB0, 0xC8) | set(range(0x3000, 0x3040)) | set(range(0xFF01, 0xFF5F)) | set(range(0x2010, 0x2050)) | used
def cut(src, chars, dst):
    o = subset.Options(); o.layout_features = []; o.hinting = False; o.notdef_outline = True; o.name_IDs = [1, 2, 3, 4, 6]; o.glyph_names = False; o.desubroutinize = False
    f = TTFont(src); s = subset.Subsetter(o); s.populate(unicodes=sorted(chars)); s.subset(f)
    f.save(dst); return os.path.getsize(dst)
d = os.path.dirname(os.path.abspath(outcpp))
a = cut(jp, jpset, d + "/ja-cut.otf"); b = cut(kr, krset, d + "/ko-cut.otf")
print("ja", a, "ko", b)
def arr(name, path):
    data = open(path, "rb").read()
    lines = ["extern const unsigned char %s[] = {" % name]
    for i in range(0, len(data), 24): lines.append(",".join(str(x) for x in data[i:i+24]) + ",")
    lines.append("};"); lines.append("extern const unsigned int %sSize = %d;" % (name, len(data)))
    return "\n".join(lines)
open(outcpp, "w", newline="\n").write("// Made by tools/localisation/mkcjk.py: Noto Sans JP and KR (SIL Open Font License 1.1), cut down. See fonts/FONT-LICENSES.txt.\n#include \"launcher_cjk.h\"\n" + arr("kCjkJp", d + "/ja-cut.otf") + "\n" + arr("kCjkKr", d + "/ko-cut.otf") + "\n")
