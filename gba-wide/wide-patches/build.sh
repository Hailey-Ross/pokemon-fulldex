#!/bin/sh
# Builds the widescreen IPS patches: build.sh <pokefirered checkout> <pokeemerald checkout> <out folder>
# Needs arm-none-eabi-gcc and python3. The decompilations only provide headers (structs and constants).
# SPDX-License-Identifier: GPL-3.0-or-later
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
FR=${1:?pokefirered checkout}
EM=${2:?pokeemerald checkout}
OUT=${3:?out folder}

CFLAGS="-mthumb -mthumb-interwork -mcpu=arm7tdmi -mabi=apcs-gnu -mlong-calls -O2 -Wall -Wno-unused-function
 -fno-common -ffunction-sections -fno-builtin -DMODERN=1"

# family <name> <decomp> <defines> <versions...>
family() {
    name=$1 decomp=$2 defines=$3
    shift 3
    rm -rf "$OUT/$name"
    mkdir -p "$OUT/$name"
    for src in "$HERE"/src/*.c; do
        arm-none-eabi-gcc $CFLAGS $defines -I"$HERE/include" -I"$HERE/include/$name" -iquote "$decomp/include" -iquote "$decomp" \
            -c "$src" -o "$OUT/$name/$(basename "$src" .c).o"
    done
    for v in "$@"; do
        cat "$HERE/link.ld" "$HERE/versions/$v.ld" > "$OUT/$v-link.ld"
        arm-none-eabi-gcc -nostdlib -mthumb -mcpu=arm7tdmi -T "$OUT/$v-link.ld" -Wl,--no-warn-rwx-segments \
            -o "$OUT/$v.elf" "$OUT/$name"/*.o
        python3 "$HERE/tools/mkips.py" arm-none-eabi-nm arm-none-eabi-objcopy "$OUT/$v.elf" "$OUT/$v.ips"
    done
}

family firered "$FR" "-DWIDE_FIRERED -DFIRERED" firered firered11 leafgreen leafgreen11
family emerald "$EM" "-DWIDE_EMERALD" emerald
