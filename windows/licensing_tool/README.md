# Star Term Licensing Tool

Dev-only tools for issuing Star Term perpetual license keys. This directory
is **not** packaged by the installer (`setup.nsi` only ships `star_term/*.py`)
and should never be distributed to customers.

## Setup (one-time)

```
python generate_keypair.py
```

Generates `private_key.pem` in this directory and prints the corresponding
public key hex. The public key hex is already embedded as `PUBLIC_KEY_HEX`
in `star_term/licensing.py` - re-run this and update that constant only if
you intend to invalidate all previously issued license keys.

**Back up `private_key.pem` somewhere safe (e.g. a password manager or
encrypted storage).** It is the master key for issuing all future license
keys - if it's lost, you can't issue new keys without re-keying (and
invalidating) the product. Never commit it to git or ship it with the app.

## Issuing a license key

```
python generate_license.py --licensee "Jane Doe" --email jane@example.com
```

Prints a single license key string. Send this to the customer - they enter
it in Star Term via Preferences > License (or the startup "License Required"
dialog if their trial has expired).

Use `--private-key /path/to/private_key.pem` if it's not in this directory.

## GUI alternative

For day-to-day license issuing without a terminal, see `license_manager/`
(sibling project) - a small installable GUI app with the same name/email
fields and a Generate/Copy workflow, pointed at this directory's
`private_key.pem`.
