# Star Term License Manager

A small Windows desktop app for issuing Star Term perpetual license keys.
Enter a customer's name and email, click Generate, copy the resulting key
and send it to them.

License keys produced here are verified offline by
`windows/star_term/star_term/licensing.py` against an embedded Ed25519
public key - this app signs with the matching private key.

## Run from source

```
python -m venv .venv
.venv\Scripts\activate
pip install -r requirements.txt
python -m license_manager.main
```

## Building the installer

Requires [NSIS](https://nsis.sourceforge.io/) (`makensis`) on the build machine.

```
cd installer
python make_icon.py
makensis setup.nsi
```

The output installer is written to `installer/Output/license_manager_setup.exe`.

## Usage

1. **Private key** - click Browse... and select your `private_key.pem` (see
   `windows/licensing_tool/`). This is the master signing key - keep it
   somewhere secure and local; it is never bundled with this app or its
   installer. The chosen path is remembered for next time.
2. Enter the customer's **licensee name** and **email**.
3. Click **Generate License Key**. The signed key appears in the read-only
   field below.
4. Click **Copy to Clipboard** and send the key to the customer. They enter
   it in Star Term via Preferences > License (or the startup "License
   Required" prompt once their trial ends).
