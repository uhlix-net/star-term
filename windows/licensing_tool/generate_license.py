"""Issues a Star Term perpetual license key, signed with private_key.pem
(see generate_keypair.py).

Usage:
    python generate_license.py --licensee "Jane Doe" --email jane@example.com
"""

import argparse
import base64
import datetime
import json
import struct
from pathlib import Path

from cryptography.hazmat.primitives.serialization import load_pem_private_key

PRODUCT = "star_term"


def build_license_key(licensee: str, email: str, private_key_path: Path) -> str:
    private_key = load_pem_private_key(private_key_path.read_bytes(), password=None)

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


def main():
    parser = argparse.ArgumentParser(description="Generate a Star Term license key")
    parser.add_argument("--licensee", required=True, help="Customer name")
    parser.add_argument("--email", required=True, help="Customer email")
    parser.add_argument(
        "--private-key",
        default=Path(__file__).parent / "private_key.pem",
        type=Path,
        help="Path to private_key.pem (default: ./private_key.pem)",
    )
    args = parser.parse_args()

    key = build_license_key(args.licensee, args.email, args.private_key)
    print(key)


if __name__ == "__main__":
    main()
