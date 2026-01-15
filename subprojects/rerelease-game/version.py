#!/usr/bin/python3

import os
import re
import subprocess
import sys
from pathlib import Path

SEMVER_RE = re.compile(r"^\s*v?(\d+)\.(\d+)\.(\d+)(?:-([0-9A-Za-z.-]+))?\s*$")


def read_version_file():
    env_version = os.environ.get("WORR_VERSION")
    if env_version:
        return env_version.strip()

    here = Path(__file__).resolve().parent
    candidates = [
        Path.cwd() / "VERSION",
        here / "VERSION",
        here.parent / "VERSION",
        here.parent.parent / "VERSION",
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate.read_text(encoding="utf-8").splitlines()[0].strip()
    return None


def parse_semver(raw):
    if not raw:
        return "0", "0", "0", ""
    match = SEMVER_RE.match(raw)
    if not match:
        return "0", "0", "0", ""
    major, minor, patch, prerelease = match.groups()
    return major, minor, patch, prerelease or ""


def git_info():
    try:
        kwargs = {"capture_output": True, "encoding": "utf-8", "check": True}
        rev = subprocess.run(["git", "rev-list", "--count", "HEAD"], **kwargs).stdout.strip()
        sha = subprocess.run(["git", "rev-parse", "--short", "HEAD"], **kwargs).stdout.strip()
        return rev, sha
    except Exception:
        return "", ""


def build_versions():
    raw_version = read_version_file()
    major, minor, patch, prerelease = parse_semver(raw_version)
    rev, sha = git_info()

    base = f"{major}.{minor}.{patch}"

    prerelease_parts = []
    if prerelease:
        prerelease_parts.append(prerelease)

    is_release = os.environ.get("WORR_RELEASE") == "1" or os.environ.get("GITHUB_REF_TYPE") == "tag"
    if not is_release:
        if rev:
            prerelease_parts.append(f"dev.{rev}")
        else:
            prerelease_parts.append("dev")

    semver = base
    if prerelease:
        semver = f"{base}-{prerelease}"

    display = base
    if prerelease_parts:
        display = f"{base}-" + ".".join(prerelease_parts)
    if sha and not is_release:
        display = f"{display}+g{sha}"

    return {
        "version": display,
        "semver": semver,
        "revision": rev or "0",
        "major": major,
        "minor": minor,
        "patch": patch,
        "prerelease": prerelease,
        "raw": raw_version or "",
    }


def main() -> int:
    versions = build_versions()
    arg = sys.argv[1] if len(sys.argv) > 1 else "--version"

    if arg in ("--version", "--display"):
        print(versions["version"])
        return 0
    if arg == "--semver":
        print(versions["semver"])
        return 0
    if arg == "--revision":
        print(versions["revision"])
        return 0
    if arg == "--major":
        print(versions["major"])
        return 0
    if arg == "--minor":
        print(versions["minor"])
        return 0
    if arg == "--patch":
        print(versions["patch"])
        return 0
    if arg == "--prerelease":
        print(versions["prerelease"])
        return 0

    print(versions["version"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
