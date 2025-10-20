#!/usr/bin/env python3
from __future__ import annotations

import os
import re
from pathlib import Path


def main() -> int:
    source_root = Path(os.environ.get('MESON_SOURCE_ROOT', Path(__file__).resolve().parents[1]))
    header = source_root / 'subprojects' / 'libpng-1.6.50' / 'pngconf.h'

    if not header.exists():
        return 0

    original = header.read_text(encoding='utf-8')
    updated = original
    changed = False

    patch_path = source_root / 'subprojects' / 'packagefiles' / 'libpng' / 'msvc-restrict.patch'
    replacements: list[tuple[str, str]] = []
    if patch_path.exists():
        pending: str | None = None
        for line in patch_path.read_text(encoding='utf-8').splitlines():
            if line.startswith(('---', '+++', '@@')):
                pending = None
                continue
            if line.startswith('-') and not line.startswith('--'):
                pending = line[1:]
                continue
            if line.startswith('+') and not line.startswith('++') and pending is not None:
                replacements.append((pending, line[1:]))
                pending = None

    replacements.extend([
        ('__declspec(PNG_RESTRICT)', '__declspec(restrict)'),
        ('__declspec(__restrict)', '__declspec(restrict)'),
    ])

    for old, new in replacements:
        if old in updated:
            updated = updated.replace(old, new)
            changed = True

    pattern = re.compile(r'(#\s*define\s+PNG_ALLOCATED\s+__declspec\()\s*PNG_RESTRICT\s*(\))')
    updated, count = pattern.subn(r'\1restrict\2', updated)
    if count > 0:
        changed = True

    if changed:
        header.write_text(updated, encoding='utf-8')

    return 0


if __name__ == '__main__':
    raise SystemExit(main())
