"""One-time script: generates the Ed25519 keypair used to sign Star Term
license keys.

Writes `private_key.pem` next to this script (PKCS8, unencrypted) and
prints the public key hex to embed as PUBLIC_KEY_HEX in
star_term/licensing.py.

Run this ONCE. Back up private_key.pem somewhere safe and never commit or
ship it - it's the master key for issuing all future license keys."""

from pathlib import Path

from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey


def main():
    out_path = Path(__file__).parent / "private_key.pem"
    if out_path.exists():
        raise SystemExit(f"{out_path} already exists - refusing to overwrite.")

    private_key = Ed25519PrivateKey.generate()

    pem = private_key.private_bytes(
        encoding=serialization.Encoding.PEM,
        format=serialization.PrivateFormat.PKCS8,
        encryption_algorithm=serialization.NoEncryption(),
    )
    out_path.write_bytes(pem)

    public_bytes = private_key.public_key().public_bytes(
        encoding=serialization.Encoding.Raw,
        format=serialization.PublicFormat.Raw,
    )

    print(f"Wrote {out_path}")
    print()
    print("Paste this into PUBLIC_KEY_HEX in star_term/licensing.py:")
    print(public_bytes.hex())


if __name__ == "__main__":
    main()
