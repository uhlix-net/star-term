"""Builds Star Term perpetual license keys, signed with the product's
Ed25519 private key. Verified by windows/star_term/star_term/licensing.py
against the matching embedded public key."""

import base64
import datetime
import json
import struct
from pathlib import Path

from cryptography.hazmat.primitives.serialization import load_pem_private_key

PRODUCT = "star_term"


def build_license_key(licensee: str, email: str, private_key_path: str) -> str:
    private_key = load_pem_private_key(Path(private_key_path).read_bytes(), password=None)

    payload = {
        "licensee": licensee,
        "email": email,
        "issued": datetime.date.today().isoformat(),
        "product": PRODUCT,
    }
    payload_bytes = json.dumps(payload, separators=(",", ":"), sort_keys=True).encode("utf-8")
    signature = private_key.sign(payload_bytes)

    token = struct.pack(">H", len(payload_bytes)) + payload_bytes + signature
    return base64.urlsafe_b64encode(token).decode("ascii")
