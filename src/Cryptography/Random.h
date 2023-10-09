#pragma once

#include "../Bytes.h"

#include <RNG.h>
#include <stdint.h>

namespace RNS { namespace Cryptography {

	inline Bytes random(size_t length) {
        Bytes rand;
        RNG.rand(rand.writable(length), length);
        return rand;
    }

} }
