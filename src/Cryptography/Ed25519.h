#pragma once

#include "Bytes.h"

#include <Ed25519.h>

#include <memory>

/*

Note that the library currently in use for Ed25519 does not support generating keys from a seed.

*/

namespace RNS { namespace Cryptography {

	class Ed25519PublicKey {

	public:
		using Ptr = std::shared_ptr<Ed25519PublicKey>;

	public:
		Ed25519PublicKey(const Bytes& publicKey) {
			_publicKey = publicKey;
		}
		~Ed25519PublicKey() {}

	public:
		// creates a new instance with specified seed
		static inline Ptr from_public_bytes(const Bytes& publicKey) {
			return Ptr(new Ed25519PublicKey(publicKey));
		}

		inline const Bytes& public_bytes() {
			return _publicKey;
		}

		inline bool verify(const Bytes& signature, const Bytes& message) {
			return Ed25519::verify(signature.data(), _publicKey.data(), message.data(), message.size());
		}

	private:
		Bytes _publicKey;

	};

	class Ed25519PrivateKey {

	public:
		using Ptr = std::shared_ptr<Ed25519PrivateKey>;

	public:
		Ed25519PrivateKey(const Bytes& privateKey) {
			if (privateKey) {
				// use specified private key
				_privateKey = privateKey;
			}
			else {
				// create random private key
				Ed25519::generatePrivateKey(_privateKey.writable(32));
			}
			// derive public key from private key
			Ed25519::derivePublicKey(_publicKey.writable(32), _privateKey.data());
		}
		~Ed25519PrivateKey() {}

	public:
		// creates a new instance with a random seed
		static inline Ptr generate() {
			// CBA TODO determine why below is confused with (implicit) copy constructor
			//return Ptr(new Ed25519PrivateKey({Bytes::NONE}));
			return Ptr(new Ed25519PrivateKey(Bytes::NONE));
		}

		// creates a new instance with specified seed
		static inline Ptr from_private_bytes(const Bytes& privateKey) {
			return Ptr(new Ed25519PrivateKey(privateKey));
		}

		inline const Bytes& private_bytes() {
			return _privateKey;
		}

		// creates a new instance of public key for this private key
		inline Ed25519PublicKey::Ptr public_key() {
			return Ed25519PublicKey::from_public_bytes(_publicKey);
		}

		inline const Bytes sign(const Bytes& message) {
			//z return _sk.sign(message);
			Bytes signature;
			Ed25519::sign(signature.writable(64), _privateKey.data(), _publicKey.data(), message.data(), message.size());
			return signature;
		}

	private:
		Bytes _privateKey;
		Bytes _publicKey;

	};

} }
