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
