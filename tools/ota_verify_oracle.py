#!/usr/bin/env python3
# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0
"""Host-test oracle: ECDSA P-256/SHA-256 verify over a message file.

Usage: ota_verify_oracle.py <pubkey_hex_65> <message_file> <signature_file>
Exit 0 = valid, 1 = invalid. Used only by the host test, which has no mbedTLS.
"""

import sys
from pathlib import Path

from cryptography.exceptions import InvalidSignature
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives.asymmetric.utils import encode_dss_signature


def main():
    try:
        public = ec.EllipticCurvePublicKey.from_encoded_point(
            ec.SECP256R1(), bytes.fromhex(sys.argv[1])
        )
        message = Path(sys.argv[2]).read_bytes()
        signature = Path(sys.argv[3]).read_bytes()
        r = int.from_bytes(signature[:32], "big")
        s = int.from_bytes(signature[32:], "big")
        public.verify(encode_dss_signature(r, s), message, ec.ECDSA(hashes.SHA256()))
    except (InvalidSignature, ValueError, IndexError):
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
