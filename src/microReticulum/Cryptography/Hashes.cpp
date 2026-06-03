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

#include "Hashes.h"

#include "../Bytes.h"

#include <SHA256.h>
#include <SHA512.h>

using namespace RNS;

/*
The SHA primitives are abstracted here to allow platform-
aware hardware acceleration in the future. Currently only
uses Python's internal SHA-256 implementation. All SHA-256
calls in RNS end up here.
*/

const Bytes RNS::Cryptography::sha256(const Bytes& data) {
	//TRACEF("Cryptography::sha256: data: %s", data.toHex().c_str());
	SHA256 digest;
	digest.reset();
	digest.update(data.data(), data.size());
	Bytes hash;
	digest.finalize(hash.writable(32), 32);
	//TRACEF("Cryptography::sha256: hash: %s", hash.toHex().c_str());
	return hash;
}

const Bytes RNS::Cryptography::sha512(const Bytes& data) {
	SHA512 digest;
	digest.reset();
	digest.update(data.data(), data.size());
	Bytes hash;
	digest.finalize(hash.writable(64), 64);
	//TRACEF("Cryptography::sha512: hash: %s", hash.toHex().c_str());
	return hash;
}
