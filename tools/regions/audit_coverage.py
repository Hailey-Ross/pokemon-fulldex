#!/usr/bin/env python3
"""Read-only regional coverage audit. Does not establish ROM compatibility.

Pass --funcs once per unpacked platform's funcs directory and --portable for
its generated rc_portable.cpp. No ROMs or saves are opened.
"""
import argparse
import json
from pathlib import Path
import re


def audit(root, funcs=(), portable=()):
    errors = []
    aliases = dict(re.findall(r'\{\s*"([A-Z0-9]{4})",\s*"([A-Z0-9]{4})"\s*\}',
                              (root / 'frontend/gamecode.h').read_text()))
    launcher = dict(re.findall(r'\{\s*"([A-Z0-9]{4})",\s*"[^"]+",\s*"([^"]+\.sav)"\s*\}',
                               (root / 'frontend/launcher.h').read_text()))
    shifts = {}
    for path in sorted((root / 'tools/soulsilver').glob('*.json')):
        profile = json.loads(path.read_text())
        code = profile['code']
        if code in shifts:
            errors.append(f'{code}: duplicate portable profile')
        shifts[code] = path
    expected_tables = set()
    for code in sorted(launcher):
        if code in shifts or code == 'IPGE':
            continue
        expected_tables.add(code)
    for code, base in sorted(aliases.items()):
        if code not in launcher or base not in launcher:
            errors.append(f'{code}: alias or base missing from launcher')
        candidates = [root / 'tools/regions' / f'{code}.txt',
                      root / 'tools/versions' / f'{code}.txt']
        if code not in shifts and not any(p.is_file() and p.stat().st_size for p in candidates):
            errors.append(f'{code}: missing regional source metadata')
    for code in shifts:
        if code not in launcher:
            errors.append(f'{code}: portable profile missing from launcher')
    saves = list(launcher.values())
    if len(saves) != len(set(saves)):
        errors.append('launcher: shared save filename')
    for directory in funcs:
        for code in sorted(expected_tables):
            path = directory / f'{code}.txt'
            if not path.is_file() or not path.stat().st_size:
                errors.append(f'{directory}: missing or empty {code}.txt')
            direct = root / 'tools/regions' / f'{code}.txt'
            if direct.is_file() and path.is_file() and direct.read_bytes() != path.read_bytes():
                errors.append(f'{directory}: stale {code}.txt')
    for path in portable:
        text = path.read_text()
        codes = set(re.findall(r'\{"([A-Z0-9]{4})",\s*"Pokemon ', text))
        for code in sorted(set(shifts) | {'IPGE'}):
            if code not in codes:
                errors.append(f'{path}: missing portable profile {code}')
    return {'regional_aliases': len(aliases), 'function_tables': sorted(expected_tables),
            'portable_profiles': sorted(set(shifts) | {'IPGE'}),
            'checked_func_directories': [str(p) for p in funcs],
            'checked_portable_files': [str(p) for p in portable],
            'errors': errors, 'ok': not errors,
            'scope': 'Source and supplied unpacked data only; not binary embedding, revision validation or gameplay certification.'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--funcs', type=Path, action='append', default=[])
    parser.add_argument('--portable', type=Path, action='append', default=[])
    args = parser.parse_args()
    try:
        result = audit(args.root, args.funcs, args.portable)
    except (OSError, ValueError, KeyError) as error:
        parser.exit(1, f'audit failed: {error}\n')
    print(json.dumps(result, indent=2))
    return 0 if result['ok'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
