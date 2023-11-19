#pragma once

#include <cmath>
#include <unistd.h>
#include <time.h>
#include <stdint.h>
#include <sys/time.h>

namespace RNS { namespace Utilities {

	class OS {

	public:
        // sleep for specified milliseconds
		static inline void sleep(float seconds) { ::sleep(seconds); }
		//static inline void sleep(uint32_t milliseconds) { ::sleep((float)milliseconds / 1000.0); }
        // return current time in milliseconds since 00:00:00, January 1, 1970 (Unix Epoch)
		static uint64_t time() { timeval time; ::gettimeofday(&time, NULL); return (uint64_t)(time.tv_sec * 1000) + (uint64_t)(time.tv_usec / 1000); }
        static inline float round(float value, uint8_t precision) { return std::round(value / precision) * precision; }

    };

} }
