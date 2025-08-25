#include <unity.h>

#include <string.h>
#include <unistd.h>
#include <time.h>
#include <stdint.h>
#include <sys/time.h>
#include <stdio.h>

void setUp(void) {
    // set stuff up here before each test
}

void tearDown(void) {
    // clean stuff up here after each test
}

#ifndef ARDUINO
unsigned long millis() {
	timeval time;
	::gettimeofday(&time, NULL);
	return (uint64_t)(time.tv_sec * 1000) + (uint64_t)(time.tv_usec / 1000);
	return 0;
}
#endif

#ifdef ARDUINO
void testsleep(float seconds) { delay((uint32_t)(seconds * 1000)); }
#else
void testsleep(float seconds) { timespec time; time.tv_sec = (time_t)(seconds); time.tv_nsec = (seconds - (float)time.tv_sec) * 1000000000; ::nanosleep(&time, nullptr); }
#endif

uint64_t timeOffset = 0;

uint64_t ltime() {
	// handle roll-over of 32-bit millis (approx. 49 days)
	static uint32_t low32, high32;
	uint32_t new_low32 = millis();
	if (new_low32 < low32) high32++;
	low32 = new_low32;
	return ((uint64_t)high32 << 32 | low32) + timeOffset;
}

void testTime() {
	//uint64_t start = RNS::Utilities::OS::ltime();
	//RNS::Utilities::OS::sleep(0.1);
	//TEST_ASSERT_TRUE((RNS::Utilities::OS::ltime() - start) > 100);
	uint64_t start = ltime();
	testsleep(0.1);
	uint64_t end = ltime();
	TEST_ASSERT_TRUE((end - start) > 50);
	TEST_ASSERT_TRUE((end - start) < 200);
}

void testConvert() {
	//uint64_t time = ltime();
	//printf("time: %lld\n", time);

	uint64_t num = 1234567890;
	TEST_ASSERT_EQUAL_UINT64(1234567890, num);

	char str[16];
	snprintf(str, 16, "%lld", num);
	TEST_ASSERT_EQUAL_STRING("1234567890", str);

	char* buf = (char*)&num;

	//uint64_t newnum = (uint64_t)(*buf);
	uint64_t newnum = *(uint64_t*)buf;
	TEST_ASSERT_EQUAL_UINT64(1234567890, newnum);
}

int runUnityTests(void) {
    UNITY_BEGIN();
    RUN_TEST(testTime);
    RUN_TEST(testConvert);
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
