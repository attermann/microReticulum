//#define NDEBUG

#include <microStore/FileSystem.h>
#include <microStore/Adapters/UniversalFileSystem.h>
#include <LoRaInterface.h>

#include <Reticulum.h>
#include <Packet.h>
#include <Transport.h>
#include <Interface.h>
#include <Log.h>
#include <Bytes.h>
#include <Type.h>
#include <Utilities/OS.h>

#ifdef ARDUINO
#include <Arduino.h>
#if defined(ARDUINO_ARCH_NRF52) || defined(ARDUINO_NRF52_ADAFRUIT)
#include <Adafruit_TinyUSB.h>
#endif
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

void reticulum_setup() {
	INFO("Setting up Reticulum...");

	try {

		// Initialize and register filesystem
		HEAD("Registering FileSystem...", RNS::LOG_TRACE);
		microStore::FileSystem filesystem{microStore::Adapters::UniversalFileSystem()};
		filesystem.init();
		HEAD("Formatting FileSystem...", RNS::LOG_TRACE);
		filesystem.format();
		RNS::Utilities::OS::register_filesystem(filesystem);

		// Initialize and register interface
		HEAD("Registering LoRaInterface...", RNS::LOG_TRACE);
		lora_interface = new LoRaInterface();
		lora_interface.mode(RNS::Type::Interface::MODE_GATEWAY);
		RNS::Transport::register_interface(lora_interface);
		lora_interface.start();

		HEAD("Creating Reticulum instance...", RNS::LOG_TRACE);
		reticulum = RNS::Reticulum();
		reticulum.transport_enabled(true);
		reticulum.probe_destination_enabled(true);
		reticulum.start();

		HEAD("RNS Transport Ready!", RNS::LOG_TRACE);
	}
	catch (const std::exception& e) {
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
	catch (const std::exception& e) {
		ERRORF("!!! Exception in reticulum_teardown: %s", e.what());
	}
}

void setup() {

#if defined(ARDUINO)
	Serial.begin(115200);
	while (!Serial) {
		if (millis() > 5000)
			break;
		delay(500);
	}
	Serial.println("Serial initialized");

#if defined(ESP32)
	Serial.print("Total SRAM: ");
	Serial.println(ESP.getHeapSize());
	Serial.print("Free SRAM: ");
	Serial.println(ESP.getFreeHeap());
#elif defined(ARDUINO_ARCH_NRF52) || defined(ARDUINO_NRF52_ADAFRUIT)
	Serial.print("Total SRAM: ");
	Serial.println(dbgHeapTotal());
	Serial.print("Free SRAM: ");
	Serial.println(dbgHeapFree());
#endif
#endif

	RNS::loglevel(RNS::LOG_TRACE);
	//RNS::loglevel(RNS::LOG_MEM);

	reticulum_setup();
}

void loop() {

	reticulum.loop();

}

#if defined(ARDUINO)
int _write(int file, char *ptr, int len) {
    int wrote = Serial.write(ptr, len);
	Serial.flush();
	return wrote;
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
