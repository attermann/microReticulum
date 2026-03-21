#include "Fernet.h"

#include "HMAC.h"
#include "PKCS7.h"
#include "AES.h"
#include "../Log.h"

#include <stdexcept>
#include <time.h>

using namespace RNS;
using namespace RNS::Cryptography;

Fernet::Fernet(const Bytes& key) {

	if (!key) {
		throw std::invalid_argument("Fernet key cannot be None");
	}

	if (key.size() != 32) {
		throw std::invalid_argument("Fernet key must be 32 bytes, not " + std::to_string(key.size()));
	}

	//self._signing_key = key[:16]
	_signing_key = key.left(16);
	//self._encryption_key = key[16:]
	_encryption_key = key.mid(16);

	MEM("Fernet object created");
}

Fernet::~Fernet() {
	MEM("Fernet object destroyed");
}

bool Fernet::verify_hmac(const Bytes& token) {

	if (token.size() <= 32) {
		throw std::invalid_argument("Cannot verify HMAC on token of only " + std::to_string(token.size()) + " bytes");
	}

	//received_hmac = token[-32:]
	Bytes received_hmac = token.right(32);
	DEBUGF("Fernet::verify_hmac: received_hmac: %s", received_hmac.toHex().c_str());
	//expected_hmac = HMAC.new(self._signing_key, token[:-32]).digest()
	Bytes expected_hmac = HMAC::generate(_signing_key, token.left(token.size()-32))->digest();
	DEBUGF("Fernet::verify_hmac: expected_hmac: %s", expected_hmac.toHex().c_str());

	return (received_hmac == expected_hmac);
}

const Bytes Fernet::encrypt(const Bytes& data) {

	DEBUGF("Fernet::encrypt: plaintext length: %lu", data.size());
	Bytes iv = random(16);
	//double current_time = OS::time();
	TRACEF("Fernet::encrypt: iv:         %s", iv.toHex().c_str());

	TRACEF("Fernet::encrypt: plaintext:  %s", data.toHex().c_str());
	Bytes ciphertext = AES_128_CBC::encrypt(
		PKCS7::pad(data),
		_encryption_key,
		iv
	);
	DEBUGF("Fernet::encrypt: padded ciphertext length: %lu", ciphertext.size());
	TRACEF("Fernet::encrypt: ciphertext: %s", ciphertext.toHex().c_str());

	Bytes signed_parts = iv + ciphertext;

	//return signed_parts + HMAC::generate(_signing_key, signed_parts)->digest();
	Bytes sig(HMAC::generate(_signing_key, signed_parts)->digest());
	TRACEF("Fernet::encrypt: sig:        %s", sig.toHex().c_str());
	Bytes token(signed_parts + sig);
	DEBUGF("Fernet::encrypt: token length: %lu", token.size());
	return token;
}


const Bytes Fernet::decrypt(const Bytes& token) {

	DEBUGF("Fernet::decrypt: token length: %lu", token.size());
	if (token.size() < 48) {
		throw std::invalid_argument("Cannot decrypt token of only " + std::to_string(token.size()) + " bytes");
	}

	if (!verify_hmac(token)) {
		throw std::invalid_argument("Fernet token HMAC was invalid");
	}

	//iv = token[:16]
	Bytes iv = token.left(16);
	TRACEF("Fernet::decrypt: iv:         %s", iv.toHex().c_str());

	//ciphertext = token[16:-32]
	Bytes ciphertext = token.mid(16, token.size()-48);
	TRACEF("Fernet::decrypt: ciphertext: %s", ciphertext.toHex().c_str());

	try {
		Bytes plaintext = PKCS7::unpad(
			AES_128_CBC::decrypt(
				ciphertext,
				_encryption_key,
				iv
			)
		);
		DEBUGF("Fernet::encrypt: unpadded plaintext length: %lu", plaintext.size());
		TRACEF("Fernet::decrypt: plaintext:  %s", plaintext.toHex().c_str());

		DEBUGF("Fernet::decrypt: plaintext length: %lu", plaintext.size());
		return plaintext;
	}
	catch (const std::exception& e) {
		WARNING("Could not decrypt Fernet token");
		throw std::runtime_error("Could not decrypt Fernet token");
	}
}