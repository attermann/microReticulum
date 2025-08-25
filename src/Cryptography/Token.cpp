#include "Token.h"

#include "HMAC.h"
#include "PKCS7.h"
#include "AES.h"
#include "../Log.h"

#include <stdexcept>
#include <time.h>

using namespace RNS;
using namespace RNS::Cryptography;
using namespace RNS::Type::Cryptography::Token;

Token::Token(const Bytes& key, token_mode mode /*= AES*/) {

	if (!key) {
		throw std::invalid_argument("Token key cannot be None");
	}

	if (mode == MODE_AES) {
		if (key.size() == 32) {
			_mode = MODE_AES_128_CBC;
			//p self._signing_key = key[:16]
			_signing_key = key.left(16);
			//p self._encryption_key = key[16:]
			_encryption_key = key.mid(16);
		}
		else if (key.size() == 64) {
			_mode = MODE_AES_256_CBC;
			//p self._signing_key = key[:32]
			_signing_key = key.left(32);
			//p self._encryption_key = key[32:]
			_encryption_key = key.mid(32);
		}
		else {
			throw std::invalid_argument("Token key must be 128 or 256 bits, not " + std::to_string(key.size()*8));
		}
	}
	else {
		throw std::invalid_argument("Invalid token mode: " + std::to_string(mode));
	}

	MEM("Token object created");
}

Token::~Token() {
	MEM("Token object destroyed");
}

bool Token::verify_hmac(const Bytes& token) {

	if (token.size() <= 32) {
		throw std::invalid_argument("Cannot verify HMAC on token of only " + std::to_string(token.size()) + " bytes");
	}

	//received_hmac = token[-32:]
	Bytes received_hmac = token.right(32);
	DEBUG("Token::verify_hmac: received_hmac: " + received_hmac.toHex());
	//expected_hmac = HMAC.new(self._signing_key, token[:-32]).digest()
	Bytes expected_hmac = HMAC::generate(_signing_key, token.left(token.size()-32))->digest();
	DEBUG("Token::verify_hmac: expected_hmac: " + expected_hmac.toHex());

	return (received_hmac == expected_hmac);
}

const Bytes Token::encrypt(const Bytes& data) {

	DEBUG("Token::encrypt: plaintext length: " + std::to_string(data.size()));
	Bytes iv = random(16);
	//double current_time = OS::time();
	TRACE("Token::encrypt: iv:         " + iv.toHex());

	TRACE("Token::encrypt: plaintext:  " + data.toHex());
	Bytes ciphertext;
	if (_mode == MODE_AES_128_CBC) {
		ciphertext = AES_128_CBC::encrypt(
			PKCS7::pad(data),
			_encryption_key,
			iv
		);
	}
	else if (_mode == MODE_AES_256_CBC) {
		ciphertext = AES_256_CBC::encrypt(
			PKCS7::pad(data),
			_encryption_key,
			iv
		);
	}
	else {
		throw new std::invalid_argument("Invalid token mode "+std::to_string(_mode));
	}
	DEBUG("Token::encrypt: padded ciphertext length: " + std::to_string(ciphertext.size()));
	TRACE("Token::encrypt: ciphertext: " + ciphertext.toHex());

	Bytes signed_parts = iv + ciphertext;

	//return signed_parts + HMAC::generate(_signing_key, signed_parts)->digest();
	Bytes sig(HMAC::generate(_signing_key, signed_parts)->digest());
	TRACE("Token::encrypt: sig:        " + sig.toHex());
	Bytes token(signed_parts + sig);
	DEBUG("Token::encrypt: token length: " + std::to_string(token.size()));
	return token;
}


const Bytes Token::decrypt(const Bytes& token) {

	DEBUG("Token::decrypt: token length: " + std::to_string(token.size()));
	if (token.size() < 48) {
		throw std::invalid_argument("Cannot decrypt token of only " + std::to_string(token.size()) + " bytes");
	}

	if (!verify_hmac(token)) {
		throw std::invalid_argument("Token token HMAC was invalid");
	}

	//iv = token[:16]
	Bytes iv = token.left(16);
	TRACE("Token::decrypt: iv:         " + iv.toHex());

	//ciphertext = token[16:-32]
	Bytes ciphertext = token.mid(16, token.size()-48);
	TRACE("Token::decrypt: ciphertext: " + ciphertext.toHex());

	try {
		Bytes plaintext;
		if (_mode == MODE_AES_128_CBC) {
			plaintext = PKCS7::unpad(
				AES_128_CBC::decrypt(
					ciphertext,
					_encryption_key,
					iv
				)
			);
		}
		else if (_mode == MODE_AES_256_CBC) {
			plaintext = PKCS7::unpad(
				AES_256_CBC::decrypt(
					ciphertext,
					_encryption_key,
					iv
				)
			);
		}
		else {
			throw new std::invalid_argument("Invalid token mode "+std::to_string(_mode));
		}
		DEBUG("Token::encrypt: unpadded plaintext length: " + std::to_string(plaintext.size()));
		TRACE("Token::decrypt: plaintext:  " + plaintext.toHex());

		DEBUG("Token::decrypt: plaintext length: " + std::to_string(plaintext.size()));
		return plaintext;
	}
	catch (std::exception& e) {
		WARNING("Could not decrypt Token token");
		throw std::runtime_error("Could not decrypt Token token");
	}
}