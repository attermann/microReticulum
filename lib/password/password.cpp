#include "password.h"

// ── constant-time comparison ──────────────────────────────────────────────────
// Always iterates all `len` bytes so timing doesn't reveal where the first
// mismatch is, preventing side-channel leaks on HMAC comparison.
static bool ct_equal(const uint8_t* a, const uint8_t* b, size_t len) {
    uint8_t diff = 0;
    for (size_t i = 0; i < len; i++) diff |= a[i] ^ b[i];
    return diff == 0;
}

// ── PBKDF2-HMAC-SHA256 ────────────────────────────────────────────────────────
// Computes one 32-byte output block of PBKDF2 (RFC 2898 §5.2).
// `block_num` is 1-indexed; block 1 = AES key, block 2 = HMAC key.
static void pbkdf2_block(const uint8_t* pw, size_t pwlen,
                         const uint8_t* salt, uint32_t block_num,
                         uint8_t* out32) {
    SHA256 sha;
    // U1 = HMAC(password, salt || block_num_be)
    sha.resetHMAC(pw, pwlen);
    sha.update(salt, 16);
    uint8_t blk[4] = {
        (uint8_t)(block_num >> 24), (uint8_t)(block_num >> 16),
        (uint8_t)(block_num >> 8),  (uint8_t)(block_num)
    };
    sha.update(blk, 4);
    uint8_t u[32];
    sha.finalizeHMAC(pw, pwlen, u, 32);
    memcpy(out32, u, 32);
    // Ui = HMAC(password, U_{i-1}), XOR into accumulator
    for (uint32_t i = 1; i < PBKDF2_ITERATIONS; i++) {
        sha.resetHMAC(pw, pwlen);
        sha.update(u, 32);
        sha.finalizeHMAC(pw, pwlen, u, 32);
        for (int j = 0; j < 32; j++) out32[j] ^= u[j];
    }
}

// Derives 64 bytes from password + salt via two independent PBKDF2 blocks:
//   out64[0:32]  → AES-256-CTR encryption key
//   out64[32:64] → HMAC-SHA256 authentication key
// Running two full chains means an attacker must compute 2×PBKDF2_ITERATIONS
// iterations per password guess.
static void pbkdf2_64(const char* password, const uint8_t* salt, uint8_t* out64) {
    const uint8_t* pw = (const uint8_t*)password;
    size_t pwlen = strlen(password);
    pbkdf2_block(pw, pwlen, salt, 1, out64);
    pbkdf2_block(pw, pwlen, salt, 2, out64 + 32);
}

// ── HMAC-SHA256 helper ────────────────────────────────────────────────────────
static void hmac_sha256(const uint8_t* key, const uint8_t* msg, size_t msglen,
                        uint8_t* out32) {
    SHA256 sha;
    sha.resetHMAC(key, 32);
    sha.update(msg, msglen);
    sha.finalizeHMAC(key, 32, out32, 32);
}

// ── public API ────────────────────────────────────────────────────────────────
// File layout: version(1) + salt(16) + IV(16) + ciphertext(N) + HMAC-SHA256(32)

bool password_protect(const char* path, const char* password,
                      const uint8_t* data, size_t len) {
    uint8_t salt[16], iv[16];
    RNG.rand(salt, 16);
    RNG.rand(iv,   16);

    uint8_t keys[64];
    pbkdf2_64(password, salt, keys);

    uint8_t* ct = (uint8_t*)malloc(len);
    if (!ct) { memset(keys, 0, 64); return false; }

    CTR<AES256> ctr;
    ctr.setKey(keys, 32);
    ctr.setIV(iv, 16);
    ctr.encrypt(ct, data, len);

    // HMAC over version + salt + IV + ciphertext
    size_t auth_len = 1 + 16 + 16 + len;
    uint8_t* auth = (uint8_t*)malloc(auth_len);
    if (!auth) { free(ct); memset(keys, 0, 64); return false; }
    auth[0] = PASSWORD_FILE_VERSION;
    memcpy(auth + 1,  salt, 16);
    memcpy(auth + 17, iv,   16);
    memcpy(auth + 33, ct,   len);

    uint8_t mac[32];
    hmac_sha256(keys + 32, auth, auth_len, mac);
    free(auth);

    File f = SD.open(path, FILE_WRITE);
    if (!f) { free(ct); memset(keys, 0, 64); return false; }
    f.write((uint8_t)PASSWORD_FILE_VERSION);
    f.write(salt, 16);
    f.write(iv,   16);
    f.write(ct,   len);
    f.write(mac,  32);
    f.close();

    free(ct);
    memset(keys, 0, 64);
    return true;
}

bool password_open(const char* path, const char* password,
                   uint8_t* data, size_t len) {
    File f = SD.open(path, FILE_READ);
    if (!f) return false;
    if ((size_t)f.size() != len + PASSWORD_FILE_OVERHEAD) { f.close(); return false; }

    uint8_t version, salt[16], iv[16], mac_stored[32];
    if (f.read(&version, 1)    != 1)        { f.close(); return false; }
    if (f.read(salt,    16)    != 16)       { f.close(); return false; }
    if (f.read(iv,      16)    != 16)       { f.close(); return false; }

    uint8_t* ct = (uint8_t*)malloc(len);
    if (!ct) { f.close(); return false; }
    if (f.read(ct,       len)  != (int)len) { f.close(); free(ct); return false; }
    if (f.read(mac_stored, 32) != 32)       { f.close(); free(ct); return false; }
    f.close();

    uint8_t keys[64];
    pbkdf2_64(password, salt, keys);

    // Verify HMAC before decrypting — avoids decryption oracle attacks
    size_t auth_len = 1 + 16 + 16 + len;
    uint8_t* auth = (uint8_t*)malloc(auth_len);
    if (!auth) { free(ct); memset(keys, 0, 64); return false; }
    auth[0] = version;
    memcpy(auth + 1,  salt, 16);
    memcpy(auth + 17, iv,   16);
    memcpy(auth + 33, ct,   len);

    uint8_t mac_expected[32];
    hmac_sha256(keys + 32, auth, auth_len, mac_expected);
    free(auth);

    if (!ct_equal(mac_expected, mac_stored, 32)) {
        free(ct);
        memset(keys, 0, 64);
        return false;   // wrong password or tampered file
    }

    CTR<AES256> ctr;
    ctr.setKey(keys, 32);
    ctr.setIV(iv, 16);
    ctr.decrypt(data, ct, len);

    free(ct);
    memset(keys, 0, 64);
    return true;
}
