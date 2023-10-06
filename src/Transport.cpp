#include "Transport.h"

#include "Destination.h"

#include "Log.h"

using namespace RNS;

Transport::Transport() {
	log("Transport object created", LOG_EXTREME);
}

Transport::~Transport() {
	log("Transport object destroyed", LOG_EXTREME);
}


/*static*/ void Transport::register_destination(Destination &destination) {
	destination.mtu(Reticulum::MTU);
/*
	if (destination->direction == Destination::IN) {
		for (registered_destination in Transport.destinations) {
			if (destination->hash == registered_destination->hash) {
				raise KeyError("Attempt to register an already registered destination.")
				throw std::runtime_error("Attempt to register an already registered destination.");
			}
		}
		
		Transport.destinations.append(destination);

		if (Transport.owner.is_connected_to_shared_instance) {
			if (destination->type == Destination::SINGLE) {
				destination->announce(path_response=True);
			}
		}
	}
*/
}

/*static*/ bool Transport::outbound(const Packet &packet) {
	// mock
	return true;
}
