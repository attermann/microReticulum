//#define NDEBUG

#include "Test/Test.h"

#include "Reticulum.h"
#include "Identity.h"
#include "Destination.h"
#include "Packet.h"
#include "Transport.h"
#include "Interface.h"
#include "Log.h"
#include "Bytes.h"
#include "Type.h"
#include "Interfaces/UDPInterface.h"
#include "Utilities/OS.h"

#ifdef ARDUINO
#include <Arduino.h>
#endif

#include <stdlib.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <map>
#include <functional>
//#include <sstream>


#ifndef NDEBUG
//#define RUN_TESTS
#endif
#define RUN_RETICULUM
//#define RETICULUM_PACKET_TEST

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
	TestInterface(const char* name) : RNS::Interface(name) {
		IN(true);
		OUT(true);
	}
	virtual ~TestInterface() {
		name("deleted");
	}
	virtual void processIncoming(const RNS::Bytes& data) {
		RNS::extreme("TestInterface.processIncoming: data: " + data.toHex());
	}
	virtual void processOutgoing(const RNS::Bytes& data) {
		RNS::extreme("TestInterface.processOutgoing: data: " + data.toHex());
	}
	virtual inline std::string toString() const { return "TestInterface[" + name() + "]"; }
};

class TestLoopbackInterface : public RNS::Interface {
public:
	TestLoopbackInterface(RNS::Interface& loopback_interface) : RNS::Interface("TestLoopbackInterface"), _loopback_interface(loopback_interface) {
		IN(true);
		OUT(true);
	}
	TestLoopbackInterface(RNS::Interface& loopback_interface, const char* name) : RNS::Interface(name), _loopback_interface(loopback_interface) {
		IN(true);
		OUT(true);
	}
	virtual ~TestLoopbackInterface() {
		name("deleted");
	}
	virtual void processIncoming(const RNS::Bytes& data) {
		RNS::extreme("TestLoopbackInterface.processIncoming: data: " + data.toHex());
		_loopback_interface.processOutgoing(data);
	}
	virtual void processOutgoing(const RNS::Bytes& data) {
		RNS::extreme("TestLoopbackInterface.processOutgoing: data: " + data.toHex());
		_loopback_interface.processIncoming(data);
	}
	virtual inline std::string toString() const { return "TestLoopbackInterface[" + name() + "]"; }
private:
	RNS::Interface& _loopback_interface;
};

class TestOutInterface : public RNS::Interface {
public:
	TestOutInterface() : RNS::Interface("TestOutInterface") {
		OUT(true);
		IN(false);
	}
	TestOutInterface(const char* name) : RNS::Interface(name) {
		OUT(true);
		IN(false);
	}
	virtual ~TestOutInterface() {
		name("(deleted)");
	}
	virtual void processOutgoing(const RNS::Bytes& data) {
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
	TestInInterface(const char* name) : RNS::Interface(name) {
		OUT(false);
		IN(true);
	}
	virtual ~TestInInterface() {
		name("(deleted)");
	}
	virtual void processIncoming(const RNS::Bytes& data) {
		RNS::head("TestInInterface.processIncoming: data: " + data.toHex(), RNS::LOG_INFO);
		RNS::Interface::processIncoming(data);
	}
	virtual inline std::string toString() const { return "TestInInterface[" + name() + "]"; }
};

// Test AnnounceHandler
class ExampleAnnounceHandler : public RNS::AnnounceHandler {
public:
	ExampleAnnounceHandler(const char* aspect_filter = nullptr) : AnnounceHandler(aspect_filter) {}
	virtual ~ExampleAnnounceHandler() {}
	virtual void received_announce(const RNS::Bytes& destination_hash, const RNS::Identity& announced_identity, const RNS::Bytes& app_data) {
		RNS::info("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
		RNS::info("ExampleAnnounceHandler: destination hash: " + destination_hash.toHex());
		if (announced_identity) {
			RNS::info("ExampleAnnounceHandler: announced identity hash: " + announced_identity.hash().toHex());
			RNS::info("ExampleAnnounceHandler: announced identity app data: " + announced_identity.app_data().toHex());
		}
        if (app_data) {
			RNS::info("ExampleAnnounceHandler: app data text: \"" + app_data.toString() + "\"");
		}
		RNS::info("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
	}
};

// Test packet receive callback
void onPacket(const RNS::Bytes& data, const RNS::Packet& packet) {
	RNS::info("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
	RNS::head("onPacket: data: " + data.toHex(), RNS::LOG_INFO);
	RNS::head("onPacket: data string: \"" + data.toString() + "\"", RNS::LOG_INFO);
	//RNS::head("onPacket: " + packet.debugString(), RNS::LOG_EXTREME);
	RNS::info("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
}


#if defined(RUN_RETICULUM)
RNS::Reticulum reticulum({RNS::Type::NONE});

//TestInterface interface;
TestOutInterface outinterface;
TestInInterface ininterface;
TestLoopbackInterface loopinterface(ininterface);
RNS::Interfaces::UDPInterface udpinterface;

//ExampleAnnounceHandler announce_handler((const char*)"example_utilities.announcesample.fruits");
//RNS::HAnnounceHandler announce_handler(new ExampleAnnounceHandler("example_utilities.announcesample.fruits"));
RNS::HAnnounceHandler announce_handler(new ExampleAnnounceHandler());
#endif

#if defined(RUN_TESTS)
void run_tests() {

	try {

		RNS::extreme("Running tests...");
		RNS::LogLevel loglevel = RNS::loglevel();
		//RNS::loglevel(RNS::LOG_WARNING);
		RNS::loglevel(RNS::LOG_EXTREME);
		test();
		//testReference();
		//testCrypto();
		RNS::loglevel(loglevel);
		RNS::extreme("Finished running tests");
		//return;
	}
	catch (std::exception& e) {
		RNS::error(std::string("!!! Exception in test: ") + e.what() + " !!!");
	}

}
#endif

#if defined(RUN_RETICULUM)
void setup_reticulum() {
	RNS::info("Setting up Reticulum...");

	try {

		//std::stringstream test;
		// !!! just adding this single stringstream alone (not even using it) adds a whopping 17.1% !!!
		// !!! JUST SAY NO TO STRINGSTREAM !!!

		// 18.5% completely empty program

		// 21.8% baseline here with serial

		RNS::head("Creating Reticulum instance...", RNS::LOG_EXTREME);
		//RNS::Reticulum reticulum;
		reticulum = RNS::Reticulum();
		//return;
		// 21.9% (+0.1%)

		RNS::head("Registering Interface instances with Transport...", RNS::LOG_EXTREME);
		//RNS::Transport::register_interface(interface);
		RNS::Transport::register_interface(outinterface);
		RNS::Transport::register_interface(ininterface);
		RNS::Transport::register_interface(loopinterface);
		RNS::Transport::register_interface(udpinterface);

		RNS::head("Starting UDPInterface...", RNS::LOG_EXTREME);
		udpinterface.start();

		RNS::head("Creating Identity instance...", RNS::LOG_EXTREME);
		RNS::Identity identity;
		// 22.6% (+0.7%)

		RNS::head("Creating Destination instance...", RNS::LOG_EXTREME);
		RNS::Destination destination(identity, RNS::Type::Destination::IN, RNS::Type::Destination::SINGLE, "app", "aspects");
		// 23.0% (+0.4%)


		destination.set_proof_strategy(RNS::Type::Destination::PROVE_ALL);

		RNS::head("Registering announce handler with Transport...", RNS::LOG_EXTREME);
		RNS::Transport::register_announce_handler(announce_handler);

		RNS::head("Announcing destination...", RNS::LOG_EXTREME);
		//destination.announce(RNS::bytesFromString(fruits[rand() % 7]));
		// test path
		//destination.announce(RNS::bytesFromString(fruits[rand() % 7]), true, nullptr, RNS::bytesFromString("test_tag"));
		// test packet send
		destination.announce(RNS::bytesFromString(fruits[rand() % 7]));
		// 23.9% (+0.8%)

#if defined (RETICULUM_PACKET_TEST)
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
		RNS::Packet recv_packet({RNS::Type::NONE}, send_packet.raw());
		recv_packet.unpack();
		RNS::extreme("Test recv_packet: " + recv_packet.debugString());

		RNS::head("Spoofing recv packet to destination...", RNS::LOG_EXTREME);
		destination.receive(recv_packet);
#endif

	}
	catch (std::exception& e) {
		RNS::error(std::string("!!! Exception in setup_reticulum: ") + e.what() + " !!!");
	}
}

void teardown_reticulum() {
	RNS::info("Tearing down Reticulum...");

	try {

		RNS::head("Deregistering announce handler with Transport...", RNS::LOG_EXTREME);
		RNS::Transport::deregister_announce_handler(announce_handler);

		RNS::head("Deregistering Interface instances with Transport...", RNS::LOG_EXTREME);
		RNS::Transport::deregister_interface(udpinterface);
		RNS::Transport::deregister_interface(loopinterface);
		RNS::Transport::deregister_interface(ininterface);
		RNS::Transport::deregister_interface(outinterface);
		//RNS::Transport::deregister_interface(interface);

	}
	catch (std::exception& e) {
		RNS::error(std::string("!!! Exception in teardown_reticulum: ") + e.what() + " !!!");
	}
}
#endif

void setup() {

#ifdef ARDUINO
	Serial.begin(115200);
	Serial.print("Hello from T-Beam on PlatformIO!\n");
#endif

	RNS::loglevel(RNS::LOG_EXTREME);

/*
	{
		//RNS::Reticulum reticulum_test;
		RNS::Destination destination_test({RNS::Type::NONE}, RNS::Type::Destination::IN, RNS::Type::Destination::SINGLE, "test", "test");
	}
*/

#if defined(RUN_TESTS)
	run_tests();
#endif

#if defined(RUN_RETICULUM)
	setup_reticulum();
#endif

#ifdef ARDUINO
	Serial.print("Goodbye from T-Beam on PlatformIO!\n");
#endif
}

void loop() {

#if defined(RUN_RETICULUM)
	reticulum.loop();
	udpinterface.loop();
#endif

}

int main(void) {
	printf("Hello from Native on PlatformIO!\n");

	setup();

	//while (true) {
	//	loop();
	//}

	printf("Goodbye from Native on PlatformIO!\n");
}
