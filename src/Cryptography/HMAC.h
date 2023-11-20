#pragma once

#include "../Bytes.h"

#include <Hash.h>
#include <SHA256.h>
#include <SHA512.h>
#include <stdexcept>
#include <memory>

namespace RNS { namespace Cryptography {

    class HMAC {

    public:
		enum Digest {
			DIGEST_NONE,
			DIGEST_SHA256,
			DIGEST_SHA512,
		};

		using Ptr = std::shared_ptr<HMAC>;

	public:
		/*
		Create a new HMAC object.
		key: bytes or buffer, key for the keyed hash object.
		msg: bytes or buffer, Initial input for the hash or None.
		digest: The underlying hash algorithm to use
		*/
		HMAC(const Bytes &key, const Bytes &msg = {Bytes::NONE}, Digest digest = DIGEST_SHA256) {

			if (digest == DIGEST_NONE) {
				throw std::invalid_argument("Cannot derive key from empty input material");
			}

			switch (digest) {
			case DIGEST_SHA256:
				_hash = std::unique_ptr<Hash>(new SHA256());
				break;
			case DIGEST_SHA512:
				_hash = std::unique_ptr<Hash>(new SHA512());
				break;
			default:
				throw std::invalid_argument("Unknown ior unsuppored digest");
			}

			_key = key;
			_hash->resetHMAC(key.data(), key.size());

			if (msg) {
				update(msg);
			}
		}

		/*
		Feed data from msg into this hashing object.
		*/
		void update(const Bytes &msg) {
			assert(_hash);
			_hash->update(msg.data(), msg.size());
		}

		/*
		Return the hash value of this hashing object.
		This returns the hmac value as bytes.  The object is
		not altered in any way by this function; you can continue
		updating the object after calling this function.
		*/
		Bytes digest() {
			assert(_hash);
			Bytes result;
			_hash->finalizeHMAC(_key.data(), _key.size(), result.writable(_hash->hashSize()), _hash->hashSize());
			return result;
		}

		/*
		Create a new hashing object and return it.
		key: bytes or buffer, The starting key for the hash.
		msg: bytes or buffer, Initial input for the hash, or None.
		digest: The underlying hash algorithm to use.
		You can now feed arbitrary bytes into the object using its update()
		method, and can ask for the hash value at any time by calling its digest()
		method.
		*/
		static inline Ptr generate(const Bytes &key, const Bytes &msg = {Bytes::NONE}, Digest digest = DIGEST_SHA256) {
			return Ptr(new HMAC(key, msg, digest));
		}

	private:
		Bytes _key;
		std::unique_ptr<Hash> _hash;

	};

	/*
	Fast inline implementation of HMAC.
	key: bytes or buffer, The key for the keyed hash object.
	msg: bytes or buffer, Input message.
	digest: The underlying hash algorithm to use.
	*/
	inline const Bytes digest(const Bytes &key, const Bytes &msg, HMAC::Digest digest = HMAC::DIGEST_SHA256) {
		HMAC hmac(key, msg, digest);
		hmac.update(msg);
		return hmac.digest();
	}

} }
