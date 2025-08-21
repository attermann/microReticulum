//#include <unity.h>

#include "Reticulum.h"
#include "Identity.h"
#include "Destination.h"
#include "Packet.h"
#include "Bytes.h"
#include "Log.h"
#include "Utilities/Crc.h"

#include "Cryptography/HMAC.h"
#include "Cryptography/PKCS7.h"

#include <assert.h>

// Imported from Crypto.cpp
// Computes CRC-8/HITAG checksum
// CBA Doesn't appeart to support incremental checksum building
extern uint8_t crypto_crc8(uint8_t tag, const void *data, unsigned size);

void testCryptoMain() {
	HEAD("testCryptoMain", RNS::LOG_TRACE);

	RNS::Reticulum reticulum_crypto;
	assert(reticulum_crypto);

	RNS::Identity identity;
	assert(identity);

	RNS::Destination destination(identity, RNS::Type::Destination::IN, RNS::Type::Destination::SINGLE, "appname", "aspects");
	//assert(encryptionPrivateKey().toHex().compare("") == );
	//assert(signingPrivateKey().toHex().compare("") == );
	//assert(encryptionPublicKey().toHex().compare("") == );
	//assert(signingPublicKey().toHex().compare("") == );

	//Packet packet = destination.announce("appdata");
}

void testHMAC() {
	HEAD("testHMAC", RNS::LOG_TRACE);

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
		TRACEF("expected hash: %s", hash.toHex().c_str());
		RNS::Cryptography::HMAC hmac(key, data);
		RNS::Bytes result = hmac.digest();
		TRACEF("result hash:   %s", result.toHex().c_str());
		assert(memcmp(hash.data(), result.data(), result.size()) == 0);
	}
}

void testPKCS7() {
	HEAD("testPKCS7", RNS::LOG_TRACE);
	
	const uint8_t str[] = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";

	// test buffer of half blocksize
	{
		size_t len = RNS::Cryptography::PKCS7::BLOCKSIZE / 2;
		RNS::Bytes bytes(str, len);
		bytes = RNS::Cryptography::PKCS7::pad(bytes);
		assert(bytes.size() == RNS::Cryptography::PKCS7::BLOCKSIZE);

		bytes = RNS::Cryptography::PKCS7::unpad(bytes);
		assert(bytes.size() == len);
		assert(memcmp(bytes.data(), str, len) == 0);
	}

	// test buffer of one less blocksize
	{
		size_t len = RNS::Cryptography::PKCS7::BLOCKSIZE - 1;
		RNS::Bytes bytes(str, len);
		bytes = RNS::Cryptography::PKCS7::pad(bytes);
		assert(bytes.size() == RNS::Cryptography::PKCS7::BLOCKSIZE);

		bytes = RNS::Cryptography::PKCS7::unpad(bytes);
		assert(bytes.size() == len);
		assert(memcmp(bytes.data(), str, len) == 0);
	}

	// test buffer of blocksize
	{
		size_t len = RNS::Cryptography::PKCS7::BLOCKSIZE;
		RNS::Bytes bytes(str, len);
		bytes = RNS::Cryptography::PKCS7::pad(bytes);
		assert(bytes.size() == RNS::Cryptography::PKCS7::BLOCKSIZE * 2);

		bytes = RNS::Cryptography::PKCS7::unpad(bytes);
		assert(bytes.size() == len);
		assert(memcmp(bytes.data(), str, len) == 0);
	}

	// test inplace buffer of half blocksize
	{
		size_t len = RNS::Cryptography::PKCS7::BLOCKSIZE / 2;
		RNS::Bytes bytes(str, len);
		RNS::Cryptography::PKCS7::inplace_pad(bytes);
		assert(bytes.size() == RNS::Cryptography::PKCS7::BLOCKSIZE);

		RNS::Cryptography::PKCS7::inplace_unpad(bytes);
		assert(bytes.size() == len);
		assert(memcmp(bytes.data(), str, len) == 0);
	}

	// test inplace buffer of one less blocksize
	{
		size_t len = RNS::Cryptography::PKCS7::BLOCKSIZE - 1;
		RNS::Bytes bytes(str, len);
		RNS::Cryptography::PKCS7::inplace_pad(bytes);
		assert(bytes.size() == RNS::Cryptography::PKCS7::BLOCKSIZE);

		RNS::Cryptography::PKCS7::inplace_unpad(bytes);
		assert(bytes.size() == len);
		assert(memcmp(bytes.data(), str, len) == 0);
	}

	// test inplace buffer of blocksize
	{
		size_t len = RNS::Cryptography::PKCS7::BLOCKSIZE;
		RNS::Bytes bytes(str, len);
		RNS::Cryptography::PKCS7::inplace_pad(bytes);
		assert(bytes.size() == RNS::Cryptography::PKCS7::BLOCKSIZE * 2);

		RNS::Cryptography::PKCS7::inplace_unpad(bytes);
		assert(bytes.size() == len);
		assert(memcmp(bytes.data(), str, len) == 0);
	}
}

void testCrc8() {
	HEAD("testCrc8", RNS::LOG_TRACE);
	
	char data[16];
	uint8_t crc;

	strcpy(data, "foo");
	crc = crypto_crc8(0, data, strlen(data));
	TRACEF("crc1: 0x%X", crc);
	//TEST_ASSERT_EQUAL_UINT8(0x48, crc);
	assert(0x48 == crc);

	strcpy(data, "bar");
	crc = crypto_crc8(0, data, strlen(data));
	TRACEF("crc2: 0x%X", crc);
	//TEST_ASSERT_EQUAL_UINT8(0xED, crc);
	assert(0xED == crc);

	strcpy(data, "foo");
	crc = crypto_crc8(0, data, strlen(data));
	TRACEF("crc3: 0x%X", crc);
	//TEST_ASSERT_EQUAL_UINT8(0x48, crc);
	assert(0x48 == crc);

	strcpy(data, "foobarfoo");
	crc = crypto_crc8(0, data, strlen(data));
	TRACEF("crc4: 0x%X", crc);
	//TEST_ASSERT_EQUAL_UINT8(0x00, crc);
	assert(0x00 == crc);
}

void testCrc32() {
	HEAD("testCrc32", RNS::LOG_TRACE);
	
	char data[16];
	uint32_t crc;

	strcpy(data, "foo");
	crc = RNS::Utilities::Crc::crc32(0, data);
	TRACEF("crc1: 0x%X", crc);
	//TEST_ASSERT_EQUAL_UINT8(0x8C736521, crc);
	assert(0x8C736521 == crc);

	strcpy(data, "bar");
	crc = RNS::Utilities::Crc::crc32(0, data);
	TRACEF("crc2: 0x%X", crc);
	//TEST_ASSERT_EQUAL_UINT8(0x76FF8CAA, crc);
	assert(0x76FF8CAA == crc);

	strcpy(data, "foo");
	crc = RNS::Utilities::Crc::crc32(0, data);
	TRACEF("crc3: 0x%X", crc);
	//TEST_ASSERT_EQUAL_UINT8(0x8C736521, crc);
	assert(0x8C736521 == crc);

	strcpy(data, "foobarfoo");
	crc = RNS::Utilities::Crc::crc32(0, data);
	TRACEF("crc4: 0x%X", crc);
	//TEST_ASSERT_EQUAL_UINT8(0xEE2F4613, crc);
	assert(0xEE2F4613 == crc);
}

void testIncrementalCrc32() {
	HEAD("testIncrementalCrc32", RNS::LOG_TRACE);
	
	char data[16];
	uint32_t crc;

	strcpy(data, "foo");
	crc = RNS::Utilities::Crc::crc32(0, data);
	TRACEF("crc1: 0x%X", crc);
	//TEST_ASSERT_EQUAL_UINT8(0x8C736521, crc);
	assert(0x8C736521 == crc);

	strcpy(data, "bar");
	crc = RNS::Utilities::Crc::crc32(crc, data);
	TRACEF("crc1: 0x%X", crc);
	//TEST_ASSERT_EQUAL_UINT8(0x9EF61F95, crc);
	assert(0x9EF61F95 == crc);

	strcpy(data, "foo");
	crc = RNS::Utilities::Crc::crc32(crc, data);
	TRACEF("crc3: 0x%X", crc);
	//TEST_ASSERT_EQUAL_UINT8(0xEE2F4613, crc);
	assert(0xEE2F4613 == crc);

	strcpy(data, "foobarfoo");
	crc = RNS::Utilities::Crc::crc32(0, data);
	TRACEF("crc:  0x%X", crc);
	//TEST_ASSERT_EQUAL_UINT8(0xEE2F4613, crc);
	assert(0xEE2F4613 == crc);
}

void testByteCrc32() {
	HEAD("testByteCrc32", RNS::LOG_TRACE);
	
	char data[16];
	strcpy(data, "foobarfoo");

	uint32_t crc = 0;
	for (int i = 0; i < strlen(data); i++) {
		crc = RNS::Utilities::Crc::crc32(crc, data[i]);
	}
	TRACEF("crc:  0x%X", crc);
	//TEST_ASSERT_EQUAL_UINT8(0xEE2F4613, crc);
	assert(0xEE2F4613 == crc);
}

void testCrypto() {
	HEAD("Running testCrypto...", RNS::LOG_TRACE);
	//testCryptoMain();
	testHMAC();
	testPKCS7();
	testCrc8();
	testCrc32();
	testIncrementalCrc32();
	testByteCrc32();
}

/*
int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(testCrypto);
	return UNITY_END();
}
*/
