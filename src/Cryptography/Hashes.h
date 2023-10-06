#pragma once

#include <stdint.h>

namespace RNS { namespace Cryptography {

	void sha256(uint8_t *hash, const uint8_t *data, uint16_t data_len);
	void sha512(uint8_t *hash, const uint8_t *data, uint16_t data_len);

} }
