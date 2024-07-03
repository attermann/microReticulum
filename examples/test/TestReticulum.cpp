//#include <unity.h>

#include "Reticulum.h"
#include "Transport.h"
#include "Log.h"

#include <assert.h>

RNS::Reticulum reticulum = {RNS::Type::NONE};

void testDestinationTable() {

}

void testReticulum() {
	RNS::head("Running testReticulum...", RNS::LOG_EXTREME);

    reticulum = RNS::Reticulum();
	reticulum.transport_enabled(true);
	reticulum.probe_destination_enabled(true);
	reticulum.start();

	testDestinationTable();

	reticulum = {RNS::Type::NONE};
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
