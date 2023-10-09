#pragma once

#include "Bytes.h"
#include "Log.h"

#include <Curve25519.h>

#include <memory>
#include <stdexcept>
#include <stdint.h>

namespace RNS { namespace Cryptography {

	class X25519PublicKey {

	public:
/*
		X25519PublicKey(const Bytes &x) {
			_x = x;
		}
*/
		X25519PublicKey(const Bytes &publicKey) {
			_publicKey = publicKey;
		}
		~X25519PublicKey() {}

		using Ptr = std::shared_ptr<X25519PublicKey>;

	public:
		// creates a new instance with specified seed
/*
		static inline Ptr from_public_bytes(const Bytes &data) {
			return Ptr(new X25519PublicKey(_unpack_number(data)));
		}
*/
		static inline Ptr from_public_bytes(const Bytes &publicKey) {
			return Ptr(new X25519PublicKey(publicKey));
		}

/*
		Bytes public_bytes() {
			return _pack_number(_x);
		}
*/
		Bytes public_bytes() {
			return _publicKey;
		}

	private:
		//Bytes _x;
		Bytes _publicKey;

	};

	class X25519PrivateKey {

	public:
		const float MIN_EXEC_TIME = 0.002;
		const float MAX_EXEC_TIME = 0.5;
		const uint8_t DELAY_WINDOW = 10;

		//zT_CLEAR = None
		const uint8_t T_MAX = 0;

	public:
/*
		X25519PrivateKey(const Bytes &a) {
			_a = a;
		}
*/
		X25519PrivateKey(const Bytes &privateKey) {
			if (privateKey) {
				// use specified private key
				_privateKey = privateKey;
				// similar to derive public key from private key
				// second param "f" is secret
				//eval(uint8_t result[32], const uint8_t s[32], const uint8_t x[32])
				// derive public key from private key
				Curve25519::eval(_publicKey.writable(32), _privateKey.data(), 0);
			}
			else {
				// create random private key and derive public key
				// second param "f" is secret
				//dh1(uint8_t k[32], uint8_t f[32])
				Curve25519::dh1(_publicKey.writable(32), _privateKey.writable(32));
			}
		}
		~X25519PrivateKey() {}

		using Ptr = std::shared_ptr<X25519PrivateKey>;

	public:
		// creates a new instance with a random seed
/*
		static inline Ptr generate() {
			return from_private_bytes(os.urandom(32));
		}
*/
		static inline Ptr generate() {
			return from_private_bytes(Bytes::NONE);
		}

		// creates a new instance with specified seed
/*
		static inline Ptr from_private_bytes(const Bytes &data) {
			return Ptr(new X25519PrivateKey(_fix_secret(_unpack_number(data))));
		}
*/
		static inline Ptr from_private_bytes(const Bytes &privateKey) {
			return Ptr(new X25519PrivateKey(privateKey));
		}

/*
		inline Bytes private_bytes() {
			return _pack_number(_a);
		}
*/
		inline Bytes private_bytes() {
			return _privateKey;
		}

		// creates a new instance of public key for this private key
/*
		inline X25519PublicKey::Ptr public_key() {
			return X25519PublicKey::from_public_bytes(_pack_number(_raw_curve25519(9, _a)));
		}
*/
		inline X25519PublicKey::Ptr public_key() {
			return X25519PublicKey::from_public_bytes(_publicKey);
		}

/*
		inline Bytes exchange(const Bytes &peer_public_key) {
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
		}
*/
		inline Bytes exchange(const Bytes &peer_public_key) {
			debug("X25519PublicKey::exchange: public key:       " + _publicKey.toHex());
			debug("X25519PublicKey::exchange: peer public key:  " + peer_public_key.toHex());
			debug("X25519PublicKey::exchange: pre private key:  " + _privateKey.toHex());
			Bytes sharedKey;
			if (!Curve25519::eval(sharedKey.writable(32), _privateKey.data(), peer_public_key.data())) {
				throw std::runtime_error("Peer key is invalid");
			}
			debug("X25519PublicKey::exchange: shared key:       " + sharedKey.toHex());
			debug("X25519PublicKey::exchange: post private key: " + _privateKey.toHex());
			return sharedKey;
		}

		inline bool verify(const Bytes &peer_public_key) {
			debug("X25519PublicKey::exchange: public key:       " + _publicKey.toHex());
			debug("X25519PublicKey::exchange: peer public key:  " + peer_public_key.toHex());
			debug("X25519PublicKey::exchange: pre private key:  " + _privateKey.toHex());
			Bytes sharedKey(peer_public_key);
			bool success = Curve25519::dh2(sharedKey.writable(32), _privateKey.writable(32));
			debug("X25519PublicKey::exchange: shared key:       " + sharedKey.toHex());
			debug("X25519PublicKey::exchange: post private key: " + _privateKey.toHex());
			return success;
		}

	private:
		//Bytes _a;
		Bytes _privateKey;
		Bytes _publicKey;

	};

} }
