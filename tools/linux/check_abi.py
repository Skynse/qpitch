#!/usr/bin/env python3
"""Reject Linux plugin binaries compiled above our supported glibc baseline."""
import pathlib
import re
import subprocess
import sys

root = pathlib.Path(sys.argv[1])
binaries = list(root.glob("VST3/**/*.so")) + list(root.glob("CLAP/*.clap"))
if len(binaries) != 2:
    raise SystemExit(f"Expected VST3 and CLAP binaries under {root}, found {len(binaries)}")
for binary in binaries:
    versions = subprocess.check_output(["readelf", "--version-info", str(binary)], text=True)
    requirements = {tuple(map(int, value.split("."))) for value in re.findall(r"GLIBC_(\d+\.\d+(?:\.\d+)?)", versions)}
    maximum = max(requirements)
    if maximum > (2, 36):
        raise SystemExit(f"{binary}: glibc {'.'.join(map(str, maximum))} exceeds the 2.36 baseline; use tools/linux/build.sh")
    dependencies = subprocess.check_output(["readelf", "-d", str(binary)], text=True)
    if "librubberband" in dependencies:
        raise SystemExit(f"{binary}: Rubber Band must be statically linked")
    print(f"PASS {binary.name}: glibc <= {'.'.join(map(str, maximum))}, Rubber Band bundled")
