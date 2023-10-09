#pragma once

#include "Random.h"
#include "../Bytes.h"

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
	class Fernet {

	public:
		static const uint8_t FERNET_OVERHEAD  = 48; // Bytes

	public:
		static inline Bytes generate_key() { return random(32); }

	public:
		Fernet(const Bytes &key);
		~Fernet();

	public:
		bool verify_hmac(const Bytes &token);
		Bytes encrypt(const Bytes &data);
		Bytes decrypt(const Bytes &token);

	private:
		Bytes _signing_key;
		Bytes _encryption_key;
	};

} }
