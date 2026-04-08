// encrypted_announce example
//
// Offline test for password-protected identity and encrypted message storage.
// No network interface required.
//
// On first run (no /identity.bin on SD):
//   - A new Reticulum identity is generated.
//   - You are prompted for a password; the identity is saved to /identity.bin
//     encrypted with that password via PBKDF2 + AES-256-CTR.
//
// On subsequent runs:
//   - Enter your password; the same identity is restored from SD.
//
// After loading, three offline tests run automatically:
//   1. Announce  — create an announce packet and validate it
//   2. Packet    — encrypt a packet to the destination, decrypt it, verify payload
//   3. Storage   — write a message encrypted to SD, read it back, verify it
//
// Button / 'r' over serial: re-run all tests.

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>

#include "password.h"
#include "encrypted_store.h"

#include <microStore/FileSystem.h>
#include <microStore/Adapters/UniversalFileSystem.h>

#include <Reticulum.h>
#include <Identity.h>
#include <Destination.h>
#include <Packet.h>
#include <Transport.h>
#include <Log.h>
#include <Bytes.h>
#include <Type.h>
#include <Utilities/OS.h>

#ifndef BUTTON_PIN
#define BUTTON_PIN  38
#endif

#ifndef SD_CS_PIN
#define SD_CS_PIN    5
#endif

#ifndef SD_SPEED
#define SD_SPEED 4000000
#endif

const char* APP_NAME      = "example_utilities";
const char* IDENTITY_PATH = "/identity.bin";
const char* MSG_PATH      = "/msgs/last.enc";

// identity blob: prv_bytes(64)
#define IDENTITY_BLOB_LEN 64
#define PASSWORD_BUF_LEN  64

const char* TEST_PAYLOAD = "The quick brown fox jumps over the lazy dog";

volatile bool run_tests_flag = false;

// Shared state for the packet test callback
static RNS::Bytes  _test_packet_data;
static bool        _test_packet_received = false;
static void testPacketCallback(const RNS::Bytes& data, const RNS::Packet&) {
    _test_packet_data     = data;
    _test_packet_received = true;
}

microStore::FileSystem filesystem{microStore::Adapters::UniversalFileSystem()};

RNS::Reticulum   reticulum({RNS::Type::NONE});
RNS::Identity    identity({RNS::Type::NONE});
RNS::Destination destination({RNS::Type::NONE});

// ── helpers ───────────────────────────────────────────────────────────────────

static void pass(const char* label) {
    Serial.print("  PASS  ");
    Serial.println(label);
}

static void fail(const char* label, const char* reason = nullptr) {
    Serial.print("  FAIL  ");
    Serial.print(label);
    if (reason) { Serial.print(" — "); Serial.print(reason); }
    Serial.println();
}

// Read a line from serial into `out` (max `maxlen-1` chars), echoing '*'.
// Returns true when at least one character was entered.
static bool readPasswordSerial(const char* prompt, char* out, size_t maxlen) {
    Serial.print(prompt);
    size_t len = 0;
    for (;;) {
        if (Serial.available()) {
            char c = (char)Serial.read();
            if (c == '\n' || c == '\r') {
                if (len > 0) break;
            } else if (len < maxlen - 1) {
                out[len++] = c;
                Serial.print('*');
            }
        }
    }
    out[len] = '\0';
    Serial.println();
    return len > 0;
}

// ── tests ─────────────────────────────────────────────────────────────────────

// 1. Announce: sign an announce packet with the identity's Ed25519 key and
//    verify the signature + destination hash binding are correct.
//    This proves the identity keypair is intact and signing works.
static void test_announce() {
    Serial.println("[ 1/3 announce signature ]");
    Serial.print("    identity: "); Serial.println(identity.hexhash().c_str());
    Serial.print("    destination: "); Serial.println(destination.hash().toHex().c_str());
    try {
        RNS::Packet announce_packet = destination.announce(
            RNS::bytesFromString("test"), false, {RNS::Type::NONE}, {}, false);
        announce_packet.pack();
        Serial.print("    announce packet size: "); Serial.print(announce_packet.raw().size()); Serial.println(" bytes");
        if (RNS::Identity::validate_announce(announce_packet)) {
            pass("Ed25519 signature + destination hash binding valid");
        } else {
            fail("Ed25519 signature + destination hash binding valid", "signature check failed");
        }
    }
    catch (const std::exception& e) {
        fail("announce", e.what());
    }
}

// 2. Packet encrypt/decrypt: encrypt a packet with the destination's X25519
//    public key, reconstruct it from raw bytes (simulating receipt over the
//    air), decrypt with the private key, and verify the plaintext is intact.
static void test_packet() {
    Serial.println("[ 2/3 packet X25519 encrypt/decrypt ]");
    Serial.print("    plaintext ("); Serial.print(strlen(TEST_PAYLOAD)); Serial.println(" bytes):");
    Serial.print("      \""); Serial.print(TEST_PAYLOAD); Serial.println("\"");
    _test_packet_received = false;
    _test_packet_data     = RNS::Bytes();
    destination.set_packet_callback(testPacketCallback);
    try {
        RNS::Packet send_packet(destination, RNS::bytesFromString(TEST_PAYLOAD));
        send_packet.pack();
        Serial.print("    encrypted packet size: "); Serial.print(send_packet.raw().size()); Serial.println(" bytes");

        RNS::Packet recv_packet(send_packet.raw());
        recv_packet.unpack();
        destination.receive(recv_packet);   // decrypts and fires callback

        if (_test_packet_received &&
            _test_packet_data.size() == strlen(TEST_PAYLOAD) &&
            memcmp(_test_packet_data.data(), TEST_PAYLOAD, _test_packet_data.size()) == 0) {
            Serial.println("    decrypted payload matches original");
            pass("X25519 encrypt → decrypt round-trip");
        } else {
            fail("X25519 encrypt → decrypt round-trip",
                 _test_packet_received ? "payload mismatch" : "callback not called");
        }
    }
    catch (const std::exception& e) {
        fail("packet", e.what());
    }
    destination.set_packet_callback(nullptr);
}

// 3. SD encrypted storage: encrypt a message with the identity's X25519
//    private key via AES-256-CTR, write it to SD, read it back, and verify
//    the plaintext matches. The file on SD is unreadable without the password.
static void test_storage() {
    Serial.println("[ 3/3 SD encrypted storage ]");
    Serial.print("    writing to: "); Serial.println(MSG_PATH);

    if (!RNS::Utilities::OS::directory_exists("/msgs")) RNS::Utilities::OS::create_directory("/msgs");

    const uint8_t* plain = (const uint8_t*)TEST_PAYLOAD;
    size_t len = strlen(TEST_PAYLOAD);

    if (!encstore_write(MSG_PATH, identity, plain, len)) {
        fail("write encrypted file", "encstore_write returned false");
        return;
    }
    Serial.print("    file size on SD: "); Serial.print(encstore_size(MSG_PATH) + 16);
    Serial.println(" bytes (16-byte IV + ciphertext)");

    uint8_t* buf = (uint8_t*)malloc(len);
    if (!buf) { fail("read encrypted file", "malloc failed"); return; }

    bool ok = encstore_read(MSG_PATH, identity, buf, len);
    if (ok && memcmp(buf, plain, len) == 0) {
        Serial.println("    decrypted payload matches original");
        pass("AES-256-CTR write → read round-trip");
    } else {
        fail("AES-256-CTR write → read round-trip",
             ok ? "payload mismatch" : "encstore_read returned false");
    }
    free(buf);
}

static void run_all_tests() {
    Serial.println("\n=== security tests ===");
    Serial.println("  payload: \"" + String(TEST_PAYLOAD) + "\"");
    test_announce();
    test_packet();
    test_storage();
    Serial.println("=== done ===\n");
}

// ── RNS setup ─────────────────────────────────────────────────────────────────

static void reticulum_setup() {
    try {
        reticulum = RNS::Reticulum();
        reticulum.transport_enabled(false);
        reticulum.start();

        destination = RNS::Destination(identity, RNS::Type::Destination::IN,
                                       RNS::Type::Destination::SINGLE,
                                       APP_NAME, "announcesample");
        destination.set_proof_strategy(RNS::Type::Destination::PROVE_ALL);

        INFOF("Identity hash: %s", identity.hexhash().c_str());
    }
    catch (const std::exception& e) {
        ERRORF("reticulum_setup: %s", e.what());
    }
}

// ── Arduino entry points ──────────────────────────────────────────────────────

void IRAM_ATTR userKey() {
    run_tests_flag = true;
}

void setup() {
    Serial.begin(115200);
    // Wait up to 3s for terminal; don't block forever on USB-CDC boards.
    for (int i = 0; i < 300 && !Serial; i++) { delay(10); }
    Serial.println("\nencrypted_announce offline test");

    // ── SD init ───────────────────────────────────────────────────────────────
    // If SPI_SCK/MISO/MOSI are defined, bring up an explicit bus.
    // Useful on boards where SD shares a bus with other peripherals (e.g. LoRa).
    // Define LORA_CS (and any other peripheral CS pins) to assert them HIGH
    // before touching the bus so they don't respond to SD traffic.
#ifdef LORA_CS
    pinMode(LORA_CS, OUTPUT); digitalWrite(LORA_CS, HIGH);
#endif
#ifdef SPI_SCK
    pinMode(SD_CS_PIN, OUTPUT); digitalWrite(SD_CS_PIN, HIGH);
    SPIClass spi(HSPI);
    spi.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
    if (!SD.begin(SD_CS_PIN, spi, SD_SPEED)) {
#else
    if (!SD.begin(SD_CS_PIN)) {
#endif
        Serial.println("SD init failed — check wiring and SD_CS_PIN");
        while (true) { delay(1000); }
    }
    Serial.println("SD ready.");

    // ── filesystem init (must happen before any OS:: calls) ───────────────────
    filesystem.init();
    RNS::Utilities::OS::register_filesystem(filesystem);

    // DEBUG: wipe identity and stored messages so you can re-enrol with a new password.
    // Comment these out once everything is working.
    // RNS::Utilities::OS::remove_file(IDENTITY_PATH);
    // RNS::Utilities::OS::remove_file(MSG_PATH);
    // Serial.println("DEBUG: wiped identity and messages.");

    // ── button ────────────────────────────────────────────────────────────────
    pinMode(BUTTON_PIN, INPUT);
    attachInterrupt(BUTTON_PIN, userKey, FALLING);

    RNS::loglevel(RNS::LOG_TRACE);

    // ── identity load / create ────────────────────────────────────────────────
    char password[PASSWORD_BUF_LEN] = {0};

    uint8_t id_blob[IDENTITY_BLOB_LEN];

    if (!RNS::Utilities::OS::file_exists(IDENTITY_PATH)) {
        Serial.println("No identity file — generating new identity...");
        readPasswordSerial("Password: ", password, sizeof(password));
        RNS::Identity new_id;
        memcpy(id_blob, new_id.get_private_key().data(), 64);

        if (!password_protect(IDENTITY_PATH, password, id_blob, sizeof(id_blob))) {
            Serial.println("Failed to save identity.");
            memset(password, 0, sizeof(password));
            while (true) { delay(1000); }
        }
        Serial.print("Identity saved to ");
        Serial.println(IDENTITY_PATH);
        identity = new_id;
    } else {
        while (true) {
            readPasswordSerial("Password: ", password, sizeof(password));
            if (password_open(IDENTITY_PATH, password, id_blob, sizeof(id_blob))) {
                break;
            }
            Serial.println("Wrong password or corrupt identity file.");
            memset(password, 0, sizeof(password));
        }
        identity = RNS::Identity(false);
        identity.load_private_key(RNS::Bytes(id_blob, 64));
        Serial.println("Identity loaded.");
    }

    memset(password, 0, sizeof(password));
    memset(id_blob,  0, sizeof(id_blob));

    reticulum_setup();
    run_all_tests();
}

void loop() {
    reticulum.loop();

    if (run_tests_flag) {
        run_all_tests();
        run_tests_flag = false;
    }

    if (Serial.available()) {
        char c = (char)Serial.read();
        if (c == 'r') run_all_tests();
    }
}
