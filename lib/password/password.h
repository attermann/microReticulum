#pragma once
#include <Arduino.h>
#include <SHA256.h>
#include <CTR.h>
#include <AES.h>
#include <RNG.h>

// PBKDF2 iteration count.
// Higher = slower brute-force at the cost of slower unlock.
// Initial identity load is a one-time operation, so a high value is fine.
// ~500ms per 10,000 iterations on ESP32 → 100,000 ≈ 5 seconds.
#define PBKDF2_ITERATIONS 100000

// File format version stored as the first byte of every protected file.
// Increment if the format ever changes so old files can be detected.
#define PASSWORD_FILE_VERSION 0x01

// File layout: version(1) + salt(16) + IV(16) + ciphertext(N) + HMAC-SHA256(32)
// Total per-file overhead beyond the plaintext: 65 bytes.
#define PASSWORD_FILE_OVERHEAD 65

// Encrypt `data` of `len` bytes and write to `path`.
// Two 32-byte keys are derived from `password` via PBKDF2 (PBKDF2_ITERATIONS):
//   key[0:32]  → AES-256-CTR encryption
//   key[32:64] → HMAC-SHA256 over (version + salt + IV + ciphertext)
// The HMAC is appended to the file and verified on open, catching both
// wrong passwords and file corruption without any plaintext oracle.
// Returns true on success.
bool password_protect(const char* path, const char* password, const uint8_t* data, size_t len);

// Decrypt a file written by password_protect.
// Returns true on success.
// Returns false if the file is missing, too small, or the HMAC does not match
// (wrong password or corrupted file) — no plaintext is written in that case.
bool password_open(const char* path, const char* password, uint8_t* data, size_t len);
