# password

Password-protected file storage for microReticulum identity keys on Arduino/ESP32 with an SD card.

Encrypts a blob of bytes with a password and writes it to a file. Reading back requires the same password. Designed for storing a Reticulum identity's 64-byte private key so the device can be powered off safely.

## Security design

### Key derivation — PBKDF2-HMAC-SHA256

Two independent 32-byte keys are derived from the password and a random 16-byte salt using PBKDF2 with 100,000 iterations:

- **Block 1** → AES-256-CTR encryption key  
- **Block 2** → HMAC-SHA256 authentication key  

Running two full iteration chains means an attacker must compute 200,000 PBKDF2 iterations per password guess. At ~500 ms per 10,000 iterations on an ESP32, guessing a single password takes roughly **10 seconds on the device's own hardware**. A GPU can go faster, but the cost scales linearly with iterations.

The iteration count is `PBKDF2_ITERATIONS` in `password.h` and can be tuned for your hardware. The initial unlock is a one-time operation, so a high value is preferred.

### Authenticated encryption

The HMAC-SHA256 tag covers the entire file (version + salt + IV + ciphertext). It is verified with a **constant-time comparison** before any decryption happens. A wrong password or a tampered file always produces an authentication failure — there is no plaintext oracle and no timing leak on the comparison.

### File format

```
┌──────────┬──────────┬──────────┬──────────────┬─────────────────┐
│ version  │ salt     │ IV       │ ciphertext   │ HMAC-SHA256     │
│ 1 byte   │ 16 bytes │ 16 bytes │ N bytes      │ 32 bytes        │
└──────────┴──────────┴──────────┴──────────────┴─────────────────┘
Total overhead: 65 bytes beyond the plaintext.
```

- **version** — always `0x01`; allows future format changes to be detected
- **salt** — random per file; ensures two files protected with the same password have different keys
- **IV** — random per file; AES-CTR initialisation vector
- **ciphertext** — AES-256-CTR encrypted plaintext
- **HMAC** — authenticates everything above it

## Brute-force resistance

### What an attacker needs

To crack the password an attacker must obtain the identity file (the SD card, or a copy of it) and then guess passwords offline. Each guess requires computing 200,000 HMAC-SHA256 operations — two full PBKDF2 chains. The HMAC at the end of the file tells them immediately whether the guess was right, so they can automate this at scale.

### How fast can a modern machine guess?

SHA-256 is GPU-friendly, so a dedicated cracking rig can outpace the device significantly:

| Hardware | Guesses / second |
|---|---|
| ESP32 (the device itself) | ~0.1 |
| Modern laptop CPU | ~50–200 |
| RTX 4090 (high-end GPU) | ~2,000–5,000 |
| 100× RTX 4090 cloud cluster | ~200,000–500,000 |

### How long does cracking take?

These are **average** times (half the keyspace), assuming the attacker has the identity file:

| Password type | Example | Combinations | Single RTX 4090 | 100-GPU cluster |
|---|---|---|---|---|
| 6 random lowercase | `kxqmbt` | 3 × 10⁸ | < 1 minute | < 1 second |
| 8 random alphanumeric | `aB3mK9xQ` | 2 × 10¹⁴ | ~14 years | ~7 weeks |
| 4-word diceware | `coral-lamp-fence-drum` | 3.6 × 10¹⁵ | ~228 years | ~2 years |
| 12 random alphanumeric | `aB3mK9xQpL2w` | 3 × 10²¹ | ~100 billion years | ~1 billion years |
| 5-word diceware | `coral-lamp-fence-drum-river` | 2.8 × 10¹⁹ | ~1.7 billion years | ~17 million years |

### Known limitation: PBKDF2 is GPU-friendly

PBKDF2-SHA256 is the best option available on constrained hardware, but it is not memory-hard. A purpose-built GPU cluster can attack it far faster than the device can defend. A modern memory-hard function like **Argon2id** would be 100–1,000× more expensive for a GPU attacker at the same device-side cost — but Argon2id requires ~64 MB of working memory per operation, which rules it out on an ESP32.

### Recommendations

**Do:**
- Use at least a 4-word [diceware](https://diceware.dmuth.org/) passphrase or 10+ random characters.
- Generate your password with a proper random source (dice, a password manager, etc.), not a memorable phrase.
- Treat the SD card with the same care as the device itself — physical possession of the card enables offline attacks.

**Avoid:**
- Dictionary words, names, dates, or any phrase you can remember easily.
- Passwords shorter than 10 characters.
- Reusing a password from another service.

**Increasing `PBKDF2_ITERATIONS`:** If your hardware is faster or your application tolerates a longer unlock delay, raising this value directly multiplies the attacker's cost. Doubling it halves the number of guesses per second. The identity file only needs to be unlocked once per boot, so a 10–30 second delay is often acceptable.

## API

```cpp
#include "password.h"

// Encrypt `data` (len bytes) and write to `path`.
// Returns true on success.
bool password_protect(const char* path, const char* password,
                      const uint8_t* data, size_t len);

// Decrypt a file written by password_protect into `data` (must be len bytes).
// Returns true on success.
// Returns false if the file is missing, wrong size, or authentication fails
// (wrong password or corrupted file). No plaintext is written on failure.
bool password_open(const char* path, const char* password,
                   uint8_t* data, size_t len);
```

The `password` parameter is a plain `const char*`. **Zero the password buffer** with `memset` as soon as you are done with it.

## Usage example

```cpp
#include <SD.h>
#include "password.h"

// Store a 64-byte identity private key
uint8_t prv[64];        // fill with key material
char pw[64] = {0};
// ... read pw from serial ...

password_protect("/identity.bin", pw, prv, sizeof(prv));
memset(pw, 0, sizeof(pw));

// Later: load it back
uint8_t prv_loaded[64];
char pw2[64] = {0};
// ... read pw2 from serial ...

if (password_open("/identity.bin", pw2, prv_loaded, sizeof(prv_loaded))) {
    // use prv_loaded ...
}
memset(pw2, 0, sizeof(pw2));
memset(prv_loaded, 0, sizeof(prv_loaded));
```

## Dependencies

- `ArduinoCryptography` (attermann/Crypto) — SHA256, AES256, CTR, RNG
- `SD` — file I/O (Arduino built-in)
