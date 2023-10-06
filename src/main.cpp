//#define NDEBUG

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

#include <assert.h>

// Let's define an app name. We'll use this for all
// destinations we create. Since this basic example
// is part of a range of example utilities, we'll put
// them all within the app namespace "example_utilities"
const char* APP_NAME = "example_utilities";

// We initialise two lists of strings to use as app_data
const char* fruits[] = {"Peach", "Quince", "Date", "Tangerine", "Pomelo", "Carambola", "Grape"};
const char* noble_gases[] = {"Helium", "Neon", "Argon", "Krypton", "Xenon", "Radon", "Oganesson"};

void testMap()
{
	const uint8_t prestr[] = "Hello";
	const uint8_t poststr[] = "World";

	RNS::Bytes prebuf(prestr, 5);
	assert(prebuf.size() == 5);
	assert(memcmp(prebuf.data(), "Hello", prebuf.size()) == 0);

	RNS::Bytes postbuf(poststr, 5);
	assert(postbuf.size() == 5);
	assert(memcmp(postbuf.data(), "World", postbuf.size()) == 0);

	std::map<RNS::Bytes, std::string> map;
	map.insert({prebuf, "hello"});
	map.insert({postbuf, "world"});
	assert(map.size() == 2);

	auto preit = map.find(prebuf);
	assert(preit != map.end());
	assert((*preit).second.compare("hello") == 0);
	//if (preit != map.end()) {
	//	RNS::log(std::string("found prebuf: ") + (*preit).second);
	//}

	auto postit = map.find(postbuf);
	assert(postit != map.end());
	assert((*postit).second.compare("world") == 0);
	//if (postit != map.end()) {
	//	RNS::log(std::string("found postbuf: ") + (*postit).second);
	//}

	const uint8_t newstr[] = "World";
	RNS::Bytes newbuf(newstr, 5);
	assert(newbuf.size() == 5);
	assert(memcmp(newbuf.data(), "World", newbuf.size()) == 0);
	auto newit = map.find(newbuf);
	assert(newit != map.end());
	assert((*newit).second.compare("world") == 0);
	//if (newit != map.end()) {
	//	RNS::log(std::string("found newbuf: ") + (*newit).second);
	//}

	std::string str = map["World"];
	assert(str.size() == 5);
	assert(str.compare("world") == 0);
}

void testBytes() {

	RNS::Bytes bytes;
	assert(!bytes);
	assert(bytes.size() == 0);
	assert(bytes.empty() == true);
	assert(bytes.data() == nullptr);

	const uint8_t prestr[] = "Hello";
	const uint8_t poststr[] = " World";

	RNS::Bytes prebuf(prestr, 5);
	assert(prebuf);
	assert(prebuf.size() == 5);
	assert(memcmp(prebuf.data(), "Hello", prebuf.size()) == 0);
	assert(!(bytes == prebuf));
	assert(bytes != prebuf);
	assert(bytes < prebuf);

	RNS::Bytes postbuf(poststr, 6);
	assert(postbuf);
	assert(postbuf.size() == 6);
	assert(memcmp(postbuf.data(), " World", postbuf.size()) == 0);
	assert(!(postbuf == bytes));
	assert(postbuf != bytes);
	assert(postbuf > bytes);

	assert(!(prebuf == postbuf));
	assert(prebuf != postbuf);

	//if (prebuf == postbuf) {
	//	RNS::log("bytess are the same");
	//}
	//else {
	//	RNS::log("bytess are different");
	//}

	bytes += prebuf + postbuf;
	assert(bytes.size() == 11);
	assert(memcmp(bytes.data(), "Hello World", bytes.size()) == 0);
	//RNS::log("assign bytes: " + bytes.toString());
	//RNS::log("assign prebuf: " + prebuf.toString());
	//RNS::log("assign postbuf: " + postbuf.toString());

	bytes = "Foo";
	assert(bytes.size() == 3);
	assert(memcmp(bytes.data(), "Foo", bytes.size()) == 0);

	bytes = prebuf + postbuf;
	assert(bytes.size() == 11);
	assert(memcmp(bytes.data(), "Hello World", bytes.size()) == 0);

	// stream into empty bytes
	{
		RNS::Bytes strmbuf;
		strmbuf << prebuf << postbuf;
		//RNS::extreme("stream strmbuf: " + strmbuf.toString());
		//RNS::extreme("stream prebuf: " + prebuf.toString());
		//RNS::extreme("stream postbuf: " + postbuf.toString());
		assert(strmbuf.size() == 11);
		assert(memcmp(strmbuf.data(), "Hello World", strmbuf.size()) == 0);
		assert(prebuf.size() == 5);
		assert(memcmp(prebuf.data(), "Hello", prebuf.size()) == 0);
		assert(postbuf.size() == 6);
		assert(memcmp(postbuf.data(), " World", postbuf.size()) == 0);
	}

	// stream into populated bytes
	{
		RNS::Bytes strmbuf("Stream ");
		assert(strmbuf);
		assert(strmbuf.size() == 7);
		assert(memcmp(strmbuf.data(), "Stream ", strmbuf.size()) == 0);

		strmbuf << prebuf << postbuf;
		//RNS::extreme("stream strmbuf: " + strmbuf.toString());
		//RNS::extreme("stream prebuf: " + prebuf.toString());
		//RNS::extreme("stream postbuf: " + postbuf.toString());
		assert(strmbuf.size() == 18);
		assert(memcmp(strmbuf.data(), "Stream Hello World", strmbuf.size()) == 0);
		assert(prebuf.size() == 5);
		assert(memcmp(prebuf.data(), "Hello", prebuf.size()) == 0);
		assert(postbuf.size() == 6);
		assert(memcmp(postbuf.data(), " World", postbuf.size()) == 0);
	}

	// stream with assignment
	// (this is a known and correct but perhaps unexpected and non-intuitive side-effect of assignment with stream)
	{
		RNS::Bytes strmbuf = prebuf << postbuf;
		//RNS::extreme("stream strmbuf: " + strmbuf.toString());
		//RNS::extreme("stream prebuf: " + prebuf.toString());
		//RNS::extreme("stream postbuf: " + postbuf.toString());
		assert(strmbuf.size() == 11);
		assert(memcmp(strmbuf.data(), "Hello World", strmbuf.size()) == 0);
		assert(prebuf.size() == 11);
		assert(memcmp(prebuf.data(), "Hello World", prebuf.size()) == 0);
		assert(postbuf.size() == 6);
		assert(memcmp(postbuf.data(), " World", postbuf.size()) == 0);
	}

	{
		RNS::Bytes nonebuf = nullptr;
		assert(!nonebuf);
		assert(nonebuf.size() == 0);
		assert(nonebuf.data() == nullptr);
	}

}

void testCowBytes() {

	RNS::Bytes bytes1("1");
	assert(bytes1.size() == 1);
	assert(memcmp(bytes1.data(), "1", bytes1.size()) == 0);

	RNS::Bytes bytes2(bytes1);
	assert(bytes2.size() == 1);
	assert(memcmp(bytes2.data(), "1", bytes2.size()) == 0);
	assert(bytes2.data() == bytes1.data());

	RNS::Bytes bytes3(bytes2);
	assert(bytes3.size() == 1);
	assert(memcmp(bytes3.data(), "1", bytes3.size()) == 0);
	assert(bytes3.data() == bytes2.data());

	//RNS::log("pre bytes1 ptr: " + std::to_string((uint32_t)bytes1.data()) + " data: " + bytes1.toString());
	//RNS::log("pre bytes2 ptr: " + std::to_string((uint32_t)bytes2.data()) + " data: " + bytes2.toString());
	//RNS::log("pre bytes3 ptr: " + std::to_string((uint32_t)bytes3.data()) + " data: " + bytes3.toString());

	//bytes1.append("mississippi");
	//assert(bytes1.size() == 12);
	//assert(memcmp(bytes1.data(), "1mississippi", bytes1.size()) == 0);
	//assert(bytes1.data() != bytes2.data());

	bytes2.append("mississippi");
	assert(bytes2.size() == 12);
	assert(memcmp(bytes2.data(), "1mississippi", bytes2.size()) == 0);
	assert(bytes2.data() != bytes1.data());

	bytes3.assign("mississippi");
	assert(bytes3.size() == 11);
	assert(memcmp(bytes3.data(), "mississippi", bytes3.size()) == 0);
	assert(bytes3.data() != bytes2.data());

	//RNS::log("post bytes1 ptr: " + std::to_string((uint32_t)bytes1.data()) + " data: " + bytes1.toString());
	//RNS::log("post bytes2 ptr: " + std::to_string((uint32_t)bytes2.data()) + " data: " + bytes2.toString());
	//RNS::log("post bytes3 ptr: " + std::to_string((uint32_t)bytes3.data()) + " data: " + bytes3.toString());
}

void testObjects() {

	RNS::Reticulum reticulum_default;
	assert(reticulum_default);

	RNS::Reticulum reticulum_none(RNS::Reticulum::NONE);
	assert(!reticulum_none);

	RNS::Reticulum reticulum_default_copy(reticulum_default);
	assert(reticulum_default_copy);

	RNS::Reticulum reticulum_none_copy(reticulum_none);
	assert(!reticulum_none_copy);

}

void testBytesConversion() {

	{
		RNS::Bytes bytes("Hello World");
		std::string hex = bytes.toHex();
		RNS::extreme("text: \"" + bytes.toString() + "\" upper hex: \"" + hex + "\"");
		assert(hex.length() == 22);
		assert(hex.compare("48656C6C6F20576F726C64") == 0);
	}
	{
		RNS::Bytes bytes("Hello World");
		std::string hex = bytes.toHex(false);
		RNS::extreme("text: \"" + bytes.toString() + "\" lower hex: \"" + hex + "\"");
		assert(hex.length() == 22);
		assert(hex.compare("48656c6c6f20576f726c64") == 0);
	}
	{
		std::string hex("48656C6C6F20576F726C64");
		RNS::Bytes bytes;
		bytes.assignHex(hex.c_str());
		std::string text = bytes.toString();
		RNS::extreme("hex: \"" + hex + "\" text: \"" + text + "\"");
		assert(text.length() == 11);
		assert(text.compare("Hello World") == 0);
	}
	{
		std::string hex("48656c6c6f20576f726c64");
		RNS::Bytes bytes;
		bytes.assignHex(hex.c_str());
		std::string text = bytes.toString();
		RNS::extreme("hex: \"" + hex + "\" text: \"" + text + "\"");
		assert(text.length() == 11);
		assert(text.compare("Hello World") == 0);

		bytes.assignHex(hex.c_str());
		text = bytes.toString();
		RNS::extreme("hex: \"" + hex + "\" text: \"" + text + "\"");
		assert(text.length() == 11);
		assert(text.compare("Hello World") == 0);

		bytes.appendHex(hex.c_str());
		text = bytes.toString();
		RNS::extreme("hex: \"" + hex + "\" text: \"" + text + "\"");
		assert(text.length() == 22);
		assert(text.compare("Hello WorldHello World") == 0);
	}

}


/*
void announceLoop(RNS::Destination &destination_1, RNS::Destination &destination_2) {
	// Let the user know that everything is ready
	RNS::log("Announce example running, hit enter to manually send an announce (Ctrl-C to quit)");

	// We enter a loop that runs until the users exits.
	// If the user hits enter, we will announce our server
	// destination on the network, which will let clients
	// know how to create messages directed towards it.
	//while (true) {
		//zentered = input();
		RNS::log("Sending announce...");
		
		// Randomly select a fruit
		//const char* fruit = fruits[rand() % sizeof(fruits)];
		const char* fruit = fruits[rand() % 7];
		//RNS::log(fruit);
		RNS::log(std::string("fruit: ") + fruit);

		// Send the announce including the app data
		if (destination_1) {
			//destination_1.announce(RNS::bytesFromString(fruit));
			// CBA TEST path
			destination_1.announce(RNS::bytesFromString(fruit), true, nullptr, RNS::bytesFromString("test_tag"));
			//zRNS::log(std::string("Sent announce from ") + RNS::prettyhexrep(destination_1->_hash) +" ("+ (const char*)destination_1->_name + ")");
		}

		// Randomly select a noble gas
		//const char* noble_gas = noble_gases[rand() % sizeof(noble_gas)];
		const char* noble_gas = noble_gases[rand() % 7];
		//RNS::log(noble_gas);
		RNS::log(std::string("noble_gas: ") + noble_gas);

		// Send the announce including the app data
		if (destination_2) {
			destination_2.announce(RNS::bytesFromString(noble_gas));
			//zRNS::log(std::string("Sent announce from ") + RNS::prettyhexrep(destination_2->_hash) + " (" + destination_2->_name + ")");
		}

#ifndef NATIVE
		delay(1000);
#else
		usleep(1000000);
#endif
	//}
}

// This initialisation is executed when the program is started
void program_setup() {

	// We must first initialise Reticulum
	RNS::Reticulum reticulum;

	// Randomly create a new identity for our example
	RNS::Identity identity;

	// Using the identity we just created, we create two destinations
	// in the "example_utilities.announcesample" application space.
	//
	// Destinations are endpoints in Reticulum, that can be addressed
	// and communicated with. Destinations can also announce their
	// existence, which will let the network know they are reachable
	// and autoomatically create paths to them, from anywhere else
	// in the network.
	RNS::Destination destination_1(identity, RNS::Destination::IN, RNS::Destination::SINGLE, APP_NAME, "announcesample.fruits");
	// CBA TEST no identity
	//RNS::Destination destination_1(RNS::Identity::NONE, RNS::Destination::IN, RNS::Destination::SINGLE, APP_NAME, "announcesample.fruits");

	//RNS::Destination *destination_2(identity, RNS::Destination::IN, RNS::Destination::SINGLE, APP_NAME, "announcesample.noble_gases");
	RNS::Destination destination_2(RNS::Destination::NONE);

	// We configure the destinations to automatically prove all
	// packets adressed to it. By doing this, RNS will automatically
	// generate a proof for each incoming packet and transmit it
	// back to the sender of that packet. This will let anyone that
	// tries to communicate with the destination know whether their
	// communication was received correctly.
	//zdestination_1->set_proof_strategy(RNS::Destination::PROVE_ALL);
	//zdestination_2->set_proof_strategy(RNS::Destination::PROVE_ALL);

	// We create an announce handler and configure it to only ask for
	// announces from "example_utilities.announcesample.fruits".
	// Try changing the filter and see what happens.
	//zannounce_handler = ExampleAnnounceHandler(
	//z    aspect_filter="example_utilities.announcesample.fruits";
	//z)

	// We register the announce handler with Reticulum
	//zRNS::Transport.register_announce_handler(announce_handler);

	// Everything's ready!
	// Let's hand over control to the announce loop
	announceLoop(destination_1, destination_2);
}
*/

#ifndef NATIVE

void setup() {

	Serial.begin(115200);
	Serial.print("Hello from T-Beam on PlatformIO!\n");

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

	destination.announce(RNS::bytesFromString("fruit"), true, nullptr, RNS::bytesFromString("test_tag"));
	// 23.9% (+0.8%)

#ifndef NDEBUG
	// begin with logging turned down for unit tests
	RNS::loglevel(RNS::LOG_WARNING);
	//RNS::loglevel(RNS::LOG_EXTREME);

	testMap();
	testBytes();
	testCowBytes();
	testObjects();
	testBytesConversion();

	// 24.8% (+0.9%)
#endif

	// increase logging for functional tests
	RNS::loglevel(RNS::LOG_EXTREME);

	//program_setup();

	Serial.print("Goodbye from T-Beam on PlatformIO!\n");
}

void loop() {
}

#else

int main(int argc, char **argv) {
	printf("Hello from Native on PlatformIO!\n");

	program_setup();

	printf("Goodbye from Native on PlatformIO!\n");
}

#endif
