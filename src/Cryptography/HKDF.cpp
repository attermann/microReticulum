#include "HKDF.h"

#include <HKDF.h>
#include <SHA256.h>

using namespace RNS;

const Bytes RNS::Cryptography::hkdf(size_t length, const Bytes& derive_from, const Bytes& salt /*= {Bytes::NONE}*/, const Bytes& context /*= {Bytes::NONE}*/) {

	if (length <= 0) {
		throw std::invalid_argument("Invalid output key length");
	}

	if (!derive_from) {
		throw std::invalid_argument("Cannot derive key from empty input material");
	}

	HKDF<SHA256> hkdf;
	if (salt) {
		hkdf.setKey(derive_from.data(), derive_from.size(), salt.data(), salt.size());
	}
	else {
		hkdf.setKey(derive_from.data(), derive_from.size());
	}
	Bytes derived;
	hkdf.extract(derived.writable(length), length);
	return derived;
}
