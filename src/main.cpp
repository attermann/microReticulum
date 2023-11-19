//#define NDEBUG

#include "Test/Test.h"

#include "Reticulum.h"
#include "Identity.h"
#include "Destination.h"
#include "Packet.h"
#include "Interfaces/Interface.h"
#include "Bytes.h"

#ifndef NATIVE
#include <Arduino.h>
#endif

#include <stdlib.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <map>
#include <functional>
//#include <sstream>

// Let's define an app name. We'll use this for all
// destinations we create. Since this basic example
// is part of a range of example utilities, we'll put
// them all within the app namespace "example_utilities"
const char* APP_NAME = "example_utilities";

// We initialise two lists of strings to use as app_data
const char* fruits[] = {"Peach", "Quince", "Date", "Tangerine", "Pomelo", "Carambola", "Grape"};
const char* noble_gases[] = {"Helium", "Neon", "Argon", "Krypton", "Xenon", "Radon", "Oganesson"};

class TestInterface : public RNS::Interface {
public:
	TestInterface() : RNS::Interface("TestInterface") {
		IN(true);
		OUT(true);
	}
	TestInterface(const char *name) : RNS::Interface(name) {
		IN(true);
		OUT(true);
	}
	virtual ~TestInterface() {
		name("deleted");
	}
	virtual void processIncoming(const RNS::Bytes &data) {
		RNS::extreme("TestInterface.processIncoming: data: " + data.toHex());
	}
	virtual void processOutgoing(const RNS::Bytes &data) {
		RNS::extreme("TestInterface.processOutgoing: data: " + data.toHex());
	}
	virtual inline std::string toString() const { return "TestInterface[" + name() + "]"; }
};

class TestLoopbackInterface : public RNS::Interface {
public:
	TestLoopbackInterface(RNS::Interface &loopback_interface) : RNS::Interface("TestLoopbackInterface"), _loopback_interface(loopback_interface) {
		IN(true);
		OUT(true);
	}
	TestLoopbackInterface(RNS::Interface &loopback_interface, const char *name) : RNS::Interface(name), _loopback_interface(loopback_interface) {
		IN(true);
		OUT(true);
	}
	virtual ~TestLoopbackInterface() {
		name("deleted");
	}
	virtual void processIncoming(const RNS::Bytes &data) {
		RNS::extreme("TestLoopbackInterface.processIncoming: data: " + data.toHex());
		_loopback_interface.processOutgoing(data);
	}
	virtual void processOutgoing(const RNS::Bytes &data) {
		RNS::extreme("TestLoopbackInterface.processOutgoing: data: " + data.toHex());
		_loopback_interface.processIncoming(data);
	}
	virtual inline std::string toString() const { return "TestLoopbackInterface[" + name() + "]"; }
private:
	RNS::Interface &_loopback_interface;
};

class TestOutInterface : public RNS::Interface {
public:
	TestOutInterface() : RNS::Interface("TestOutInterface") {
		OUT(true);
		IN(false);
	}
	TestOutInterface(const char *name) : RNS::Interface(name) {
		OUT(true);
		IN(false);
	}
	virtual ~TestOutInterface() {
		name("(deleted)");
	}
	virtual void processOutgoing(const RNS::Bytes &data) {
		RNS::head("TestOutInterface.processOutgoing: data: " + data.toHex(), RNS::LOG_EXTREME);
		RNS::Interface::processOutgoing(data);
	}
	virtual inline std::string toString() const { return "TestOutInterface[" + name() + "]"; }
};

class TestInInterface : public RNS::Interface {
public:
	TestInInterface() : RNS::Interface("TestInInterface") {
		OUT(false);
		IN(true);
	}
	TestInInterface(const char *name) : RNS::Interface(name) {
		OUT(false);
		IN(true);
	}
	virtual ~TestInInterface() {
		name("(deleted)");
	}
	virtual void processIncoming(const RNS::Bytes &data) {
		RNS::head("TestInInterface.processIncoming: data: " + data.toHex(), RNS::LOG_EXTREME);
		RNS::Interface::processIncoming(data);
	}
	virtual inline std::string toString() const { return "TestInInterface[" + name() + "]"; }
};

void onPacket(const RNS::Bytes &data, const RNS::Packet &packet) {
	RNS::head("onPacket: data: " + data.toHex(), RNS::LOG_EXTREME);
	RNS::head("onPacket: data string: \"" + data.toString() + "\"", RNS::LOG_EXTREME);
	//RNS::head("onPacket: " + packet.debugString(), RNS::LOG_EXTREME);
}

void setup() {

#ifndef NATIVE
	Serial.begin(115200);
	Serial.print("Hello from T-Beam on PlatformIO!\n");
#endif

	try {

#ifndef NDEBUG
		RNS::loglevel(RNS::LOG_WARNING);
		//RNS::loglevel(RNS::LOG_EXTREME);
		RNS::extreme("Running tests...");
		test();
		//testReference();
		//testCrypto();
		RNS::extreme("Finished running tests");
#endif

		//std::stringstream test;
		// !!! just adding this single stringstream alone (not even using it) adds a whopping 17.1% !!!
		// !!! JUST SAY NO TO STRINGSTREAM !!!

		RNS::loglevel(RNS::LOG_EXTREME);

		// 18.5% completely empty program

		// 21.8% baseline here with serial

		RNS::head("Creating Reticulum instance...", RNS::LOG_EXTREME);
		RNS::Reticulum reticulum;
		// 21.9% (+0.1%)

		RNS::head("Creating Interface instances...", RNS::LOG_EXTREME);
		//TestInterface interface;
		TestOutInterface outinterface;
		TestInInterface ininterface;
		TestLoopbackInterface loopinterface(ininterface);

		RNS::head("Registering Interface instances with Transport...", RNS::LOG_EXTREME);
		//RNS::Transport::register_interface(interface);
		RNS::Transport::register_interface(outinterface);
		RNS::Transport::register_interface(ininterface);
		RNS::Transport::register_interface(loopinterface);

		RNS::head("Creating Identity instance...", RNS::LOG_EXTREME);
		RNS::Identity identity;
		// 22.6% (+0.7%)

		RNS::head("Creating Destination instance...", RNS::LOG_EXTREME);
		RNS::Destination destination(identity, RNS::Destination::IN, RNS::Destination::SINGLE, "test", "context");
		// 23.0% (+0.4%)

/*
		RNS::head("Testing map...", RNS::LOG_EXTREME);
		{
			std::map<RNS::Bytes, RNS::Destination&> destinations;
			destinations.insert({destination.hash(), destination});
			//for (RNS::Destination &destination : destinations) {
			for (auto &[hash, destination] : destinations) {
				RNS::extreme("Iterated destination: " + destination.toString());
			}
			RNS::Bytes hash = destination.hash();
			auto iter = destinations.find(hash);
			if (iter != destinations.end()) {
				RNS::Destination &destination = (*iter).second;
				RNS::extreme("Found destination: " + destination.toString());
			}
			return;
		}
*/

		destination.set_proof_strategy(RNS::Destination::PROVE_ALL);

		//zRNS::head("Registering announce handler with Transport...", RNS::LOG_EXTREME);
		//zannounce_handler = ExampleAnnounceHandler(
		//z    aspect_filter="example_utilities.announcesample.fruits";
		//z)
		//zRNS::Transport.register_announce_handler(announce_handler);

		RNS::head("Announcing destination...", RNS::LOG_EXTREME);
		//destination.announce(RNS::bytesFromString(fruits[rand() % 7]));
		// test path
		//destination.announce(RNS::bytesFromString(fruits[rand() % 7]), true, nullptr, RNS::bytesFromString("test_tag"));
		// test packet send
		destination.announce(RNS::bytesFromString(fruits[rand() % 7]));
		// 23.9% (+0.8%)

/*
		// test data send packet
		RNS::head("Creating send packet...", RNS::LOG_EXTREME);
		RNS::Packet send_packet(destination, "The quick brown fox jumps over the lazy dog");

		RNS::head("Sending send packet...", RNS::LOG_EXTREME);
		send_packet.pack();
		RNS::extreme("Test send_packet: " + send_packet.debugString());

		// test data receive packet
		RNS::head("Registering packet callback with Destination...", RNS::LOG_EXTREME);
		destination.set_packet_callback(onPacket);

		RNS::head("Creating recv packet...", RNS::LOG_EXTREME);
		RNS::Packet recv_packet(RNS::Destination::NONE, send_packet.raw());
		recv_packet.unpack();
		RNS::extreme("Test recv_packet: " + recv_packet.debugString());

		RNS::head("Spoofing recv packet to destination...", RNS::LOG_EXTREME);
		destination.receive(recv_packet);
*/

	}
	catch (std::exception& e) {
		RNS::error(std::string("!!! Exception in main: ") + e.what() + " !!!");
	}

#ifndef NATIVE
	Serial.print("Goodbye from T-Beam on PlatformIO!\n");
#endif
}

void loop() {
}

int main(void) {
	printf("Hello from Native on PlatformIO!\n");

/*
	RNS::loglevel(RNS::LOG_EXTREME);
	TestInterface testinterface;

	std::set<std::reference_wrapper<RNS::Interface>, std::less<RNS::Interface>> interfaces;
	interfaces.insert(testinterface);
	for (auto iter = interfaces.begin(); iter != interfaces.end(); ++iter) {
		RNS::Interface &interface = (*iter);
		RNS::extreme("Found interface: " + interface.toString());
		RNS::Bytes data;
		const_cast<RNS::Interface&>(interface).processOutgoing(data);
	}
	return 0;
*/
/*
	RNS::loglevel(RNS::LOG_EXTREME);
	TestInterface testinterface;

	std::set<std::reference_wrapper<RNS::Interface>, std::less<RNS::Interface>> interfaces;
	interfaces.insert(testinterface);
	for (auto &interface : interfaces) {
		RNS::extreme("Found interface: " + interface.toString());
		RNS::Bytes data;
		const_cast<RNS::Interface&>(interface).processOutgoing(data);
	}
	return 0;
*/
/*
	RNS::loglevel(RNS::LOG_EXTREME);
	TestInterface testinterface;

	std::list<std::reference_wrapper<RNS::Interface>> interfaces;
	interfaces.push_back(testinterface);
	for (auto iter = interfaces.begin(); iter != interfaces.end(); ++iter) {
		RNS::Interface &interface = (*iter);
		RNS::extreme("Found interface: " + interface.toString());
		RNS::Bytes data;
		const_cast<RNS::Interface&>(interface).processOutgoing(data);
	}
	return 0;
*/
/*
	RNS::loglevel(RNS::LOG_EXTREME);
	TestInterface testinterface;

	std::list<std::reference_wrapper<RNS::Interface>> interfaces;
	interfaces.push_back(testinterface);
	//for (auto &interface : interfaces) {
	for (RNS::Interface &interface : interfaces) {
		RNS::extreme("Found interface: " + interface.toString());
		RNS::Bytes data;
		const_cast<RNS::Interface&>(interface).processOutgoing(data);
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
			RNS::Interface &interface = (*iter);
			RNS::extreme("1 Found interface: " + interface.toString());
			RNS::Bytes data;
			const_cast<RNS::Interface&>(interface).processOutgoing(data);
		}
	}
	for (auto iter = interfaces.begin(); iter != interfaces.end(); ++iter) {
		RNS::Interface &interface = (*iter);
		RNS::extreme("2 Found interface: " + interface.toString());
		RNS::Bytes data;
		const_cast<RNS::Interface&>(interface).processOutgoing(data);
	}
	return 0;
*/

	setup();

	//while (true) {
	//	loop();
	//}

	printf("Goodbye from Native on PlatformIO!\n");
}
