#pragma once

#include "../Bytes.h"

#include <RNG.h>
#include <stdint.h>

namespace RNS { namespace Cryptography {

    // return vector specified length of random bytes
	inline const Bytes random(size_t length) {
        Bytes rand;
        RNG.rand(rand.writable(length), length);
        return rand;
    }

    // return 32 bit random unigned int
    inline uint32_t randomnum() {
        Bytes rand;
        RNG.rand(rand.writable(4), 4);
        uint32_t randnum = uint32_t((unsigned char)(rand.data()[0]) << 24 |
                                    (unsigned char)(rand.data()[0]) << 16 |
                                    (unsigned char)(rand.data()[0]) << 8 |
                                    (unsigned char)(rand.data()[0]));
        return randnum;
    }

    // return 32 bit random unigned int between 0 and specified value
    inline uint32_t randomnum(uint32_t max) {
        return randomnum() % max;
    }

    // return random float value from 0 to 1
    inline float random() {
        return (float)(randomnum() / (float)0xffffffff);
    }

} }
