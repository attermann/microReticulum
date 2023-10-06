#pragma once

#include <stdint.h>

namespace RNS { namespace Cryptography {

	class Fernet {

	public:
		static const uint8_t FERNET_OVERHEAD  = 48; // Bytes

	public:
		Fernet();
		~Fernet();

	};

} }
