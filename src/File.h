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

	class FileImpl {

	protected:
		FileImpl() { MEMF("FileImpl object created, this: 0x%X", this); }
	public:
		virtual ~FileImpl() { MEMF("FileImpl object destroyed, this: 0x%X", this); }

	protected:
		// File overrides
		virtual const char* name() const = 0;
		virtual size_t size() const = 0;
		virtual void close() = 0;

		// Print overrides
		virtual size_t write(uint8_t byte) = 0;
		virtual size_t write(const uint8_t *buffer, size_t size) = 0;

		// Stream overrides
		virtual int available() = 0;
		virtual int read() = 0;
		virtual int peek() = 0;
		virtual void flush() = 0;

	friend class File;
	};

	class File : public Stream {

	public:
		enum MODE {
			MODE_READ,
			MODE_WRITE,
			MODE_APPEND,
		};

	public:
		File(Type::NoneConstructor none) {
			MEMF("File NONE object created, this: 0x%X, impl: 0x%X", this, _impl.get());
		}
		File(const File& obj) : _impl(obj._impl), _crc(obj._crc) {
			MEMF("File object copy created, this: 0x%X, impl: 0x%X", this, _impl.get());
		}
		File(FileImpl* impl) : _impl(impl) {
			MEMF("File object impl created, this: 0x%X, impl: 0x%X", this, _impl.get());
		}
		virtual ~File() {
			MEMF("File object destroyed, this: 0x%X, impl: 0x%X", this, _impl.get());
		}

		inline virtual File& operator = (const File& obj) {
			_impl = obj._impl;
			_crc = obj._crc;
			MEMF("File object copy created by assignment, this: 0x%X, impl: 0x%X", this, _impl.get());
			return *this;
		}
		inline File& operator = (FileImpl* impl) {
			_impl.reset(impl);
			MEMF("File object copy created by impl assignment, this: 0x%X, impl: 0x%X", this, _impl.get());
			return *this;
		}
		inline operator bool() const {
			MEMF("File object bool, this: 0x%X, impl: 0x%X", this, _impl.get());
			return _impl.get() != nullptr;
		}
		inline bool operator < (const File& obj) const {
			MEMF("File object <, this: 0x%X, impl: 0x%X", this, _impl.get());
			return _impl.get() < obj._impl.get();
		}
		inline bool operator > (const File& obj) const {
			MEMF("File object <, this: 0x%X, impl: 0x%X", this, _impl.get());
			return _impl.get() > obj._impl.get();
		}
		inline bool operator == (const File& obj) const {
			MEMF("File object ==, this: 0x%X, impl: 0x%X", this, _impl.get());
			return _impl.get() == obj._impl.get();
		}
		inline bool operator != (const File& obj) const {
			MEMF("File object !=, this: 0x%X, impl: 0x%X", this, _impl.get());
			return _impl.get() != obj._impl.get();
		}
		inline FileImpl* get() {
			return _impl.get();
		}
		inline void clear() {
			_impl.reset();
		}

		//NEW operator bool() const;

	public:
		inline uint32_t crc() { return _crc; }
		inline size_t write(const char* str) { return write((const uint8_t*)str, strlen(str)); }

		inline const char* name() const { assert(_impl); return _impl->name(); }
		//NEW inline const char* path() const { assert(_impl); return _impl->path(); }
		inline size_t size() const { assert(_impl); return _impl->size(); }
		//NEW inline size_t position() const { assert(_impl); return _impl->position(); }
		inline void close() { assert(_impl); _impl->close(); }

		inline size_t write(uint8_t byte) override { assert(_impl); _crc = Utilities::Crc::crc32(_crc, byte); return _impl->write(byte); }
		inline size_t write(const uint8_t *buffer, size_t size) override { assert(_impl); _crc = Utilities::Crc::crc32(_crc, buffer, size); return _impl->write(buffer, size); }
		inline void flush() override { assert(_impl); _impl->flush(); }

		inline int available() override { assert(_impl); return _impl->available(); }
		inline int peek() override { assert(_impl); return _impl->peek(); }
		int read() override {
			assert(_impl);
			if (_impl->available() <= 0) return EOF;
			int ch = _impl->read();
			uint8_t byte = (uint8_t)ch;
			_crc = Utilities::Crc::crc32(_crc, byte);
			return ch;
		}
		//NEW size_t read(uint8_t *buf, size_t size) {
		//NEW }
		//NEW size_t readBytes(char *buffer, size_t length) {
		//NEW 	return read((uint8_t *)buffer, length);
		//NEW }

		//NEW bool seek(uint32_t pos, SeekMode mode);
		//NEW bool seek(uint32_t pos) {
		//NEW return seek(pos, SeekSet);
		//NEW }

		//NEW boolean isDirectory(void);
		//NEW boolean seekDir(long position);
		//NEW File openNextFile(const char *mode = FILE_READ);
		//NEW String getNextFileName(void);
		//NEW String getNextFileName(boolean *isDir);
		//NEW void rewindDirectory(void);

		// getters/setters
	protected:
	public:

#ifndef NDEBUG
		inline std::string debugString() const {
			std::string dump;
			dump = "File object, this: " + std::to_string((uintptr_t)this) + ", data: " + std::to_string((uintptr_t)_impl.get());
			return dump;
		}
#endif

	protected:
		std::shared_ptr<FileImpl> _impl;
		uint32_t _crc = 0;
	};

}
