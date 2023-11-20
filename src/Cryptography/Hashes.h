#pragma once

#include "../Bytes.h"

#include <stdint.h>

namespace RNS { namespace Cryptography {

	const Bytes sha256(const Bytes& data);
	const Bytes sha512(const Bytes& data);

} }
