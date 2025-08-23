#pragma once

#include "CBC.h"

#include "../Bytes.h"

#include <AES.h>

namespace RNS { namespace Cryptography {

	class AES_128_CBC {

	public:
		static inline const Bytes encrypt(const Bytes& plaintext, const Bytes& key, const Bytes& iv) {
			CBC<AES128> cbc;
			cbc.setKey(key.data(), key.size());
			cbc.setIV(iv.data(), iv.size());
			Bytes ciphertext;
			cbc.encrypt(ciphertext.writable(plaintext.size()), plaintext.data(), plaintext.size());
			return ciphertext;
		}

		static inline const Bytes decrypt(const Bytes& ciphertext, const Bytes& key, const Bytes& iv) {
			CBC<AES128> cbc;
			cbc.setKey(key.data(), key.size());
			cbc.setIV(iv.data(), iv.size());
			Bytes plaintext;
			cbc.decrypt(plaintext.writable(ciphertext.size()), ciphertext.data(), ciphertext.size());
			return plaintext;
		}

		// EXPERIMENTAL - overwrites passed buffer
		static inline void inplace_encrypt(Bytes& plaintext, const Bytes& key, const Bytes& iv) {
			CBC<AES128> cbc;
			cbc.setKey(key.data(), key.size());
			cbc.setIV(iv.data(), iv.size());
			cbc.encrypt((uint8_t*)plaintext.data(), plaintext.data(), plaintext.size());
		}

		// EXPERIMENTAL - overwrites passed buffer
		static inline void inplace_decrypt(Bytes& ciphertext, const Bytes& key, const Bytes& iv) {
			CBC<AES128> cbc;
			cbc.setKey(key.data(), key.size());
			cbc.setIV(iv.data(), iv.size());
			cbc.decrypt((uint8_t*)ciphertext.data(), ciphertext.data(), ciphertext.size());
		}

	};

	class AES_256_CBC {

	public:
		static inline const Bytes encrypt(const Bytes& plaintext, const Bytes& key, const Bytes& iv) {
			CBC<AES256> cbc;
			cbc.setKey(key.data(), key.size());
			cbc.setIV(iv.data(), iv.size());
			Bytes ciphertext;
			cbc.encrypt(ciphertext.writable(plaintext.size()), plaintext.data(), plaintext.size());
			return ciphertext;
		}

		static inline const Bytes decrypt(const Bytes& ciphertext, const Bytes& key, const Bytes& iv) {
			CBC<AES256> cbc;
			cbc.setKey(key.data(), key.size());
			cbc.setIV(iv.data(), iv.size());
			Bytes plaintext;
			cbc.decrypt(plaintext.writable(ciphertext.size()), ciphertext.data(), ciphertext.size());
			return plaintext;
		}

		// EXPERIMENTAL - overwrites passed buffer
		static inline void inplace_encrypt(Bytes& plaintext, const Bytes& key, const Bytes& iv) {
			CBC<AES256> cbc;
			cbc.setKey(key.data(), key.size());
			cbc.setIV(iv.data(), iv.size());
			cbc.encrypt((uint8_t*)plaintext.data(), plaintext.data(), plaintext.size());
		}

		// EXPERIMENTAL - overwrites passed buffer
		static inline void inplace_decrypt(Bytes& ciphertext, const Bytes& key, const Bytes& iv) {
			CBC<AES256> cbc;
			cbc.setKey(key.data(), key.size());
			cbc.setIV(iv.data(), iv.size());
			cbc.decrypt((uint8_t*)ciphertext.data(), ciphertext.data(), ciphertext.size());
		}

	};

} }
