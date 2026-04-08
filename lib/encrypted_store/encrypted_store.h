#pragma once
#include <Arduino.h>
#include <CTR.h>
#include <AES.h>
#include "Identity.h"

// Authenticated encrypted message storage using the Reticulum identity's
// X25519 encryption private key.
//
// Announcements and peers are public — this module is for sent/received
// message blobs that must be protected at rest.
//
// Key derivation:
//   Two 32-byte keys are derived from the identity's private key via HKDF
//   (the same primitive Reticulum uses for packet encryption):
//     keys[0:32]  → AES-256-CTR encryption key
//     keys[32:64] → HMAC-SHA256 authentication key
//   The identity hash is used as the HKDF salt so keys are bound to the
//   specific identity, not just the raw key bytes.
//
// File layout: version(1) + IV(16) + ciphertext(N) + HMAC-SHA256(32)
//   Total overhead per file: 49 bytes.
//
// The HMAC covers version + IV + ciphertext, providing authenticated
// encryption: tampering, corruption, or a wrong identity are all detected
// before any plaintext is produced.
//
// Load the identity first (e.g. via password_open from lib/password) so
// the private key is available before calling these functions.

#define ENCSTORE_FILE_VERSION  0x01
#define ENCSTORE_FILE_OVERHEAD 49   // 1 (version) + 16 (IV) + 32 (HMAC)

// Write `len` bytes from `data` to `path` encrypted and authenticated with
// `identity`'s key. Returns true on success.
bool encstore_write(const char* path, const RNS::Identity& identity,
                    const uint8_t* data, size_t len);

// Read and authenticate the message stored at `path`, then decrypt into
// `data` (must be at least encstore_size(path) bytes).
// Returns true on success; false means file missing, wrong identity, or
// corruption — no plaintext is written in that case.
bool encstore_read(const char* path, const RNS::Identity& identity,
                   uint8_t* data, size_t len);

// Returns the plaintext byte count for the file at `path` (file_size - 49),
// or 0 if the file is missing or too small to be valid.
size_t encstore_size(const char* path);
