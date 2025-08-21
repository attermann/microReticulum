//#include <unity.h>

#include "Log.h"
#include "Utilities/OS.h"

#include <assert.h>

using namespace RNS::Utilities;

void testTime()
{

    uint64_t start_ltime = OS::ltime();
    double start_dtime = OS::time();

    double sleep_time = 1.23456;
    OS::sleep(sleep_time);

    uint64_t end_ltime = OS::ltime();
    double end_dtime = OS::time();

    double diff_time;

    diff_time = (double)(end_ltime - start_ltime) / 1000.0;
	TRACE(std::string("ltime diff: ") + std::to_string(diff_time));
    assert(diff_time > sleep_time * 0.99);
    assert(diff_time < sleep_time * 1.01);

    diff_time = end_dtime - start_dtime;
	TRACE(std::string("dtime diff: ") + std::to_string(diff_time));
    assert(diff_time > sleep_time * 0.99);
    assert(diff_time < sleep_time * 1.01);

}

void testConvert() {
	//uint64_t time = ltime();
	//printf("time: %lld\n", time);

	uint64_t num = 1234567890;
	//TEST_ASSERT_EQUAL_UINT64(1234567890, num);
	assert(1234567890 == num);

	char str[16];
	snprintf(str, 16, "%lld", num);
	//TEST_ASSERT_EQUAL_STRING("1234567890", str);
	assert(strncmp("1234567890", str, sizeof(str)) == 0);

	char* buf = (char*)&num;

	//uint64_t newnum = (uint64_t)(*buf);
	uint64_t newnum = *(uint64_t*)buf;
	//TEST_ASSERT_EQUAL_UINT64(1234567890, newnum);
	assert(1234567890 == newnum);
}

void testOS() {
	HEAD("Running testOS...", RNS::LOG_TRACE);
	testTime();
	testConvert();
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
	RUN_TEST(testCollections);
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

