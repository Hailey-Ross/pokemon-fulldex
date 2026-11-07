#!/usr/bin/env python3
# Makes frontend/launcher_lang_data.h from frontend/lang/launcher-xx.txt (the launcher's words in each language).
import glob, os, sys
root = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "frontend")
files = sorted(glob.glob(os.path.join(root, "lang", "launcher-*.txt")))
o = ["#pragma once", "// Made by mklang.py from lang/launcher-xx.txt (the launcher's words in each language): do not edit here.", "#include <cstring>"]
codes = []
for f in files:
    code = os.path.basename(f)[len("launcher-"):-4]
    codes.append(code)
    txt = open(f, encoding="utf-8").read()
    assert ')FDXLANG"' not in txt
    o.append('static const char* const kLauncherLang_%s = u8R"FDXLANG(%s)FDXLANG";' % (code, txt))
o.append("inline const char* LauncherLangData(const char* code)")
o.append("{")
for c in codes:
    o.append('    if (!strcmp(code, "%s")) return (const char*)kLauncherLang_%s;' % (c, c))
o.append("    return nullptr;")
o.append("}")
open(os.path.join(root, "launcher_lang_data.h"), "w", encoding="utf-8", newline="\n").write("\n".join(o) + "\n")
print("languages:", codes)
