#pragma once

#include <Filesystem.h>
#include <Bytes.h>

class TestFilesystem : public RNS::Filesystem {

public:
	TestFilesystem();
	virtual ~TestFilesystem();

protected:
	virtual bool file_exists(const char* file_path);
	virtual const RNS::Bytes read_file(const char* file_path);
	virtual bool write_file(const RNS::Bytes& data, const char* file_path);
	virtual bool remove_file(const char* file_path);
	virtual bool rename_file(const char* from_file_path, const char* to_file_path);
	virtual bool create_directory(const char* directory_path);

};
