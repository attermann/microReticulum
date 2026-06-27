# encrypted_store

Authenticated encrypted file storage for microReticulum messages on Arduino/ESP32 with an SD card.

Encrypts a message blob using an identity's private key and writes it to a file. Reading back requires the same identity. Designed to protect sent/received Reticulum messages at rest — announcements and peer records are public and do not need this treatment.

## Security design

### Key derivation — HKDF-SHA256

Two independent 32-byte keys are derived from the identity's X25519 encryption private key using HKDF-SHA256 (the same key derivation primitive Reticulum uses for packet encryption):

- **keys[0:32]** → AES-256-CTR encryption key  
- **keys[32:64]** → HMAC-SHA256 authentication key  

The identity's **hash** is used as the HKDF salt, which binds the derived keys to this specific identity. A different identity — even one with a similarly shaped key — produces completely different derived keys.

### Authenticated encryption

The HMAC-SHA256 tag covers the version byte, IV, and ciphertext. It is verified with a **constant-time comparison** before any decryption happens. A wrong identity, corrupted file, or tampered ciphertext all fail here. No plaintext is ever produced from an unauthenticated file.

### IV generation

The 16-byte IV is generated via `Cryptography::random()` — the same entropy source Reticulum uses for all its own IV and nonce generation.

### File format

```
┌──────────┬──────────┬──────────────┬─────────────────┐
│ version  │ IV       │ ciphertext   │ HMAC-SHA256     │
│ 1 byte   │ 16 bytes │ N bytes      │ 32 bytes        │
└──────────┴──────────┴──────────────┴─────────────────┘
Total overhead: 49 bytes beyond the plaintext.
```

- **version** — always `0x01`; allows future format changes to be detected
- **IV** — fresh random 16 bytes per write; AES-CTR initialisation vector
- **ciphertext** — AES-256-CTR encrypted plaintext
- **HMAC** — authenticates version + IV + ciphertext

## API

```cpp
#include "encrypted_store.h"

// Encrypt `data` (len bytes) and write to `path` using `identity`'s key.
// Returns true on success.
bool encstore_write(const char* path, const RNS::Identity& identity,
                    const uint8_t* data, size_t len);

// Decrypt the file at `path` into `data` (must be encstore_size(path) bytes).
// Returns true on success.
// Returns false if the file is missing, wrong size, or authentication fails
// (wrong identity or corrupted file). No plaintext is written on failure.
bool encstore_read(const char* path, const RNS::Identity& identity,
                   uint8_t* data, size_t len);

// Returns the plaintext byte count stored at `path` (file_size - 49),
// or 0 if the file does not exist or is too small to be valid.
size_t encstore_size(const char* path);
```

The identity must have its **private key loaded** before calling these functions (use `password_open` from `lib/password` to restore a saved identity).

## Usage example

```cpp
#include "encrypted_store.h"

// After loading identity via lib/password ...

// Store an encrypted message
const char* msg = "hello reticulum";
encstore_write("/msgs/001.enc", identity,
               (const uint8_t*)msg, strlen(msg));

// Read it back
size_t sz = encstore_size("/msgs/001.enc");
uint8_t* buf = (uint8_t*)malloc(sz + 1);
if (encstore_read("/msgs/001.enc", identity, buf, sz)) {
    buf[sz] = '\0';
    Serial.println((char*)buf);
}
free(buf);
```

## Relationship to lib/password

These two libraries cover different layers:

| Layer | Library | Key source | Purpose |
|---|---|---|---|
| Identity file | `lib/password` | User password → PBKDF2 | Protect the private key on SD |
| Message files | `lib/encrypted_store` | Identity private key → HKDF | Protect messages on SD |

The typical flow is: `password_open` → load identity → use `encstore_*` for messages.

## Dependencies

- `microReticulum` — `Identity`, `Cryptography::hkdf`, `Cryptography::HMAC`, `Cryptography::random`
- `ArduinoCryptography` (attermann/Crypto) — AES256, CTR (transitive via microReticulum)
- `SD` — file I/O (Arduino built-in)
