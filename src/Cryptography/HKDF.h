#pragma once

#include "../Bytes.h"

namespace RNS { namespace Cryptography {

	const Bytes hkdf(size_t length, const Bytes &derive_from, const Bytes &salt = {Bytes::NONE}, const Bytes &context = {Bytes::NONE});

} }
