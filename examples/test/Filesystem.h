#pragma once

#include <Filesystem.h>
#include <Bytes.h>

class Filesystem : public RNS::Filesystem {

public:
	bool init();

protected:
	virtual bool file_exists(const char* file_path);
	virtual size_t read_file(const char* file_path, RNS::Bytes& data);
	virtual size_t write_file(const char* file_path, const RNS::Bytes& data);
	virtual bool remove_file(const char* file_path);
	virtual bool rename_file(const char* from_file_path, const char* to_file_path);
	virtual bool directory_exists(const char* directory_path);
	virtual bool create_directory(const char* directory_path);
	virtual bool remove_directory(const char* directory_path);
	virtual std::list<std::string> list_directory(const char* directory_path);
	virtual size_t storage_size();
	virtual size_t storage_available();

};
