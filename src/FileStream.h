#pragma once

#include "Utilities/Crc.h"
#include "Log.h"
#include "Bytes.h"
#include "Type.h"

#ifdef ARDUINO
#include <Stream.h>
#else
#include "Utilities/Stream.h"
#endif

#include <list>
#include <memory>
#include <cassert>
#include <stdint.h>

namespace RNS {

	class FileStreamImpl {

	protected:
		FileStreamImpl() { MEMF("FileStreamImpl object created, this: 0x%X", this); }
	public:
		virtual ~FileStreamImpl() { MEMF("FileStreamImpl object destroyed, this: 0x%X", this); }

	protected:
		// CBA TODO Can these be pure-virtual???
		virtual const char* name() = 0;
		virtual size_t size() = 0;
		virtual void close() = 0;

		// Print overrides
		virtual size_t write(uint8_t byte) = 0;
		virtual size_t write(const uint8_t *buffer, size_t size) = 0;

		// Stream overrides
		virtual int available() = 0;
		virtual int read() = 0;
		virtual int peek() = 0;
		virtual void flush() = 0;

	friend class FileStream;
	};

	class FileStream : public Stream {

	public:
		enum MODE {
			MODE_READ,
			MODE_WRITE,
			MODE_APPEND,
		};

	public:
		FileStream(Type::NoneConstructor none) {
			MEMF("FileStream object NONE created, this: 0x%X, data: 0x%X", this, _impl.get());
		}
		FileStream(const FileStream& obj) : _impl(obj._impl), _crc(obj._crc) {
			MEMF("FileStream object copy created, this: 0x%X, data: 0x%X", this, _impl.get());
		}
		FileStream(FileStreamImpl* impl) : _impl(impl) {
			MEMF("FileStream object impl created, this: 0x%X, data: 0x%X", this, _impl.get());
		}
		virtual ~FileStream() {
			MEMF("FileStream object destroyed, this: 0x%X, data: 0x%X", this, _impl.get());
		}

		inline virtual FileStream& operator = (const FileStream& obj) {
			_impl = obj._impl;
			_crc = obj._crc;
			MEMF("FileStream object copy created by assignment, this: 0x%X, data: 0x%X", this, _impl.get());
			return *this;
		}
		inline FileStream& operator = (FileStreamImpl* impl) {
			_impl.reset(impl);
			MEMF("FileStream object copy created by impl assignment, this: 0x%X, data: 0x%X", this, _impl.get());
			return *this;
		}
		inline operator bool() const {
			MEMF("FileStream object bool, this: 0x%X, data: 0x%X", this, _impl.get());
			return _impl.get() != nullptr;
		}
		inline bool operator < (const FileStream& obj) const {
			MEMF("FileStream object <, this: 0x%X, data: 0x%X", this, _impl.get());
			return _impl.get() < obj._impl.get();
		}
		inline bool operator > (const FileStream& obj) const {
			MEMF("FileStream object <, this: 0x%X, data: 0x%X", this, _impl.get());
			return _impl.get() > obj._impl.get();
		}
		inline bool operator == (const FileStream& obj) const {
			MEMF("FileStream object ==, this: 0x%X, data: 0x%X", this, _impl.get());
			return _impl.get() == obj._impl.get();
		}
		inline bool operator != (const FileStream& obj) const {
			MEMF("FileStream object !=, this: 0x%X, data: 0x%X", this, _impl.get());
			return _impl.get() != obj._impl.get();
		}
		inline FileStreamImpl* get() {
			return _impl.get();
		}
		inline void clear() {
			_impl.reset();
		}

	public:
		inline uint32_t crc() { return _crc; }
		inline size_t write(const char* str) { return write((const uint8_t*)str, strlen(str)); }

		// File overrides
		inline const char* name() { assert(_impl); return _impl->name(); }
		inline size_t size() { assert(_impl); return _impl->size(); }
		inline void close() { assert(_impl); _impl->close(); }

		// Print overrides
		inline size_t write(uint8_t byte) { assert(_impl); _crc = Utilities::Crc::crc32(_crc, byte); return _impl->write(byte); }
		inline size_t write(const uint8_t *buffer, size_t size) { assert(_impl); _crc = Utilities::Crc::crc32(_crc, buffer, size); return _impl->write(buffer, size); }

		// Stream overrides
		inline int available() { assert(_impl); return _impl->available(); }
		inline int read() { assert(_impl); if (_impl->available() <= 0) return EOF; int ch = _impl->read(); uint8_t byte = (uint8_t)ch; _crc = Utilities::Crc::crc32(_crc, byte); return ch; }
		inline int peek() { assert(_impl); return _impl->peek(); }
		inline void flush() { assert(_impl); _impl->flush(); }

		// getters/setters
	protected:
	public:

#ifndef NDEBUG
		inline std::string debugString() const {
			std::string dump;
			dump = "FileStream object, this: " + std::to_string((uintptr_t)this) + ", data: " + std::to_string((uintptr_t)_impl.get());
			return dump;
		}
#endif

	protected:
		std::shared_ptr<FileStreamImpl> _impl;
		uint32_t _crc = 0;
	};

}
