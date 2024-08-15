#pragma once

#include "Log.h"

#include <ArduinoJson.h>

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include <memory>

namespace RNS {

	#define COW

	class Bytes {

	private:
		//typedef std::vector<uint8_t> Data;
		using Data = std::vector<uint8_t>;
		//typedef std::shared_ptr<Data> SharedData;
		using SharedData = std::shared_ptr<Data>;

	public:
		enum NoneConstructor {
			NONE
		};

	public:
		Bytes() {
			MEMF("Bytes object created from defaul, this: %lu, data: %lu", this, _data.get());
		}
		Bytes(const NoneConstructor none) {
			MEMF("Bytes object created from NONE, this: %lu, data: %lu", this, _data.get());
		}
		Bytes(const Bytes& bytes) {
			MEM("Bytes is using shared data");
			assign(bytes);
			MEMF("Bytes object copy created from bytes \"%s\", this: %lu, data: %lu", toString().c_str(), this, _data.get());
		}
		Bytes(const uint8_t* chunk, size_t size) {
			assign(chunk, size);
			MEMF("Bytes object created from chunk \"%s\", this: %lu, data: %lu", toString().c_str(), this, _data.get());
		}
		Bytes(const char* string) {
			assign(string);
			MEMF("Bytes object created from string \"%s\", this: %lu, data: %lu", toString().c_str(), this, _data.get());
		}
		Bytes(const std::string& string) {
			assign(string);
			MEMF("Bytes object created from std::string \"%s\", this: %lu, data: %lu", toString().c_str(), this, _data.get());
		}
		Bytes(size_t capacity) {
			newData(capacity);
			MEMF("Bytes object created with capacity %u, this: %lu, data: %lu", capacity, this, _data.get());
		}
		virtual ~Bytes() {
			MEMF("Bytes object destroyed \"%s\", this: %lu, data: %lu", toString().c_str(), this, _data.get());
		}

		inline const Bytes& operator = (const Bytes& bytes) {
			assign(bytes);
			return *this;
		}
		inline const Bytes& operator += (const Bytes& bytes) {
			append(bytes);
			return *this;
		}

		inline Bytes operator + (const Bytes& bytes) const {
			Bytes newbytes(*this);
			newbytes.append(bytes);
			return newbytes;
		}
		inline bool operator == (const Bytes& bytes) const {
			return compare(bytes) == 0;
		}
		inline bool operator != (const Bytes& bytes) const {
			return compare(bytes) != 0;
		}
		inline bool operator < (const Bytes& bytes) const {
			return compare(bytes) < 0;
		}
		inline bool operator > (const Bytes& bytes) const {
			return compare(bytes) > 0;
		}
		inline operator bool() const {
			return (_data && !_data->empty());
		}

	private:
		inline SharedData shareData() const {
			MEM("Bytes is sharing its own data");
			_exclusive = false;
			return _data;
		}
		void newData(size_t capacity = 0);
		void exclusiveData(bool copy = true, size_t capacity = 0);

	public:
		inline void clear() {
			_data = nullptr;
			_exclusive = true;
		}

		inline void assign(const Bytes& bytes) {
#ifdef COW
			_data = bytes.shareData();
			_exclusive = false;
#else
			// if assignment is empty then clear data and don't bother creating new
			if (bytes.size() <= 0) {
				_data = nullptr;
				_exclusive = true;
				return;
			}
			exclusiveData(false, bytes._data->size());
			_data->insert(_data->begin(), bytes._data->begin(), bytes._data->end());
#endif
		}
		inline void assign(const uint8_t* chunk, size_t chunk_size) {
			// if assignment is empty then clear data and don't bother creating new
			if (chunk == nullptr || chunk_size <= 0) {
				_data = nullptr;
				_exclusive = true;
				return;
			}
			exclusiveData(false, chunk_size);
			_data->insert(_data->begin(), chunk, chunk + chunk_size);
		}
		inline void assign(const char* string) {
			// if assignment is empty then clear data and don't bother creating new
			if (string == nullptr || string[0] == 0) {
				_data = nullptr;
				_exclusive = true;
				return;
			}
			size_t string_size = strlen(string);
			exclusiveData(false, string_size);
			_data->insert(_data->begin(), (uint8_t* )string, (uint8_t* )string + string_size);
		}
		//inline void assign(const std::string& string) { assign(string.c_str()); }
		inline void assign(const std::string& string) { assign((uint8_t*)string.c_str(), string.length()); }
		void assignHex(const uint8_t* hex, size_t hex_size);
		inline void assignHex(const char* hex) { assignHex((uint8_t*)hex, strlen(hex)); }

		inline void append(const Bytes& bytes) {
			// if append is empty then do nothing
			if (bytes.size() <= 0) {
				return;
			}
			exclusiveData(true, size() + bytes.size());
			_data->insert(_data->end(), bytes._data->begin(), bytes._data->end());
		}
		inline void append(const uint8_t* chunk, size_t chunk_size) {
			// if append is empty then do nothing
			if (chunk == nullptr || chunk_size <= 0) {
				return;
			}
			exclusiveData(true, size() + chunk_size);
			_data->insert(_data->end(), chunk, chunk + chunk_size);
		}
		inline void append(const char* string) {
			// if append is empty then do nothing
			if (string == nullptr || string[0] == 0) {
				return;
			}
			size_t string_size = strlen(string);
			exclusiveData(true, size() + string_size);
			_data->insert(_data->end(), (uint8_t* )string, (uint8_t* )string + string_size);
		}
		inline void append(uint8_t byte) {
			exclusiveData(true, size() + 1);
			_data->push_back(byte);
		}
		//inline void append(const std::string& string) { append(string.c_str()); }
		inline void append(const std::string& string) { append((uint8_t*)string.c_str(), string.length()); }
		void appendHex(const uint8_t* hex, size_t hex_size);
		inline void appendHex(const char* hex) { appendHex((uint8_t*)hex, strlen(hex)); }

		inline uint8_t* writable(size_t size) {
			if (size > 0) {
				// create exclusive data with reserved capacity
				exclusiveData(false, size);
				// actually expand data to requested size
				resize(size);
				return _data->data();
			}
			else if (_data) {
				size_t current_size = _data->size();
				// create exclusive data with reserved capacity
				exclusiveData(false, current_size);
				// actually expand data to current size
				resize(current_size);
				return _data->data();
			}
			return nullptr;
		}

		inline void resize(size_t newsize) {
			// if size is unchanged then do nothing
			if (newsize == size()) {
				return;
			}
			// CBA TODO Determine whether or not to reserve capacity here since when size is shrunk the call to
			// exclusive data may copy all data firt which will effectively grow the data again before being
			// shrunk below (inefficient).
			exclusiveData(true);
			_data->resize(newsize);
		}

	public:
		int compare(const Bytes& bytes) const;
		int compare(const uint8_t* buf, size_t size) const;
		inline int compare(const char* str) const { return compare((const uint8_t*)str, strlen(str)); }
		inline size_t size() const { if (!_data) return 0; return _data->size(); }
		inline bool empty() const { if (!_data) return true; return _data->empty(); }
		inline size_t capacity() const { if (!_data) return 0; return _data->capacity(); }
		inline void reserve(size_t capacity) const { if (!_data) return; _data->reserve(capacity); }
		inline const uint8_t* data() const { if (!_data) return nullptr; return _data->data(); }

		inline std::string toString() const { if (!_data) return ""; return {(const char*)data(), size()}; }
		std::string toHex(bool upper = false) const;
		Bytes mid(size_t beginpos, size_t len) const;
		Bytes mid(size_t beginpos) const;
		inline Bytes left(size_t len) const { if (!_data) return NONE; if (len > size()) len = size(); return {data(), len}; }
		inline Bytes right(size_t len) const { if (!_data) return NONE; if (len > size()) len = size(); return {data() + (size() - len), len}; }
		inline int find(int pos, const char* str) {
			if (!_data || _data->data() == nullptr || pos >= _data->size()) {
				return -1;
			}
			const char* ptr = strnstr((const char*)(_data->data() + pos), str, (_data->size() - pos));
			if (ptr == nullptr) {
				return -1;
			}
			return (int)((uint8_t*)ptr - _data->data());
		}
		inline int find(const char* str) { return find(0, str); }

		// Python array indexing
		// [8:16]
		//   pos 8 to pos 16
		//   mid(8, 8)
		// [:16]
		//   start to pos 16 (same as first 16)
		//   left(16)
		// [16:]
		//   pos 16 to end
		//   mid(16)
		// [-16:]
		//   last 16
		//   right(16)
		// [:-16]
		//   all except the last 16
		//   left(size()-16)
		//   mid(0, size()-16)
		// [-1]
		//   last element
		// [-2]
		//   second to last element

	private:
		SharedData _data;
		mutable bool _exclusive = true;

	};

	// following array function doesn't work without size since it's past as a pointer to the array sizeof() is of the pointer
	//inline Bytes bytesFromArray(const uint8_t arr[]) { return Bytes(arr, sizeof(arr)); }
	//inline Bytes bytesFromChunk(const uint8_t* ptr, size_t len) { return Bytes(ptr, len); }
	inline Bytes bytesFromChunk(const uint8_t* ptr, size_t len) { return {ptr, len}; }
	//inline Bytes bytesFromString(const char* str) { return Bytes((uint8_t*)str, strlen(str)); }
	inline Bytes bytesFromString(const char* str) { return {(uint8_t*)str, strlen(str)}; }
	//z inline Bytes bytesFromInt(const int) { return {(uint8_t*)str, strlen(str)}; }

	inline std::string stringFromBytes(const Bytes& bytes) { return bytes.toString(); }
	inline std::string hexFromBytes(const Bytes& bytes) { return bytes.toHex(); }
	std::string hexFromByte(uint8_t byte, bool upper = true);

}

inline RNS::Bytes& operator << (RNS::Bytes& lhbytes, const RNS::Bytes& rhbytes) {
	MEM("Appending right-hand Bytes to left-hand Bytes");
	lhbytes.append(rhbytes);
	MEM("Returning left-hand Bytes");
	return lhbytes;
}

inline RNS::Bytes& operator << (RNS::Bytes& lhbytes, uint8_t rhbyte) {
	MEM("Appending right-hand byte to left-hand Bytes");
	lhbytes.append(rhbyte);
	MEM("Returning left-hand Bytes");
	return lhbytes;
}

inline RNS::Bytes& operator << (RNS::Bytes& lhbytes, const char* rhstr) {
	MEM("Appending right-hand str to left-hand Bytes");
	lhbytes.append(rhstr);
	MEM("Returning left-hand Bytes");
	return lhbytes;
}

namespace ArduinoJson {
	inline bool convertToJson(const RNS::Bytes& src, JsonVariant dst) {
		return dst.set(src.toHex());
	}
	inline void convertFromJson(JsonVariantConst src, RNS::Bytes& dst) {
		dst.assignHex(src.as<const char*>());
	}
	inline bool canConvertFromJson(JsonVariantConst src, const RNS::Bytes&) {
		return src.is<const char*>();
	}
}
