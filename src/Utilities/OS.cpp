#include "OS.h"

#include "../Log.h"

//#ifdef ARDUINO
#if 0
#ifdef __arm__
// should use uinstd.h to define sbrk but Due causes a conflict
//extern "C" char* sbrk(int incr);
#else
extern char *__brkval;
#endif
#endif

using namespace RNS;
using namespace RNS::Utilities;

/*static*/ Filesystem OS::_filesystem = {Type::NONE};
/*static*/ uint64_t OS::_time_offset = 0;

/*static*/ void OS::register_filesystem(Filesystem& filesystem) {
	TRACE("Registering filesystem...");
	OS::_filesystem = filesystem;
}

/*static*/ void OS::deregister_filesystem() {
	TRACE("Deregistering filesystem...");
	OS::_filesystem = {Type::NONE};
}

/*static*/ bool OS::file_exists(const char* file_path) {
	if (!_filesystem) {
		WARNING("file_exists: filesystem not registered");
		throw std::runtime_error("Filesystem has not been registered");
	}
	return _filesystem.do_file_exists(file_path);
}

/*static*/ size_t OS::read_file(const char* file_path, Bytes& data) {
	if (!_filesystem) {
		throw std::runtime_error("Filesystem has not been registered");
	}
	return _filesystem.do_read_file(file_path, data);
}

/*static*/ size_t OS::write_file(const char* file_path, const Bytes& data) {
	if (!_filesystem) {
		throw std::runtime_error("Filesystem has not been registered");
	}
	return _filesystem.do_write_file(file_path, data);
}

/*static*/ bool OS::remove_file(const char* file_path) {
	if (!_filesystem) {
		throw std::runtime_error("Filesystem has not been registered");
	}
	return _filesystem.do_remove_file(file_path);
}

/*static*/ bool OS::rename_file(const char* from_file_path, const char* to_file_path) {
	if (!_filesystem) {
		throw std::runtime_error("Filesystem has not been registered");
	}
	return _filesystem.do_rename_file(from_file_path, to_file_path);
}

/*static*/ bool OS::directory_exists(const char* directory_path) {
	if (!_filesystem) {
		throw std::runtime_error("Filesystem has not been registered");
	}
	return _filesystem.do_directory_exists(directory_path);
}

/*static*/ bool OS::create_directory(const char* directory_path) {
	if (!_filesystem) {
		throw std::runtime_error("Filesystem has not been registered");
	}
	return _filesystem.do_create_directory(directory_path);
}

/*static*/ bool OS::remove_directory(const char* directory_path) {
	if (!_filesystem) {
		throw std::runtime_error("Filesystem has not been registered");
	}
	return _filesystem.do_remove_directory(directory_path);
}

/*static*/ std::list<std::string> OS::list_directory(const char* directory_path) {
	if (!_filesystem) {
		throw std::runtime_error("Filesystem has not been registered");
	}
	return _filesystem.do_list_directory(directory_path);
}

/*static*/ size_t OS::storage_size() {
	if (!_filesystem) {
		throw std::runtime_error("Filesystem has not been registered");
	}
	return _filesystem.do_storage_size();
}

/*static*/ size_t OS::storage_available() {
	if (!_filesystem) {
		throw std::runtime_error("Filesystem has not been registered");
	}
	return _filesystem.do_storage_available();
}

/*
size_t maxContiguousAllocation() {
	// Brute-force determine maximum allocation size
	//const size_t block_size = 256;
	const size_t block_size = 32;
	size_t block_count;
	for(block_count = 1; ; block_count++) {
		void* ptr = malloc(block_count * block_size);
		if (ptr == nullptr) {
			break;
		}
		free(ptr);
	}
	return (block_count - 1) * block_size;
}
*/

/*static*/ size_t OS::heap_size() {
#if defined(ESP32)
	return ESP.getHeapSize();
#elif defined(ARDUINO_ARCH_NRF52) || defined(ARDUINO_NRF52_ADAFRUIT)
	return dbgHeapTotal();
#else
	return 0;
#endif
}

/*static*/ size_t OS::heap_available() {
#if defined(ESP32)
	return ESP.getFreeHeap();
	//return ESP.getMaxAllocHeap();
#elif defined(ARDUINO_ARCH_NRF52) || defined(ARDUINO_NRF52_ADAFRUIT)
	return dbgHeapFree();
#else
	return 0;
#endif
}
