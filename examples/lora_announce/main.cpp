//#define NDEBUG

#include <LoRaInterface.h>
#include <UniversalFileSystem.h>

#include <Reticulum.h>
#include <Identity.h>
#include <Destination.h>
#include <Packet.h>
#include <Transport.h>
#include <Interface.h>
#include <Log.h>
#include <Bytes.h>
#include <Type.h>
#include <Utilities/OS.h>

#ifdef ARDUINO
#include <Arduino.h>
#include <SPIFFS.h>
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

//#define RETICULUM_PACKET_TEST

#define BUTTON_PIN                  38
//#define BUTTON_PIN_MASK             GPIO_SEL_38

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
		INFO("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
		INFO("ExampleAnnounceHandler: destination hash: " + destination_hash.toHex());
		if (announced_identity) {
			INFO("ExampleAnnounceHandler: announced identity hash: " + announced_identity.hash().toHex());
			INFO("ExampleAnnounceHandler: announced identity app data: " + announced_identity.app_data().toHex());
		}
        if (app_data) {
			INFO("ExampleAnnounceHandler: app data text: \"" + app_data.toString() + "\"");
		}
		INFO("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
	}
};

// Test packet receive callback
void onPacket(const RNS::Bytes& data, const RNS::Packet& packet) {
	INFO("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
	INFO("onPacket: data: " + data.toHex());
	INFO("onPacket: text: \"" + data.toString() + "\"");
	//TRACE("onPacket: " + packet.debugString());
	INFO("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
}

// Ping packet receive callback
void onPingPacket(const RNS::Bytes& data, const RNS::Packet& packet) {
	INFO("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
	INFO("onPingPacket: data: " + data.toHex());
	INFO("onPingPacket: text: \"" + data.toString() + "\"");
	//TRACE("onPingPacket: " + packet.debugString());
	INFO("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
}


RNS::Reticulum reticulum({RNS::Type::NONE});
RNS::Interface lora_interface(RNS::Type::NONE);
RNS::FileSystem universal_filesystem(RNS::Type::NONE);
RNS::Identity identity({RNS::Type::NONE});
RNS::Destination destination({RNS::Type::NONE});

LoRaInterface* lora_interface_impl = nullptr;
UniversalFileSystem* universal_filesystem_impl = nullptr;

//ExampleAnnounceHandler announce_handler((const char*)"example_utilities.announcesample.fruits");
//RNS::HAnnounceHandler announce_handler(new ExampleAnnounceHandler("example_utilities.announcesample.fruits"));
RNS::HAnnounceHandler announce_handler(new ExampleAnnounceHandler());

void reticulum_announce() {
	if (destination) {
		HEAD("Announcing destination...", RNS::LOG_TRACE);
		//destination.announce(RNS::bytesFromString(fruits[RNS::Cryptography::randomnum() % 7]));
		// test path
		//destination.announce(RNS::bytesFromString(fruits[RNS::Cryptography::randomnum() % 7]), true, nullptr, RNS::bytesFromString("test_tag"));
		// test packet send
		destination.announce(RNS::bytesFromString(fruits[RNS::Cryptography::randomnum() % 7]));
	}
}

void reticulum_setup() {
	INFO("Setting up Reticulum...");

	try {

		//std::stringstream test;
		// !!! just adding this single stringstream alone (not even using it) adds a whopping 17.1% !!!
		// !!! JUST SAY NO TO STRINGSTREAM !!!

		// 18.5% completely empty program

		// 21.8% baseline here with serial


		HEAD("Registering FileSystem with OS...", RNS::LOG_TRACE);
		universal_filesystem_impl = new UniversalFileSystem();
		universal_filesystem = universal_filesystem_impl;
		universal_filesystem.init();
		RNS::Utilities::OS::register_filesystem(universal_filesystem);

		HEAD("Registering LoRaInterface instances with Transport...", RNS::LOG_TRACE);
		lora_interface_impl = new LoRaInterface();
		lora_interface = lora_interface_impl;
		lora_interface.mode(RNS::Type::Interface::MODE_GATEWAY);
		RNS::Transport::register_interface(lora_interface);

		HEAD("Starting LoRaInterface...", RNS::LOG_TRACE);
		lora_interface_impl->start();

		HEAD("Creating Reticulum instance...", RNS::LOG_TRACE);
		//RNS::Reticulum reticulum;
		reticulum = RNS::Reticulum();
		reticulum.transport_enabled(true);
		reticulum.start();
		//return;
		// 21.9% (+0.1%)

		HEAD("Creating Identity instance...", RNS::LOG_TRACE);
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

		HEAD("Creating Destination instance...", RNS::LOG_TRACE);
		//RNS::Destination destination(identity, RNS::Type::Destination::IN, RNS::Type::Destination::SINGLE, "app", "aspects");
		destination = RNS::Destination(identity, RNS::Type::Destination::IN, RNS::Type::Destination::SINGLE, "app", "aspects");
		// 23.0% (+0.4%)

		// Register DATA packet callback
		HEAD("Registering packet callback with Destination...", RNS::LOG_TRACE);
		destination.set_packet_callback(onPacket);
		destination.set_proof_strategy(RNS::Type::Destination::PROVE_ALL);

		{
			// Register PING packet callback
			HEAD("Creating PING Destination instance...", RNS::LOG_TRACE);
			RNS::Destination ping_destination(identity, RNS::Type::Destination::IN, RNS::Type::Destination::SINGLE, "example_utilities", "echo.request");

			HEAD("Registering packet callback with PING Destination...", RNS::LOG_TRACE);
			ping_destination.set_packet_callback(onPingPacket);
			ping_destination.set_proof_strategy(RNS::Type::Destination::PROVE_ALL);
		}

		HEAD("Registering announce handler with Transport...", RNS::LOG_TRACE);
		RNS::Transport::register_announce_handler(announce_handler);

/*
		HEAD("Announcing destination...", RNS::LOG_TRACE);
		//destination.announce(RNS::bytesFromString(fruits[RNS::Cryptography::randomnum() % 7]));
		// test path
		//destination.announce(RNS::bytesFromString(fruits[RNS::Cryptography::randomnum() % 7]), true, nullptr, RNS::bytesFromString("test_tag"));
		// test packet send
		destination.announce(RNS::bytesFromString(fruits[RNS::Cryptography::randomnum() % 7]));
		// 23.9% (+0.8%)
*/

#if defined (RETICULUM_PACKET_TEST)
		// test data send packet
		HEAD("Creating send packet...", RNS::LOG_TRACE);
		RNS::Packet send_packet(destination, "The quick brown fox jumps over the lazy dog");

		HEAD("Sending send packet...", RNS::LOG_TRACE);
		send_packet.pack();
#ifndef NDEBUG
		TRACE("Test send_packet: " + send_packet.debugString());
#endif

		HEAD("Creating recv packet...", RNS::LOG_TRACE);
		RNS::Packet recv_packet({RNS::Type::NONE}, send_packet.raw());
		recv_packet.unpack();
#ifndef NDEBUG
		TRACE("Test recv_packet: " + recv_packet.debugString());
#endif

		HEAD("Spoofing recv packet to destination...", RNS::LOG_TRACE);
		destination.receive(recv_packet);
#endif

		HEAD("Ready!", RNS::LOG_TRACE);
	}
	catch (std::exception& e) {
		ERRORF("!!! Exception in reticulum_setup: %s", e.what());
	}
}

void reticulum_teardown() {
	INFO("Tearing down Reticulum...");

	RNS::Transport::persist_data();

	try {

		HEAD("Deregistering announce handler with Transport...", RNS::LOG_TRACE);
		RNS::Transport::deregister_announce_handler(announce_handler);

		HEAD("Deregistering Interface instances with Transport...", RNS::LOG_TRACE);
		RNS::Transport::deregister_interface(lora_interface);

	}
	catch (std::exception& e) {
		ERRORF("!!! Exception in reticulum_teardown: %s", e.what());
	}
}

#ifdef ARDUINO
void userKey(void)
{
	//delay(10);
	if (digitalRead(BUTTON_PIN) == LOW) {
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
	pinMode(BUTTON_PIN, INPUT);
	attachInterrupt(BUTTON_PIN, userKey, FALLING);  

	// Setup filesystem
	if (!SPIFFS.begin(true, "")){
		ERROR("SPIFFS filesystem mount failed");
	}
	else {
		DEBUG("SPIFFS filesystem is ready");
	}
#endif

	RNS::loglevel(RNS::LOG_TRACE);
	//RNS::loglevel(RNS::LOG_MEM);

/*
	{
		//RNS::Reticulum reticulum_test;
		RNS::Destination destination_test({RNS::Type::NONE}, RNS::Type::Destination::IN, RNS::Type::Destination::SINGLE, "test", "test");
	}
*/

	reticulum_setup();

#ifdef ARDUINO
	//Serial.print("Goodbye from T-Beam on PlatformIO!\n");
#endif
}

void loop() {

	reticulum.loop();
	lora_interface_impl->loop();

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

	reticulum_teardown();

	printf("Goodbye from Native on PlatformIO!\n");
}
#endif
