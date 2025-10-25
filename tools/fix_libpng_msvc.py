#!/usr/bin/env python3
from __future__ import annotations

import os
import re
from pathlib import Path


def _find_headers(source_root: Path, filename: str) -> list[Path]:
    subprojects = source_root / 'subprojects'
    headers: list[Path] = []
    seen: set[Path] = set()

    search_patterns = [
        subprojects.glob(f'libpng-1.6.50*/**/{filename}'),
        (subprojects / 'packagecache').glob(f'libpng_*/*/**/{filename}'),
    ]

    for pattern in search_patterns:
        for candidate in pattern:
            if candidate.is_file() and candidate not in seen:
                seen.add(candidate)
                headers.append(candidate)

    return sorted(headers)


def _collect_patch_replacements(patch_path: Path) -> list[tuple[str, str]]:
    replacements: list[tuple[str, str]] = []

    if not patch_path.exists():
        return replacements

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

    return replacements


def _apply_replacements(header: Path, replacements: list[tuple[str, str]]) -> bool:
    original = header.read_text(encoding='utf-8')
    updated = original
    changed = False

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

    return changed


def main() -> int:
    source_root = Path(os.environ.get('MESON_SOURCE_ROOT', Path(__file__).resolve().parents[1]))

    headers = []
    for name in ('pngconf.h', 'png.h'):
        headers.extend(_find_headers(source_root, name))

    if not headers:
        return 0

    patch_path = source_root / 'subprojects' / 'packagefiles' / 'libpng' / 'msvc-restrict.patch'
    replacements = _collect_patch_replacements(patch_path)
    replacements.extend([
        ('__declspec(PNG_RESTRICT)', '__declspec(restrict)'),
        ('__declspec(__restrict)', '__declspec(restrict)'),
    ])

    any_changed = False
    for header in headers:
        if _apply_replacements(header, replacements):
            any_changed = True

    if not any_changed:
        raise RuntimeError('Failed to update libpng headers: no replacements applied.')

    return 0


if __name__ == '__main__':
    raise SystemExit(main())
