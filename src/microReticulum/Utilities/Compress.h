/*
 * Copyright (c) 2026 Chad Attermann
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

#include "../Bytes.h"

#include <stddef.h>
#include <stdint.h>

namespace RNS { namespace Utilities {

	// Thin C++ wrapper over the heatshrink C library. Configured for static
	// allocation (HEATSHRINK_DYNAMIC_ALLOC=0) via platformio.ini build flags
	// so no malloc happens inside heatshrink itself. The window/lookahead
	// constants must match between firmware and any cooperating client (e.g.
	// the JS decoder embedded in the web console).
	class Compress {

	public:
		// Heatshrink-encode 'in_data'. Returns the compressed bytes on
		// success. Returns an empty Bytes on encoder error or when the
		// compressed output would be at least as large as the input — the
		// caller is expected to fall back to the uncompressed payload in
		// that case, since wrapping unprofitable output only grows the wire.
		static Bytes encode(const uint8_t* in_data, size_t in_len);

		// Heatshrink-decode 'in_data'. 'max_out' caps the output buffer
		// growth as a defensive bound against pathological / hostile
		// streams; returns an empty Bytes if decoding fails or if the
		// decoded size would exceed 'max_out'.
		static Bytes decode(const uint8_t* in_data, size_t in_len, size_t max_out);

	};

} }
