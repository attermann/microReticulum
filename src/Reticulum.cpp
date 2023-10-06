#include "Reticulum.h"

#include "Log.h"

using namespace RNS;

Reticulum::Reticulum() : _object(new Object()) {
	extreme("Reticulum object created");
}

Reticulum::~Reticulum() {
	extreme("Reticulum object destroyed");
}

