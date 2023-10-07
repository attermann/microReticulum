#pragma once

#include "Reticulum.h"
// CBA TODO determine why including Destination.h here causes build errors
//#include "Destination.h"
#include "Log.h"
#include "Bytes.h"
#include "Cryptography/Fernet.h"
#include "Cryptography/X25519.h"
#include "Cryptography/Ed25519.h"


#include <memory>
#include <string>

namespace RNS {

	class Destination;
	class Packet;

	class Identity {

	public:
		enum NoneConstructor {
			NONE
		};

		//static const char CURVE[] = "Curve25519";
		static constexpr const char* CURVE = "Curve25519";
		// The curve used for Elliptic Curve DH key exchanges

		static const uint16_t KEYSIZE     = 256*2;
		// X25519 key size in bits. A complete key is the concatenation of a 256 bit encryption key, and a 256 bit signing key.

		// Non-configurable constants
		static const uint8_t FERNET_OVERHEAD           = RNS::Cryptography::Fernet::FERNET_OVERHEAD;
		static const uint8_t AES128_BLOCKSIZE           = 16;          // In bytes
		static const uint16_t HASHLENGTH                = Reticulum::HASHLENGTH;	// In bits
		static const uint16_t SIGLENGTH                 = KEYSIZE;     // In bits

		static const uint8_t NAME_HASH_LENGTH     = 80;
		static const uint16_t TRUNCATED_HASHLENGTH = Reticulum::TRUNCATED_HASHLENGTH;	// In bits
		// Constant specifying the truncated hash length (in bits) used by Reticulum
		// for addressable hashes and other purposes. Non-configurable.

	public:
		Identity(NoneConstructor none) {
			extreme("Identity NONE object created, this: " + std::to_string((ulong)this) + ", data: " + std::to_string((ulong)_object.get()));
		}
		Identity(const Identity &identity) : _object(identity._object) {
			extreme("Identity object copy created, this: " + std::to_string((ulong)this) + ", data: " + std::to_string((ulong)_object.get()));
		}
		Identity(bool create_keys = true);
		~Identity() {
			extreme("Identity object destroyed, this: " + std::to_string((ulong)this) + ", data: " + std::to_string((ulong)_object.get()));
		}

		inline Identity& operator = (const Identity &identity) {
			_object = identity._object;
			extreme("Identity object copy created by assignment, this: " + std::to_string((ulong)this) + ", data: " + std::to_string((uint32_t)_object.get()));
			return *this;
		}
		inline operator bool() const {
			return _object.get() != nullptr;
		}

	public:
		void createKeys();
		Bytes get_public_key();
		void update_hashes();

		static Bytes full_hash(const Bytes &data);
		static Bytes truncated_hash(const Bytes &data);

		Bytes encrypt(const Bytes &plaintext);
		Bytes decrypt(const Bytes &ciphertext_token);
		Bytes sign(const Bytes &message);
		bool validate(const Bytes &signature, const Bytes &message);
		//void prove(const Packet &packet, const Destination &destination = Destination::NONE);
		void prove(const Packet &packet, const Destination &destination);

		// getters/setters
		inline Bytes encryptionPrivateKey() const { assert(_object); return _object->_prv_bytes; }
		inline Bytes signingPrivateKey() const { assert(_object); return _object->_sig_prv_bytes; }
		inline Bytes encryptionPublicKey() const { assert(_object); return _object->_prv_bytes; }
		inline Bytes signingPublicKey() const { assert(_object); return _object->_sig_prv_bytes; }
		inline Bytes hash() const { assert(_object); return _object->_hash; }
		inline std::string hexhash() const { assert(_object); return _object->_hexhash; }

		inline std::string toString() const { assert(_object); return _object->_hash.toHex(); }

	private:
		class Object {
		public:
			Object() { extreme("Identity::Data object created, this: " + std::to_string((ulong)this)); }
			~Object() { extreme("Identity::Data object destroyed, this: " + std::to_string((ulong)this)); }
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