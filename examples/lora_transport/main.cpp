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
//#include <SPIFFS.h>
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

RNS::Reticulum reticulum({RNS::Type::NONE});
RNS::Interface lora_interface(RNS::Type::NONE);
RNS::FileSystem universal_filesystem(RNS::Type::NONE);
RNS::Identity identity({RNS::Type::NONE});
RNS::Destination destination({RNS::Type::NONE});

LoRaInterface* lora_interface_impl = nullptr;
UniversalFileSystem* universal_filesystem_impl = nullptr;

void reticulum_setup() {
	INFO("Setting up Reticulum...");

	try {

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
		reticulum = RNS::Reticulum();
		reticulum.transport_enabled(true);
		reticulum.probe_destination_enabled(true);
		reticulum.start();

		HEAD("Creating Identity instance...", RNS::LOG_TRACE);
		// new identity
		//identity = RNS::Identity();
		// predefined identity
		identity = RNS::Identity(false);
		RNS::Bytes prv_bytes;
#ifdef ARDUINO
		prv_bytes.assignHex("78E7D93E28D55871608FF13329A226CABC3903A357388A035B360162FF6321570B092E0583772AB80BC425F99791DF5CA2CA0A985FF0415DAB419BBC64DDFAE8");
#else
		prv_bytes.assignHex("E0D43398EDC974EBA9F4A83463691A08F4D306D4E56BA6B275B8690A2FBD9852E9EBE7C03BC45CAEC9EF8E78C830037210BFB9986F6CA2DEE2B5C28D7B4DE6B0");
#endif
		identity.load_private_key(prv_bytes);

		HEAD("RNS Transport Ready!", RNS::LOG_TRACE);
	}
	catch (std::exception& e) {
		ERRORF("!!! Exception in reticulum_setup: %s", e.what());
	}
}

void reticulum_teardown() {
	INFO("Tearing down Reticulum...");

	RNS::Transport::persist_data();

	try {

		HEAD("Deregistering Interface instances with Transport...", RNS::LOG_TRACE);
		RNS::Transport::deregister_interface(lora_interface);

	}
	catch (std::exception& e) {
		ERRORF("!!! Exception in reticulum_teardown: %s", e.what());
	}
}

void setup() {

#ifdef ARDUINO
	Serial.begin(115200);
	Serial.print("Hello from Arduino on PlatformIO!\n");

/*
	// Setup filesystem
	if (!SPIFFS.begin(true, "")){
		ERROR("SPIFFS filesystem mount failed");
	}
	else {
		DEBUG("SPIFFS filesystem is ready");
	}
*/
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
}

void loop() {

	reticulum.loop();

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
