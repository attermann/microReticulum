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

    // return 32 bit random unsigned int between 0 and specified value
    inline uint32_t randomnum(uint32_t max) {
        return randomnum() % max;
    }

    // return random float value from 0 to 1
    inline float random() {
        return (float)(randomnum() / (float)0xffffffff);
    }

} }
