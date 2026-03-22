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

#include "Random.h"
#include "../Bytes.h"

#include <stdint.h>

namespace RNS { namespace Cryptography {

    /*
    This class provides a slightly modified implementation of the Fernet spec
    found at: https://github.com/fernet/spec/blob/master/Spec.md

    According to the spec, a Fernet token includes a one byte VERSION and
    eight byte TIMESTAMP field at the start of each token. These fields are
    not relevant to Reticulum. They are therefore stripped from this
    implementation, since they incur overhead and leak initiator metadata.
    */
	class Fernet {

	public:
		using Ptr = std::shared_ptr<Fernet>;

	public:
		static inline const Bytes generate_key() { return random(32); }

	public:
		Fernet(const Bytes& key);
		~Fernet();

	public:
		bool verify_hmac(const Bytes& token);
		const Bytes encrypt(const Bytes& data);
		const Bytes decrypt(const Bytes& token);

	private:
		Bytes _signing_key;
		Bytes _encryption_key;
	};

} }
