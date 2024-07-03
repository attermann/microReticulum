#include "Filesystem.h"

#include <Utilities/OS.h>
#include <Log.h>

#ifdef ARDUINO
#ifdef BOARD_ESP32
//#include <FS.h>
#include <SPIFFS.h>
#elif BOARD_NRF52
//#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>
using namespace Adafruit_LittleFS_Namespace;
#endif
#else
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#ifdef ARDUINO
void listDir(const char* dir){
	Serial.print("DIR: ");
	Serial.println(dir);
#ifdef BOARD_ESP32
	File root = SPIFFS.open(dir);
	if (!root) {
		Serial.println("Failed to opend directory");
		return;
	}
	File file = root.openNextFile();
	while(file){
		Serial.print("  FILE: ");
		Serial.println(file.name());
		file.close();
		file = root.openNextFile();
	}
	root.close();
#elif BOARD_NRF52
	File root = InternalFS.open(dir);
	if (!root) {
		Serial.println("Failed to opend directory");
		return;
	}
	File file = root.openNextFile();
	while(file){
		Serial.print("  FILE: ");
		Serial.println(file.name());
		file.close();
		file = root.openNextFile();
	}
	root.close();
#endif
}
#endif

bool Filesystem::init() {
	TRACE("Filesystem initializing...");

#ifdef ARDUINO
#ifdef BOARD_ESP32
	// Setup filesystem
	RNS::info("SPIFFS mounting filesystem");
	if (!SPIFFS.begin(true, "")){
		RNS::error("SPIFFS filesystem mount failed");
		return false;
	}
	uint32_t size = SPIFFS.totalBytes() / (1024 * 1024);
	Serial.print("size: ");
	Serial.print(size);
	Serial.println(" MB");
	uint32_t used = SPIFFS.usedBytes() / (1024 * 1024);
	Serial.print("used: ");
	Serial.print(used);
	Serial.println(" MB");
	// ensure filesystem is writable and format if not
	RNS::Bytes test;
	if (!write_file("/test", test)) {
		RNS::info("SPIFFS filesystem is being formatted, please wait...");
		SPIFFS.format();
	}
	else {
		remove_file("/test");
	}
	DEBUG("SPIFFS filesystem is ready");
#elif BOARD_NRF52
	// Initialize Internal File System
	RNS::info("InternalFS mounting filesystem");
	InternalFS.begin();
	RNS::info("InternalFS filesystem is ready");
#endif
	listDir("/");
#endif
	return true;
}

/*virtual*/ bool Filesystem::file_exists(const char* file_path) {
#ifdef ARDUINO
#ifdef BOARD_ESP32
	File file = SPIFFS.open(file_path, FILE_READ);
	if (file) {
#elif BOARD_NRF52
	File file(InternalFS);
	if (file.open(file_path, FILE_O_READ)) {
#else
	if (false) {
#endif
#else
	// Native
	FILE* file = fopen(file_path, "r");
	if (file != nullptr) {
#endif
		//TRACE("file_exists: file exists, closing file");
#ifdef ARDUINO
#ifdef BOARD_ESP32
		file.close();
#elif BOARD_NRF52
		file.close();
#endif
#else
		// Native
		fclose(file);
#endif
		return true;
	}
	else {
		RNS::error("file_exists: failed to open file " + std::string(file_path));
		return false;
	}
}

/*virtual*/ size_t Filesystem::read_file(const char* file_path, RNS::Bytes& data) {
	size_t read = 0;
#ifdef ARDUINO
#ifdef BOARD_ESP32
	File file = SPIFFS.open(file_path, FILE_READ);
	if (file) {
		size_t size = file.size();
		read = file.readBytes((char*)data.writable(size), size);
#elif BOARD_NRF52
	File file(InternalFS);
	if (file.open(file_path, FILE_O_READ)) {
		size_t size = file.size();
		read = file.readBytes((char*)data.writable(size), size);
#else
	if (false) {
		size_t size = 0;
#endif
#else
	// Native
	FILE* file = fopen(file_path, "r");
	if (file != nullptr) {
        fseek(file, 0, SEEK_END);
        size_t size = ftell(file);
        rewind(file);
		//size_t read = fread(data.writable(size), size, 1, file);
		size_t read = fread(data.writable(size), 1, size, file);
#endif
		TRACE("read_file: read " + std::to_string(read) + " RNS::Bytes from file " + std::string(file_path));
		if (read != size) {
			RNS::error("read_file: failed to read file " + std::string(file_path));
            data.clear();
		}
		//TRACE("read_file: closing input file");
#ifdef ARDUINO
#ifdef BOARD_ESP32
		file.close();
#elif BOARD_NRF52
		file.close();
#endif
#else
		// Native
		fclose(file);
#endif
	}
	else {
		RNS::error("read_file: failed to open input file " + std::string(file_path));
	}
    return read;
}

/*virtual*/ size_t Filesystem::write_file(const char* file_path, const RNS::Bytes& data) {
    size_t wrote = 0;
#ifdef ARDUINO
#ifdef BOARD_ESP32
	File file = SPIFFS.open(file_path, FILE_WRITE);
	if (file) {
		wrote = file.write(data.data(), data.size());
#elif BOARD_NRF52
	File file(InternalFS);
	if (file.open(file_path, FILE_O_WRITE)) {
		wrote = file.write(data.data(), data.size());
#else
	if (false) {
#endif
#else
	// Native
	FILE* file = fopen(file_path, "w");
	if (file != nullptr) {
        //size_t wrote = fwrite(data.data(), data.size(), 1, file);
        size_t wrote = fwrite(data.data(), 1, data.size(), file);
#endif
        TRACE("write_file: wrote " + std::to_string(wrote) + " RNS::Bytes to file " + std::string(file_path));
        if (wrote < data.size()) {
			RNS::warning("write_file: not all data was written to file " + std::string(file_path));
		}
		//TRACE("write_file: closing output file");
#ifdef ARDUINO
#ifdef BOARD_ESP32
		file.close();
#elif BOARD_NRF52
		file.close();
#endif
#else
		// Native
		fclose(file);
#endif
	}
	else {
		RNS::error("write_file: failed to open output file " + std::string(file_path));
	}
    return wrote;
}

/*virtual*/ bool Filesystem::remove_file(const char* file_path) {
#ifdef ARDUINO
#ifdef BOARD_ESP32
	return SPIFFS.remove(file_path);
#elif BOARD_NRF52
	return InternalFS.remove(file_path);
#else
	return false;
#endif
#else
	// Native
	return (remove(file_path) == 0);
#endif
}

/*virtual*/ bool Filesystem::rename_file(const char* from_file_path, const char* to_file_path) {
#ifdef ARDUINO
#ifdef BOARD_ESP32
	return SPIFFS.rename(from_file_path, to_file_path);
#elif BOARD_NRF52
	return InternalFS.rename(from_file_path, to_file_path);
#else
	return false;
#endif
#else
	// Native
	return (rename(from_file_path, to_file_path) == 0);
#endif
}

/*virtua*/ bool Filesystem::directory_exists(const char* directory_path) {
	RNS::extreme("directory_exists: checking for existence of directory " + std::string(directory_path));
#ifdef ARDUINO
#ifdef BOARD_ESP32
	File file = SPIFFS.open(directory_path, FILE_READ);
	if (file) {
		bool is_directory = file.isDirectory();
		file.close();
		return is_directory;
	}
#elif BOARD_NRF52
	File file(InternalFS);
	if (file.open(directory_path, FILE_O_READ)) {
		bool is_directory = file.isDirectory();
		file.close();
		return is_directory;
	}
#else
	if (false) {
		return false;
	}
#endif
	else {
		return false;
	}
#else
	// Native
	return false;
#endif
}

/*virtual*/ bool Filesystem::create_directory(const char* directory_path) {
#ifdef ARDUINO
#ifdef BOARD_ESP32
	if (!SPIFFS.mkdir(directory_path)) {
		RNS::error("create_directory: failed to create directorty " + std::string(directory_path));
		return false;
	}
	return true;
#elif BOARD_NRF52
	if (!InternalFS.mkdir(directory_path)) {
		RNS::error("create_directory: failed to create directorty " + std::string(directory_path));
		return false;
	}
	return true;
#else
	return false;
#endif
#else
	// Native
	struct stat st = {0};
	if (stat(directory_path, &st) == 0) {
		return true;
	}
	return (mkdir(directory_path, 0700) == 0);
#endif
}

/*virtua*/ bool Filesystem::remove_directory(const char* directory_path) {
	RNS::extreme("remove_directory: removing directory " + std::string(directory_path));
#ifdef ARDUINO
#ifdef BOARD_ESP32
	//if (!LittleFS.rmdir_r(directory_path)) {
	if (!SPIFFS.rmdir(directory_path)) {
		RNS::error("remove_directory: failed to remove directorty " + std::string(directory_path));
		return false;
	}
	return true;
#elif BOARD_NRF52
	if (!InternalFS.rmdir_r(directory_path)) {
		RNS::error("remove_directory: failed to remove directory " + std::string(directory_path));
		return false;
	}
	return true;
#else
	return false;
#endif
#else
	// Native
	return false;
#endif
}

/*virtua*/ std::list<std::string> Filesystem::list_directory(const char* directory_path) {
	RNS::extreme("list_directory: listing directory " + std::string(directory_path));
	std::list<std::string> files;
#ifdef ARDUINO
#ifdef BOARD_ESP32
	File root = SPIFFS.open(directory_path);
#elif BOARD_NRF52
	File root = InternalFS.open(directory_path);
#endif
	if (!root) {
		RNS::error("list_directory: failed to open directory " + std::string(directory_path));
		return files;
	}
	File file = root.openNextFile();
	while (file) {
		if (!file.isDirectory()) {
			char* name = (char*)file.name();
			files.push_back(name);
		}
		// CBA Following close required to avoid leaking memory
		file.close();
		file = root.openNextFile();
	}
	RNS::extreme("list_directory: returning directory listing");
	root.close();
	return files;
#else
	// Native
	return files;
#endif
}


#ifdef ARDUINO

#ifdef BOARD_ESP32

/*virtual*/ size_t Filesystem::storage_size() {
	return SPIFFS.totalBytes();
}

/*virtual*/ size_t Filesystem::storage_available() {
	return (SPIFFS.totalBytes() - SPIFFS.usedBytes());
}

#elif BOARD_NRF52

static int _countLfsBlock(void *p, lfs_block_t block){
	lfs_size_t *size = (lfs_size_t*) p;
	*size += 1;
	return 0;
}

static lfs_ssize_t getUsedBlockCount() {
    lfs_size_t size = 0;
    lfs_traverse(InternalFS._getFS(), _countLfsBlock, &size);
    return size;
}

static int totalBytes() {
	const lfs_config* config = InternalFS._getFS()->cfg;
	return config->block_size * config->block_count;
}

static int usedBytes() {
	const lfs_config* config = InternalFS._getFS()->cfg;
	const int usedBlockCount = getUsedBlockCount();
	return config->block_size * usedBlockCount;
}

/*virtual*/ size_t Filesystem::storage_size() {
	return totalBytes();
}

/*virtual*/ size_t Filesystem::storage_available() {
	return (totalBytes() - usedBytes());
}

#endif

#else

/*virtual*/ size_t Filesystem::storage_size() {
	return 0;
}

/*virtual*/ size_t Filesystem::storage_available() {
	return 0;
}

#endif
