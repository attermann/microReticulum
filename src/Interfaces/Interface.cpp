#include "Interface.h"

#include "Log.h"

using namespace RNS;

Interface::Interface() {
	log("Interface object created", LOG_EXTREME);
}

Interface::~Interface() {
	log("Interface object destroyed", LOG_EXTREME);
}

