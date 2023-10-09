//#define NDEBUG

#include "Test/Test.h"

#include "Reticulum.h"
#include "Identity.h"
#include "Destination.h"
#include "Packet.h"
#include "Bytes.h"

#ifndef NATIVE
#include <Arduino.h>
#endif

#include <stdlib.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <map>
//#include <sstream>

// Let's define an app name. We'll use this for all
// destinations we create. Since this basic example
// is part of a range of example utilities, we'll put
// them all within the app namespace "example_utilities"
const char* APP_NAME = "example_utilities";

// We initialise two lists of strings to use as app_data
const char* fruits[] = {"Peach", "Quince", "Date", "Tangerine", "Pomelo", "Carambola", "Grape"};
const char* noble_gases[] = {"Helium", "Neon", "Argon", "Krypton", "Xenon", "Radon", "Oganesson"};


void setup() {

#ifndef NATIVE
	Serial.begin(115200);
	Serial.print("Hello from T-Beam on PlatformIO!\n");
#endif

	try {

#ifndef NDEBUG
		RNS::loglevel(RNS::LOG_WARNING);
		//RNS::loglevel(RNS::LOG_EXTREME);
		test();
#endif

		//std::stringstream test;
		// !!! just adding this single stringstream alone (not even using it) adds a whopping 17.1% !!!
		// !!! JUST SAY NO TO STRINGSTREAM !!!

		RNS::loglevel(RNS::LOG_EXTREME);

		// 18.5% completely empty program

		// 21.8% baseline here with serial

		RNS::Reticulum reticulum;
		// 21.9% (+0.1%)

		RNS::Identity identity;
		// 22.6% (+0.7%)

		RNS::Destination destination(identity, RNS::Destination::IN, RNS::Destination::SINGLE, "test", "context");
		// 23.0% (+0.4%)

		//destination.announce(RNS::bytesFromString(fruits[rand() % 7]));
		// test path
		//destination.announce(RNS::bytesFromString(fruits[rand() % 7]), true, nullptr, RNS::bytesFromString("test_tag"));
		// test packet send
		destination.announce(RNS::bytesFromString(fruits[rand() % 7]));
		// 23.9% (+0.8%)

		RNS::Packet send_packet(destination, "The quick brown fox jumps over the lazy dog");
		send_packet.pack();
		RNS::extreme("Test send_packet packet: " + send_packet.toString());

		// test packet receive
		RNS::Packet recv_packet(RNS::Destination::NONE, send_packet._raw);
		recv_packet.unpack();
		RNS::extreme("Test recv_packet packet: " + recv_packet.toString());

		destination.receive(recv_packet);

		//zdestination.set_proof_strategy(RNS::Destination::PROVE_ALL);

		//zannounce_handler = ExampleAnnounceHandler(
		//z    aspect_filter="example_utilities.announcesample.fruits";
		//z)

		//zRNS::Transport.register_announce_handler(announce_handler);

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

	setup();

	//while (true) {
	//	loop();
	//}

	printf("Goodbye from Native on PlatformIO!\n");
}
