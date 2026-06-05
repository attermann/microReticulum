#include <unity.h>

#include "microReticulum/Bytes.h"
#include "microReticulum/Identity.h"
#include "microReticulum/Packet.h"
#include "microReticulum/Utilities/Crc.h"
#include "microReticulum/Cryptography/HMAC.h"
#include "microReticulum/Cryptography/PKCS7.h"

#include <string.h>
#include <vector>
#include <unistd.h>
#include <time.h>
#include <stdint.h>
#include <sys/time.h>
#include <stdio.h>

// Imported from Crypto.cpp
// Computes CRC-8/HITAG checksum
// CBA Doesn't appear to support incremental checksum building
extern uint8_t crypto_crc8(uint8_t tag, const void *data, unsigned size);

void testHMAC() {

	{
		const char keystr[] = "key";
		const char datastr[] = "The quick brown fox jumps over the lazy dog";
		const uint8_t hasharr[] = {
			0xf7, 0xbc, 0x83, 0xf4, 0x30, 0x53, 0x84, 0x24,
			0xb1, 0x32, 0x98, 0xe6, 0xaa, 0x6f, 0xb1, 0x43,
			0xef, 0x4d, 0x59, 0xa1, 0x49, 0x46, 0x17, 0x59,
			0x97, 0x47, 0x9d, 0xbc, 0x2d, 0x1a, 0x3c, 0xd8
		};
		RNS::Bytes key(keystr);
		RNS::Bytes data(datastr);
		RNS::Bytes hash(hasharr, sizeof(hasharr));
		//TRACEF("expected hash: %s", hash.toHex().c_str());
		RNS::Cryptography::HMAC hmac(key, data);
		RNS::Bytes result = hmac.digest();
		//TRACEF("result hash:   %s", result.toHex().c_str());
		TEST_ASSERT_EQUAL_INT(0, memcmp(hash.data(), result.data(), result.size()));
	}
}

void testPKCS7() {
	
	const uint8_t str[] = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";

	// test buffer of half blocksize
	{
		size_t len = RNS::Cryptography::PKCS7::BLOCKSIZE / 2;
		RNS::Bytes bytes(str, len);
		bytes = RNS::Cryptography::PKCS7::pad(bytes);
		TEST_ASSERT_EQUAL_size_t(RNS::Cryptography::PKCS7::BLOCKSIZE, bytes.size());

		bytes = RNS::Cryptography::PKCS7::unpad(bytes);
		TEST_ASSERT_EQUAL_size_t(len, bytes.size());
		TEST_ASSERT_EQUAL_INT(0, memcmp(bytes.data(), str, len));
	}

	// test buffer of one less blocksize
	{
		size_t len = RNS::Cryptography::PKCS7::BLOCKSIZE - 1;
		RNS::Bytes bytes(str, len);
		bytes = RNS::Cryptography::PKCS7::pad(bytes);
		TEST_ASSERT_EQUAL_size_t(RNS::Cryptography::PKCS7::BLOCKSIZE, bytes.size());

		bytes = RNS::Cryptography::PKCS7::unpad(bytes);
		TEST_ASSERT_EQUAL_size_t(len, bytes.size());
		TEST_ASSERT_EQUAL_INT(0, memcmp(bytes.data(), str, len));
	}

	// test buffer of blocksize
	{
		size_t len = RNS::Cryptography::PKCS7::BLOCKSIZE;
		RNS::Bytes bytes(str, len);
		bytes = RNS::Cryptography::PKCS7::pad(bytes);
		TEST_ASSERT_EQUAL_size_t(RNS::Cryptography::PKCS7::BLOCKSIZE * 2, bytes.size());

		bytes = RNS::Cryptography::PKCS7::unpad(bytes);
		TEST_ASSERT_EQUAL_size_t(len, bytes.size());
		TEST_ASSERT_EQUAL_INT(0, memcmp(bytes.data(), str, len));
	}

	// test inplace buffer of half blocksize
	{
		size_t len = RNS::Cryptography::PKCS7::BLOCKSIZE / 2;
		RNS::Bytes bytes(str, len);
		RNS::Cryptography::PKCS7::inplace_pad(bytes);
		TEST_ASSERT_EQUAL_size_t(RNS::Cryptography::PKCS7::BLOCKSIZE, bytes.size());

		RNS::Cryptography::PKCS7::inplace_unpad(bytes);
		TEST_ASSERT_EQUAL_size_t(len, bytes.size());
		TEST_ASSERT_EQUAL_INT(0, memcmp(bytes.data(), str, len));
	}

	// test inplace buffer of one less blocksize
	{
		size_t len = RNS::Cryptography::PKCS7::BLOCKSIZE - 1;
		RNS::Bytes bytes(str, len);
		RNS::Cryptography::PKCS7::inplace_pad(bytes);
		TEST_ASSERT_EQUAL_size_t(RNS::Cryptography::PKCS7::BLOCKSIZE, bytes.size());

		RNS::Cryptography::PKCS7::inplace_unpad(bytes);
		TEST_ASSERT_EQUAL_size_t(len, bytes.size());
		TEST_ASSERT_EQUAL_INT(0, memcmp(bytes.data(), str, len));
	}

	// test inplace buffer of blocksize
	{
		size_t len = RNS::Cryptography::PKCS7::BLOCKSIZE;
		RNS::Bytes bytes(str, len);
		RNS::Cryptography::PKCS7::inplace_pad(bytes);
		TEST_ASSERT_EQUAL_size_t(RNS::Cryptography::PKCS7::BLOCKSIZE * 2, bytes.size());

		RNS::Cryptography::PKCS7::inplace_unpad(bytes);
		TEST_ASSERT_EQUAL_size_t(len, bytes.size());
		TEST_ASSERT_EQUAL_INT(0, memcmp(bytes.data(), str, len));
	}
}

void testCrc8() {
	char data[32];
	uint8_t crc;

	strcpy(data, "foo");
	crc = crypto_crc8(0, data, strlen(data));
	TEST_ASSERT_EQUAL_UINT8(0x48, crc);

	strcpy(data, "bar");
	crc = crypto_crc8(0, data, strlen(data));
	TEST_ASSERT_EQUAL_UINT8(0xED, crc);

	strcpy(data, "foo");
	crc = crypto_crc8(0, data, strlen(data));
	TEST_ASSERT_EQUAL_UINT8(0x48, crc);

	strcpy(data, "foobarfoo");
	crc = crypto_crc8(0, data, strlen(data));
	TEST_ASSERT_EQUAL_UINT8(0x00, crc);

}

void testCrc32() {
	char data[16];
	uint32_t crc;

	strcpy(data, "foo");
	crc = RNS::Utilities::Crc::crc32(0, data);
	TEST_ASSERT_EQUAL_UINT32(0x8C736521, crc);

	strcpy(data, "bar");
	crc = RNS::Utilities::Crc::crc32(0, data);
	TEST_ASSERT_EQUAL_UINT32(0x76FF8CAA, crc);

	strcpy(data, "foo");
	crc = RNS::Utilities::Crc::crc32(0, data);
	TEST_ASSERT_EQUAL_UINT32(0x8C736521, crc);

	strcpy(data, "foobarfoo");
	crc = RNS::Utilities::Crc::crc32(0, data);
	TEST_ASSERT_EQUAL_UINT32(0xEE2F4613, crc);
}

void testIncrementalCrc32() {
	char data[16];
	uint32_t crc;

	strcpy(data, "foo");
	crc = RNS::Utilities::Crc::crc32(0, data);
	TEST_ASSERT_EQUAL_UINT32(0x8C736521, crc);

	strcpy(data, "bar");
	crc = RNS::Utilities::Crc::crc32(crc, data);
	TEST_ASSERT_EQUAL_UINT32(0x9EF61F95, crc);

	strcpy(data, "foo");
	crc = RNS::Utilities::Crc::crc32(crc, data);
	TEST_ASSERT_EQUAL_UINT32(0xEE2F4613, crc);

	strcpy(data, "foobarfoo");
	crc = RNS::Utilities::Crc::crc32(0, data);
	TEST_ASSERT_EQUAL_UINT32(0xEE2F4613, crc);
}

void testByteCrc32() {
	char data[16];
	strcpy(data, "foobarfoo");

	uint32_t crc = 0;
	for (int i = 0; i < strlen(data); i++) {
		crc = RNS::Utilities::Crc::crc32(crc, data[i]);
	}
	TEST_ASSERT_EQUAL_UINT32(0xEE2F4613, crc);
}

void testIdentityValidate() {
	RNS::Identity identity(true);
	RNS::Bytes message("the quick brown fox jumps over the lazy dog");

	RNS::Bytes signature = identity.sign(message);
	TEST_ASSERT_TRUE(identity.validate(signature, message));

	// A signature with a flipped byte must not validate.
	std::vector<uint8_t> tampered(signature.data(), signature.data() + signature.size());
	tampered[0] ^= 0xFF;
	RNS::Bytes bad_signature(tampered.data(), tampered.size());
	TEST_ASSERT_FALSE(identity.validate(bad_signature, message));

	// A valid signature against a different message must not validate.
	RNS::Bytes other_message("the quick brown fox jumps over the lazy cat");
	TEST_ASSERT_FALSE(identity.validate(signature, other_message));
}

void testDirectAnnounceValidate() {
	RNS::Bytes raw;
	raw.assignHex("0100f083a7f4b00d799808c44a4634bba7d7006afd960bf3b01801a2e88b2ce1f7040817dc1b6bffa366b103468f3988e0db7f00dc1fdc15fa7fd31a34a02207cfb4d26e11e57504e43686ec7fad84774bec88fd68805f2ea383c8d6f6c39824652e00698fd5fd1088b38832f247a9daebf017d8bfe641882d9fe9b37cf49a97402b7e3d8bec61b4950d39c0996588dd0288bf6a7a0a4390bb331bd82704b618f107cf8bf2230f");
	RNS::Packet packet(raw);
	packet.unpack();
	TEST_ASSERT_TRUE(RNS::Identity::validate_announce(packet));
}

void testRebroadcastAnnounceValidate() {
	RNS::Bytes raw;
	raw.assignHex("510139745d39d5108615635d433d6cb14803f083a7f4b00d799808c44a4634bba7d7006afd960bf3b01801a2e88b2ce1f7040817dc1b6bffa366b103468f3988e0db7f00dc1fdc15fa7fd31a34a02207cfb4d26e11e57504e43686ec7fad84774bec88fd68805f2ea383c8d6f6c39824652e00698fd5fd1088b38832f247a9daebf017d8bfe641882d9fe9b37cf49a97402b7e3d8bec61b4950d39c0996588dd0288bf6a7a0a4390bb331bd82704b618f107cf8bf2230f");
	RNS::Packet packet(raw);
	packet.unpack();
	TEST_ASSERT_TRUE(RNS::Identity::validate_announce(packet));
}

void testDirectRatchetAnnounceValidate() {
	RNS::Bytes raw;
	raw.assignHex("21003dc438c85235151be9a59020807930d200a3dc290f674385c482eeac8da9108c20d5d30b9d20680d2a39bf3d95d6fcb04f1e2bd080869ca0b7f3ca0271205899635b94e19f2463b77e5c4f60e82287ab5e6ec60bc318e2c0f0d9087a321643c100698fd5f11045f113f98185b9f5b01de11f59f21848f7244bcbc54bfe5bad99f3a6e0d2264b78687aa12e62b9ad581161acc9202bcce4978bdc68a73595e908b2396c8152c689d6c3abf99081dd00727f4744caaf76aaf7978c3866e31577c6e6c066830092c40e416e6f6e796d6f75732050656572c0");
	RNS::Packet packet(raw);
	packet.unpack();
	TEST_ASSERT_TRUE(RNS::Identity::validate_announce(packet));
}

void testRebroadcastRatchetAnnounceValidate() {
	RNS::Bytes raw;
	raw.assignHex("710139745d39d5108615635d433d6cb148033dc438c85235151be9a59020807930d200a3dc290f674385c482eeac8da9108c20d5d30b9d20680d2a39bf3d95d6fcb04f1e2bd080869ca0b7f3ca0271205899635b94e19f2463b77e5c4f60e82287ab5e6ec60bc318e2c0f0d9087a321643c100698fd5f11045f113f98185b9f5b01de11f59f21848f7244bcbc54bfe5bad99f3a6e0d2264b78687aa12e62b9ad581161acc9202bcce4978bdc68a73595e908b2396c8152c689d6c3abf99081dd00727f4744caaf76aaf7978c3866e31577c6e6c066830092c40e416e6f6e796d6f75732050656572c0");
	RNS::Packet packet(raw);
	packet.unpack();
	TEST_ASSERT_TRUE(RNS::Identity::validate_announce(packet));
}

void setUp(void) {
    // set stuff up here before each test
}

void tearDown(void) {
    // clean stuff up here after each test
}

int runUnityTests(void) {
    UNITY_BEGIN();
	RUN_TEST(testHMAC);
	RUN_TEST(testPKCS7);
	RUN_TEST(testCrc8);
	RUN_TEST(testCrc32);
	RUN_TEST(testIncrementalCrc32);
	RUN_TEST(testByteCrc32);
	RUN_TEST(testIdentityValidate);
	RUN_TEST(testDirectAnnounceValidate);
	RUN_TEST(testRebroadcastAnnounceValidate);
	RUN_TEST(testDirectRatchetAnnounceValidate);
	RUN_TEST(testRebroadcastRatchetAnnounceValidate);
    return UNITY_END();
}

// For native dev-platform or for some embedded frameworks
int main(void) {
    return runUnityTests();
}

#ifdef ARDUINO
// For Arduino framework
void setup() {
    // Wait ~2 seconds before the Unity test runner
    // establishes connection with a board Serial interface
    delay(2000);
    
    runUnityTests();
}
void loop() {}
#endif

// For ESP-IDF framework
void app_main() {
    runUnityTests();
}
