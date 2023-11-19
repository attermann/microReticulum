#pragma once

#include "Log.h"

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
			//extreme("Bytes object created from default, this: " + std::to_string((uintptr_t)this) + ", data: " + std::to_string((unsigned long)_data.get()));
		}
		Bytes(NoneConstructor none) {
			//extreme("Bytes object created from NONE, this: " + std::to_string((uintptr_t)this) + ", data: " + std::to_string((unsigned long)_data.get()));
		}
		Bytes(const Bytes &bytes) {
			//extreme("Bytes is using shared data");
			assign(bytes);
			//extreme("Bytes object copy created from bytes \"" + toString() + "\", this: " + std::to_string((uintptr_t)this) + ", data: " + std::to_string((unsigned long)_data.get()));
		}
		Bytes(const uint8_t *chunk, size_t size) {
			assign(chunk, size);
			//extreme(std::string("Bytes object created from chunk \"") + toString() + "\", this: " + std::to_string((uintptr_t)this) + ", data: " + std::to_string((unsigned long)_data.get()));
		}
		Bytes(const char *string) {
			assign(string);
			//extreme(std::string("Bytes object created from string \"") + toString() + "\", this: " + std::to_string((uintptr_t)this) + ", data: " + std::to_string((unsigned long)_data.get()));
		}
		Bytes(const std::string &string) {
			assign(string);
			//extreme(std::string("Bytes object created from std::string \"") + toString() + "\", this: " + std::to_string((uintptr_t)this) + ", data: " + std::to_string((unsigned long)_data.get()));
		}
		virtual ~Bytes() {
			//extreme(std::string("Bytes object destroyed \"") + toString() + "\", this: " + std::to_string((uintptr_t)this) + ", data: " + std::to_string((unsigned long)_data.get()));
		}

		inline Bytes& operator = (const Bytes &bytes) {
			assign(bytes);
			return *this;
		}
		inline Bytes& operator += (const Bytes &bytes) {
			append(bytes);
			return *this;
		}

		inline Bytes operator + (const Bytes &bytes) const {
			Bytes newbytes(*this);
			newbytes.append(bytes);
			return newbytes;
		}
		inline bool operator == (const Bytes &bytes) const {
			return compare(bytes) == 0;
		}
		inline bool operator != (const Bytes &bytes) const {
			return compare(bytes) != 0;
		}
		inline bool operator < (const Bytes &bytes) const {
			return compare(bytes) < 0;
		}
		inline bool operator > (const Bytes &bytes) const {
			return compare(bytes) > 0;
		}
		inline operator bool() const {
			return (_data && !_data->empty());
		}

	private:
		inline SharedData shareData() const {
			//extreme("Bytes is sharing its own data");
			_owner = false;
			return _data;
		}
		inline void newData(size_t size = 0) {
			//extreme("Bytes is creating its own data");
			if (size > 0) {
				_data = SharedData(new Data(size));
			}
			else {
				_data = SharedData(new Data());
			}
			_owner = true;
		}
		void ownData();

	public:
		inline void clear() {
			_data = nullptr;
			_owner = true;
		}

		inline void assign(const Bytes& bytes) {
#ifdef COW
			_data = bytes.shareData();
			_owner = false;
#else
			// if assignment is empty then clear data and don't bother creating new
			if (bytes.size() <= 0) {
				_data = nullptr;
				_owner = true;
				return;
			}
			newData();
			_data->insert(_data->begin(), bytes._data->begin(), bytes._data->end());
#endif
		}
		inline void assign(const uint8_t *chunk, size_t size) {
			// if assignment is empty then clear data and don't bother creating new
			if (chunk == nullptr || size <= 0) {
				_data = nullptr;
				_owner = true;
				return;
			}
			newData();
			_data->insert(_data->begin(), chunk, chunk + size);
		}
		inline void assign(const char* string) {
			// if assignment is empty then clear data and don't bother creating new
			if (string == nullptr || string[0] == 0) {
				_data = nullptr;
				_owner = true;
				return;
			}
			newData();
			_data->insert(_data->begin(), (uint8_t *)string, (uint8_t *)string + strlen(string));
		}
		inline void assign(const std::string &string) { assign(string.c_str()); }
		void assignHex(const char* hex);

		inline void append(const Bytes& bytes) {
			// if append is empty then do nothing
			if (bytes.size() <= 0) {
				return;
			}
			ownData();
			_data->insert(_data->end(), bytes._data->begin(), bytes._data->end());
		}
		inline void append(const uint8_t *chunk, size_t size) {
			// if append is empty then do nothing
			if (chunk == nullptr || size <= 0) {
				return;
			}
			ownData();
			_data->insert(_data->end(), chunk, chunk + size);
		}
		inline void append(const char* string) {
			// if append is empty then do nothing
			if (string == nullptr || string[0] == 0) {
				return;
			}
			ownData();
			_data->insert(_data->end(), (uint8_t *)string, (uint8_t *)string + strlen(string));
		}
		inline void append(const std::string &string) { append(string.c_str()); }
		inline void append(uint8_t byte) {
			ownData();
			_data->push_back(byte);
		}
		void appendHex(const char* hex);

		inline void resize(size_t newsize) {
			// if size is unchanged then do nothing
			if (newsize == size()) {
				return;
			}
			ownData();
			_data->resize(newsize);
		}

		inline uint8_t *writable(size_t size) {
			newData(size);
			return _data->data();
		}

	public:
		int8_t compare(const Bytes &bytes) const;
		inline size_t size() const { if (!_data) return 0; return _data->size(); }
		inline bool empty() const { if (!_data) return true; return _data->empty(); }
		inline size_t capacity() const { if (!_data) return 0; return _data->capacity(); }
		inline const uint8_t *data() const { if (!_data) return nullptr; return _data->data(); }

		inline std::string toString() const { if (!_data) return ""; return {(const char*)data(), size()}; }
		std::string toHex(bool upper = true) const;
		Bytes mid(size_t beginpos, size_t len) const;
		Bytes mid(size_t beginpos) const;
		inline Bytes left(size_t len) const { if (!_data) return NONE; if (len > size()) len = size(); return {data(), len}; }
		inline Bytes right(size_t len) const { if (!_data) return NONE; if (len > size()) len = size(); return {data() + (size() - len), len}; }

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
		mutable bool _owner = true;

	};

	// following array function doesn't work without size since it's past as a pointer to the array sizeof() is of the pointer
	//inline Bytes bytesFromArray(const uint8_t arr[]) { return Bytes(arr, sizeof(arr)); }
	//inline Bytes bytesFromChunk(const uint8_t *ptr, size_t len) { return Bytes(ptr, len); }
	inline Bytes bytesFromChunk(const uint8_t *ptr, size_t len) { return {ptr, len}; }
	//inline Bytes bytesFromString(const char *str) { return Bytes((uint8_t*)str, strlen(str)); }
	inline Bytes bytesFromString(const char *str) { return {(uint8_t*)str, strlen(str)}; }
	//zinline Bytes bytesFromInt(const int) { return {(uint8_t*)str, strlen(str)}; }

	inline std::string stringFromBytes(const Bytes& bytes) { return bytes.toString(); }
	inline std::string hexFromBytes(const Bytes& bytes) { return bytes.toHex(); }
	std::string hexFromByte(uint8_t byte, bool upper = true);

}

inline RNS::Bytes& operator << (RNS::Bytes &lhbytes, const RNS::Bytes &rhbytes) {
	lhbytes.append(rhbytes);
	return lhbytes;
}

inline RNS::Bytes& operator << (RNS::Bytes &lhbytes, uint8_t rhbyte) {
	lhbytes.append(rhbyte);
	return lhbytes;
}
