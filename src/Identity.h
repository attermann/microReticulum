#pragma once

#include "Log.h"
#include "Bytes.h"
#include "Type.h"
#include "Cryptography/Hashes.h"
#include "Cryptography/Ed25519.h"
#include "Cryptography/X25519.h"
#include "Cryptography/Fernet.h"

#include <map>
#include <string>
#include <memory>
#include <cassert>

namespace RNS {

	class Destination;
	class Packet;

	class Identity {

	private:
		class IdentityEntry {
		public:
			IdentityEntry(double timestamp, const Bytes& packet_hash, const Bytes& public_key, const Bytes& app_data) :
				_timestamp(timestamp),
				_packet_hash(packet_hash),
				_public_key(public_key),
				_app_data(app_data)
			{
			}
		public:
			double _timestamp = 0;
			Bytes _packet_hash;
			Bytes _public_key;
			Bytes _app_data;
		};

	public:
		static std::map<Bytes, IdentityEntry> _known_destinations;
		static bool _saving_known_destinations;

	public:
		Identity(Type::NoneConstructor none) {
			mem("Identity NONE object created, this: " + std::to_string((uintptr_t)this) + ", data: " + std::to_string((uintptr_t)_object.get()));
		}
		Identity(const Identity& identity) : _object(identity._object) {
			mem("Identity object copy created, this: " + std::to_string((uintptr_t)this) + ", data: " + std::to_string((uintptr_t)_object.get()));
		}
		Identity(bool create_keys = true);
		virtual ~Identity() {
			mem("Identity object destroyed, this: " + std::to_string((uintptr_t)this) + ", data: " + std::to_string((uintptr_t)_object.get()));
		}

		inline Identity& operator = (const Identity& identity) {
			_object = identity._object;
			mem("Identity object copy created by assignment, this: " + std::to_string((uintptr_t)this) + ", data: " + std::to_string((uintptr_t)_object.get()));
			return *this;
		}
		inline operator bool() const {
			return _object.get() != nullptr;
		}
		inline bool operator < (const Identity& identity) const {
			return _object.get() < identity._object.get();
		}

	public:
		void createKeys();

		/*
		:returns: The private key as *bytes*
		*/
		inline const Bytes get_private_key() const {
			assert(_object);
			return _object->_prv_bytes + _object->_sig_prv_bytes;
		}
		/*
		:returns: The public key as *bytes*
		*/
		inline const Bytes get_public_key() const {
			assert(_object);
			return _object->_pub_bytes + _object->_sig_pub_bytes;
		}
		bool load_private_key(const Bytes& prv_bytes);
		void load_public_key(const Bytes& pub_bytes);
		inline void update_hashes() {
			assert(_object);
			_object->_hash = truncated_hash(get_public_key());
			extreme("Identity::update_hashes: hash:       " + _object->_hash.toHex());
			_object->_hexhash = _object->_hash.toHex();
		};
		bool load(const char* path);
		bool to_file(const char* path);

		inline const Bytes get_salt() const { assert(_object); return _object->_hash; }
		inline const Bytes get_context() const { return {Bytes::NONE}; }

		const Bytes encrypt(const Bytes& plaintext) const;
		const Bytes decrypt(const Bytes& ciphertext_token) const;
		const Bytes sign(const Bytes& message) const;
		bool validate(const Bytes& signature, const Bytes& message) const;
		// CBA following default for reference value requires inclusiion of header
		//void prove(const Packet& packet, const Destination& destination = {Type::NONE}) const;
		void prove(const Packet& packet, const Destination& destination) const;
		void prove(const Packet& packet) const;

		static const Identity from_file(const char* path);
		static void remember(const Bytes& packet_hash, const Bytes& destination_hash, const Bytes& public_key, const Bytes& app_data = {Bytes::NONE});
		static Identity recall(const Bytes& destination_hash);
		static Bytes recall_app_data(const Bytes& destination_hash);
		static bool save_known_destinations();
		static void load_known_destinations();
		/*
		Get a SHA-256 hash of passed data.

		:param data: Data to be hashed as *bytes*.
		:returns: SHA-256 hash as *bytes*
		*/
		static inline const Bytes full_hash(const Bytes& data) {
			return Cryptography::sha256(data);
		}
		/*
		Get a truncated SHA-256 hash of passed data.

		:param data: Data to be hashed as *bytes*.
		:returns: Truncated SHA-256 hash as *bytes*
		*/
		static inline const Bytes truncated_hash(const Bytes& data) {
			//p return Identity.full_hash(data)[:(Identity.TRUNCATED_HASHLENGTH//8)]
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

		static bool validate_announce(const Packet& packet);

		// getters/setters
		inline const Bytes& encryptionPrivateKey() const { assert(_object); return _object->_prv_bytes; }
		inline const Bytes& signingPrivateKey() const { assert(_object); return _object->_sig_prv_bytes; }
		inline const Bytes& encryptionPublicKey() const { assert(_object); return _object->_prv_bytes; }
		inline const Bytes& signingPublicKey() const { assert(_object); return _object->_sig_prv_bytes; }
		inline const Bytes& hash() const { assert(_object); return _object->_hash; }
		inline std::string hexhash() const { assert(_object); return _object->_hexhash; }
		inline const Bytes& app_data() const { assert(_object); return _object->_app_data; }
		inline void app_data(const Bytes& app_data) { assert(_object); _object->_app_data = app_data; }
		inline const Cryptography::X25519PrivateKey::Ptr prv() const { assert(_object); return _object->_prv; }
		inline const Cryptography::X25519PublicKey::Ptr pub() const { assert(_object); return _object->_pub; }

		inline std::string toString() const { if (!_object) return ""; return "{Identity:" + _object->_hash.toHex() + "}"; }

	private:
		class Object {
		public:
			Object() { mem("Identity::Data object created, this: " + std::to_string((uintptr_t)this)); }
			virtual ~Object() { mem("Identity::Data object destroyed, this: " + std::to_string((uintptr_t)this)); }
		private:

			Cryptography::X25519PrivateKey::Ptr _prv;
			Bytes _prv_bytes;

			Cryptography::Ed25519PrivateKey::Ptr _sig_prv;
			Bytes _sig_prv_bytes;

			Cryptography::X25519PublicKey::Ptr _pub;
			Bytes _pub_bytes;

			Cryptography::Ed25519PublicKey::Ptr _sig_pub;
			Bytes _sig_pub_bytes;

			Bytes _hash;
			std::string _hexhash;

			Bytes _app_data;

		friend class Identity;
		};
		std::shared_ptr<Object> _object;

	};

}