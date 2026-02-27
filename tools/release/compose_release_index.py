#!/usr/bin/env python3

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import pathlib
from typing import Any


def sha256_file(path: pathlib.Path) -> str:
    hasher = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            hasher.update(chunk)
    return hasher.hexdigest()


def load_metadata_files(artifacts_root: pathlib.Path) -> list[pathlib.Path]:
    files = sorted(artifacts_root.rglob("metadata-*.json"))
    if not files:
        raise SystemExit(f"No metadata files found under {artifacts_root}")
    return files


def find_asset(bundle_dir: pathlib.Path, name: str) -> pathlib.Path:
    path = bundle_dir / name
    if path.is_file():
        return path
    matches = list(bundle_dir.rglob(name))
    for match in matches:
        if match.is_file():
            return match
    raise SystemExit(f"Expected asset not found: {name} in {bundle_dir}")


def main() -> int:
    parser = argparse.ArgumentParser(description="Compose cross-platform release index from build metadata.")
    parser.add_argument("--artifacts-root", required=True, help="Directory containing downloaded build artifacts")
    parser.add_argument("--repo", required=True, help="GitHub owner/repo")
    parser.add_argument("--channel", required=True, help="Release channel")
    parser.add_argument("--version", required=True, help="Release version")
    parser.add_argument("--tag", required=True, help="Release tag")
    parser.add_argument("--commit-sha", required=True, help="Release commit")
    parser.add_argument("--index-path", required=True, help="Output release index JSON path")
    parser.add_argument("--asset-list-path", required=True, help="Output newline-delimited release asset paths")
    args = parser.parse_args()

    artifacts_root = pathlib.Path(args.artifacts_root).resolve()
    metadata_files = load_metadata_files(artifacts_root)

    targets: list[dict[str, Any]] = []
    asset_paths: list[pathlib.Path] = []

    for metadata_path in metadata_files:
        metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
        bundle_dir = metadata_path.parent
        target_files = []
        for artifact in metadata.get("artifacts", []):
            name = artifact["name"]
            file_path = find_asset(bundle_dir, name)
            rel_path = file_path.relative_to(artifacts_root).as_posix()
            target_files.append(
                {
                    "name": name,
                    "kind": artifact.get("kind", "archive"),
                    "role": artifact.get("role", ""),
                    "path": rel_path,
                    "size": file_path.stat().st_size,
                    "sha256": sha256_file(file_path),
                }
            )
            asset_paths.append(file_path)

        targets.append(
            {
                "platform_id": metadata["platform_id"],
                "os": metadata["os"],
                "arch": metadata["arch"],
                "archive_format": metadata["archive_format"],
                "autoupdater": metadata.get("autoupdater", {}),
                "files": target_files,
            }
        )

    targets.sort(key=lambda item: item["platform_id"])
    asset_paths = sorted(set(asset_paths))

    index = {
        "schema_version": 1,
        "generated_at_utc": dt.datetime.now(dt.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "repo": args.repo,
        "channel": args.channel,
        "version": args.version,
        "tag": args.tag,
        "commit_sha": args.commit_sha,
        "targets": targets,
    }

    index_path = pathlib.Path(args.index_path).resolve()
    index_path.parent.mkdir(parents=True, exist_ok=True)
    index_path.write_text(json.dumps(index, indent=2), encoding="utf-8")
    asset_paths.append(index_path)

    asset_list_path = pathlib.Path(args.asset_list_path).resolve()
    asset_list_path.parent.mkdir(parents=True, exist_ok=True)
    cwd = pathlib.Path.cwd().resolve()
    with asset_list_path.open("w", encoding="utf-8", newline="\n") as handle:
        for path in sorted(set(asset_paths)):
            resolved = path.resolve()
            try:
                display_path = resolved.relative_to(cwd).as_posix()
            except ValueError:
                display_path = resolved.as_posix()
            handle.write(f"{display_path}\n")

    print(f"Wrote {index_path}")
    print(f"Wrote {asset_list_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
