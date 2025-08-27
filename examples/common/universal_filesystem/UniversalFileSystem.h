#pragma once

#include <FileSystem.h>
#include <FileStream.h>
#include <Bytes.h>

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
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

class UniversalFileSystem : public RNS::FileSystemImpl {

public:
	UniversalFileSystem() {}

public:
	static void listDir(const char* dir);

public:
	virtual bool init();
	virtual bool file_exists(const char* file_path);
	virtual size_t read_file(const char* file_path, RNS::Bytes& data);
	virtual size_t write_file(const char* file_path, const RNS::Bytes& data);
	virtual RNS::FileStream open_file(const char* file_path, RNS::FileStream::MODE file_mode);
	virtual bool remove_file(const char* file_path);
	virtual bool rename_file(const char* from_file_path, const char* to_file_path);
	virtual bool directory_exists(const char* directory_path);
	virtual bool create_directory(const char* directory_path);
	virtual bool remove_directory(const char* directory_path);
	virtual std::list<std::string> list_directory(const char* directory_path);
	virtual size_t storage_size();
	virtual size_t storage_available();

protected:

#if defined(BOARD_ESP32) || defined(BOARD_NRF52)
	class UniversalFileStream : public RNS::FileStreamImpl {

	private:
		std::unique_ptr<File> _file;
		bool _closed = false;

	public:
		UniversalFileStream(File* file) : RNS::FileStreamImpl(), _file(file) {}
		virtual ~UniversalFileStream() { if (!_closed) close(); }

	public:
		inline virtual const char* name() { return _file->name(); }
		inline virtual size_t size() { return _file->size(); }
		inline virtual void close() { _closed = true; _file->close(); }

		// Print overrides
		inline virtual size_t write(uint8_t byte) { return _file->write(byte); }
		inline virtual size_t write(const uint8_t *buffer, size_t size) { return _file->write(buffer, size); }

		// Stream overrides
		inline virtual int available() { return _file->available(); }
		inline virtual int read() { return _file->read(); }
		inline virtual int peek() { return _file->peek(); }
		inline virtual void flush() { _file->flush(); }

	};
#else
	class UniversalFileStream : public RNS::FileStreamImpl {

	private:
		FILE* _file = nullptr;
		bool _closed = false;
		size_t _available = 0;
		char _filename[1024];

	public:
		UniversalFileStream(FILE* file) : RNS::FileStreamImpl(), _file(file) {
			_available = size();
		}
		virtual ~UniversalFileStream() { if (!_closed) close(); }

	public:
		inline virtual const char* name() {
			assert(_file);
#if 0
			char proclnk[1024];
			snprintf(proclnk, sizeof(proclnk), "/proc/self/fd/%d", fileno(_file));
			int r = readlink(proclnk, _filename, sizeof(_filename));
			if (r < 0) {
				return nullptr);
			}
			_filename[r] = '\0';
			return _filename;
#elif 0
			if (fcntl(fd, F_GETPATH, _filename) < 0) {
				rerturn nullptr;
			}
			return _filename;
#else
			return nullptr;
#endif
		}
		inline virtual size_t size() {
			assert(_file);
			struct stat st;
			fstat(fileno(_file), &st);
			return st.st_size;
		}
		inline virtual void close() {
			assert(_file);
			_closed = true;
			fclose(_file);
			_file = nullptr;
		}

		// Print overrides
		inline virtual size_t write(uint8_t byte) {
			assert(_file);
			int ch = fputc(byte, _file);
			if (ch == EOF) {
				return 0;
			}
			++_available;
			return 1;
		}
		inline virtual size_t write(const uint8_t *buffer, size_t size) {
			assert(_file);
			size_t wrote = fwrite(buffer, sizeof(uint8_t), size, _file);
			_available += wrote;
			return wrote;
		}

		// Stream overrides
		inline virtual int available() {
#if 0
			assert(_file);
			int size = 0;
			ioctl(fileno(_file), FIONREAD, &size);
			TRACEF("FileStream::available: %d", size);
			return size;
#else
			return _available;
#endif
		}
		inline virtual int read() {
			if (_available <= 0) {
				return EOF;
			}
			assert(_file);
			int ch = fgetc(_file);
			if (ch == EOF) {
				return ch;
			}
			--_available;
			//TRACEF("FileStream::read: %c", ch);
			return ch;
		}
		inline virtual int peek() {
			if (_available <= 0) {
				return EOF;
			}
			assert(_file);
			int ch = fgetc(_file);
			ungetc(ch, _file);
			TRACEF("FileStream::peek: %c", ch);
			return ch;
		}
		inline virtual void flush() {
			assert(_file);
			fflush(_file);
			TRACE("FileStream::flush");
		}

	};
#endif

};
