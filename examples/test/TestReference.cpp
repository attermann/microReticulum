//#include <unity.h>

#include "Reticulum.h"
#include "Bytes.h"

#include <assert.h>

void testReference() {
	RNS::head("Running testReference...", RNS::LOG_EXTREME);

	RNS::Reticulum reticulum_default;
	assert(reticulum_default);

	RNS::Reticulum reticulum_none({RNS::Type::NONE});
	assert(!reticulum_none);

	RNS::Reticulum reticulum_default_copy(reticulum_default);
	assert(reticulum_default_copy);

	RNS::Reticulum reticulum_none_copy(reticulum_none);
	assert(!reticulum_none_copy);

/*
	RNS::loglevel(RNS::LOG_EXTREME);
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
	RNS::loglevel(RNS::LOG_EXTREME);
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
	RNS::loglevel(RNS::LOG_EXTREME);
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
	RNS::loglevel(RNS::LOG_EXTREME);
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
		RNS::loglevel(RNS::LOG_EXTREME);
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
	RNS::head("Testing map...", RNS::LOG_EXTREME);
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

/*
int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(testReference);
	return UNITY_END();
}
*/
