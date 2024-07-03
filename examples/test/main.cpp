//#define NDEBUG

#include "Filesystem.h"

#include "Test.h"

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

Filesystem filesystem;

void setup() {

#ifdef ARDUINO
	Serial.begin(115200);
	while (!Serial) {
		if (millis() > 2000)
			break;
	}
  	Serial.print("Hello from T-Beam on PlatformIO!\n");
#endif

	RNS::loglevel(RNS::LOG_TRACE);
	//RNS::loglevel(RNS::LOG_MEM);

	if (filesystem) {
		INFO("Filesystem exists");
		filesystem.init();
	}
	RNS::Utilities::OS::register_filesystem(filesystem);

	try {

		TRACE("Running tests...");
		RNS::LogLevel loglevel = RNS::loglevel();
		//RNS::loglevel(RNS::LOG_WARNING);
		RNS::loglevel(RNS::LOG_TRACE);

		// all tests
		//test();

		// individiual tests
		//testReference();
		//testCrypto();
		//testPersistence();
		testBytes();
		//testFilesystem();
		//testReticulum();

		RNS::loglevel(loglevel);
		TRACE("Finished running tests");
		//return;
	}
	catch (std::exception& e) {
		ERROR(std::string("!!! Exception in test: ") + e.what() + " !!!");
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
