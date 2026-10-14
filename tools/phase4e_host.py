#!/usr/bin/env python3
# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0
"""Shared host-compiler helpers for the Phase 4E tools (no hardware)."""

import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CORE_SOURCES = [
    ROOT / "src/gtfs_rt_decoder.cpp",
    ROOT / "src/gtfs_rt_state.cpp",
    ROOT / "src/previous_stop.cpp",
    ROOT / "src/generated/gtfs_topology_table.cpp",
    ROOT / "src/gtfs_adapter.cpp",
    ROOT / "src/generated/gtfs_stop_table.cpp",
    ROOT / "src/train_position_resolver.cpp",
    ROOT / "src/semantic_api.cpp",
    ROOT / "src/generated/semantic_tables.cpp",
    ROOT / "src/led_map.cpp",
    ROOT / "src/train_frame.cpp",
    ROOT / "src/rgb_renderer.cpp",
    ROOT / "src/train_pipeline.cpp",
]


def compiler():
    for name in ("g++", "clang++"):
        if shutil.which(name):
            return [name]
    probe = subprocess.run([sys.executable, "-m", "ziglang", "version"],
                           capture_output=True, text=True)
    if probe.returncode == 0:
        return [sys.executable, "-m", "ziglang", "cc", "-x", "c++"]
    sys.exit("no host C++ compiler found (install g++/clang++ or `pip install ziglang`)")


def build(main_cpp, exe):
    cmd = compiler() + ["-std=c++17", "-Wall", "-Wextra", "-Werror", "-O1", "-fno-exceptions",
                        f"-I{ROOT / 'include'}", f"-I{ROOT / 'tools/host_tests'}",
                        str(main_cpp), *map(str, CORE_SOURCES), "-o", str(exe)]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        print(r.stdout + r.stderr)
    return r.returncode


def build_and_run(main_cpp, args):
    with tempfile.TemporaryDirectory() as tmp:
        exe = Path(tmp) / "phase4e_host.exe"
        rc = build(main_cpp, exe)
        if rc != 0:
            return rc
        run = subprocess.run([str(exe), *args], capture_output=True, text=True,
                             encoding="utf-8", errors="replace")
        sys.stdout.write(run.stdout)
        sys.stdout.write(run.stderr)
        return run.returncode
