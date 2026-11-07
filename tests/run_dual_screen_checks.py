#!/usr/bin/env python3
"""Offline host checks for second-display geometry, input and layout fallback.
Requires Java 11+, g++, pkg-config and SDL2 development headers; uses no device or ROM.
"""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="fulldex-displays-") as temporary:
    output = Path(temporary)
    subprocess.run([
        "javac", "--release", "11", "-d", str(output),
        str(root / "tools/android/allgames/java/io/github/mamesj/pokemonds/DsScreenGeometry.java"),
        str(root / "tests/android/DsScreenGeometryTest.java"),
    ], check=True)
    subprocess.run(["java", "-cp", str(output),
                    "io.github.mamesj.pokemonds.DsScreenGeometryTest"], check=True)
    flags = subprocess.check_output(["pkg-config", "--cflags", "sdl2"], text=True).split()
    subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-I" + str(root / "frontend"),
                    *flags, str(root / "tests/local_dual_screen.cpp"),
                    "-o", str(output / "native-check")], check=True)
    subprocess.run([str(output / "native-check")], check=True)
