#pragma once

#include "Bytes.h"

#include <memory>
#include <stdint.h>

namespace RNS { namespace Cryptography {

	class X25519PublicKey {

	public:
		X25519PublicKey(const Bytes &x) {
			_x = x;
		}
		~X25519PublicKey() {}

		using Ptr = std::shared_ptr<X25519PublicKey>;

	public:
		// creates a new instance with specified seed
		static inline Ptr from_public_bytes(const Bytes &data) {
			//return Ptr(new X25519PublicKey(_unpack_number(data)));
			// MOCK
			return Ptr(new X25519PublicKey(nullptr));
		}

		Bytes public_bytes() {
			//return _pack_number(_x);
			// MOCK
			return nullptr;
		}

	private:
		Bytes _x;

	};

	class X25519PrivateKey {

	public:
		const float MIN_EXEC_TIME = 0.002;
		const float MAX_EXEC_TIME = 0.5;
		const uint8_t DELAY_WINDOW = 10;

		//zT_CLEAR = None
		const uint8_t T_MAX = 0;

	public:
		X25519PrivateKey(const Bytes &a) {
			_a = a;
		}
		~X25519PrivateKey() {}

		using Ptr = std::shared_ptr<X25519PrivateKey>;

	public:
		// creates a new instance with a random seed
		static inline Ptr generate() {
			//return from_private_bytes(os.urandom(32));
			// MOCK
			return from_private_bytes(nullptr);
		}

		// creates a new instance with specified seed
		static inline Ptr from_private_bytes(const Bytes &data) {
			//return Ptr(new X25519PrivateKey(_fix_secret(_unpack_number(data))));
			// MOCK
			return Ptr(new X25519PrivateKey(nullptr));
		}

		inline Bytes private_bytes() {
			//return _pack_number(_a);
			// MOCK
			return nullptr;
		}

		// creates a new instance of public key for this private key
		inline X25519PublicKey::Ptr public_key() {
			//return X25519PublicKey::from_public_bytes(_pack_number(_raw_curve25519(9, _a)));
			// MOCK
			return X25519PublicKey::from_public_bytes(nullptr);
		}

		inline Bytes exchange(const Bytes &peer_public_key) {
/*
			if isinstance(peer_public_key, bytes):
				peer_public_key = X25519PublicKey.from_public_bytes(peer_public_key)

			start = time.time()
			
			shared = _pack_number(_raw_curve25519(peer_public_key.x, _a))
			
			end = time.time()
			duration = end-start

			if X25519PrivateKey.T_CLEAR == None:
				X25519PrivateKey.T_CLEAR = end + X25519PrivateKey.DELAY_WINDOW

			if end > X25519PrivateKey.T_CLEAR:
				X25519PrivateKey.T_CLEAR = end + X25519PrivateKey.DELAY_WINDOW
				X25519PrivateKey.T_MAX = 0
			
			if duration < X25519PrivateKey.T_MAX or duration < X25519PrivateKey.MIN_EXEC_TIME:
				target = start+X25519PrivateKey.T_MAX

				if target > start+X25519PrivateKey.MAX_EXEC_TIME:
					target = start+X25519PrivateKey.MAX_EXEC_TIME

				if target < start+X25519PrivateKey.MIN_EXEC_TIME:
					target = start+X25519PrivateKey.MIN_EXEC_TIME

				try:
					time.sleep(target-time.time())
				except Exception as e:
					pass

			elif duration > X25519PrivateKey.T_MAX:
				X25519PrivateKey.T_MAX = duration

			return shared
*/
			return nullptr;
		}

	private:
		Bytes _a;

	};

} }
