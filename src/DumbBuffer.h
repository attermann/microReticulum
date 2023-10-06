#pragma once

#include "Log.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <vector>
#include <string>
#include <memory>

#define COW

namespace RNS {

	class Bytes {

	public:
		Bytes(size_t capacity = 0) {
			if (capacity > 0) {
				//_vector.reserve(capacity);
				_vector = std::shared_ptr<std::vector<uint8_t>>(new std::vector<uint8_t>(capacity));
			}
			else {
				_vector = std::shared_ptr<std::vector<uint8_t>>(new std::vector<uint8_t>());
			}
			log("Bytes object created from default");
		}
		Bytes(const Bytes &bytes, size_t capacity = 0) : Bytes(capacity) {
			//append(bytes);
			_vector = bytes._vector;
			_owner = false;
			log(std::string("Bytes object created from bytes \"") + toString() + "\"");
		}
		Bytes(const uint8_t *chunk, size_t size, size_t capacity = 0) : Bytes(capacity) {
			append(chunk, size);
			log(std::string("Bytes object created from chunk \"") + toString() + "\"");
		}
		Bytes(const char *string, size_t capacity = 0) : Bytes(capacity) {
			append(string);
			log(std::string("Bytes object created from string \"") + toString() + "\"");
		}
		~Bytes() {
			log(std::string("Bytes object destroyed \"") + toString() + "\"");
		}

		inline Bytes& operator + (const Bytes &bytes) {
			append(bytes);
			return *this;
		}
		inline Bytes& operator += (const Bytes &bytes) {
			append(bytes);
			return *this;
		}
		inline bool operator == (const Bytes &bytes) const {
			return _vector == bytes._vector;
		}
		inline bool operator < (const Bytes &bytes) const {
			return _vector < bytes._vector;
		}

	public:
		inline size_t size() const { return _vector->size(); }
		inline bool empty() const { return _vector->empty(); }
		inline size_t capacity() const { return _vector->capacity(); }
		inline const uint8_t *data() const { return _vector->data(); }

		void append(const Bytes& bytes) {
			_vector->insert(_vector->end(), bytes._vector->begin(), bytes._vector->end());
		}
		void append(const uint8_t *chunk, size_t size) {
			_vector->insert(_vector->end(), chunk, chunk + size);
		}
		void append(const char* string) {
			_vector->insert(_vector->end(), (uint8_t *)string, (uint8_t *)string + strlen(string));
		}

		inline std::string toString() { return {(const char*)data(), size()}; }

	private:
		//std::vector<uint8_t> _vector;
		std::shared_ptr<std::vector<uint8_t>> _vector;
		bool _owner = true;;

	};

	// following array function doesn't work without size since it's past as a pointer to the array sizeof() is of the pointer
	//static inline Bytes bytesFromArray(const uint8_t arr[]) { return Bytes(arr, sizeof(arr)); }
	//static inline Bytes bytesFromChunk(const uint8_t *ptr, size_t len) { return Bytes(ptr, len); }
	static inline Bytes bytesFromChunk(const uint8_t *ptr, size_t len) { return {ptr, len}; }
	//static inline Bytes bytesFromString(const char *str) { return Bytes((uint8_t*)str, strlen(str)); }
	static inline Bytes bytesFromString(const char *str) { return {(uint8_t*)str, strlen(str)}; }
	//zstatic inline Bytes bytesFromInt(const int) { return {(uint8_t*)str, strlen(str)}; }

	static inline std::string stringFromBytes(const Bytes& bytes) { return {(const char*)bytes.data(), bytes.size()}; }

}

inline RNS::Bytes& operator << (RNS::Bytes &lhs, const RNS::Bytes &rhs) {
	lhs.append(rhs);
	return lhs;
}
