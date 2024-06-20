#include "OS.h"

#include "../Log.h"

#ifdef ARDUINO
#ifdef __arm__
// should use uinstd.h to define sbrk but Due causes a conflict
//extern "C" char* sbrk(int incr);
#else
extern char *__brkval;
#endif
#endif

using namespace RNS;
using namespace RNS::Utilities;

/*static*/ Filesystem OS::filesystem = {Type::NONE};
/*static*/ uint64_t OS::timeOffset = 0;

/*static*/ int OS::freeMemory() {
#ifdef ARDUINO
	char top;
#ifdef __arm__
	return &top - reinterpret_cast<char*>(sbrk(0));
#elif defined(CORE_TEENSY) || (ARDUINO > 103 && ARDUINO != 151)
	return &top - __brkval;
#else
	return __brkval ? &top - __brkval : &top - __malloc_heap_start;
#endif
	return -1;
#endif
}

/*static*/ void OS::register_filesystem(Filesystem& filesystem) {
	extreme("Registering filesystem...");
	OS::filesystem = filesystem;
}

/*static*/ void OS::deregister_filesystem() {
	extreme("Deregistering filesystem...");
	OS::filesystem = {Type::NONE};
}

/*static*/ bool OS::file_exists(const char* file_path) {
	if (!filesystem) {
		warning("file_exists: filesystem not registered");
		throw std::runtime_error("Filesystem has not been registered");
	}
	return filesystem.do_file_exists(file_path);
}

/*static*/ const Bytes OS::read_file(const char* file_path) {
	if (!filesystem) {
		throw std::runtime_error("Filesystem has not been registered");
	}
	return filesystem.do_read_file(file_path);
}

/*static*/ bool OS::write_file(const Bytes& data, const char* file_path) {
	if (!filesystem) {
		throw std::runtime_error("Filesystem has not been registered");
	}
	return filesystem.do_write_file(data, file_path);
}

/*static*/ bool OS::remove_file(const char* file_path) {
	if (!filesystem) {
		throw std::runtime_error("Filesystem has not been registered");
	}
	return filesystem.do_remove_file(file_path);
}

/*static*/ bool OS::rename_file(const char* from_file_path, const char* to_file_path) {
	if (!filesystem) {
		throw std::runtime_error("Filesystem has not been registered");
	}
	return filesystem.do_rename_file(from_file_path, to_file_path);
}

/*static*/ bool OS::directory_exists(const char* directory_path) {
	if (!filesystem) {
		throw std::runtime_error("Filesystem has not been registered");
	}
	return filesystem.do_directory_exists(directory_path);
}

/*static*/ bool OS::create_directory(const char* directory_path) {
	if (!filesystem) {
		throw std::runtime_error("Filesystem has not been registered");
	}
	return filesystem.do_create_directory(directory_path);
}

/*static*/ bool OS::remove_directory(const char* directory_path) {
	if (!filesystem) {
		throw std::runtime_error("Filesystem has not been registered");
	}
	return filesystem.do_remove_directory(directory_path);
}

/*static*/ std::list<std::string> OS::list_directory(const char* directory_path) {
	if (!filesystem) {
		throw std::runtime_error("Filesystem has not been registered");
	}
	return filesystem.do_list_directory(directory_path);
}
