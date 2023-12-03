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
#include "Interfaces/LoRaInterface.h"
#include "Utilities/OS.h"

#ifdef ARDUINO
#include <Arduino.h>
#else
#include <termios.h>
#include <fcntl.h>
#include <stdio.h>
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
#define UDP_INTERFACE
#define LORA_INTERFACE
//#define RETICULUM_PACKET_TEST

#define USER_BUTTON_PIN 38

// Let's define an app name. We'll use this for all
// destinations we create. Since this basic example
// is part of a range of example utilities, we'll put
// them all within the app namespace "example_utilities"
const char* APP_NAME = "example_utilities";

// We initialise two lists of strings to use as app_data
const char* fruits[] = {"Peach", "Quince", "Date", "Tangerine", "Pomelo", "Carambola", "Grape"};
const char* noble_gases[] = {"Helium", "Neon", "Argon", "Krypton", "Xenon", "Radon", "Oganesson"};

double last_announce = 0.0;
bool send_announce = false;

// Test AnnounceHandler
class ExampleAnnounceHandler : public RNS::AnnounceHandler {
public:
	ExampleAnnounceHandler(const char* aspect_filter = nullptr) : AnnounceHandler(aspect_filter) {}
	virtual ~ExampleAnnounceHandler() {}
	virtual void received_announce(const RNS::Bytes& destination_hash, const RNS::Identity& announced_identity, const RNS::Bytes& app_data) {
		RNS::info("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
		RNS::info("ExampleAnnounceHandler: destination hash: " + destination_hash.toHex());
		if (announced_identity) {
			RNS::info("ExampleAnnounceHandler: announced identity hash: " + announced_identity.hash().toHex());
			RNS::info("ExampleAnnounceHandler: announced identity app data: " + announced_identity.app_data().toHex());
		}
        if (app_data) {
			RNS::info("ExampleAnnounceHandler: app data text: \"" + app_data.toString() + "\"");
		}
		RNS::info("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
	}
};

// Test packet receive callback
void onPacket(const RNS::Bytes& data, const RNS::Packet& packet) {
	RNS::info("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
	RNS::info("onPacket: data: " + data.toHex());
	RNS::info("onPacket: text: \"" + data.toString() + "\"");
	//RNS::extreme("onPacket: " + packet.debugString());
	RNS::info("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
}

// Ping packet receive callback
void onPingPacket(const RNS::Bytes& data, const RNS::Packet& packet) {
	RNS::info("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
	RNS::info("onPingPacket: data: " + data.toHex());
	RNS::info("onPingPacket: text: \"" + data.toString() + "\"");
	//RNS::extreme("onPingPacket: " + packet.debugString());
	RNS::info("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
}


#if defined(RUN_RETICULUM)
RNS::Reticulum reticulum({RNS::Type::NONE});
RNS::Identity identity({RNS::Type::NONE});
RNS::Destination destination({RNS::Type::NONE});

#ifdef UDP_INTERFACE
RNS::Interfaces::UDPInterface udp_interface;
#endif
#ifdef LORA_INTERFACE
RNS::Interfaces::LoRaInterface lora_interface;
#endif

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

void reticulum_announce() {
	if (destination) {
		RNS::head("Announcing destination...", RNS::LOG_EXTREME);
		//destination.announce(RNS::bytesFromString(fruits[RNS::Cryptography::randomnum() % 7]));
		// test path
		//destination.announce(RNS::bytesFromString(fruits[RNS::Cryptography::randomnum() % 7]), true, nullptr, RNS::bytesFromString("test_tag"));
		// test packet send
		destination.announce(RNS::bytesFromString(fruits[RNS::Cryptography::randomnum() % 7]));
	}
}

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
#ifdef UDP_INTERFACE
		udp_interface.mode(RNS::Type::Interface::MODE_GATEWAY);
		RNS::Transport::register_interface(udp_interface);
#endif
#ifdef LORA_INTERFACE
		lora_interface.mode(RNS::Type::Interface::MODE_GATEWAY);
		RNS::Transport::register_interface(lora_interface);
#endif

#ifdef UDP_INTERFACE
		RNS::head("Starting UDPInterface...", RNS::LOG_EXTREME);
		udp_interface.start("some_ssid", "some_password", 2424);
#endif

#ifdef LORA_INTERFACE
		RNS::head("Starting LoRaInterface...", RNS::LOG_EXTREME);
		lora_interface.start();
#endif

		RNS::head("Creating Identity instance...", RNS::LOG_EXTREME);
		// new identity
		//RNS::Identity identity;
		//identity = RNS::Identity();
		// predefined identity
		//RNS::Identity identity(false);
		identity = RNS::Identity(false);
		RNS::Bytes prv_bytes;
#ifdef ARDUINO
		prv_bytes.assignHex("78E7D93E28D55871608FF13329A226CABC3903A357388A035B360162FF6321570B092E0583772AB80BC425F99791DF5CA2CA0A985FF0415DAB419BBC64DDFAE8");
#else
		prv_bytes.assignHex("E0D43398EDC974EBA9F4A83463691A08F4D306D4E56BA6B275B8690A2FBD9852E9EBE7C03BC45CAEC9EF8E78C830037210BFB9986F6CA2DEE2B5C28D7B4DE6B0");
#endif
		identity.load_private_key(prv_bytes);
		// 22.6% (+0.7%)

		RNS::head("Creating Destination instance...", RNS::LOG_EXTREME);
		//RNS::Destination destination(identity, RNS::Type::Destination::IN, RNS::Type::Destination::SINGLE, "app", "aspects");
		destination = RNS::Destination(identity, RNS::Type::Destination::IN, RNS::Type::Destination::SINGLE, "app", "aspects");
		// 23.0% (+0.4%)

		// test data receive packet
		RNS::head("Registering packet callback with Destination...", RNS::LOG_EXTREME);
		destination.set_packet_callback(onPacket);
		destination.set_proof_strategy(RNS::Type::Destination::PROVE_ALL);

		{
			RNS::head("Creating PING Destination instance...", RNS::LOG_EXTREME);
			RNS::Destination ping_destination(identity, RNS::Type::Destination::IN, RNS::Type::Destination::SINGLE, "example_utilities", "echo.request");

			RNS::head("Registering packet callback with PING Destination...", RNS::LOG_EXTREME);
			ping_destination.set_packet_callback(onPingPacket);
			ping_destination.set_proof_strategy(RNS::Type::Destination::PROVE_ALL);
		}

		RNS::head("Registering announce handler with Transport...", RNS::LOG_EXTREME);
		RNS::Transport::register_announce_handler(announce_handler);

/*
		RNS::head("Announcing destination...", RNS::LOG_EXTREME);
		//destination.announce(RNS::bytesFromString(fruits[RNS::Cryptography::randomnum() % 7]));
		// test path
		//destination.announce(RNS::bytesFromString(fruits[RNS::Cryptography::randomnum() % 7]), true, nullptr, RNS::bytesFromString("test_tag"));
		// test packet send
		destination.announce(RNS::bytesFromString(fruits[RNS::Cryptography::randomnum() % 7]));
		// 23.9% (+0.8%)
*/

#if defined (RETICULUM_PACKET_TEST)
		// test data send packet
		RNS::head("Creating send packet...", RNS::LOG_EXTREME);
		RNS::Packet send_packet(destination, "The quick brown fox jumps over the lazy dog");

		RNS::head("Sending send packet...", RNS::LOG_EXTREME);
		send_packet.pack();
		RNS::extreme("Test send_packet: " + send_packet.debugString());

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
#ifdef UDP_INTERFACE
		RNS::Transport::deregister_interface(udp_interface);
#endif
#ifdef LORA_INTERFACE
		RNS::Transport::deregister_interface(lora_interface);
#endif

	}
	catch (std::exception& e) {
		RNS::error(std::string("!!! Exception in teardown_reticulum: ") + e.what() + " !!!");
	}
}
#endif

#ifdef ARDUINO
void userKey(void)
{
	//delay(10);
	if (digitalRead(USER_BUTTON_PIN) == LOW) {
		//Serial.print("T-Beam USER button press\n");
		send_announce = true;
	}
}
#endif

void setup() {

#ifdef ARDUINO
	Serial.begin(115200);
	Serial.print("Hello from T-Beam on PlatformIO!\n");

	// Setup user button
	pinMode(USER_BUTTON_PIN, INPUT);
	attachInterrupt(USER_BUTTON_PIN, userKey, FALLING);  
#endif

	RNS::loglevel(RNS::LOG_EXTREME);
	//RNS::loglevel(RNS::LOG_MEM);

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
#ifdef UDP_INTERFACE
	udp_interface.loop();
#endif
#ifdef LORA_INTERFACE
	lora_interface.loop();
#endif

#ifdef ARDUINO
/*
	if ((RNS::Utilities::OS::time() - last_announce) > 10) {
		reticulum_announce();
		last_announce = RNS::Utilities::OS::time();
	}
*/
#endif
	if (send_announce) {
		reticulum_announce();
		send_announce = false;
	}

#endif

}

#ifndef ARDUINO
int getch( ) {
	termios oldt;
	termios newt;
	tcgetattr( STDIN_FILENO, &oldt );
	newt = oldt;
	newt.c_lflag &= ~( ICANON | ECHO );
	tcsetattr( STDIN_FILENO, TCSANOW, &newt );
    fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);
	int ch = getchar();
	tcsetattr( STDIN_FILENO, TCSANOW, &oldt );
	return ch;
}

int main(void) {
	printf("Hello from Native on PlatformIO!\n");

	setup();

	bool run = true;
	while (run) {
		loop();
		int ch = getch();
		if (ch > 0) {
			switch (ch) {
			case 'a':
				reticulum_announce();
				break;
			case 'q':
				run = false;
				break;
			}
		}
		RNS::Utilities::OS::sleep(0.01);
	}

	printf("Goodbye from Native on PlatformIO!\n");
}
#endif
