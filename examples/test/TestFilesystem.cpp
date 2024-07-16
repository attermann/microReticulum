//#include <unity.h>

#include "Filesystem.h"

#include <Utilities/OS.h>
#include "Log.h"

#include <assert.h>

Filesystem test_filesystem;

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
	assert(diff_memory == 0);
}

void testWriteFile(const char* file_path) {

	size_t pre_memory = RNS::Utilities::OS::heap_available();
	TRACEF("testWriteFile: pre-mem: %u", pre_memory);

	{
		RNS::Bytes data("test");
		size_t wrote = RNS::Utilities::OS::write_file(file_path, data);
		//assert(wrote == 4);
	}

	size_t post_memory = RNS::Utilities::OS::heap_available();
	size_t diff_memory = (int)pre_memory - (int)post_memory;
	TRACEF("testWriteFile: post-mem: %u", post_memory);
	TRACEF("testWriteFile: diff-mem: %u", diff_memory);
	assert(diff_memory == 0);

}

void testReadFile(const char* file_path) {

	size_t pre_memory = RNS::Utilities::OS::heap_available();
	TRACEF("testReadFile: pre-mem: %u", pre_memory);

	{
		RNS::Bytes data;
		size_t read = RNS::Utilities::OS::read_file(file_path, data);
		assert(read == 4);
	}

	size_t post_memory = RNS::Utilities::OS::heap_available();
	size_t diff_memory = (int)pre_memory - (int)post_memory;
	TRACEF("testReadFile: post-mem: %u", post_memory);
	TRACEF("testReadFile: diff-mem: %u", diff_memory);
	assert(diff_memory == 0);

}

void testRemoveFile(const char* file_path) {

	size_t pre_memory = RNS::Utilities::OS::heap_available();
	TRACEF("testRemoveFile: pre-mem: %u", pre_memory);

	assert(RNS::Utilities::OS::remove_file(file_path));

	size_t post_memory = RNS::Utilities::OS::heap_available();
	size_t diff_memory = (int)pre_memory - (int)post_memory;
	TRACEF("testRemoveFile: post-mem: %u", post_memory);
	TRACEF("testRemoveFile: diff-mem: %u", diff_memory);
	assert(diff_memory == 0);

}

void testReadNonexistantFile() {

	size_t pre_memory = RNS::Utilities::OS::heap_available();
	TRACEF("testReadNonexistantFile: pre-mem: %u", pre_memory);

	{
		RNS::Bytes data;
		size_t read = RNS::Utilities::OS::read_file("/foo", data);
		assert(read == 0);
	}

	size_t post_memory = RNS::Utilities::OS::heap_available();
	size_t diff_memory = (int)pre_memory - (int)post_memory;
	TRACEF("testReadNonexistantFile: post-mem: %u", post_memory);
	TRACEF("testReadNonexistantFile: diff-mem: %u", diff_memory);
	assert(diff_memory == 0);

}

void testWriteFail() {

	size_t pre_memory = RNS::Utilities::OS::heap_available();
	TRACEF("testWriteFail: pre-mem: %u", pre_memory);

	{
		RNS::Bytes data("test");
		size_t wrote = RNS::Utilities::OS::write_file("", data);
		assert(wrote == 4);
	}

	size_t post_memory = RNS::Utilities::OS::heap_available();
	size_t diff_memory = (int)pre_memory - (int)post_memory;
	TRACEF("testWriteFail: post-mem: %u", post_memory);
	TRACEF("testWriteFail: diff-mem: %u", diff_memory);
	assert(diff_memory == 0);

}

void testFilesystem() {
	HEAD("Running testFilesystem...", RNS::LOG_TRACE);

	test_filesystem.init();
	test_filesystem.listDir("/");
	test_filesystem.listDir("/cache");
	RNS::Utilities::OS::register_filesystem(test_filesystem);


	size_t pre_memory = RNS::Utilities::OS::heap_available();
	TRACEF("testFilesystem: pre-mem: %u", pre_memory);

	// CBA Seems that first call to write_file allocates some memory (at least with NRF52 LittleFS) so get that out of the way first
	{
		RNS::Bytes data("foo");
		RNS::Utilities::OS::write_file("/foo", data);
		RNS::Utilities::OS::remove_file("/foo");
	}

	size_t post_memory = RNS::Utilities::OS::heap_available();
	size_t diff_memory = (int)pre_memory - (int)post_memory;
	TRACEF("testFilesystem: post-mem: %u", post_memory);
	TRACEF("testFilesystem: diff-mem: %u", diff_memory);


	testListDirectory();
	testWriteFile("/test1");
	testReadFile("/test1");
	//testRemoveFile("/test1");
	testReadNonexistantFile();
	//testWriteFail();

	// CBA Attempt to reproduce failure to open file for write that is leaking mmeory
	if (!RNS::Utilities::OS::directory_exists("/cache")) {
		RNS::Utilities::OS::create_directory("/cache");
	}
	testWriteFile("/cache/test");
	testWriteFile("/cache/45c50662af11f1b26889efaab547942b");
	testWriteFile("/cache/40fe4ab8105b591cca1ef159d476a7b4");
	testWriteFile("/cache/c755609f4d0ab75b5119905a032eeb33");
	testWriteFile("/cache/426f66f9aadf866933358b71295f59d0");

	RNS::Utilities::OS::deregister_filesystem();

}

/*
void setUp(void) {
	// set stuff up here
}

void tearDown(void) {
	// clean stuff up here
}

int runUnityTests(void) {
	UNITY_BEGIN();
	RUN_TEST(testBytes);
	RUN_TEST(testCowBytes);
	RUN_TEST(testBytesConversion);
	RUN_TEST(testMap);
	return UNITY_END();
}

// For native dev-platform or for some embedded frameworks
int main(void) {
	return runUnityTests();
}

// For Arduino framework
void setup() {
	// Wait ~2 seconds before the Unity test runner
	// establishes connection with a board Serial interface
	delay(2000);

	runUnityTests();
}
void loop() {}

// For ESP-IDF framework
void app_main() {
	runUnityTests();
}
*/
