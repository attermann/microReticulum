#include <unity.h>

#include "Reticulum.h"
#include "Bytes.h"

void testReference() {
	HEAD("Running testReference...", RNS::LOG_TRACE);

	RNS::Reticulum reticulum_default;
	TEST_ASSERT_TRUE(reticulum_default);

	RNS::Reticulum reticulum_none({RNS::Type::NONE});
	TEST_ASSERT_FALSE(reticulum_none);

	RNS::Reticulum reticulum_default_copy(reticulum_default);
	TEST_ASSERT_TRUE(reticulum_default_copy);

	RNS::Reticulum reticulum_none_copy(reticulum_none);
	TEST_ASSERT_FALSE(reticulum_none_copy);

/*
	RNS::loglevel(RNS::LOG_TRACE);
	TestInterface testinterface;

	std::set<std::reference_wrapper<RNS::Interface>, std::less<RNS::Interface>> interfaces;
	interfaces.insert(testinterface);
	for (auto iter = interfaces.begin(); iter != interfaces.end(); ++iter) {
		RNS::Interface& interface = (*iter);
		TRACE("Found interface: " + interface.toString());
		RNS::Bytes data;
		const_cast<RNS::Interface&>(interface).on_outgoing(data);
	}
	return 0;
*/
/*
	RNS::loglevel(RNS::LOG_TRACE);
	TestInterface testinterface;

	std::set<std::reference_wrapper<RNS::Interface>, std::less<RNS::Interface>> interfaces;
	interfaces.insert(testinterface);
	for (auto& interface : interfaces) {
		TRACE("Found interface: " + interface.toString());
		RNS::Bytes data;
		const_cast<RNS::Interface&>(interface).on_outgoing(data);
	}
	return 0;
*/
/*
	RNS::loglevel(RNS::LOG_TRACE);
	TestInterface testinterface;

	std::list<std::reference_wrapper<RNS::Interface>> interfaces;
	interfaces.push_back(testinterface);
	for (auto iter = interfaces.begin(); iter != interfaces.end(); ++iter) {
		RNS::Interface& interface = (*iter);
		TRACE("Found interface: " + interface.toString());
		RNS::Bytes data;
		const_cast<RNS::Interface&>(interface).on_outgoing(data);
	}
	return 0;
*/
/*
	RNS::loglevel(RNS::LOG_TRACE);
	TestInterface testinterface;

	std::list<std::reference_wrapper<RNS::Interface>> interfaces;
	interfaces.push_back(testinterface);
	//for (auto& interface : interfaces) {
	for (RNS::Interface& interface : interfaces) {
		TRACE("Found interface: " + interface.toString());
		RNS::Bytes data;
		const_cast<RNS::Interface&>(interface).on_outgoing(data);
	}
	return 0;
*/
/*
	std::list<std::reference_wrapper<RNS::Interface>> interfaces;
	{
		RNS::loglevel(RNS::LOG_TRACE);
		TestInterface testinterface;
		interfaces.push_back(testinterface);
		for (auto iter = interfaces.begin(); iter != interfaces.end(); ++iter) {
			RNS::Interface& interface = (*iter);
			TRACE("1 Found interface: " + interface.toString());
			RNS::Bytes data;
			const_cast<RNS::Interface&>(interface).on_outgoing(data);
		}
	}
	for (auto iter = interfaces.begin(); iter != interfaces.end(); ++iter) {
		RNS::Interface& interface = (*iter);
		TRACE("2 Found interface: " + interface.toString());
		RNS::Bytes data;
		const_cast<RNS::Interface&>(interface).on_outgoing(data);
	}
	return 0;
*/
/*
	HEAD("Testing map...", RNS::LOG_TRACE);
	{
		std::map<RNS::Bytes, RNS::Destination&> destinations;
		destinations.insert({destination.hash(), destination});
		//for (RNS::Destination& destination : destinations) {
		for (auto& [hash, destination] : destinations) {
			TRACE("Iterated destination: " + destination.toString());
		}
		RNS::Bytes hash = destination.hash();
		auto iter = destinations.find(hash);
		if (iter != destinations.end()) {
			RNS::Destination& destination = (*iter).second;
			TRACE("Found destination: " + destination.toString());
		}
		return;
	}
*/

}


void setUp(void) {
	// set stuff up here before each test
}

void tearDown(void) {
	// clean stuff up here after each test
}

int runUnityTests(void) {
	UNITY_BEGIN();
	RUN_TEST(testReference);
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
