#include "TestFilesystem.h"

#include <Log.h>

#ifdef ARDUINO
#ifdef ESP32
#include <FS.h>
#include <SPIFFS.h>
#elif NRF52
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>
using namespace Adafruit_LittleFS_Namespace;
#endif
#else
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

using namespace RNS;

#ifdef ARDUINO
void listDir(char * dir){
  Serial.print("DIR: ");
  Serial.println(dir);
#ifdef ESP32
  File root = SPIFFS.open(dir);
  File file = root.openNextFile();
  while(file){
      Serial.print("  FILE: ");
      Serial.println(file.name());
      file = root.openNextFile();
  }
#elif NRF52
  File root = InternalFS.open(dir);
  File file = root.openNextFile();
  while(file){
      Serial.print("  FILE: ");
      Serial.println(file.name());
      file = root.openNextFile();
  }
#endif
}
#endif

TestFilesystem::TestFilesystem() : Filesystem() {
	extreme("TestFilesystem initializing...");

#ifdef ARDUINO
#ifdef ESP32
	// Setup filesystem
	info("SPIFFS mounting filesystem");
	if (!SPIFFS.begin(true, "")){
		error("SPIFFS filesystem mount failed");
		return;
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
	Bytes test;
	if (!OS::write_file(test, "/test")) {
		info("SPIFFS filesystem is being formatted, please wait...");
		SPIFFS.format();
	}
	else {
		OS::remove_file("/test");
	}
	debug("SPIFFS filesystem is ready");
#elif NRF52
	// Initialize Internal File System
	info("InternalFS mounting filesystem");
	InternalFS.begin();
	info("InternalFS filesystem is ready");
#endif
		listDir("/");
#endif

}

/*virtual*/ TestFilesystem::~TestFilesystem() {
}

/*virtual*/ bool TestFilesystem::file_exists(const char* file_path) {
#ifdef ARDUINO
#ifdef ESP32
	File file = SPIFFS.open(file_path, FILE_READ);
	if (file) {
#elif NRF52
	File file(InternalFS);
	file.open(file_path, FILE_O_READ);
	if (file) {
#else
	if (false) {
#endif
#else
	FILE* file = fopen(file_path, "r");
	if (file != nullptr) {
#endif
		//extreme("file_exists: file exists, closing file");
#ifdef ARDUINO
#ifdef ESP32
		file.close();
#elif NRF52
		file.close();
#endif
#else
		fclose(file);
#endif
		return true;
	}
	else {
		error("file_exists: failed to open file " + std::string(file_path));
		return false;
	}
}

/*virtual*/ const Bytes TestFilesystem::read_file(const char* file_path) {
    Bytes data;
#ifdef ARDUINO
#ifdef ESP32
	File file = SPIFFS.open(file_path, FILE_READ);
	if (file) {
		size_t size = file.size();
		size_t read = file.readBytes((char*)data.writable(size), size);
#elif NRF52
	File file(InternalFS);
	file.open(file_path, FILE_O_READ);
	if (file) {
		size_t size = file.size();
		size_t read = file.readBytes((char*)data.writable(size), size);
#else
	if (false) {
		size_t size = 0;
		size_t read = 0;
#endif
#else
	FILE* file = fopen(file_path, "r");
	if (file != nullptr) {
        fseek(file, 0, SEEK_END);
        size_t size = ftell(file);
        rewind(file);
		//size_t read = fread(data.writable(size), size, 1, file);
		size_t read = fread(data.writable(size), 1, size, file);
#endif
		extreme("read_file: read " + std::to_string(read) + " Bytes from file " + std::string(file_path));
		if (read != size) {
			error("read_file: failed to read file " + std::string(file_path));
            data.clear();
		}
		//extreme("read_file: closing input file");
#ifdef ARDUINO
#ifdef ESP32
		file.close();
#elif NRF52
		file.close();
#endif
#else
		fclose(file);
#endif
	}
	else {
		error("read_file: failed to open input file " + std::string(file_path));
	}
    return data;
}

/*virtual*/ bool TestFilesystem::write_file(const Bytes& data, const char* file_path) {
    bool success = false;
#ifdef ARDUINO
#ifdef ESP32
	File file = SPIFFS.open(file_path, FILE_WRITE);
	if (file) {
		size_t wrote = file.write(data.data(), data.size());
#elif NRF52
	File file(InternalFS);
	file.open(file_path, FILE_O_WRITE);
	if (file) {
		size_t wrote = file.write(data.data(), data.size());
#else
	if (false) {
		size_t wrote = 0;
#endif
#else
	FILE* file = fopen(file_path, "w");
	if (file != nullptr) {
        //size_t wrote = fwrite(data.data(), data.size(), 1, file);
        size_t wrote = fwrite(data.data(), 1, data.size(), file);
#endif
        extreme("write_file: wrote " + std::to_string(wrote) + " Bytes to file " + std::string(file_path));
        if (wrote == data.size()) {
            success = true;
        }
        else {
			error("write_file: failed to write file " + std::string(file_path));
		}
		//extreme("write_file: closing output file");
#ifdef ARDUINO
#ifdef ESP32
		file.close();
#elif NRF52
		file.close();
#endif
#else
		fclose(file);
#endif
	}
	else {
		error("write_file: failed to open output file " + std::string(file_path));
	}
    return success;
}

/*virtual*/ bool TestFilesystem::remove_file(const char* file_path) {
#ifdef ARDUINO
#ifdef ESP32
	return SPIFFS.remove(file_path);
#elif NRF52
	return InternalFS.remove(file_path);
#else
	return false;
#endif
#else
	return (remove(file_path) == 0);
#endif
}

/*virtual*/ bool TestFilesystem::rename_file(const char* from_file_path, const char* to_file_path) {
#ifdef ARDUINO
#ifdef ESP32
	return SPIFFS.rename(from_file_path, to_file_path);
#elif NRF52
	return InternalFS.rename(from_file_path, to_file_path);
#else
	return false;
#endif
#else
	return (rename(from_file_path, to_file_path) == 0);
#endif
}

/*virtual*/ bool TestFilesystem::create_directory(const char* directory_path) {
#ifdef ARDUINO
#ifdef ESP32
	if (!SPIFFS.mkdir(directory_path)) {
		error("create_directory: failed to create directorty " + std::string(directory_path));
		return false;
	}
	return true;
#elif NRF52
	if (!InternalFS.mkdir(directory_path)) {
		error("create_directory: failed to create directorty " + std::string(directory_path));
		return false;
	}
	return true;
#else
	return false;
#endif
#else
	struct stat st = {0};
	if (stat(directory_path, &st) == 0) {
		return true;
	}
	return (mkdir(directory_path, 0700) == 0);
#endif
}
