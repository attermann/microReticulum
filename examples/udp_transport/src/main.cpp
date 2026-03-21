//#define NDEBUG

#include <microStore/FileSystem.h>
#include <microStore/Adapters/UniversalFileSystem.h>
#include <UDPInterface.h>

#include <Reticulum.h>
#include <Identity.h>
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
RNS::Interface udp_interface(RNS::Type::NONE);

void reticulum_setup() {
	INFO("Setting up Reticulum...");

	try {

		// Initialize and register filesystem
		INFO("Registering FileSystem with OS...");
		microStore::FileSystem filesystem{microStore::Adapters::UniversalFileSystem()};
		filesystem.init();
		RNS::Utilities::OS::register_filesystem(filesystem);

		// Initialize and register interface
		INFO("Registering UDPInterface instances with Transport...");
		udp_interface = new UDPInterface();
		udp_interface.mode(RNS::Type::Interface::MODE_GATEWAY);
		RNS::Transport::register_interface(udp_interface);
		udp_interface.start();

		INFO("Creating Reticulum instance...");
		reticulum = RNS::Reticulum();
		reticulum.transport_enabled(true);
		reticulum.probe_destination_enabled(true);
		reticulum.start();

		INFO("RNS Transport Ready!");
	}
	catch (const std::exception& e) {
		ERRORF("!!! Exception in reticulum_setup: %s", e.what());
	}
}

void reticulum_teardown() {
	INFO("Tearing down Reticulum...");

	RNS::Transport::persist_data();

	try {

		INFO("Deregistering Interface instances with Transport...");
		RNS::Transport::deregister_interface(udp_interface);

	}
	catch (const std::exception& e) {
		ERRORF("!!! Exception in reticulum_teardown: %s", e.what());
	}
}

void setup() {

#ifdef ARDUINO
	Serial.begin(115200);
	Serial.print("Hello from Arduino on PlatformIO!\n");
#endif

#if defined(RNS_MEM_LOG)
		RNS::loglevel(RNS::LOG_MEM);
#else
		//RNS::loglevel(RNS::LOG_WARNING);
		//RNS::loglevel(RNS::LOG_DEBUG);
		RNS::loglevel(RNS::LOG_TRACE);
#endif

	reticulum_setup();
}

void loop() {

	reticulum.loop();

}

#ifdef ARDUINO
int _write(int file, char *ptr, int len){
    return Serial.write(ptr, len);
}
#else
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
