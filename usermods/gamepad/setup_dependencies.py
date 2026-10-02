#!/usr/bin/env python3
"""Initialise pinned gamepad dependencies and apply the tracked compatibility patches."""
import argparse
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
PATCHES = Path(__file__).resolve().parent / "patches"
DEPENDENCIES = (
    (ROOT, "lib/btstack"),
    (ROOT, "lib/pico-sdk"),
    (ROOT, "lib/tinyusb"),
    (ROOT, "usermods/gamepad/lib/bluepad32"),
    (ROOT / "lib/pico-sdk", "lib/cyw43-driver"),
)
PATCH_SERIES = (
    ("lib/btstack", "btstack-synchronous-gap-buffer.patch"),
    ("lib/pico-sdk", "pico-sdk-static-bt-firmware-buffers.patch"),
    ("usermods/gamepad/lib/bluepad32", "bluepad32-pico-micropython-compat.patch"),
    ("usermods/gamepad/lib/bluepad32", "bluepad32-classic-reconnect.patch"),
)


def git(directory, *args, check=True):
    return subprocess.run(
        ["git", "-C", str(directory), *args], check=check,
        text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
    )


def initialise(parent, path, check_only):
    directory = parent / path
    entry = git(parent, "ls-files", "--stage", "--", path).stdout.split()
    if len(entry) < 3 or entry[0] != "160000" or entry[2] != "0":
        raise RuntimeError(f"{directory}: missing or conflicted submodule registration")
    expected = entry[1]
    if not (directory / ".git").exists():
        if check_only:
            raise RuntimeError(f"{directory}: not initialised; run this script without --check")
        git(parent, "submodule", "update", "--init", "--", path)
    actual = git(directory, "rev-parse", "HEAD").stdout.strip()
    if actual != expected:
        raise RuntimeError(
            f"{directory}: HEAD is {actual}, expected {expected}; "
            "resolve the revision mismatch manually (local work is preserved)"
        )
    print(f"Pinned {directory.relative_to(ROOT)}: {actual}", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="verify only; do not fetch or apply patches")
    args = parser.parse_args()
    for parent, path in DEPENDENCIES:
        initialise(parent, path, args.check)

    pending = []
    # Validate the entire series before making any source changes. Already
    # applied patches are recognised, so repeated setup does not reapply them.
    for path, filename in PATCH_SERIES:
        directory = ROOT / path
        patch = PATCHES / filename
        if git(directory, "apply", "--reverse", "--check", str(patch), check=False).returncode == 0:
            print(f"Already applied: {filename}", flush=True)
            continue
        forward = git(directory, "apply", "--check", str(patch), check=False)
        if forward.returncode:
            raise RuntimeError(f"{filename}: patch conflicts with local source\n{forward.stderr}")
        pending.append((directory, patch))
    if args.check and pending:
        raise RuntimeError("Missing patches: " + ", ".join(p.name for _, p in pending))
    for directory, patch in pending:
        git(directory, "apply", str(patch))
        print(f"Applied: {patch.name}", flush=True)
    print("Gamepad dependencies ready. Patched submodules are intentionally dirty.")


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, subprocess.CalledProcessError) as error:
        print(f"Dependency setup failed: {error}", file=sys.stderr)
        if isinstance(error, subprocess.CalledProcessError):
            print(error.stderr, file=sys.stderr)
        sys.exit(1)
