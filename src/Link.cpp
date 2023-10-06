#include "Link.h"

#include "Log.h"

using namespace RNS;

Link::Link() {
	log("Link object created", LOG_EXTREME);
}

Link::~Link() {
	log("Link object destroyed", LOG_EXTREME);
}

