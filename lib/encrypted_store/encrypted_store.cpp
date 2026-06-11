#include "encrypted_store.h"
#include <Cryptography/HKDF.h>
#include <Cryptography/HMAC.h>
#include <Cryptography/Random.h>
#include <Utilities/OS.h>
#include <microStore/File.h>

// ── constant-time comparison ──────────────────────────────────────────────────
static bool ct_equal(const uint8_t* a, const uint8_t* b, size_t len) {
    uint8_t diff = 0;
    for (size_t i = 0; i < len; i++) diff |= a[i] ^ b[i];
    return diff == 0;
}

// ── key derivation ────────────────────────────────────────────────────────────
// Derives 64 bytes from the identity's X25519 private key via HKDF-SHA256.
// The identity hash is used as salt, binding the derived keys to this specific
// identity (not just the raw key bytes).
//   out[0:32]  → AES-256-CTR encryption key
//   out[32:64] → HMAC-SHA256 authentication key
static void derive_keys(const RNS::Identity& identity, uint8_t out[64]) {
    RNS::Bytes k = RNS::Cryptography::hkdf(64,
        identity.encryptionPrivateKey(),
        identity.hash());
    memcpy(out, k.data(), 64);
}

// ── HMAC helper ───────────────────────────────────────────────────────────────
// Computes HMAC-SHA256 over: version(1) + IV(16) + ciphertext(N)
static RNS::Bytes compute_mac(const uint8_t* mac_key,
                               uint8_t version,
                               const uint8_t* iv,
                               const uint8_t* ct, size_t ctlen) {
    RNS::Bytes key_bytes(mac_key, 32);
    RNS::Cryptography::HMAC hmac(key_bytes);
    uint8_t ver = version;
    hmac.update(RNS::Bytes(&ver, 1));
    hmac.update(RNS::Bytes(iv, 16));
    hmac.update(RNS::Bytes(ct, ctlen));
    return hmac.digest();
}

// ── public API ────────────────────────────────────────────────────────────────
// File layout: version(1) + IV(16) + ciphertext(N) + HMAC-SHA256(32)

bool encstore_write(const char* path, const RNS::Identity& identity,
                    const uint8_t* data, size_t len) {
    uint8_t keys[64];
    derive_keys(identity, keys);

    // Fresh random IV via Cryptography::random() for consistent entropy source
    RNS::Bytes iv_bytes = RNS::Cryptography::random(16);
    const uint8_t* iv = iv_bytes.data();

    uint8_t* ct = (uint8_t*)malloc(len);
    if (!ct) { memset(keys, 0, 64); return false; }

    CTR<AES256> ctr;
    ctr.setKey(keys, 32);
    ctr.setIV(iv, 16);
    ctr.encrypt(ct, data, len);

    RNS::Bytes mac = compute_mac(keys + 32, ENCSTORE_FILE_VERSION, iv, ct, len);

    // Assemble: version(1) + IV(16) + ciphertext(len) + HMAC(32)
    size_t file_len = ENCSTORE_FILE_OVERHEAD + len;
    RNS::Bytes file_data;
    uint8_t* fbuf = file_data.writable(file_len);
    fbuf[0] = (uint8_t)ENCSTORE_FILE_VERSION;
    memcpy(fbuf + 1,        iv,         16);
    memcpy(fbuf + 17,       ct,         len);
    memcpy(fbuf + 17 + len, mac.data(), 32);
    file_data.resize(file_len);

    free(ct);
    memset(keys, 0, 64);
    return RNS::Utilities::OS::write_file(path, file_data) == file_len;
}

bool encstore_read(const char* path, const RNS::Identity& identity,
                   uint8_t* data, size_t len) {
    RNS::Bytes file_data;
    size_t read = RNS::Utilities::OS::read_file(path, file_data);
    if (read != len + ENCSTORE_FILE_OVERHEAD) return false;

    const uint8_t* fbuf       = file_data.data();
    uint8_t        version    = fbuf[0];
    const uint8_t* iv         = fbuf + 1;
    const uint8_t* ct         = fbuf + 17;
    const uint8_t* mac_stored = fbuf + 17 + len;

    uint8_t keys[64];
    derive_keys(identity, keys);

    // Verify HMAC before decrypting — wrong identity or tampered file detected here
    RNS::Bytes mac_expected = compute_mac(keys + 32, version, iv, ct, len);
    if (!ct_equal(mac_expected.data(), mac_stored, 32)) {
        memset(keys, 0, 64);
        return false;
    }

    CTR<AES256> ctr;
    ctr.setKey(keys, 32);
    ctr.setIV(iv, 16);
    ctr.decrypt(data, ct, len);

    memset(keys, 0, 64);
    return true;
}

size_t encstore_size(const char* path) {
    microStore::File f = RNS::Utilities::OS::open_file(path, microStore::File::ModeRead);
    if (!f) return 0;
    size_t sz = f.size();
    return (sz > ENCSTORE_FILE_OVERHEAD) ? (sz - ENCSTORE_FILE_OVERHEAD) : 0;
}
