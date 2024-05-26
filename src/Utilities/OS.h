#pragma once

#include "../Filesystem.h"
#include "../Bytes.h"

#include <cmath>
#include <unistd.h>
#include <time.h>
#include <stdint.h>
#include <sys/time.h>

#ifdef ARDUINO
#include <Arduino.h>
#endif

#undef round

namespace RNS { namespace Utilities {

	class OS {

	private:
		static Filesystem filesystem;

	public:
        // sleep for specified milliseconds
		//static inline void sleep(float seconds) { ::sleep(seconds); }
#ifdef ARDUINO
		static inline void sleep(float seconds) { delay((uint32_t)(seconds * 1000)); }
#else
		static inline void sleep(float seconds) { timespec time; time.tv_sec = (time_t)(seconds); time.tv_nsec = (seconds - (float)time.tv_sec) * 1000000000; ::nanosleep(&time, nullptr); }
#endif
		//static inline void sleep(uint32_t milliseconds) { ::sleep((float)milliseconds / 1000.0); }

        // return current time in milliseconds since 00:00:00, January 1, 1970 (Unix Epoch)
		static inline uint64_t ltime() { timeval time; ::gettimeofday(&time, NULL); return (uint64_t)(time.tv_sec * 1000) + (uint64_t)(time.tv_usec / 1000); }

        // return current time in float seconds since 00:00:00, January 1, 1970 (Unix Epoch)
		static inline double time() { timeval time; ::gettimeofday(&time, NULL); return (double)time.tv_sec + ((double)time.tv_usec / 1000000); }

		// round decimal number to specified precision
		//static inline float round(float value, uint8_t precision) { return std::round(value / precision) * precision; }
		//static inline double round(double value, uint8_t precision) { return std::round(value / precision) * precision; }
		static inline double round(double value, uint8_t precision) { return std::round(value / precision) * precision; }

		static void register_filesystem(Filesystem& filesystem);
		static void deregister_filesystem();

		static bool file_exists(const char* file_path);
		static const RNS::Bytes read_file(const char* file_path);
		static bool write_file(const RNS::Bytes& data, const char* file_path);
		static bool remove_file(const char* file_path);
		static bool rename_file(const char* from_file_path, const char* to_file_path);
		static bool create_directory(const char* directory_path);

    };

} }
