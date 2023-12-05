#include "OS.h"

#include "../Log.h"

#ifdef ARDUINO
#include <FS.h>
#include <LittleFS.h>
#else
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

using namespace RNS;
using namespace RNS::Utilities;

/*static*/ void OS::setup() {

#ifdef ARDUINO
	// Setup filesystem
    if (!LittleFS.begin(true)){
        Serial.println("LittleFS Mount Failed");
        return;
    }
#endif

}

/*static*/ bool OS::file_exists(const char* file_path) {
#ifdef ARDUINO
	File file = LittleFS.open(file_path, FILE_READ);
	if (file) {
#else
	FILE* file = fopen(file_path, "r");
	if (file != nullptr) {
#endif
		//RNS::extreme("file_exists: file exists, closing file");
#ifdef ARDUINO
		file.close();
#else
		fclose(file);
#endif
		return true;
	}
	else {
		RNS::extreme("file_exists: failed to open file " + std::string(file_path));
		return false;
	}
}

/*static*/ const Bytes OS::read_file(const char* file_path) {
    Bytes data;
#ifdef ARDUINO
	File file = LittleFS.open(file_path, FILE_READ);
	if (file) {
		size_t size = file.size();
		size_t read = file.readBytes((char*)data.writable(size), size);
#else
	FILE* file = fopen(file_path, "r");
	if (file != nullptr) {
        fseek(file, 0, SEEK_END);
        size_t size = ftell(file);
        rewind(file);
		//size_t read = fread(data.writable(size), size, 1, file);
		size_t read = fread(data.writable(size), 1, size, file);
#endif
		RNS::extreme("read_file: read " + std::to_string(read) + " bytes from file " + std::string(file_path));
		if (read != size) {
			RNS::extreme("read_file: failed to read file " + std::string(file_path));
            data.clear();
		}
		//RNS::extreme("read_file: closing input file");
#ifdef ARDUINO
		file.close();
#else
		fclose(file);
#endif
	}
	else {
		RNS::extreme("read_file: failed to open input file " + std::string(file_path));
	}
    return data;
}

/*static*/ bool OS::write_file(const Bytes& data, const char* file_path) {
    bool success = false;
#ifdef ARDUINO
	File file = LittleFS.open(file_path, FILE_WRITE);
	if (file) {
		size_t wrote = file.write(data.data(), data.size());
#else
	FILE* file = fopen(file_path, "w");
	if (file != nullptr) {
        //size_t wrote = fwrite(data.data(), data.size(), 1, file);
        size_t wrote = fwrite(data.data(), 1, data.size(), file);
#endif
        RNS::extreme("write_file: wrote " + std::to_string(wrote) + " bytes to file " + std::string(file_path));
        if (wrote == data.size()) {
            success = true;
        }
        else {
			RNS::extreme("write_file: failed to write file " + std::string(file_path));
		}
		//RNS::extreme("write_file: closing output file");
#ifdef ARDUINO
		file.close();
#else
		fclose(file);
#endif
	}
	else {
		RNS::extreme("write_file: failed to open output file " + std::string(file_path));
	}
    return success;
}

/*static*/ bool OS::remove_file(const char* file_path) {
#ifdef ARDUINO
	return LittleFS.remove(file_path);
#else
	return (remove(file_path) == 0);
#endif
}

/*static*/ bool OS::rename_file(const char* from_file_path, const char* to_file_path) {
#ifdef ARDUINO
	return LittleFS.rename(from_file_path, to_file_path);
#else
	return (rename(from_file_path, to_file_path) == 0);
#endif
}

/*static*/ bool OS::create_directory(const char* directory_path) {
#ifdef ARDUINO
	return LittleFS.mkdir(directory_path);
#else
	struct stat st = {0};
	if (stat(directory_path, &st) == 0) {
		return true;
	}
	return (mkdir(directory_path, 0700) == 0);
#endif
}
