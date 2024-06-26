#include "Hashes.h"

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

const Bytes RNS::Cryptography::sha256(const Bytes& data) {
	//TRACE("Cryptography::sha256: data: " + data.toHex() );
	SHA256 digest;
	digest.reset();
	digest.update(data.data(), data.size());
	Bytes hash;
	digest.finalize(hash.writable(32), 32);
	//TRACE("Cryptography::sha256: hash: " + hash.toHex() );
	return hash;
}

const Bytes RNS::Cryptography::sha512(const Bytes& data) {
	SHA512 digest;
	digest.reset();
	digest.update(data.data(), data.size());
	Bytes hash;
	digest.finalize(hash.writable(64), 64);
	//TRACE("Cryptography::sha512: hash: " + hash.toHex() );
	return hash;
}
