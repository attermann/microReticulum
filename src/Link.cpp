#include "Link.h"

#include "Packet.h"
#include "Log.h"

using namespace RNS;
using namespace RNS::Type::Link;

Link::Link(const Destination& destination) : _object(new Object(destination)) {
	assert(_object);

	MEM("Link object created");
}


void Link::set_link_id(const Packet& packet) {
	assert(_object);
	_object->_link_id = packet.getTruncatedHash();
	_object->_hash = _object->_link_id;
}

void Link::receive(const Packet& packet) {
}

void Link::prove() {
/*
	signed_data = self.link_id+self.pub_bytes+self.sig_pub_bytes
	signature = self.owner.identity.sign(signed_data)

	proof_data = signature+self.pub_bytes
	proof = RNS.Packet(self, proof_data, packet_type=RNS.Packet.PROOF, context=RNS.Packet.LRPROOF)
	proof.send()
	self.establishment_cost += len(proof.raw)
	self.had_outbound()
*/
}

void Link::prove_packet(const Packet& packet) {
/*
	signature = self.sign(packet.packet_hash)
	# TODO: Hardcoded as explicit proof for now
	# if RNS.Reticulum.should_use_implicit_proof():
	#   proof_data = signature
	# else:
	#   proof_data = packet.packet_hash + signature
	proof_data = packet.packet_hash + signature

	proof = RNS.Packet(self, proof_data, RNS.Packet.PROOF)
	proof.send()
	self.had_outbound()
*/
}
