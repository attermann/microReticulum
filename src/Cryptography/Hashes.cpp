#include "Hashes.h"

#include "../Log.h"

#include <SHA256.h>
#include <SHA512.h>

using namespace RNS::Cryptography;

/*
The SHA primitives are abstracted here to allow platform-
aware hardware acceleration in the future. Currently only
uses Python's internal SHA-256 implementation. All SHA-256
calls in RNS end up here.
*/

void RNS::Cryptography::sha256(uint8_t *hash, const uint8_t *data, uint16_t data_len) {
	//extreme("Cryptography::sha256: data: " + data.toHex() );
	SHA256 digest;
	digest.reset();
	digest.update(data, data_len);
	digest.finalize(hash, 32);
	//extreme("Cryptography::sha256: hash: " + hash.toHex() );
}

void RNS::Cryptography::sha512(uint8_t *hash, const uint8_t *data, uint16_t data_len) {
	SHA512 digest;
	digest.reset();
	digest.update(data, data_len);
	digest.finalize(hash, 64);
}
