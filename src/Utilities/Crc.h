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
