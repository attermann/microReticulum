#pragma once

#include "Log.h"
#include "Bytes.h"
#include "Type.h"

#include <list>
#include <memory>
#include <cassert>
#include <stdint.h>

namespace RNS {

	class Filesystem {

	protected:
		using HFilesystem = std::shared_ptr<Filesystem>;

	public:
		Filesystem(Type::NoneConstructor none) {
			mem("Filesystem object NONE created, this: " + std::to_string((uintptr_t)this) + ", data: " + std::to_string((uintptr_t)_object.get()));
		}
		Filesystem(const Filesystem& filesystem) : _object(filesystem._object) {
			mem("Filesystem object copy created, this: " + std::to_string((uintptr_t)this) + ", data: " + std::to_string((uintptr_t)_object.get()));
		}
		Filesystem() : _object(new Object(this)), _creator(true) {
			mem("Filesystem object created, this: " + std::to_string((uintptr_t)this) + ", data: " + std::to_string((uintptr_t)_object.get()));
		}
		virtual ~Filesystem() {
			if (_creator) {
				mem("Filesystem object clearing parent reference, this: " + std::to_string((uintptr_t)this) + ", data: " + std::to_string((uintptr_t)_object.get()));
				// clear reference to parent in object foir safety
				assert(_object);
				_object->_parent = nullptr;
			}
			mem("Filesystem object destroyed, this: " + std::to_string((uintptr_t)this) + ", data: " + std::to_string((uintptr_t)_object.get()));
		}

		inline Filesystem& operator = (const Filesystem& filesystem) {
			_object = filesystem._object;
			mem("Filesystem object copy created by assignment, this: " + std::to_string((uintptr_t)this) + ", data: " + std::to_string((uintptr_t)_object.get()));
			if (_creator) {
				mem("Filesystem object clearing creator flag, this: " + std::to_string((uintptr_t)this) + ", data: " + std::to_string((uintptr_t)_object.get()));
				_creator = false;
			}
			return *this;
		}
		inline operator bool() const {
			mem("Filesystem object bool, this: " + std::to_string((uintptr_t)this) + ", data: " + std::to_string((uintptr_t)_object.get()));
			return _object.get() != nullptr;
		}
		inline bool operator < (const Filesystem& filesystem) const {
			mem("Filesystem object <, this: " + std::to_string((uintptr_t)this) + ", data: " + std::to_string((uintptr_t)_object.get()));
			return _object.get() < filesystem._object.get();
		}
		inline bool operator > (const Filesystem& filesystem) const {
			mem("Filesystem object <, this: " + std::to_string((uintptr_t)this) + ", data: " + std::to_string((uintptr_t)_object.get()));
			return _object.get() > filesystem._object.get();
		}
		inline bool operator == (const Filesystem& filesystem) const {
			mem("Filesystem object ==, this: " + std::to_string((uintptr_t)this) + ", data: " + std::to_string((uintptr_t)_object.get()));
			return _object.get() == filesystem._object.get();
		}
		inline bool operator != (const Filesystem& filesystem) const {
			mem("Filesystem object !=, this: " + std::to_string((uintptr_t)this) + ", data: " + std::to_string((uintptr_t)_object.get()));
			return _object.get() != filesystem._object.get();
		}

	protected:
		// CBA These should NOT be called internally, and should be protected (only to be overridden and called by Object)
		virtual bool file_exists(const char* file_path) { return false; }
		virtual const Bytes read_file(const char* file_path) { return {Bytes::NONE}; }
		virtual bool write_file(const Bytes& data, const char* file_path) { return false; }
		virtual bool remove_file(const char* file_path) { return false; }
		virtual bool rename_file(const char* from_file_path, const char* to_file_path) { return false; }
		virtual bool create_directory(const char* directory_path) { return false; }

	public:
		inline bool do_file_exists(const char* file_path) { assert(_object); return _object->do_file_exists(file_path); }
		inline const Bytes do_read_file(const char* file_path) { assert(_object); return _object->do_read_file(file_path); }
		inline bool do_write_file(const Bytes& data, const char* file_path) { assert(_object); return _object->do_write_file(data, file_path); }
		inline bool do_remove_file(const char* file_path) { assert(_object); return _object->do_remove_file(file_path); }
		inline bool do_rename_file(const char* from_file_path, const char* to_file_path) { assert(_object); return _object->do_rename_file(from_file_path, to_file_path); }
		inline bool do_create_directory(const char* directory_path) { assert(_object); return _object->do_create_directory(directory_path); }

		// getters/setters
	protected:
	public:

#ifndef NDEBUG
		inline std::string debugString() const {
			std::string dump;
			dump = "Filesystem object, this: " + std::to_string((uintptr_t)this) + ", data: " + std::to_string((uintptr_t)_object.get());
			return dump;
		}
#endif

	private:
		class Object {
		public:
			Object(Filesystem* parent) : _parent(parent) { mem("Filesystem::Data object created, this: " + std::to_string((uintptr_t)this)); }
			Object(Filesystem* parent, const char* name) : _parent(parent) { mem("Filesystem::Data object created, this: " + std::to_string((uintptr_t)this)); }
			virtual ~Object() { mem("Filesystem::Data object destroyed, this: " + std::to_string((uintptr_t)this)); }
		private:
			virtual inline bool do_file_exists(const char* file_path) { if (_parent == nullptr) return false; return _parent->file_exists(file_path); }
			virtual inline const Bytes do_read_file(const char* file_path) { if (_parent == nullptr) return {Bytes::NONE}; return _parent->read_file(file_path); }
			virtual inline bool do_write_file(const Bytes& data, const char* file_path) { if (_parent == nullptr) return false; return  _parent->write_file(data, file_path); }
			virtual inline bool do_remove_file(const char* file_path) { if (_parent == nullptr) return false; return _parent->remove_file(file_path); }
			virtual inline bool do_rename_file(const char* from_file_path, const char* to_file_path) { if (_parent == nullptr) return false; return _parent->rename_file(from_file_path, to_file_path); }
			virtual inline bool do_create_directory(const char* directory_path) { if (_parent == nullptr) return false; return _parent->create_directory(directory_path); }
		private:
			Filesystem* _parent = nullptr;
		friend class Filesystem;
		};
		std::shared_ptr<Object> _object;
		bool _creator = false;

	friend class OS;
	};

}