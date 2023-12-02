#include "Link.h"

#include "Packet.h"
#include "Log.h"

using namespace RNS;
using namespace RNS::Type::Link;

Link::Link(const Destination& destination) : _object(new Object(destination)) {
	assert(_object);

	mem("Link object created");
}


void Link::set_link_id(const Packet& packet) {
	assert(_object);
	_object->_link_id = packet.getTruncatedHash();
	_object->_hash = _object->_link_id;
}

void Link::receive(const Packet& packet) {
}
