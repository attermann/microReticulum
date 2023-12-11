#include "OS.h"

#include "../Log.h"

#ifdef ARDUINO
#include <FS.h>
#include <SPIFFS.h>
#else
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

using namespace RNS;
using namespace RNS::Utilities;

#ifdef ARDUINO
void listDir(char * dir){
  Serial.print("DIR: ");
  Serial.println(dir);
  File root = SPIFFS.open(dir);
  File file = root.openNextFile();
  while(file){
      Serial.print("  FILE: ");
      Serial.println(file.name());
      file = root.openNextFile();
  }
}
#endif

/*static*/ void OS::setup() {

#ifdef ARDUINO
	// Setup filesystem
	if (!SPIFFS.begin(true, "")){
		error("SPIFFS filesystem mount failed");
	}
	else {
		listDir("/");
		debug("SPIFFS filesystem is ready");
	}
#endif

}

/*static*/ bool OS::file_exists(const char* file_path) {
#ifdef ARDUINO
	File file = SPIFFS.open(file_path, FILE_READ);
	if (file) {
#else
	FILE* file = fopen(file_path, "r");
	if (file != nullptr) {
#endif
		//extreme("file_exists: file exists, closing file");
#ifdef ARDUINO
		file.close();
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

/*static*/ const Bytes OS::read_file(const char* file_path) {
    Bytes data;
#ifdef ARDUINO
	File file = SPIFFS.open(file_path, FILE_READ);
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
		extreme("read_file: read " + std::to_string(read) + " bytes from file " + std::string(file_path));
		if (read != size) {
			error("read_file: failed to read file " + std::string(file_path));
            data.clear();
		}
		//extreme("read_file: closing input file");
#ifdef ARDUINO
		file.close();
#else
		fclose(file);
#endif
	}
	else {
		error("read_file: failed to open input file " + std::string(file_path));
	}
    return data;
}

/*static*/ bool OS::write_file(const Bytes& data, const char* file_path) {
    bool success = false;
#ifdef ARDUINO
	File file = SPIFFS.open(file_path, FILE_WRITE);
	if (file) {
		size_t wrote = file.write(data.data(), data.size());
#else
	FILE* file = fopen(file_path, "w");
	if (file != nullptr) {
        //size_t wrote = fwrite(data.data(), data.size(), 1, file);
        size_t wrote = fwrite(data.data(), 1, data.size(), file);
#endif
        extreme("write_file: wrote " + std::to_string(wrote) + " bytes to file " + std::string(file_path));
        if (wrote == data.size()) {
            success = true;
        }
        else {
			error("write_file: failed to write file " + std::string(file_path));
		}
		//extreme("write_file: closing output file");
#ifdef ARDUINO
		file.close();
#else
		fclose(file);
#endif
	}
	else {
		error("write_file: failed to open output file " + std::string(file_path));
	}
    return success;
}

/*static*/ bool OS::remove_file(const char* file_path) {
#ifdef ARDUINO
	return SPIFFS.remove(file_path);
#else
	return (remove(file_path) == 0);
#endif
}

/*static*/ bool OS::rename_file(const char* from_file_path, const char* to_file_path) {
#ifdef ARDUINO
	return SPIFFS.rename(from_file_path, to_file_path);
#else
	return (rename(from_file_path, to_file_path) == 0);
#endif
}

/*static*/ bool OS::create_directory(const char* directory_path) {
#ifdef ARDUINO
	if (SPIFFS.mkdir(directory_path)) {
		return true;
	}
	else {
		error("create_directory: failed to create directorty " + std::string(directory_path));
	}
#else
	struct stat st = {0};
	if (stat(directory_path, &st) == 0) {
		return true;
	}
	return (mkdir(directory_path, 0700) == 0);
#endif
}
