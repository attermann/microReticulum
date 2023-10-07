#pragma once

#include "../Bytes.h"

#include <stdint.h>

namespace RNS { namespace Cryptography {

	Bytes sha256(const Bytes &data);
	Bytes sha512(const Bytes &data);

} }
