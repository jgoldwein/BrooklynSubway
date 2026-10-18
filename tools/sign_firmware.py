#!/usr/bin/env python3
# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0
"""Offline signing tool for Brooklyn Map OTA packages (.bkota).

Package: 84-byte header || 64-byte raw ECDSA P-256 signature || payload.
Signed message: b"BROOKLYN-MAP-OTA-V1\\0" || header (which carries the
payload SHA-256, size, target, flags and version).

The PRIVATE key must live outside this repository. This tool refuses to
create or use a private key located inside the repository tree.
"""

import argparse
import hashlib
import os
import struct
import sys
from pathlib import Path

from cryptography.exceptions import InvalidSignature
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives.asymmetric.utils import (
    decode_dss_signature,
    encode_dss_signature,
)

REPO_ROOT = Path(__file__).resolve().parent.parent
MAGIC = b"BKMAPOTA"
FORMAT_VERSION = 1
FLAG_NETWORK_ENABLED = 0x01
HEADER_SIZE = 84
SIGNATURE_SIZE = 64
DOMAIN = b"BROOKLYN-MAP-OTA-V1\x00"
TARGETS = {"brooklyn-s3": 1, "xiao-s3": 2}
SLOT_LIMITS = {"brooklyn-s3": 0x640000, "xiao-s3": 0x330000}


def inside_repo(path):
    resolved = Path(path).resolve()
    try:
        resolved.relative_to(REPO_ROOT)
        return True
    except ValueError:
        return False


def encode_header(target, key_id, version, payload):
    version_bytes = version.encode("ascii")
    if not 1 <= len(version_bytes) <= 32 or any(
        b < 0x21 or b > 0x7E for b in version_bytes
    ):
        raise ValueError("version must be 1-32 printable ASCII characters, no spaces")
    header = (
        MAGIC
        + struct.pack(
            "<HHBBHI",
            FORMAT_VERSION,
            key_id,
            TARGETS[target],
            FLAG_NETWORK_ENABLED,
            0,
            len(payload),
        )
        + hashlib.sha256(payload).digest()
        + version_bytes.ljust(32, b"\x00")
    )
    assert len(header) == HEADER_SIZE
    return header


def sign_message(private_key, header):
    der = private_key.sign(DOMAIN + header, ec.ECDSA(hashes.SHA256()))
    r, s = decode_dss_signature(der)
    return r.to_bytes(32, "big") + s.to_bytes(32, "big")


def build_package(firmware, target, version, key_id, private_key):
    if not firmware:
        raise ValueError("firmware is empty")
    if firmware[0] != 0xE9:
        raise ValueError("firmware is not an ESP32 application image (magic 0xE9)")
    if len(firmware) > SLOT_LIMITS[target]:
        raise ValueError("firmware exceeds the OTA slot for %s" % target)
    header = encode_header(target, key_id, version, firmware)
    return header + sign_message(private_key, header) + firmware


def verify_package(package, public_key):
    """Returns (target_id, version) or raises ValueError."""
    if len(package) < HEADER_SIZE + SIGNATURE_SIZE + 1 or package[:8] != MAGIC:
        raise ValueError("not a package")
    header = package[:HEADER_SIZE]
    signature = package[HEADER_SIZE : HEADER_SIZE + SIGNATURE_SIZE]
    payload = package[HEADER_SIZE + SIGNATURE_SIZE :]
    size = struct.unpack_from("<I", header, 16)[0]
    if size != len(payload) or hashlib.sha256(payload).digest() != header[20:52]:
        raise ValueError("payload size or digest mismatch")
    r = int.from_bytes(signature[:32], "big")
    s = int.from_bytes(signature[32:], "big")
    try:
        public_key.verify(
            encode_dss_signature(r, s), DOMAIN + header, ec.ECDSA(hashes.SHA256())
        )
    except InvalidSignature as error:
        raise ValueError("bad signature") from error
    return header[12], header[52:].rstrip(b"\x00").decode("ascii")


def public_key_bytes(public_key):
    return public_key.public_bytes(
        serialization.Encoding.X962, serialization.PublicFormat.UncompressedPoint
    )


def format_public_key(key_id, public_key):
    raw = public_key_bytes(public_key)
    lines = []
    for i in range(0, len(raw), 13):
        lines.append("    " + ", ".join("0x%02x" % b for b in raw[i : i + 13]) + ",")
    return "    {%d, {\n%s\n    }}," % (key_id, "\n".join(lines))


def load_private_key(path):
    if inside_repo(path):
        raise SystemExit("refusing: private key path is inside the repository")
    data = Path(path).read_bytes()
    key = serialization.load_pem_private_key(data, password=None)
    if not isinstance(key, ec.EllipticCurvePrivateKey) or not isinstance(
        key.curve, ec.SECP256R1
    ):
        raise SystemExit("private key must be ECDSA P-256 (secp256r1)")
    return key


def cmd_keygen(args):
    if inside_repo(args.private_key):
        raise SystemExit("refusing: private key path is inside the repository")
    path = Path(args.private_key)
    if path.exists():
        raise SystemExit("refusing to overwrite existing file: %s" % path)
    path.parent.mkdir(parents=True, exist_ok=True)
    key = ec.generate_private_key(ec.SECP256R1())
    pem = key.private_bytes(
        serialization.Encoding.PEM,
        serialization.PrivateFormat.PKCS8,
        serialization.NoEncryption(),
    )
    fd = os.open(str(path), os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(fd, "wb") as handle:
        handle.write(pem)
    print("Private key written to %s (keep it offline and backed up)." % path)
    print("Public key entry for src/ota_trusted_keys.cpp:")
    print(format_public_key(args.key_id, key.public_key()))
    return 0


def cmd_sign(args):
    key = load_private_key(args.private_key)
    firmware = Path(args.firmware).read_bytes()
    package = build_package(firmware, args.target, args.version, args.key_id, key)
    Path(args.output).write_bytes(package)
    print(
        "Wrote %s (%d bytes) target=%s version=%s sha256=%s"
        % (
            args.output,
            len(package),
            args.target,
            args.version,
            hashlib.sha256(package).hexdigest(),
        )
    )
    return 0


def cmd_verify(args):
    key = load_private_key(args.private_key).public_key()
    target, version = verify_package(Path(args.package).read_bytes(), key)
    print("OK target_id=%d version=%s" % (target, version))
    return 0


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = parser.add_subparsers(dest="command", required=True)

    keygen = sub.add_parser("keygen", help="generate a signing key outside the repo")
    keygen.add_argument("--private-key", required=True)
    keygen.add_argument("--key-id", type=int, default=1)
    keygen.set_defaults(func=cmd_keygen)

    sign = sub.add_parser("sign", help="sign a firmware.bin into a .bkota package")
    sign.add_argument("--firmware", required=True)
    sign.add_argument("--target", required=True, choices=sorted(TARGETS))
    sign.add_argument("--version", required=True)
    sign.add_argument("--private-key", required=True)
    sign.add_argument("--key-id", type=int, default=1)
    sign.add_argument("--output", required=True)
    sign.set_defaults(func=cmd_sign)

    verify = sub.add_parser("verify", help="verify a package using the key's public half")
    verify.add_argument("--package", required=True)
    verify.add_argument("--private-key", required=True)
    verify.set_defaults(func=cmd_verify)

    args = parser.parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
