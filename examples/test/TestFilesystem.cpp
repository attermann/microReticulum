//#include <unity.h>

#include <Utilities/OS.h>
#include "Log.h"

#include <assert.h>

using namespace RNS;
using namespace RNS::Utilities;

void testListDirectory() {

	size_t pre_memory = OS::heap_available();
	info("testListDirectory: pre-mem: " + std::to_string(pre_memory));

	for (int i = 0; i < 1; i++) {
		std::list<std::string> files;
		files = OS::list_directory("/");
		for (auto& file : files) {
			info("FILE: " + file);
		}
	}

	size_t post_memory = OS::heap_available();
	size_t diff_memory = (int)pre_memory - (int)post_memory;
	info("testListDirectory: post-mem: " + std::to_string(post_memory));
	info("testListDirectory: diff-mem: " + std::to_string(diff_memory));
	assert(diff_memory == 0);
}

void testFilesystem() {
	RNS::head("Running testFilesystem...", RNS::LOG_EXTREME);

	testListDirectory();

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
