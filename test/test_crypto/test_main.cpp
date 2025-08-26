#include <unity.h>

#include "Bytes.h"
#include "Utilities/Crc.h"
#include "Cryptography/HMAC.h"
#include "Cryptography/PKCS7.h"

#include <string.h>
#include <unistd.h>
#include <time.h>
#include <stdint.h>
#include <sys/time.h>
#include <stdio.h>

// Imported from Crypto.cpp
// Computes CRC-8/HITAG checksum
// CBA Doesn't appear to support incremental checksum building
extern uint8_t crypto_crc8(uint8_t tag, const void *data, unsigned size);

void setUp(void) {
    // set stuff up here before each test
}

void tearDown(void) {
    // clean stuff up here after each test
}

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

int runUnityTests(void) {
    UNITY_BEGIN();
	RUN_TEST(testHMAC);
	RUN_TEST(testPKCS7);
	RUN_TEST(testCrc8);
	RUN_TEST(testCrc32);
	RUN_TEST(testIncrementalCrc32);
	RUN_TEST(testByteCrc32);
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
