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

#include <stdint.h>
#include <string.h>

namespace RNS { namespace Utilities {

	class Crc {

	public:
 		static uint32_t crc32(uint32_t crc, const uint8_t* buffer, size_t size);
 		static inline uint32_t crc32(uint32_t crc, uint8_t byte) { return crc32(crc, &byte, sizeof(byte)); }
 		static inline uint32_t crc32(uint32_t crc, const char* str) { return crc32(crc, (const uint8_t*)str, strlen(str)); }

	};

} }
