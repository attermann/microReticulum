#pragma once

#include "Random.h"
#include "../Bytes.h"
#include "../Type.h"

#include <stdint.h>

namespace RNS { namespace Cryptography {

    /*
    This class provides a slightly modified implementation of the Fernet spec
    found at: https://github.com/fernet/spec/blob/master/Spec.md

    According to the spec, a Fernet token includes a one byte VERSION and
    eight byte TIMESTAMP field at the start of each token. These fields are
    not relevant to Reticulum. They are therefore stripped from this
    implementation, since they incur overhead and leak initiator metadata.
    */
	class Token {

	public:
		using Ptr = std::shared_ptr<Token>;

	public:
		static inline const Bytes generate_key(RNS::Type::Cryptography::Token::token_mode mode = RNS::Type::Cryptography::Token::MODE_AES_256_CBC) {
			if (mode == RNS::Type::Cryptography::Token::MODE_AES_128_CBC) return random(32);
			else if (mode == RNS::Type::Cryptography::Token::MODE_AES_256_CBC) return random(64);
			else throw new std::invalid_argument("Invalid token mode: " + std::to_string(mode));
		}

	public:
		Token(const Bytes& key, RNS::Type::Cryptography::Token::token_mode mode = RNS::Type::Cryptography::Token::MODE_AES);
		~Token();

	public:
		bool verify_hmac(const Bytes& token);
		const Bytes encrypt(const Bytes& data);
		const Bytes decrypt(const Bytes& token);

	private:
		RNS::Type::Cryptography::Token::token_mode _mode = RNS::Type::Cryptography::Token::MODE_AES_256_CBC;
		Bytes _signing_key;
		Bytes _encryption_key;
	};

} }
