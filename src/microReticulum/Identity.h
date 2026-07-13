/*
 * Copyright (c) 2023 Chad Attermann
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */

#pragma once

#include "Log.h"
#include "Bytes.h"
#include "Type.h"
#include "Cryptography/Hashes.h"
#include "Cryptography/Ed25519.h"
#include "Cryptography/X25519.h"
#include "Cryptography/Token.h"
#include "Utilities/Memory.h"
#include "Persistence/IdentityEntry.h"

#include <map>
#include <string>
#include <memory>
#include <cassert>

namespace RNS {

	class Destination;
	class Packet;

	class Identity {

	public:
		using IdentityEntry = Persistence::IdentityEntry;

	private:
		static Persistence::KnownStore _known_store;
		static Persistence::KnownDestinations _known_destinations;
		// CBA
		static uint16_t _known_destinations_maxsize;
		static uint32_t _known_store_segment_size;
		static uint8_t _known_store_segment_count;

	public:
		Identity(bool create_keys = true);
		Identity(Type::NoneConstructor none) {
			MEMF("Identity NONE object created, this: %p, data: %p", (void*)this, (void*)_object.get());
		}
		Identity(const Identity& identity) : _object(identity._object) {
			MEMF("Identity object copy created, this: %p, data: %p", (void*)this, (void*)_object.get());
		}
		virtual ~Identity() {
			MEMF("Identity object destroyed, this: %p, data: %p", (void*)this, (void*)_object.get());
		}

		inline Identity& operator = (const Identity& identity) {
			_object = identity._object;
			MEMF("Identity object copy created by assignment, this: %p, data: %p", (void*)this, (void*)_object.get());
			return *this;
		}
		inline explicit operator bool() const {
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
			TRACEF("Identity::update_hashes: hash: %s", _object->_hash.toHex().c_str());
			_object->_hexhash = _object->_hash.toHex();
		};
		bool load(const char* path);
		bool to_file(const char* path);

		inline const Bytes& get_salt() const { assert(_object); return _object->_hash; }
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

		static bool validate_announce(const Packet& packet, bool only_validate_signature = false);

		// getters/setters
		inline const Bytes& encryptionPrivateKey() const { assert(_object); return _object->_prv_bytes; }
		inline const Bytes& signingPrivateKey() const { assert(_object); return _object->_sig_prv_bytes; }
		inline const Bytes& encryptionPublicKey() const { assert(_object); return _object->_pub_bytes; }
		inline const Bytes& signingPublicKey() const { assert(_object); return _object->_sig_pub_bytes; }
		inline const Bytes& hash() const { assert(_object); return _object->_hash; }
		inline std::string hexhash() const { assert(_object); return _object->_hexhash; }
		inline const Bytes& app_data() const { assert(_object); return _object->_app_data; }
		inline void app_data(const Bytes& app_data) { assert(_object); _object->_app_data = app_data; }
		inline const Cryptography::X25519PrivateKey::Ptr prv() const { assert(_object); return _object->_prv; }
		inline const Cryptography::Ed25519PrivateKey::Ptr sig_prv() const { assert(_object); return _object->_sig_prv; }
		inline const Cryptography::X25519PublicKey::Ptr pub() const { assert(_object); return _object->_pub; }
		inline const Cryptography::Ed25519PublicKey::Ptr sig_pub() const { assert(_object); return _object->_sig_pub; }
		inline static uint16_t known_destinations_maxsize() { return _known_destinations_maxsize; }
		inline static void known_destinations_maxsize(uint16_t known_destinations_maxsize) { _known_destinations_maxsize = known_destinations_maxsize; _known_store.set_max_recs(_known_destinations_maxsize); }
		inline static uint32_t known_store_segment_size() { return _known_store_segment_size; }
		inline static void known_store_segment_size(uint32_t value) { _known_store_segment_size = value; }
		inline static uint8_t known_store_segment_count() { return _known_store_segment_count; }
		inline static void known_store_segment_count(uint8_t value) { _known_store_segment_count = value; }

		inline static const Persistence::KnownDestinations& known_destinations() { return _known_destinations; }

		inline std::string toString() const { if (!_object) return ""; return "{Identity:" + _object->_hash.toHex() + "}"; }

	private:
		class Object {
		public:
			Object() { MEMF("Identity::Data object created, this: %p", (void*)this); }
			virtual ~Object() { MEMF("Identity::Data object destroyed, this: %p", (void*)this); }
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

	// CBA For access to private static members by Transport class
	friend class Transport;
	};

}