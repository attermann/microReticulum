#pragma once

#include "../FileSystem.h"
#include "../FileStream.h"
#include "../Bytes.h"

#include "tlsf.h"

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
		static FileSystem _filesystem;
		static uint64_t _time_offset;

	public:
		static tlsf_t _tlsf;

	public:
		static inline uint64_t getTimeOffset() { return _time_offset; }
		static inline void setTimeOffset(uint64_t offset) { _time_offset = offset; }

#ifdef ARDUINO
        // return current time in milliseconds since startup
		static inline uint64_t ltime() {
			// handle roll-over of 32-bit millis (approx. 49 days)
			static uint32_t low32, high32;
			uint32_t new_low32 = millis();
			if (new_low32 < low32) high32++;
			low32 = new_low32;
			return ((uint64_t)high32 << 32 | low32) + _time_offset;
		}
#else
        // return current time in milliseconds since 00:00:00, January 1, 1970 (Unix Epoch)
		static inline uint64_t ltime() { timeval time; ::gettimeofday(&time, NULL); return (uint64_t)(time.tv_sec * 1000) + (uint64_t)(time.tv_usec / 1000); }
#endif

#ifdef ARDUINO
        // return current time in float seconds since startup
		static inline double time() { return (double)(ltime() / 1000.0); }
#else
        // return current time in float seconds since 00:00:00, January 1, 1970 (Unix Epoch)
		static inline double time() { timeval time; ::gettimeofday(&time, NULL); return (double)time.tv_sec + ((double)time.tv_usec / 1000000); }
#endif

        // sleep for specified milliseconds
		//static inline void sleep(float seconds) { ::sleep(seconds); }
#ifdef ARDUINO
		static inline void sleep(float seconds) { delay((uint32_t)(seconds * 1000)); }
#else
		static inline void sleep(float seconds) { timespec time; time.tv_sec = (time_t)(seconds); time.tv_nsec = (seconds - (float)time.tv_sec) * 1000000000; ::nanosleep(&time, nullptr); }
#endif
		//static inline void sleep(uint32_t milliseconds) { ::sleep((float)milliseconds / 1000.0); }

		// round decimal number to specified precision
		//static inline float round(float value, uint8_t precision) { return std::round(value / precision) * precision; }
		//static inline double round(double value, uint8_t precision) { return std::round(value / precision) * precision; }
		static inline double round(double value, uint8_t precision) { return std::round(value / precision) * precision; }

#if defined(RNS_USE_ALLOCATOR)
		static void dump_allocator_stats();
#endif

		inline static void register_filesystem(FileSystem& filesystem) {
			TRACE("Registering filesystem...");
			_filesystem = filesystem;
		}

		inline static void register_filesystem(FileSystemImpl* filesystemimpl) {
			TRACE("Registering filesystem...");
			_filesystem = filesystemimpl;
		}

		inline static void deregister_filesystem() {
			TRACE("Deregistering filesystem...");
			_filesystem = {Type::NONE};
		}

		inline static bool file_exists(const char* file_path) {
			if (!_filesystem) {
				WARNING("file_exists: filesystem not registered");
				throw std::runtime_error("FileSystem has not been registered");
			}
			return _filesystem.file_exists(file_path);
		}

		inline static size_t read_file(const char* file_path, Bytes& data) {
			if (!_filesystem) {
				throw std::runtime_error("FileSystem has not been registered");
			}
			return _filesystem.read_file(file_path, data);
		}

		inline static size_t write_file(const char* file_path, const Bytes& data) {
			if (!_filesystem) {
				throw std::runtime_error("FileSystem has not been registered");
			}
			return _filesystem.write_file(file_path, data);
		}

		inline static FileStream open_file(const char* file_path, RNS::FileStream::MODE file_mode) {
			if (!_filesystem) {
				throw std::runtime_error("FileSystem has not been registered");
			}
			return _filesystem.open_file(file_path, file_mode);
		}

		inline static bool remove_file(const char* file_path) {
			if (!_filesystem) {
				throw std::runtime_error("FileSystem has not been registered");
			}
			return _filesystem.remove_file(file_path);
		}

		inline static bool rename_file(const char* from_file_path, const char* to_file_path) {
			if (!_filesystem) {
				throw std::runtime_error("FileSystem has not been registered");
			}
			return _filesystem.rename_file(from_file_path, to_file_path);
		}

		inline static bool directory_exists(const char* directory_path) {
			if (!_filesystem) {
				throw std::runtime_error("FileSystem has not been registered");
			}
			return _filesystem.directory_exists(directory_path);
		}

		inline static bool create_directory(const char* directory_path) {
			if (!_filesystem) {
				throw std::runtime_error("FileSystem has not been registered");
			}
			return _filesystem.create_directory(directory_path);
		}

		inline static bool remove_directory(const char* directory_path) {
			if (!_filesystem) {
				throw std::runtime_error("FileSystem has not been registered");
			}
			return _filesystem.remove_directory(directory_path);
		}

		inline static std::list<std::string> list_directory(const char* directory_path) {
			if (!_filesystem) {
				throw std::runtime_error("FileSystem has not been registered");
			}
			return _filesystem.list_directory(directory_path);
		}

		inline static size_t storage_size() {
			if (!_filesystem) {
				throw std::runtime_error("FileSystem has not been registered");
			}
			return _filesystem.storage_size();
		}

		inline static size_t storage_available() {
			if (!_filesystem) {
				throw std::runtime_error("FileSystem has not been registered");
			}
			return _filesystem.storage_available();
		}

		static size_t heap_size();
		static size_t heap_available();
		static void dump_heap_stats();
	
    };

} }
