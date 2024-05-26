//#define NDEBUG

#include "TestFilesystem.h"

#include <Test/Test.h>

#include "Log.h"
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

TestFilesystem filesystem;

void setup() {

#ifdef ARDUINO
	Serial.begin(115200);
	Serial.print("Hello from T-Beam on PlatformIO!\n");
#endif

	RNS::loglevel(RNS::LOG_EXTREME);
	//RNS::loglevel(RNS::LOG_MEM);

	if (filesystem) {
		RNS::info("TestFilesystem exists");
	}
	RNS::Utilities::OS::register_filesystem(filesystem);

	try {

		RNS::extreme("Running tests...");
		RNS::LogLevel loglevel = RNS::loglevel();
		//RNS::loglevel(RNS::LOG_WARNING);
		RNS::loglevel(RNS::LOG_EXTREME);
		//test();
		//testReference();
		//testCrypto();
		testPersistence();
		RNS::loglevel(loglevel);
		RNS::extreme("Finished running tests");
		//return;
	}
	catch (std::exception& e) {
		RNS::error(std::string("!!! Exception in test: ") + e.what() + " !!!");
	}

#ifdef ARDUINO
	//Serial.print("Goodbye from T-Beam on PlatformIO!\n");
#endif
}

void loop() {
}

#ifndef ARDUINO
int main(void) {
	printf("Hello from Native on PlatformIO!\n");

	setup();

	printf("Goodbye from Native on PlatformIO!\n");
}
#endif
