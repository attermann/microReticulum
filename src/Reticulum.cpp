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

/*
Returns whether proofs sent are explicit or implicit.

:returns: True if the current running configuration specifies to use implicit proofs. False if not.
*/
/*static*/ bool Reticulum::should_use_implicit_proof() {
	return __use_implicit_proof;
}

void Reticulum::loop() {
	// Perform random number gnerator housekeeping
	RNG.loop();
}
