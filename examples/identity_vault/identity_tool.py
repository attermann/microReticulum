#!/usr/bin/env python3
"""
identity_tool.py — Inspect microReticulum identity and message files on a PC.

Decrypts files written by lib/password (identity) and lib/encrypted_store
(messages) using the same algorithms as the device, so you can verify files
off-device without needing the hardware.

Usage:
    python identity_tool.py identity.bin
    python identity_tool.py identity.bin /msgs/last.enc [more_files ...]

Install dependencies:
    pip install cryptography

Inspect identity only
    ./identity_tool.py test/identity.bin

Inspect identity + decrypt message files
    ./identity_tool.py test/identity.bin test/msgs/last.enc

Also print raw key bytes (careful)
    ./identity_tool.py test/identity.bin --show-keys
"""

import sys
import os
import hashlib
import hmac as _hmac
import getpass
import argparse

try:
    from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
    from cryptography.hazmat.primitives.kdf.hkdf import HKDF
    from cryptography.hazmat.primitives import hashes
    from cryptography.hazmat.primitives.asymmetric.x25519 import X25519PrivateKey
    from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
    from cryptography.hazmat.primitives.serialization import Encoding, PublicFormat
except ImportError:
    print("Missing dependency. Run:  pip install cryptography", file=sys.stderr)
    sys.exit(1)


# ── constants (must match password.h and encrypted_store.h) ──────────────────

PBKDF2_ITERATIONS      = 100000
PASSWORD_FILE_OVERHEAD = 65   # version(1) + salt(16) + IV(16) + HMAC(32)
ENCSTORE_FILE_OVERHEAD = 49   # version(1) + IV(16) + HMAC(32)


# ── primitives ────────────────────────────────────────────────────────────────

def _pbkdf2_64(password: str, salt: bytes) -> bytes:
    """
    Derive 64 bytes via PBKDF2-HMAC-SHA256 with PBKDF2_ITERATIONS iterations.
    Python's pbkdf2_hmac produces two back-to-back 32-byte blocks (counter 1
    and counter 2), matching the C++ pbkdf2_block() calls in password.cpp.
      returned[0:32]  → AES-256-CTR key
      returned[32:64] → HMAC-SHA256 key
    """
    return hashlib.pbkdf2_hmac(
        'sha256',
        password.encode('utf-8'),
        salt,
        PBKDF2_ITERATIONS,
        dklen=64,
    )


def _aes_ctr(key: bytes, iv: bytes, data: bytes) -> bytes:
    """AES-256-CTR — same operation for encrypt and decrypt."""
    cipher = Cipher(algorithms.AES(key), modes.CTR(iv))
    ctx = cipher.decryptor()
    return ctx.update(data) + ctx.finalize()


def _hmac_sha256(key: bytes, *parts: bytes) -> bytes:
    """HMAC-SHA256 over the concatenation of all parts."""
    h = _hmac.new(key, digestmod='sha256')
    for p in parts:
        h.update(p)
    return h.digest()


def _ct_equal(a: bytes, b: bytes) -> bool:
    """Constant-time comparison — avoids timing leaks on HMAC verification."""
    return _hmac.compare_digest(a, b)


# ── identity file (lib/password) ──────────────────────────────────────────────

def open_identity(path: str, password: str) -> bytes:
    """
    Decrypt a lib/password identity file.

    File layout: version(1) + salt(16) + IV(16) + ciphertext(N) + HMAC-SHA256(32)

    Returns the raw plaintext bytes (64-byte Reticulum private key) on success.
    Raises ValueError if the file is missing content, uses an unknown version,
    or fails HMAC verification (wrong password or corrupted/tampered file).
    """
    raw = open(path, 'rb').read()

    if len(raw) <= PASSWORD_FILE_OVERHEAD:
        raise ValueError(f"File is too small ({len(raw)} bytes) to be a valid identity file")

    plaintext_len = len(raw) - PASSWORD_FILE_OVERHEAD

    version    = raw[0:1]
    salt       = raw[1:17]
    iv         = raw[17:33]
    ct         = raw[33:33 + plaintext_len]
    mac_stored = raw[33 + plaintext_len:]

    if version[0] != 0x01:
        raise ValueError(f"Unsupported file version {version[0]:#04x}")

    keys     = _pbkdf2_64(password, salt)
    aes_key  = keys[0:32]
    hmac_key = keys[32:64]

    # HMAC covers the entire file except the tag itself
    mac_expected = _hmac_sha256(hmac_key, version, salt, iv, ct)
    if not _ct_equal(mac_expected, mac_stored):
        raise ValueError("Authentication failed — wrong password or corrupted file")

    return _aes_ctr(aes_key, iv, ct)


# ── identity hash (matches Identity::truncated_hash in Identity.h) ────────────

def compute_identity_hash(prv_bytes: bytes) -> bytes:
    """
    Derive the 16-byte Reticulum identity hash from a 64-byte private key.

    Replicates:
        truncated_hash(get_public_key())
        = SHA256(x25519_pub || ed25519_pub)[:16]
    """
    x_pub  = X25519PrivateKey.from_private_bytes(prv_bytes[0:32]) \
                .public_key().public_bytes(Encoding.Raw, PublicFormat.Raw)
    ed_pub = Ed25519PrivateKey.from_private_bytes(prv_bytes[32:64]) \
                .public_key().public_bytes(Encoding.Raw, PublicFormat.Raw)
    return hashlib.sha256(x_pub + ed_pub).digest()[:16]


# ── message file (lib/encrypted_store) ───────────────────────────────────────

def open_message(path: str, prv_bytes: bytes) -> bytes:
    """
    Decrypt a lib/encrypted_store message file.

    File layout: version(1) + IV(16) + ciphertext(N) + HMAC-SHA256(32)

    Key derivation mirrors encrypted_store.cpp:
        HKDF-SHA256(ikm=prv_bytes, salt=identity_hash, info=b'', length=64)
        keys[0:32]  → AES-256-CTR key
        keys[32:64] → HMAC-SHA256 key

    Returns plaintext bytes on success.
    Raises ValueError on wrong identity, corruption, or tampered file.
    """
    raw = open(path, 'rb').read()

    if len(raw) <= ENCSTORE_FILE_OVERHEAD:
        raise ValueError(f"File is too small ({len(raw)} bytes) to be a valid message file")

    plaintext_len = len(raw) - ENCSTORE_FILE_OVERHEAD

    version    = raw[0:1]
    iv         = raw[1:17]
    ct         = raw[17:17 + plaintext_len]
    mac_stored = raw[17 + plaintext_len:]

    if version[0] != 0x01:
        raise ValueError(f"Unsupported file version {version[0]:#04x}")

    # HKDF with identity hash as salt, empty info — matches C++ hkdf() call.
    # IKM is only the X25519 encryption key (first 32 bytes), matching C++:
    #   identity.encryptionPrivateKey() → _prv_bytes (X25519 only, not Ed25519)
    id_hash = compute_identity_hash(prv_bytes)
    hkdf    = HKDF(algorithm=hashes.SHA256(), length=64, salt=id_hash, info=b'')
    keys    = hkdf.derive(prv_bytes[0:32])
    aes_key  = keys[0:32]
    hmac_key = keys[32:64]

    mac_expected = _hmac_sha256(hmac_key, version, iv, ct)
    if not _ct_equal(mac_expected, mac_stored):
        raise ValueError("Authentication failed — wrong identity or corrupted/tampered file")

    return _aes_ctr(aes_key, iv, ct)


# ── CLI ───────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description="Decrypt microReticulum lib/password identity files and "
                    "lib/encrypted_store message files.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python identity_tool.py identity.bin
  python identity_tool.py identity.bin /Volumes/SD/msgs/last.enc
        """,
    )
    parser.add_argument('identity',
        help="Path to the identity file created by lib/password")
    parser.add_argument('messages', nargs='*',
        help="Optional lib/encrypted_store message files to decrypt")
    parser.add_argument('--show-keys', action='store_true',
        help="Print raw private key bytes (handle with care)")
    args = parser.parse_args()

    # ── load identity ─────────────────────────────────────────────────────────
    password = getpass.getpass("Password: ")
    try:
        prv_bytes = open_identity(args.identity, password)
    except (ValueError, OSError) as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)
    finally:
        del password   # drop from memory as soon as possible

    id_hash = compute_identity_hash(prv_bytes)
    print(f"\nIdentity file: {args.identity}")
    print(f"  Status:         OK")
    print(f"  Identity hash:  {id_hash.hex()}")
    print(f"  Private key:    {len(prv_bytes)} bytes")
    if args.show_keys:
        print(f"  X25519 prv:   {prv_bytes[0:32].hex()}")
        print(f"  Ed25519 prv:  {prv_bytes[32:64].hex()}")

    # ── decrypt message files ─────────────────────────────────────────────────
    for msg_path in args.messages:
        print(f"\nMessage file: {msg_path}")
        try:
            plaintext = open_message(msg_path, prv_bytes)
            print(f"  Status:  OK")
            print(f"  Size:    {len(plaintext)} bytes")
            try:
                text = plaintext.decode('utf-8')
                print(f"  Content: {text!r}")
            except UnicodeDecodeError:
                print(f"  Content: (binary) {plaintext.hex()}")
        except (ValueError, OSError) as e:
            print(f"  Status:  FAIL — {e}")

    # ── wipe key material ─────────────────────────────────────────────────────
    # Python doesn't guarantee memory wiping, but this at least removes the
    # reference so the GC can reclaim the memory sooner.
    del prv_bytes


if __name__ == '__main__':
    main()
