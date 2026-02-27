#!/usr/bin/python3

import datetime as dt
import json
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

    candidates = [
        Path.cwd() / "VERSION",
        Path(__file__).resolve().parent / "VERSION",
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


def parse_int(value, default=0):
    try:
        return int(value)
    except Exception:
        return default


def clamp(value, low, high):
    return max(low, min(high, value))


def build_versions():
    raw_version = read_version_file()
    major, minor, patch, prerelease = parse_semver(raw_version)
    rev, sha = git_info()

    base = f"{major}.{minor}.{patch}"
    channel = os.environ.get("WORR_CHANNEL", "stable").strip().lower() or "stable"
    nightly_date = os.environ.get("WORR_NIGHTLY_DATE", "").strip()
    if not nightly_date:
        nightly_date = dt.datetime.now(dt.timezone.utc).strftime("%Y%m%d")

    is_release = os.environ.get("WORR_RELEASE") == "1" or os.environ.get("GITHUB_REF_TYPE") == "tag"
    release_kind = "release" if is_release else "dev"

    if channel == "nightly":
        release_kind = "nightly"
        nightly_parts = []
        if prerelease:
            nightly_parts.append(prerelease)
        nightly_parts.append(f"nightly.{nightly_date}")
        if rev:
            nightly_parts.append(f"r{parse_int(rev, 0):08d}")
        else:
            nightly_parts.append("r00000000")
        semver = f"{base}-" + ".".join(nightly_parts)
        display = semver
        if sha:
            display = f"{display}+g{sha}"
    elif is_release:
        semver = base
        if prerelease:
            semver = f"{base}-{prerelease}"
        display = semver
    else:
        semver = base
        if prerelease:
            semver = f"{base}-{prerelease}"

        prerelease_parts = []
        if prerelease:
            prerelease_parts.append(prerelease)
        if rev:
            prerelease_parts.append(f"dev.{rev}")
        else:
            prerelease_parts.append("dev")

        display = f"{base}-" + ".".join(prerelease_parts)
        if sha:
            display = f"{display}+g{sha}"

    msi_major = clamp(parse_int(major, 0), 0, 255)
    msi_minor = clamp(parse_int(minor, 0), 0, 255)
    msi_patch = parse_int(patch, 0)
    if channel == "nightly" and rev:
        msi_patch = parse_int(rev, 0)
    msi_patch = clamp(msi_patch, 0, 65535)
    msi_version = f"{msi_major}.{msi_minor}.{msi_patch}"

    return {
        "version": display,
        "semver": semver,
        "revision": rev or "0",
        "major": major,
        "minor": minor,
        "patch": patch,
        "prerelease": prerelease,
        "channel": channel,
        "nightly_date": nightly_date,
        "release_kind": release_kind,
        "msi_version": msi_version,
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
    if arg == "--channel":
        print(versions["channel"])
        return 0
    if arg == "--nightly-date":
        print(versions["nightly_date"])
        return 0
    if arg == "--msi-version":
        print(versions["msi_version"])
        return 0
    if arg == "--json":
        print(json.dumps(versions, indent=2))
        return 0

    print(versions["version"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
