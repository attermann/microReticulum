#include <unity.h>

#include "Reticulum.h"
#include "Identity.h"
#include "Destination.h"
#include "Packet.h"
#include "Bytes.h"
#include "Log.h"

#include "Cryptography/HMAC.h"
#include "Cryptography/PKCS7.h"

#include <assert.h>

void testCrypto() {

	RNS::Reticulum reticulum;

	RNS::Identity identity;

	RNS::Destination destination(identity, RNS::Destination::IN, RNS::Destination::SINGLE, "appname", "aspects");
	//assert(encryptionPrivateKey().toHex().compare("") == );
	//assert(signingPrivateKey().toHex().compare("") == );
	//assert(encryptionPublicKey().toHex().compare("") == );
	//assert(signingPublicKey().toHex().compare("") == );

	//Packet packet = destination.announce("appdata");
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
		RNS::extreme("expected hash: " + hash.toHex());
		RNS::Cryptography::HMAC hmac(key, data);
		RNS::Bytes result = hmac.digest();
		RNS::extreme("result hash:   " + result.toHex());
		assert(memcmp(hash.data(), result.data(), result.size()) == 0);
	}
}

void testPKCS7() {

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

/*
int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(testCrypto);
	return UNITY_END();
}
*/
