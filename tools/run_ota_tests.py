#!/usr/bin/env python3
# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0
"""Build and run host-side signed OTA package tests.

Generates EPHEMERAL signing keys and fixture packages in a temporary
directory; nothing is written into the repository.
"""

import os
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import phase4e_host as h  # noqa: E402
import sign_firmware as sf  # noqa: E402

from cryptography.hazmat.primitives.asymmetric import ec  # noqa: E402

_default_compiler = h.compiler


def _cxx_compiler():
    # This test uses the C++ standard library; `zig c++` ships libc++.
    command = _default_compiler()
    if command[1:3] == ["-m", "ziglang"]:
        return [sys.executable, "-m", "ziglang", "c++"]
    return command


h.compiler = _cxx_compiler


def payload(seed, size=5000, magic=True):
    data = bytearray((seed * 31 + i * 7 + (i >> 3)) & 0xFF for i in range(size))
    data[0] = 0xE9 if magic else 0x00
    return bytes(data)


def make_fixtures(directory, key, other_key):
    def write(name, data):
        (directory / name).write_bytes(data)

    firmware = payload(1)
    write("xiao_ok.bkota", sf.build_package(firmware, "xiao-s3", "1.2.3", 1, key))
    write("brooklyn_ok.bkota", sf.build_package(payload(2), "brooklyn-s3", "1.2.3", 1, key))
    write("xiao_old.bkota", sf.build_package(firmware, "xiao-s3", "0.9.0", 1, key))
    write("wrong_key.bkota", sf.build_package(firmware, "xiao-s3", "1.2.3", 1, other_key))
    write("unknown_key.bkota", sf.build_package(firmware, "xiao-s3", "1.2.3", 9, key))

    # Properly signed header whose digest does not describe the payload.
    header = sf.encode_header("xiao-s3", 1, "1.2.3", firmware)
    swapped = payload(3)
    write("wrong_digest.bkota", header + sf.sign_message(key, header) + swapped)

    # Properly signed package whose payload is not an ESP32 application image.
    bad = payload(4, magic=False)
    header = sf.encode_header("xiao-s3", 1, "1.2.3", bad)
    write("not_image.bkota", header + sf.sign_message(key, header) + bad)


def main():
    with tempfile.TemporaryDirectory() as tmp:
        tmpdir = Path(tmp)
        key = ec.generate_private_key(ec.SECP256R1())
        other = ec.generate_private_key(ec.SECP256R1())
        make_fixtures(tmpdir, key, other)
        public_hex = sf.public_key_bytes(key.public_key()).hex()

        exe = tmpdir / "ota_test.exe"
        if h.build(
            h.ROOT / "tools/host_tests/ota_test.cpp",
            exe,
            extra=[
                h.ROOT / "src/ota_package.cpp",
                h.ROOT / "src/ota_upload_gate.cpp",
                h.ROOT / "src/admin_auth.cpp",
            ],
        ):
            return 1
        run = subprocess.run(
            [
                str(exe),
                str(tmpdir),
                sys.executable,
                str(Path(__file__).resolve().parent / "ota_verify_oracle.py"),
                public_hex,
            ],
            capture_output=True,
            text=True,
        )
        print(run.stdout, end="")
        print(run.stderr, end="")
        return run.returncode


if __name__ == "__main__":
    sys.exit(main())
