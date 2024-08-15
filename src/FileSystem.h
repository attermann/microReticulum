#pragma once

#include "FileStream.h"
#include "Log.h"
#include "Bytes.h"
#include "Type.h"

#include <list>
#include <memory>
#include <cassert>
#include <stdint.h>

namespace RNS {

	class FileSystemImpl {

	protected:
		FileSystemImpl() { MEMF("FileSystem::FileSystemImpl object created, this: 0x%X", this); }
	public:
		virtual ~FileSystemImpl() { MEMF("FileSystem::FileSystemImpl object destroyed, this: 0x%X", this); }

	protected:
		virtual bool file_exists(const char* file_path) = 0;
		virtual size_t read_file(const char* file_path, Bytes& data) = 0;
		virtual size_t write_file(const char* file_path, const Bytes& data) = 0;
		virtual FileStream open_file(const char* file_path, FileStream::MODE file_mode) = 0;
		virtual bool remove_file(const char* file_path) = 0;
		virtual bool rename_file(const char* from_file_path, const char* to_file_path) = 0;
		virtual bool directory_exists(const char* directory_path) = 0;
		virtual bool create_directory(const char* directory_path) = 0;
		virtual bool remove_directory(const char* directory_path) = 0;
		virtual std::list<std::string> list_directory(const char* directory_path) = 0;
		virtual size_t storage_size() = 0;
		virtual size_t storage_available() = 0;

	friend class FileSystem;
	};

	class FileSystem {

	public:
		FileSystem(Type::NoneConstructor none) {
			MEMF("FileSystem object NONE created, this: 0x%X, data: 0x%X", this, _impl.get());
		}
		FileSystem(const FileSystem& obj) : _impl(obj._impl) {
			MEMF("FileSystem object copy created, this: 0x%X, data: 0x%X", this, _impl.get());
		}
		FileSystem(FileSystemImpl* impl) : _impl(impl) {
			MEMF("FileSystem object impl created, this: 0x%X, data: 0x%X", this, _impl.get());
		}
		virtual ~FileSystem() {
			MEMF("FileSystem object destroyed, this: 0x%X, data: 0x%X", this, _impl.get());
		}

		inline FileSystem& operator = (const FileSystem& obj) {
			_impl = obj._impl;
			MEMF("FileSystem object copy created by assignment, this: 0x%X, data: 0x%X", this, _impl.get());
			return *this;
		}
		inline FileSystem& operator = (FileSystemImpl* impl) {
			_impl.reset(impl);
			MEMF("FileSystem object copy created by impl assignment, this: 0x%X, data: 0x%X", this, _impl.get());
			return *this;
		}
		inline operator bool() const {
			MEMF("FileSystem object bool, this: 0x%X, data: 0x%X", this, _impl.get());
			return _impl.get() != nullptr;
		}
		inline bool operator < (const FileSystem& obj) const {
			MEMF("FileSystem object <, this: 0x%X, data: 0x%X", this, _impl.get());
			return _impl.get() < obj._impl.get();
		}
		inline bool operator > (const FileSystem& obj) const {
			MEMF("FileSystem object <, this: 0x%X, data: 0x%X", this, _impl.get());
			return _impl.get() > obj._impl.get();
		}
		inline bool operator == (const FileSystem& obj) const {
			MEMF("FileSystem object ==, this: 0x%X, data: 0x%X", this, _impl.get());
			return _impl.get() == obj._impl.get();
		}
		inline bool operator != (const FileSystem& obj) const {
			MEMF("FileSystem object !=, this: 0x%X, data: 0x%X", this, _impl.get());
			return _impl.get() != obj._impl.get();
		}
		inline FileSystemImpl* get() {
			return _impl.get();
		}
		inline void clear() {
			_impl.reset();
		}

	public:
		inline bool file_exists(const char* file_path) { assert(_impl); return _impl->file_exists(file_path); }
		inline size_t read_file(const char* file_path, Bytes& data) { assert(_impl); return _impl->read_file(file_path, data); }
		inline size_t write_file(const char* file_path, const Bytes& data) { assert(_impl); return _impl->write_file(file_path, data); }
		inline FileStream open_file(const char* file_path, FileStream::MODE file_mode) { return _impl->open_file(file_path, file_mode); }
		inline bool remove_file(const char* file_path) { assert(_impl); return _impl->remove_file(file_path); }
		inline bool rename_file(const char* from_file_path, const char* to_file_path) { assert(_impl); return _impl->rename_file(from_file_path, to_file_path); }
		inline bool directory_exists(const char* directory_path) { assert(_impl); return _impl->directory_exists(directory_path); }
		inline bool create_directory(const char* directory_path) { assert(_impl); return _impl->create_directory(directory_path); }
		inline bool remove_directory(const char* directory_path) { assert(_impl); return _impl->remove_directory(directory_path); }
		inline std::list<std::string> list_directory(const char* directory_path) { assert(_impl); return _impl->list_directory(directory_path); }
		inline size_t storage_size() { assert(_impl); return _impl->storage_size(); }
		inline size_t storage_available() { assert(_impl); return _impl->storage_available(); }

	private:
		std::list<std::string> _empty;

		// getters/setters
	protected:
	public:

#ifndef NDEBUG
		inline std::string debugString() const {
			std::string dump;
			dump = "FileSystem object, this: " + std::to_string((uintptr_t)this) + ", data: " + std::to_string((uintptr_t)_impl.get());
			return dump;
		}
#endif

	protected:
		std::shared_ptr<FileSystemImpl> _impl;
	};

}
