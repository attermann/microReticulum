#pragma once

#include "Log.h"
#include "Bytes.h"
#include "Type.h"
#include "Cryptography/Hashes.h"
#include "Cryptography/Ed25519.h"
#include "Cryptography/X25519.h"
#include "Cryptography/Fernet.h"


#include <memory>
#include <string>

namespace RNS {

	class Destination;
	class Packet;

	class Identity {

	public:
		Identity(Type::NoneConstructor none) {
			extreme("Identity NONE object created, this: " + std::to_string((uintptr_t)this) + ", data: " + std::to_string((uintptr_t)_object.get()));
		}
		Identity(const Identity &identity) : _object(identity._object) {
			extreme("Identity object copy created, this: " + std::to_string((uintptr_t)this) + ", data: " + std::to_string((uintptr_t)_object.get()));
		}
		Identity(bool create_keys = true);
		virtual ~Identity() {
			extreme("Identity object destroyed, this: " + std::to_string((uintptr_t)this) + ", data: " + std::to_string((uintptr_t)_object.get()));
		}

		inline Identity& operator = (const Identity &identity) {
			_object = identity._object;
			extreme("Identity object copy created by assignment, this: " + std::to_string((uintptr_t)this) + ", data: " + std::to_string((uintptr_t)_object.get()));
			return *this;
		}
		inline operator bool() const {
			return _object.get() != nullptr;
		}
		inline bool operator < (const Identity &identity) const {
			return _object.get() < identity._object.get();
		}

	public:
		void createKeys();
		/*
		:returns: The public key as *bytes*
		*/
		inline const Bytes get_public_key() {
			assert(_object);
			return _object->_pub_bytes + _object->_sig_pub_bytes;
		}
		inline void update_hashes() {
			assert(_object);
			_object->_hash = truncated_hash(get_public_key());
			debug("Identity::update_hashes: hash:       " + _object->_hash.toHex());
			_object->_hexhash = _object->_hash.toHex();
		};

		/*
		Get a SHA-256 hash of passed data.

		:param data: Data to be hashed as *bytes*.
		:returns: SHA-256 hash as *bytes*
		*/
		static inline const Bytes full_hash(const Bytes &data) {
			return Cryptography::sha256(data);
		}
		/*
		Get a truncated SHA-256 hash of passed data.

		:param data: Data to be hashed as *bytes*.
		:returns: Truncated SHA-256 hash as *bytes*
		*/
		static inline const Bytes truncated_hash(const Bytes &data) {
			//return Identity.full_hash(data)[:(Identity.TRUNCATED_HASHLENGTH//8)]
			return full_hash(data).left(Type::Identity::TRUNCATED_HASHLENGTH/8);
		}
		/*
		Get a random SHA-256 hash.

		:param data: Data to be hashed as *bytes*.
		:returns: Truncated SHA-256 hash of random data as *bytes*
		*/
		static inline const Bytes get_random_hash() {
			return truncated_hash(Cryptography::random(Type::Identity::TRUNCATED_HASHLENGTH/8));
		}

		static bool validate_announce(const Packet &packet);

		inline const Bytes get_salt() { assert(_object); return _object->_hash; }
		inline const Bytes get_context() { return {Bytes::NONE}; }

		const Bytes encrypt(const Bytes &plaintext);
		const Bytes decrypt(const Bytes &ciphertext_token);
		const Bytes sign(const Bytes &message);
		bool validate(const Bytes &signature, const Bytes &message);
		// CBA following default for reference value requires inclusiion of header
		//void prove(const Packet &packet, const Destination &destination = {Type::NONE});
		void prove(const Packet &packet, const Destination &destination);
		void prove(const Packet &packet);

		// getters/setters
		inline const Bytes &encryptionPrivateKey() const { assert(_object); return _object->_prv_bytes; }
		inline const Bytes &signingPrivateKey() const { assert(_object); return _object->_sig_prv_bytes; }
		inline const Bytes &encryptionPublicKey() const { assert(_object); return _object->_prv_bytes; }
		inline const Bytes &signingPublicKey() const { assert(_object); return _object->_sig_prv_bytes; }
		inline const Bytes &hash() const { assert(_object); return _object->_hash; }
		inline std::string hexhash() const { assert(_object); return _object->_hexhash; }

		inline std::string toString() const { assert(_object); return "{Identity:" + _object->_hash.toHex() + "}"; }

	private:
		class Object {
		public:
			Object() { extreme("Identity::Data object created, this: " + std::to_string((uintptr_t)this)); }
			virtual ~Object() { extreme("Identity::Data object destroyed, this: " + std::to_string((uintptr_t)this)); }
		private:

			RNS::Cryptography::X25519PrivateKey::Ptr _prv;
			Bytes _prv_bytes;

			RNS::Cryptography::Ed25519PrivateKey::Ptr _sig_prv;
			Bytes _sig_prv_bytes;

			RNS::Cryptography::X25519PublicKey::Ptr _pub;
			Bytes _pub_bytes;

			RNS::Cryptography::Ed25519PublicKey::Ptr _sig_pub;
			Bytes _sig_pub_bytes;

			Bytes _hash;
			std::string _hexhash;

		friend class Identity;
		};
		std::shared_ptr<Object> _object;

	};

}