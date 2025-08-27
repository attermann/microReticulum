#include <unity.h>

#include "../common/filesystem/FileSystem.h"

#include <Utilities/OS.h>
#include <Utilities/Crc.h>
#include <Log.h>
#include <fstream>
#include <ostream>
#include <iostream>

#ifdef ARDUINO
const char test_file_path[] = "/test_file";
const char test_stream_path[] = "/test_stream";
#else
const char test_file_path[] = "test_file";
const char test_stream_path[] = "test_stream";
#endif


void writeFile(const char* file_path) {

	size_t pre_memory = RNS::Utilities::OS::heap_available();
	TRACEF("testWriteFile: pre-mem: %u", pre_memory);

	{
		RNS::Bytes data("test");
		size_t wrote = RNS::Utilities::OS::write_file(file_path, data);
		TEST_ASSERT_EQUAL_size_t(4, wrote);
	}

	size_t post_memory = RNS::Utilities::OS::heap_available();
	size_t diff_memory = (int)pre_memory - (int)post_memory;
	TRACEF("testWriteFile: post-mem: %u", post_memory);
	TRACEF("testWriteFile: diff-mem: %u", diff_memory);
	TEST_ASSERT_EQUAL_size_t(0, diff_memory);

}

void readFile(const char* file_path) {

	size_t pre_memory = RNS::Utilities::OS::heap_available();
	TRACEF("testReadFile: pre-mem: %u", pre_memory);

	{
		RNS::Bytes data;
		size_t read = RNS::Utilities::OS::read_file(file_path, data);
		TEST_ASSERT_EQUAL_size_t(4, read);
		TEST_ASSERT_EQUAL_STRING("test", data.toString().c_str());
	}

	size_t post_memory = RNS::Utilities::OS::heap_available();
	size_t diff_memory = (int)pre_memory - (int)post_memory;
	TRACEF("testReadFile: post-mem: %u", post_memory);
	TRACEF("testReadFile: diff-mem: %u", diff_memory);
	TEST_ASSERT_EQUAL_size_t(0, diff_memory);

}

void overwriteFile(const char* file_path) {

	size_t pre_memory = RNS::Utilities::OS::heap_available();
	TRACEF("testOverwriteFile: pre-mem: %u", pre_memory);

	// overwrite
	{
		RNS::Bytes data("test");
		size_t wrote = RNS::Utilities::OS::write_file(file_path, data);
		TEST_ASSERT_EQUAL_size_t(4, wrote);
	}

	// read
	{
		RNS::Bytes data;
		size_t read = RNS::Utilities::OS::read_file(file_path, data);
		TEST_ASSERT_EQUAL_size_t(4, read);
		TEST_ASSERT_EQUAL_STRING("test", data.toString().c_str());
	}

	size_t post_memory = RNS::Utilities::OS::heap_available();
	size_t diff_memory = (int)pre_memory - (int)post_memory;
	TRACEF("testOverwriteFile: post-mem: %u", post_memory);
	TRACEF("testOverwriteFile: diff-mem: %u", diff_memory);
	TEST_ASSERT_EQUAL_size_t(0, diff_memory);

}

void removeFile(const char* file_path) {

	size_t pre_memory = RNS::Utilities::OS::heap_available();
	TRACEF("testRemoveFile: pre-mem: %u", pre_memory);

	TEST_ASSERT_TRUE(RNS::Utilities::OS::remove_file(file_path));

	size_t post_memory = RNS::Utilities::OS::heap_available();
	size_t diff_memory = (int)pre_memory - (int)post_memory;
	TRACEF("testRemoveFile: post-mem: %u", post_memory);
	TRACEF("testRemoveFile: diff-mem: %u", diff_memory);
	TEST_ASSERT_EQUAL_size_t(0, diff_memory);

}

void writeFileStream(const char* file_path) {

	size_t pre_memory = RNS::Utilities::OS::heap_available();
	TRACEF("testWriteFileStream: pre-mem: %u", pre_memory);

	{
		RNS::Bytes data("stream");
		RNS::FileStream stream = RNS::Utilities::OS::open_file(file_path, RNS::FileStream::MODE_WRITE);
		TEST_ASSERT_TRUE(stream);
		TRACE("testWriteFileStream: writing to file...");
		size_t wrote = stream.write(data.data(), data.size());
		TRACEF("testWriteFileStream: wrote: %u", wrote);
		// Auto-closes
		//stream.close();
		TEST_ASSERT_EQUAL_size_t(6, wrote);
		uint32_t crc = RNS::Utilities::Crc::crc32(0, data.data(), data.size());
		TRACEF("testWriteFileStream: crc: 0x%X", crc);
		TRACEF("testWriteFileStream: stream crc: 0x%X", stream.crc());
		TEST_ASSERT_EQUAL_UINT32(crc, stream.crc());
	}

	size_t post_memory = RNS::Utilities::OS::heap_available();
	size_t diff_memory = (int)pre_memory - (int)post_memory;
	TRACEF("testWriteFileStream: post-mem: %u", post_memory);
	TRACEF("testWriteFileStream: diff-mem: %u", diff_memory);
	TEST_ASSERT_EQUAL_size_t(0, diff_memory);

}

void readFileStream(const char* file_path) {

	size_t pre_memory = RNS::Utilities::OS::heap_available();
	TRACEF("testReadFileStream: pre-mem: %u", pre_memory);

	{
		RNS::Bytes data;
		RNS::FileStream stream = RNS::Utilities::OS::open_file(file_path, RNS::FileStream::MODE_READ);
		TEST_ASSERT_TRUE(stream);
		size_t size = stream.size();
		TRACEF("testReadFileStream: size: %u", size);
		TRACE("testReadFileStream: reading from file...");
		size_t read = stream.readBytes(data.writable(size), size);
		TRACEF("testReadFileStream: read: %u", read);
		// Auto-closes
		//stream.close();
		TEST_ASSERT_EQUAL_size_t(6, read);
		TEST_ASSERT_EQUAL_INT(0, data.compare("stream"));
		uint32_t crc = RNS::Utilities::Crc::crc32(0, data.data(), data.size());
		TRACEF("testWriteFileStream: crc: 0x%X", crc);
		TRACEF("testWriteFileStream: stream crc: 0x%X", stream.crc());
		TEST_ASSERT_EQUAL_UINT32(crc, stream.crc());
	}

	size_t post_memory = RNS::Utilities::OS::heap_available();
	size_t diff_memory = (int)pre_memory - (int)post_memory;
	TRACEF("testReadFileStream: post-mem: %u", post_memory);
	TRACEF("testReadFileStream: diff-mem: %u", diff_memory);
	TEST_ASSERT_EQUAL_size_t(0, diff_memory);

}


void testListDirectory() {

	size_t pre_memory = RNS::Utilities::OS::heap_available();
	INFO("testListDirectory: pre-mem: " + std::to_string(pre_memory));

	{
		for (int i = 0; i < 1; i++) {
			std::list<std::string> files;
			files = RNS::Utilities::OS::list_directory("/");
			for (auto& file : files) {
				INFO("FILE: " + file);
			}
		}
	}

	size_t post_memory = RNS::Utilities::OS::heap_available();
	size_t diff_memory = (int)pre_memory - (int)post_memory;
	INFO("testListDirectory: post-mem: " + std::to_string(post_memory));
	INFO("testListDirectory: diff-mem: " + std::to_string(diff_memory));
	TEST_ASSERT_EQUAL_size_t(0, diff_memory);
}

void testWriteFile() {
	writeFile(test_file_path);
}

void testReadFile() {
	readFile(test_file_path);
}

void testOverwriteFile() {
	overwriteFile(test_file_path);
}

void testRemoveFile(const char* file_path) {
	removeFile(test_file_path);
}

void testReadNonexistantFile() {

	size_t pre_memory = RNS::Utilities::OS::heap_available();
	TRACEF("testReadNonexistantFile: pre-mem: %u", pre_memory);

	{
		RNS::Bytes data;
		size_t read = RNS::Utilities::OS::read_file("./foo", data);
		TEST_ASSERT_EQUAL_size_t(0, read);
	}

	size_t post_memory = RNS::Utilities::OS::heap_available();
	size_t diff_memory = (int)pre_memory - (int)post_memory;
	TRACEF("testReadNonexistantFile: post-mem: %u", post_memory);
	TRACEF("testReadNonexistantFile: diff-mem: %u", diff_memory);
	TEST_ASSERT_EQUAL_size_t(0, diff_memory);

}

void testWriteFail() {

	size_t pre_memory = RNS::Utilities::OS::heap_available();
	TRACEF("testWriteFail: pre-mem: %u", pre_memory);

	{
		RNS::Bytes data("test");
		size_t wrote = RNS::Utilities::OS::write_file("", data);
		TEST_ASSERT_EQUAL_size_t(4, wrote);
	}

	size_t post_memory = RNS::Utilities::OS::heap_available();
	size_t diff_memory = (int)pre_memory - (int)post_memory;
	TRACEF("testWriteFail: post-mem: %u", post_memory);
	TRACEF("testWriteFail: diff-mem: %u", diff_memory);
	TEST_ASSERT_EQUAL_size_t(0, diff_memory);

}

void testWriteFileStream() {
	writeFileStream(test_stream_path);
}

void testReadFileStream() {
	readFileStream(test_stream_path);
}

void testStdStream() {
	std::ofstream file("./stream");
 	if(file.is_open()) {
		file << "Hello world!" << std::endl;
		file.flush();
		file.close();
	}
	TEST_ASSERT_TRUE(RNS::Utilities::OS::file_exists("./stream"));
	RNS::Utilities::OS::remove_file("./stream");
}

void testCacheWrite() {

	// CBA Attempt to reproduce failure to open file for write that is leaking mmeory
	if (!RNS::Utilities::OS::directory_exists("/cache")) {
		RNS::Utilities::OS::create_directory("/cache");
	}
	writeFile("./cache/test");
	writeFile("./cache/45c50662af11f1b26889efaab547942b45c50662af11f1b26889efaab547942b");
	writeFile("./cache/40fe4ab8105b591cca1ef159d476a7b440fe4ab8105b591cca1ef159d476a7b4");
	writeFile("./cache/c755609f4d0ab75b5119905a032eeb33c755609f4d0ab75b5119905a032eeb33");
	writeFile("./cache/426f66f9aadf866933358b71295f59d0426f66f9aadf866933358b71295f59d0");

}


void setUp(void) {
	// set stuff up here before each test
}

void tearDown(void) {
	// clean stuff up here after each test
}

int runUnityTests(void) {
	UNITY_BEGIN();

	// Suite-level setup
	RNS::FileSystem test_filesystem = new FileSystem();
	((FileSystem*)test_filesystem.get())->init();
	FileSystem::listDir("/");
	FileSystem::listDir("/cache");
	RNS::Utilities::OS::register_filesystem(test_filesystem);

	size_t pre_memory = RNS::Utilities::OS::heap_available();
	TRACEF("testFileSystem: pre-mem: %u", pre_memory);

	// CBA Seems that first call to write_file allocates some memory (at least with NRF52 LittleFS) so get that out of the way first
	{
		RNS::Bytes data("foo");
		RNS::Utilities::OS::write_file("./foo", data);
		RNS::Utilities::OS::remove_file("./foo");
	}

	size_t post_memory = RNS::Utilities::OS::heap_available();
	size_t diff_memory = (int)pre_memory - (int)post_memory;
	TRACEF("testFileSystem: post-mem: %u", post_memory);
	TRACEF("testFileSystem: diff-mem: %u", diff_memory);

	// Run tests
	RUN_TEST(testListDirectory);
	RUN_TEST(testWriteFile);
	RUN_TEST(testReadFile);
	RUN_TEST(testOverwriteFile);
	RUN_TEST(testReadNonexistantFile);
	//RUN_TEST(testWriteFail();
	RUN_TEST(testWriteFileStream);
	RUN_TEST(testReadFileStream);
	//RUN_TEST(testStdStream);
	//RUN_TEST(testCacheWrite);

	// Suite-level teardown
	removeFile(test_file_path);
	removeFile(test_stream_path);
	TRACEF("testFileSystem: size: %u", test_filesystem.storage_size());
	TRACEF("testFileSystem: available: %u", test_filesystem.storage_available());
	RNS::Utilities::OS::deregister_filesystem();

	return UNITY_END();
}

// For native dev-platform or for some embedded frameworks
int main(void) {
	return runUnityTests();
}

#ifdef ARDUINO
// For Arduino framework
void setup() {
	// Wait ~2 seconds before the Unity test runner
	// establishes connection with a board Serial interface
	delay(2000);
	
	runUnityTests();
}
void loop() {}
#endif

// For ESP-IDF framework
void app_main() {
	runUnityTests();
}
