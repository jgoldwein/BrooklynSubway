# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0

import os
import re
from pathlib import Path


Import("env")

version = os.environ.get("BROOKLYN_FIRMWARE_VERSION", "0.0.0")
if re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", version) is None:
    raise ValueError("BROOKLYN_FIRMWARE_VERSION must be a three-part numeric version")

components = [int(component) for component in version.split(".")]
if version != "0.0.0":
    # Public releases must never compile developer Wi-Fi/MQTT credentials.
    env.Append(CPPDEFINES=[("BROOKLYN_RELEASE_BUILD", 1)])
env.Append(
    CPPDEFINES=[
        ("BROOKLYN_FIRMWARE_VERSION_MAJOR", components[0]),
        ("BROOKLYN_FIRMWARE_VERSION_MINOR", components[1]),
        ("BROOKLYN_FIRMWARE_VERSION_PATCH", components[2]),
    ]
)
build_dir = Path(env.subst("$BUILD_DIR"))
build_dir.mkdir(parents=True, exist_ok=True)
(build_dir / "brooklyn_firmware_version.txt").write_text(
    version + "\n", encoding="ascii"
)
