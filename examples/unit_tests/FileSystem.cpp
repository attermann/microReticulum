#include "FileSystem.h"

#include <Utilities/OS.h>
#include <Log.h>

bool FileSystem::init() {
	TRACE("FileSystem initializing...");

#ifdef ARDUINO
#ifdef BOARD_ESP32
	// Setup FileSystem
	INFO("SPIFFS mounting FileSystem");
	if (!SPIFFS.begin(true, "")){
		ERROR("SPIFFS FileSystem mount failed");
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
	// ensure FileSystem is writable and format if not
	RNS::Bytes test("test");
	if (write_file("/test", test) < 4) {
		INFO("SPIFFS FileSystem is being formatted, please wait...");
		SPIFFS.format();
	}
	else {
		remove_file("/test");
	}
	DEBUG("SPIFFS FileSystem is ready");
#elif BOARD_NRF52
	// Initialize Internal File System
	INFO("InternalFS mounting FileSystem");
	InternalFS.begin();
	INFO("InternalFS FileSystem is ready");
#endif
#endif
	return true;
}

#ifdef ARDUINO
void FileSystem::listDir(const char* dir) {
	Serial.print("DIR: ");
	Serial.println(dir);
#ifdef BOARD_ESP32
	File root = SPIFFS.open(dir);
	if (!root) {
		Serial.println("Failed to opend directory");
		return;
	}
	File file = root.openNextFile();
	while (file) {
		if (file.isDirectory()) {
			Serial.print("  DIR: ");
		}
		else {
			Serial.print("  FILE: ");
		}
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
	while (file) {
		if (file.isDirectory()) {
			Serial.print("  DIR: ");
		}
		else {
			Serial.print("  FILE: ");
		}
		Serial.println(file.name());
		file.close();
		file = root.openNextFile();
	}
	root.close();
#endif
}
#else
void FileSystem::listDir(const char* dir) {
}
#endif

/*virtual*/ bool FileSystem::file_exists(const char* file_path) {
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
		ERROR("file_exists: failed to open file " + std::string(file_path));
		return false;
	}
}

/*virtual*/ size_t FileSystem::read_file(const char* file_path, RNS::Bytes& data) {
	size_t read = 0;
#ifdef ARDUINO
#ifdef BOARD_ESP32
	File file = SPIFFS.open(file_path, FILE_READ);
	if (file) {
		size_t size = file.size();
		read = file.readBytes((char*)data.writable(size), size);
#elif BOARD_NRF52
	//File file(InternalFS);
	//if (file.open(file_path, FILE_O_READ)) {
	File file = InternalFS.open(file_path, FILE_O_READ);
	if (file) {
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
		read = fread(data.writable(size), 1, size, file);
#endif
		TRACE("read_file: read " + std::to_string(read) + " bytes from file " + std::string(file_path));
		if (read != size) {
			ERROR("read_file: failed to read file " + std::string(file_path));
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
		ERROR("read_file: failed to open input file " + std::string(file_path));
	}
    return read;
}

/*virtual*/ size_t FileSystem::write_file(const char* file_path, const RNS::Bytes& data) {
	// CBA TODO Replace remove with working truncation
	remove_file(file_path);
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
        wrote = fwrite(data.data(), 1, data.size(), file);
#endif
        TRACE("write_file: wrote " + std::to_string(wrote) + " bytes to file " + std::string(file_path));
        if (wrote < data.size()) {
			WARNING("write_file: not all data was written to file " + std::string(file_path));
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
		ERROR("write_file: failed to open output file " + std::string(file_path));
	}
    return wrote;
}

/*virtual*/ RNS::FileStream FileSystem::open_file(const char* file_path, RNS::FileStream::MODE file_mode) {
	TRACEF("open_file: opening file %s", file_path);
#ifdef ARDUINO
#ifdef BOARD_ESP32
	const char* mode;
	if (file_mode == RNS::FileStream::MODE_READ) {
		mode = FILE_READ;
	}
	else if (file_mode == RNS::FileStream::MODE_WRITE) {
		mode = FILE_WRITE;
	}
	else if (file_mode == RNS::FileStream::MODE_APPEND) {
		mode = FILE_APPEND;
	}
	else {
		ERRORF("open_file: unsupported mode %d", file_mode);
		return {RNS::Type::NONE};
	}
	TRACEF("open_file: opening file %s in mode %s", file_path, mode);
	//// Using copy constructor to create a File* instead of local
	//File file = SPIFFS.open(file_path, mode);
	//if (!file) {
	File* file = new File(SPIFFS.open(file_path, mode));
	if (file == nullptr || !(*file)) {
		ERRORF("open_file: failed to open output file %s", file_path);
		return {RNS::Type::NONE};
	}
	TRACEF("open_file: successfully opened file %s", file_path);
	return RNS::FileStream(new FileStreamImpl(file));
#elif BOARD_NRF52
	//File file = File(InternalFS);
	File* file = new File(InternalFS);
	int mode;
	if (file_mode == RNS::FileStream::MODE_READ) {
		mode = FILE_O_READ;
	}
	else if (file_mode == RNS::FileStream::MODE_WRITE) {
		mode = FILE_O_WRITE;
		// CBA TODO Replace remove with working truncation
		if (InternalFS.exists(file_path)) {
			InternalFS.remove(file_path);
		}
	}
	else if (file_mode == RNS::FileStream::MODE_APPEND) {
		// CBA This is the default write mode for nrf52 littlefs
		mode = FILE_O_WRITE;
	}
	else {
		ERRORF("open_file: unsupported mode %d", file_mode);
		return {RNS::Type::NONE};
	}
	if (!file->open(file_path, mode)) {
		ERRORF("open_file: failed to open output file %s", file_path);
		return {RNS::Type::NONE};
	}

	// Seek to beginning to overwrite (this is failing on nrf52)
	//if (file_mode == RNS::FileStream::MODE_WRITE) {
	//	file->seek(0);
	//	file->truncate(0);
	//}
	TRACEF("open_file: successfully opened file %s", file_path);
	return RNS::FileStream(new FileStreamImpl(file));
#else
	#warning("unsuppoprted");
	return RNS::FileStream(RNS::Type::NONE);
#endif
#else	// ARDUINO
	// Native
	const char* mode;
	if (file_mode == RNS::FileStream::MODE_READ) {
		mode = "r";
	}
	else if (file_mode == RNS::FileStream::MODE_WRITE) {
		mode = "w";
	}
	else if (file_mode == RNS::FileStream::MODE_APPEND) {
		mode = "a";
	}
	else {
		ERRORF("open_file: unsupported mode %d", file_mode);
		return {RNS::Type::NONE};
	}
	TRACEF("open_file: opening file %s in mode %s", file_path, mode);
	FILE* file = fopen(file_path, mode);
	if (file == nullptr) {
		ERRORF("open_file: failed to open output file %s", file_path);
		return {RNS::Type::NONE};
	}
	TRACEF("open_file: successfully opened file %s", file_path);
	return RNS::FileStream(new FileStreamImpl(file));
#endif
}

/*virtual*/ bool FileSystem::remove_file(const char* file_path) {
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

/*virtual*/ bool FileSystem::rename_file(const char* from_file_path, const char* to_file_path) {
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

/*virtua*/ bool FileSystem::directory_exists(const char* directory_path) {
	TRACE("directory_exists: checking for existence of directory " + std::string(directory_path));
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

/*virtual*/ bool FileSystem::create_directory(const char* directory_path) {
#ifdef ARDUINO
#ifdef BOARD_ESP32
	if (!SPIFFS.mkdir(directory_path)) {
		ERROR("create_directory: failed to create directorty " + std::string(directory_path));
		return false;
	}
	return true;
#elif BOARD_NRF52
	if (!InternalFS.mkdir(directory_path)) {
		ERROR("create_directory: failed to create directorty " + std::string(directory_path));
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

/*virtua*/ bool FileSystem::remove_directory(const char* directory_path) {
	TRACE("remove_directory: removing directory " + std::string(directory_path));
#ifdef ARDUINO
#ifdef BOARD_ESP32
	//if (!LittleFS.rmdir_r(directory_path)) {
	if (!SPIFFS.rmdir(directory_path)) {
		ERROR("remove_directory: failed to remove directorty " + std::string(directory_path));
		return false;
	}
	return true;
#elif BOARD_NRF52
	if (!InternalFS.rmdir_r(directory_path)) {
		ERROR("remove_directory: failed to remove directory " + std::string(directory_path));
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

/*virtua*/ std::list<std::string> FileSystem::list_directory(const char* directory_path) {
	TRACE("list_directory: listing directory " + std::string(directory_path));
	std::list<std::string> files;
#ifdef ARDUINO
#ifdef BOARD_ESP32
	File root = SPIFFS.open(directory_path);
#elif BOARD_NRF52
	File root = InternalFS.open(directory_path);
#endif
	if (!root) {
		ERROR("list_directory: failed to open directory " + std::string(directory_path));
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
	TRACE("list_directory: returning directory listing");
	root.close();
	return files;
#else
	// Native
	return files;
#endif
}


#ifdef ARDUINO

#ifdef BOARD_ESP32

/*virtual*/ size_t FileSystem::storage_size() {
	return SPIFFS.totalBytes();
}

/*virtual*/ size_t FileSystem::storage_available() {
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

/*virtual*/ size_t FileSystem::storage_size() {
	//return totalBytes();
	return InternalFS.totalBytes();
}

/*virtual*/ size_t FileSystem::storage_available() {
	//return (totalBytes() - usedBytes());
	return (InternalFS.totalBytes() - InternalFS.usedBytes());
}

#endif

#else

/*virtual*/ size_t FileSystem::storage_size() {
	return 0;
}

/*virtual*/ size_t FileSystem::storage_available() {
	return 0;
}

#endif
