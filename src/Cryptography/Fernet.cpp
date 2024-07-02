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
	DEBUG("Fernet::verify_hmac: received_hmac: " + received_hmac.toHex());
	//expected_hmac = HMAC.new(self._signing_key, token[:-32]).digest()
	Bytes expected_hmac = HMAC::generate(_signing_key, token.left(token.size()-32))->digest();
	DEBUG("Fernet::verify_hmac: expected_hmac: " + expected_hmac.toHex());

	return (received_hmac == expected_hmac);
}

const Bytes Fernet::encrypt(const Bytes& data) {

	DEBUG("Fernet::encrypt: plaintext length: " + std::to_string(data.size()));
	Bytes iv = random(16);
	//double current_time = OS::time();
	TRACE("Fernet::encrypt: iv:         " + iv.toHex());

	TRACE("Fernet::encrypt: plaintext:  " + data.toHex());
	Bytes ciphertext = AES_128_CBC::encrypt(
		PKCS7::pad(data),
		_encryption_key,
		iv
	);
	DEBUG("Fernet::encrypt: padded ciphertext length: " + std::to_string(ciphertext.size()));
	TRACE("Fernet::encrypt: ciphertext: " + ciphertext.toHex());

	Bytes signed_parts = iv + ciphertext;

	//return signed_parts + HMAC::generate(_signing_key, signed_parts)->digest();
	Bytes sig(HMAC::generate(_signing_key, signed_parts)->digest());
	TRACE("Fernet::encrypt: sig:        " + sig.toHex());
	Bytes token(signed_parts + sig);
	DEBUG("Fernet::encrypt: token length: " + std::to_string(token.size()));
	return token;
}


const Bytes Fernet::decrypt(const Bytes& token) {

	DEBUG("Fernet::decrypt: token length: " + std::to_string(token.size()));
	if (token.size() < 48) {
		throw std::invalid_argument("Cannot decrypt token of only " + std::to_string(token.size()) + " bytes");
	}

	if (!verify_hmac(token)) {
		throw std::invalid_argument("Fernet token HMAC was invalid");
	}

	//iv = token[:16]
	Bytes iv = token.left(16);
	TRACE("Fernet::decrypt: iv:         " + iv.toHex());

	//ciphertext = token[16:-32]
	Bytes ciphertext = token.mid(16, token.size()-48);
	TRACE("Fernet::decrypt: ciphertext: " + ciphertext.toHex());

	try {
		Bytes plaintext = PKCS7::unpad(
			AES_128_CBC::decrypt(
				ciphertext,
				_encryption_key,
				iv
			)
		);
		DEBUG("Fernet::encrypt: unpadded plaintext length: " + std::to_string(plaintext.size()));
		TRACE("Fernet::decrypt: plaintext:  " + plaintext.toHex());

		DEBUG("Fernet::decrypt: plaintext length: " + std::to_string(plaintext.size()));
		return plaintext;
	}
	catch (std::exception& e) {
		warning("Could not decrypt Fernet token");
		throw std::runtime_error("Could not decrypt Fernet token");
	}
}