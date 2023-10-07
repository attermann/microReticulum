#include "Reticulum.h"

#include "Log.h"

#include <RNG.h>

using namespace RNS;

Reticulum::Reticulum() : _object(new Object()) {
	extreme("Reticulum object created");

	// Initialkize random number generator
	RNG.begin("Reticulum");
	//RNG.stir(mac_address, sizeof(mac_address));
}

Reticulum::~Reticulum() {
	extreme("Reticulum object destroyed");
}

void Reticulum::loop() {
	// Perform random number gnerator housekeeping
	RNG.loop();
}
