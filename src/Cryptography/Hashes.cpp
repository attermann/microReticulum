#include "Hashes.h"

#include "../Log.h"
#include "../Bytes.h"

#include <SHA256.h>
#include <SHA512.h>

using namespace RNS;

/*
The SHA primitives are abstracted here to allow platform-
aware hardware acceleration in the future. Currently only
uses Python's internal SHA-256 implementation. All SHA-256
calls in RNS end up here.
*/

Bytes RNS::Cryptography::sha256(const Bytes &data) {
	//extreme("Cryptography::sha256: data: " + data.toHex() );
	SHA256 digest;
	digest.reset();
	digest.update(data.data(), data.size());
	Bytes hash;
	digest.finalize(hash.writable(32), 32);
	//extreme("Cryptography::sha256: hash: " + hash.toHex() );
	return hash;
}

Bytes RNS::Cryptography::sha512(const Bytes &data) {
	SHA512 digest;
	digest.reset();
	digest.update(data.data(), data.size());
	Bytes hash;
	digest.finalize(hash.writable(64), 64);
	//extreme("Cryptography::sha512: hash: " + hash.toHex() );
	return hash;
}
