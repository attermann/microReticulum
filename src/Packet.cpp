#include "Packet.h"

#include "Transport.h"
#include "Identity.h"
#include "Log.h"
#include "Utilities/OS.h"

#include <string.h>
#include <stdexcept>

using namespace RNS;
using namespace RNS::Type::PacketReceipt;
using namespace RNS::Type::Packet;
using namespace RNS::Utilities;

ProofDestination::ProofDestination(const Packet& packet) : Destination({Type::NONE}, Type::Destination::OUT, Type::Destination::SINGLE, packet.get_hash().left(Type::Reticulum::TRUNCATED_HASHLENGTH/8))
{
}

Packet::Packet(const Destination& destination, const Interface& attached_interface, const Bytes& data, types packet_type /*= DATA*/, context_types context /*= CONTEXT_NONE*/, Type::Transport::types transport_type /*= Type::Transport::BROADCAST*/, header_types header_type /*= HEADER_1*/, const Bytes& transport_id /*= {Bytes::NONE}*/, bool create_receipt /*= true*/) : _object(new Object(destination, attached_interface)) {

	if (_object->_destination) {
		TRACE("Creating packet with destination...");
		// CBA Should never see empty transport_type
		//if (transport_type == NONE) {
		//	transport_type = Type::Transport::BROADCAST;
		//}
		// following moved to object constructor to avoid extra NONE object
		//_destination = destination;
		_object->_header_type = header_type;
		_object->_packet_type = packet_type;
		_object->_transport_type = transport_type;
		_object->_context = context;
		_object->_transport_id = transport_id;
		_object->_data = data;
		if (_object->_data.size() > MDU) {
			_object->_truncated = true;
			_object->_data.resize(MDU);
		}
		_object->_flags = get_packed_flags();
		_object->_create_receipt = create_receipt;
	}
	else {
		TRACE("Creating packet without destination...");
		_object->_raw = data;
		_object->_packed = true;
		_object->_fromPacked = true;
		_object->_create_receipt = false;
	}
	MEM("Packet object created, this: " + std::to_string((uintptr_t)this) + ", data: " + std::to_string((uintptr_t)_object.get()));
}


uint8_t Packet::get_packed_flags() {
	assert(_object);
	uint8_t packed_flags = 0;
	if (_object->_context == LRPROOF) {
		packed_flags = (_object->_header_type << 6) | (_object->_transport_type << 4) | (Type::Destination::LINK << 2) | _object->_packet_type;
	}
	else {
		if (!_object->_destination) {
			throw std::logic_error("Packet destination is required");
		}
		packed_flags = (_object->_header_type << 6) | (_object->_transport_type << 4) | (_object->_destination.type() << 2) | _object->_packet_type;
	}
	return packed_flags;
}

void Packet::unpack_flags(uint8_t flags) {
	assert(_object);
	_object->_header_type      = static_cast<header_types>((flags & 0b01000000) >> 6);
	_object->_transport_type   = static_cast<Type::Transport::types>((flags & 0b00110000) >> 4);
	_object->_destination_type = static_cast<Type::Destination::types>((flags & 0b00001100) >> 2);
	_object->_packet_type      = static_cast<types>(flags & 0b00000011);
}


/*
== Reticulum Wire Format ======

A Reticulum packet is composed of the following fields:

[HEADER 2 bytes] [ADDRESSES 16/32 bytes] [CONTEXT 1 byte] [DATA 0-465 bytes]

* The HEADER field is 2 bytes long.
  * Byte 1: [IFAC Flag], [Header Type], [Propagation Type], [Destination Type] and [Packet Type]
  * Byte 2: Number of hops

* Interface Access Code field if the IFAC flag was set.
  * The length of the Interface Access Code can vary from
    1 to 64 bytes according to physical interface
    capabilities and configuration.

* The ADDRESSES field contains either 1 or 2 addresses.
  * Each address is 16 bytes long.
  * The Header Type flag in the HEADER field determines
    whether the ADDRESSES field contains 1 or 2 addresses.
  * Addresses are SHA-256 hashes truncated to 16 bytes.

* The CONTEXT field is 1 byte.
  * It is used by Reticulum to determine packet context.

* The DATA field is between 0 and 465 bytes.
  * It contains the packets data payload.

IFAC Flag
-----------------
open             0  Packet for publically accessible interface
authenticated    1  Interface authentication is included in packet


Header Types
-----------------
type 1           0  Two byte header, one 16 byte address field
type 2           1  Two byte header, two 16 byte address fields


Propagation Types
-----------------
broadcast       00
transport       01
reserved        10
reserved        11


Destination Types
-----------------
single          00
group           01
plain           10
link            11


Packet Types
-----------------
data            00
announce        01
link request    10
proof           11


+- Packet Example -+

   HEADER FIELD           DESTINATION FIELDS            CONTEXT FIELD  DATA FIELD
 _______|_______   ________________|________________   ________|______   __|_
|               | |                                 | |               | |    |
01010000 00000100 [HASH1, 16 bytes] [HASH2, 16 bytes] [CONTEXT, 1 byte] [DATA]
|| | | |    |
|| | | |    +-- Hops             = 4
|| | | +------- Packet Type      = DATA
|| | +--------- Destination Type = SINGLE
|| +----------- Propagation Type = TRANSPORT
|+------------- Header Type      = HEADER_2 (two byte header, two address fields)
+-------------- Access Codes     = DISABLED


+- Packet Example -+

   HEADER FIELD   DESTINATION FIELD   CONTEXT FIELD  DATA FIELD
 _______|_______   _______|_______   ________|______   __|_
|               | |               | |               | |    |
00000000 00000111 [HASH1, 16 bytes] [CONTEXT, 1 byte] [DATA]
|| | | |    |
|| | | |    +-- Hops             = 7
|| | | +------- Packet Type      = DATA
|| | +--------- Destination Type = SINGLE
|| +----------- Propagation Type = BROADCAST
|+------------- Header Type      = HEADER_1 (two byte header, one address field)
+-------------- Access Codes     = DISABLED


+- Packet Example -+

   HEADER FIELD     IFAC FIELD    DESTINATION FIELD   CONTEXT FIELD  DATA FIELD
 _______|_______   ______|______   _______|_______   ________|______   __|_
|               | |             | |               | |               | |    |
10000000 00000111 [IFAC, N bytes] [HASH1, 16 bytes] [CONTEXT, 1 byte] [DATA]
|| | | |    |
|| | | |    +-- Hops             = 7
|| | | +------- Packet Type      = DATA
|| | +--------- Destination Type = SINGLE
|| +----------- Propagation Type = BROADCAST
|+------------- Header Type      = HEADER_1 (two byte header, one address field)
+-------------- Access Codes     = ENABLED


Size examples of different packet types
---------------------------------------

The following table lists example sizes of various
packet types. The size listed are the complete on-
wire size counting all fields including headers,
but excluding any interface access codes.

- Path Request    :    51  bytes
- Announce        :    167 bytes
- Link Request    :    83  bytes
- Link Proof      :    115 bytes
- Link RTT packet :    99  bytes
- Link keepalive  :    20  bytes
*/


//  Reticulum Packet Structure
//
//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |I|H|PRT|DT |PT |     hops      |       destination...          |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                      ...destination...                        |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                      ...destination...                        |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                      ...destination...                        |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |       ...destination          |    context    |    data ...   |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |I|H|PRT|DT |PT |     hops      |      destination_1...         |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                     ...destination_1...                       |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                     ...destination_1...                       |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                     ...destination_1...                       |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |      ...destination_1         |      destination_2...         |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                     ...destination_2...                       |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                     ...destination_2...                       |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                     ...destination_2...                       |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |      ...destination_2         |    context    |    data ...   |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

void Packet::pack() {
	assert(_object);
	TRACE("Packet::pack: packing packet...");

	if (!_object->_destination) {
		throw std::logic_error("Packet destination is required");
	}
	_object->_destination_hash = _object->_destination.hash();

	_object->_raw.clear();
	_object->_encrypted = false;

	_object->_raw << _object->_flags;
	_object->_raw << _object->_hops;

	if (_object->_context == LRPROOF) {
		TRACE("Packet::pack: destination link id: " + _object->_destination.link_id().toHex() );
		_object->_raw << _object->_destination.link_id();
		_object->_raw << (uint8_t)_object->_context;
		_object->_raw << _object->_data;
	}
	else {
		if (_object->_header_type == HEADER_1) {
			TRACE("Packet::pack: destination hash: " + _object->_destination.hash().toHex() );
			_object->_raw << _object->_destination.hash();
			_object->_raw << (uint8_t)_object->_context;

			if (_object->_packet_type == ANNOUNCE) {
				// Announce packets are not encrypted
				_object->_raw << _object->_data;
			}
			else if (_object->_packet_type == LINKREQUEST) {
				// Link request packets are not encrypted
				_object->_raw << _object->_data;
			}
			else if (_object->_packet_type == PROOF && _object->_context == RESOURCE_PRF) {
				// Resource proofs are not encrypted
				_object->_raw << _object->_data;
			}
			else if (_object->_packet_type == PROOF && _object->_destination.type() == Type::Destination::LINK) {
				// Packet proofs over links are not encrypted
				_object->_raw << _object->_data;
			}
			else if (_object->_context == RESOURCE) {
				// A resource takes care of encryption
				// by itself
				_object->_raw << _object->_data;
			}
			else if (_object->_context == KEEPALIVE) {
				// Keepalive packets contain no actual
				// data
				_object->_raw << _object->_data;
			}
			else if (_object->_context == CACHE_REQUEST) {
				// Cache-requests are not encrypted
				_object->_raw << _object->_data;
			}
			else {
				// In all other cases, we encrypt the packet
				// with the destination's encryption method
				_object->_raw << _object->_destination.encrypt(_object->_data);
				_object->_encrypted = true;
			}
		}
		else if (_object->_header_type == HEADER_2) {
			if (!_object->_transport_id) {
                throw std::invalid_argument("Packet with header type 2 must have a transport ID");
			}
			TRACE("Packet::pack: transport id: " + _object->_transport_id.toHex() );
			TRACE("Packet::pack: destination hash: " + _object->_destination.hash().toHex() );
			_object->_raw << _object->_transport_id;
			_object->_raw << _object->_destination.hash();
			_object->_raw << (uint8_t)_object->_context;

			if (_object->_packet_type == ANNOUNCE) {
				// Announce packets are not encrypted
				_object->_raw << _object->_data;
			}
			// CBA No default encryption here like with header type HEADER_1 ???
			// CBA Is there any packet type besides ANNOUNCE with header type HEADER_2 ???
			// CBA Safe to assume that all HEADER_2 type packets are in transport and therefore can not and will not be decrypted locally ???
		}
	}

	if (_object->_raw.size() > _object->_mtu) {
		throw std::length_error("Packet size of " + std::to_string(_object->_raw.size()) + " exceeds MTU of " + std::to_string(_object->_mtu) +" bytes");
	}

	_object->_packed = true;
	update_hash();
}

bool Packet::unpack() {
	assert(_object);
	TRACE("Packet::unpack: unpacking packet...");
	try {
		if (_object->_raw.size() < Type::Reticulum::HEADER_MINSIZE) {
			throw std::length_error("Packet size of " + std::to_string(_object->_raw.size()) + " does not meet minimum header size of " + std::to_string(Type::Reticulum::HEADER_MINSIZE) +" bytes");
		}

		const uint8_t* raw = _object->_raw.data();

		// read header
		_object->_flags = raw[0];
		_object->_hops  = raw[1];

		unpack_flags(_object->_flags);

		// CBA TODO detect invalid flags and throw error
		if (false) {
			log("Received malformed packet, dropping it.");
			return false;
		}

		if (_object->_header_type == HEADER_2) {
			if (_object->_raw.size() < Type::Reticulum::HEADER_MAXSIZE) {
				throw std::length_error("Packet size of " + std::to_string(_object->_raw.size()) + " does not meet minimum header size of " + std::to_string(Type::Reticulum::HEADER_MAXSIZE) +" bytes");
			}
			_object->_transport_id.assign(raw+2, Type::Reticulum::DESTINATION_LENGTH);
			_object->_destination_hash.assign(raw+Type::Reticulum::DESTINATION_LENGTH+2, Type::Reticulum::DESTINATION_LENGTH);
			_object->_context = static_cast<context_types>(raw[2*Type::Reticulum::DESTINATION_LENGTH+2]);
			_object->_data.assign(raw+2*Type::Reticulum::DESTINATION_LENGTH+3, _object->_raw.size()-(2*Type::Reticulum::DESTINATION_LENGTH+3));
			// uknown at this point whether data is encrypted or not
			_object->_encrypted = false;
		}
		else {
			_object->_transport_id.clear();
			_object->_destination_hash.assign(raw+2, Type::Reticulum::DESTINATION_LENGTH);
			_object->_context = static_cast<context_types>(raw[Type::Reticulum::DESTINATION_LENGTH+2]);
			_object->_data.assign(raw+Type::Reticulum::DESTINATION_LENGTH+3, _object->_raw.size()-(Type::Reticulum::DESTINATION_LENGTH+3));
			// uknown at this point whether data is encrypted or not
			_object->_encrypted = false;
		}

		_object->_packed = false;
		update_hash();
	}
	catch (std::exception& e) {
		ERROR(std::string("Received malformed packet, dropping it. The contained exception was: ") + e.what());
		return false;
	}

	return true;
}

/*
Sends the packet.

:returns: A :ref:`RNS.PacketReceipt<api-packetreceipt>` instance if *create_receipt* was set to *True* when the packet was instantiated, if not returns *None*. If the packet could not be sent *False* is returned.
*/
bool Packet::send() {
	assert(_object);
	TRACE("Packet::send: sending packet...");
	if (_object->_sent) {
        throw std::logic_error("Packet was already sent");
	}
// TODO
/*
	if (_destination->type == RNS::Destination::LINK) {
		if (_destination->status == Type::Link::CLOSED) {
            throw std::runtime_error("Attempt to transmit over a closed link");
		}
		else {
			_destination->last_outbound = time();
			_destination->tx += 1;
			_destination->txbytes += _data_len;
		}
	}
*/

	if (!_object->_packed) {
		pack();
	}

	if (Transport::outbound(*this)) {
		TRACE("Packet::send: successfully sent packet!!!");
		//z return self.receipt
		// MOCK
		return true;
	}
	else {
		ERROR("No interfaces could process the outbound packet");
		_object->_sent = false;
		//z _receipt = None;
		return false;
	}
}

/*
Re-sends the packet.

:returns: A :ref:`RNS.PacketReceipt<api-packetreceipt>` instance if *create_receipt* was set to *True* when the packet was instantiated, if not returns *None*. If the packet could not be sent *False* is returned.
*/
bool Packet::resend() {
	assert(_object);
	TRACE("Packet::resend: re-sending packet...");
	if (!_object->_sent) {
		throw std::logic_error("Packet was not sent yet");
	}
	// Re-pack the packet to obtain new ciphertext for
	// encrypted destinations
	pack();

	if (Transport::outbound(*this)) {
		TRACE("Packet::resend: successfully sent packet!!!");
		//z return self.receipt
		// MOCK
		return true;
	}
	else {
		ERROR("No interfaces could process the outbound packet");
		_object->_sent = false;
		//z self.receipt = None;
		return false;
	}
}

void Packet::prove(const Destination& destination /*= {Type::NONE}*/) {
	assert(_object);
	TRACE("Packet::prove: proving packet...");
	if (!_object->_destination) {
		throw std::logic_error("Packet destination is required");
	}
	if (_object->_fromPacked && _object->_destination) {
		if (_object->_destination.identity() && _object->_destination.identity().prv()) {
			_object->_destination.identity().prove(*this, destination);
		}
	}
	else if (_object->_fromPacked && _object->_link) {
		_object->_link.prove_packet(*this);
	}
	else {
		ERROR("Could not prove packet associated with neither a destination nor a link");
	}
}


// Generates a special destination that allows Reticulum
// to direct the proof back to the proved packet's sender
ProofDestination Packet::generate_proof_destination() const {
	return ProofDestination(*this);
}

bool Packet::validate_proof_packet(const Packet& proof_packet) {
	assert(_object);
	if (!_object->_receipt) {
		return false;
	}
	return _object->_receipt.validate_proof_packet(proof_packet);
}

bool Packet::validate_proof(const Bytes& proof) {
	assert(_object);
	if (!_object->_receipt) {
		return false;
	}
	return _object->_receipt.validate_proof(proof);
}

void Packet::update_hash() {
	assert(_object);
	_object->_packet_hash = get_hash();
}

const Bytes Packet::get_hash() const {
	assert(_object);
	Bytes hashable_part = get_hashable_part();
	// CBA MCU SHORTER HASH
	return Identity::full_hash(hashable_part);
	//return Identity::truncated_hash(hashable_part);
}

const Bytes Packet::getTruncatedHash() const {
	assert(_object);
	Bytes hashable_part = get_hashable_part();
	return Identity::truncated_hash(hashable_part);
}

const Bytes Packet::get_hashable_part() const {
	assert(_object);
	Bytes hashable_part;
	hashable_part << (uint8_t)(_object->_raw.data()[0] & 0b00001111);
	if (_object->_header_type == HEADER_2) {
		//hashable_part += self.raw[(RNS.Identity.TRUNCATED_HASHLENGTH//8)+2:]
		hashable_part << _object->_raw.mid((Type::Identity::TRUNCATED_HASHLENGTH/8)+2);
	}
	else {
		//hashable_part += self.raw[2:];
		hashable_part << _object->_raw.mid(2);
	}
	return hashable_part;
}


#ifndef NDEBUG
std::string Packet::debugString() const {
	if (!_object) {
		return "";
	}
	if (_object->_packed) {
		//unpack();
	}
	std::string str = "ph=" + _object->_packet_hash.toHex();
	str += " ht=" + std::to_string(_object->_header_type);
	str += " tt=" + std::to_string(_object->_transport_type);
	str += " dt=" + std::to_string(_object->_destination_type);
	str += " pt=" + std::to_string(_object->_packet_type);
	str += " hp=" + std::to_string(_object->_hops);
	str += " ti=" + _object->_transport_id.toHex();
	str += " dh=" + _object->_destination_hash.toHex();
	return str;
}
std::string Packet::dumpString() const {
	if (!_object) {
		return "";
	}
	//if (_object->_packed) {
	//	unpack();
	//}
	bool encrypted = true;
	std::string dump;
	dump = "\n------------------------------------------------------------------------------\n";
	dump += "hash:         " + _object->_packet_hash.toHex() + "\n";
	dump += "flags:        " + hexFromByte(_object->_flags) + "\n";
	//dump += "  header_type:      " + std::to_string(_object->_header_type) + "\n";
	dump += "  header_type:      ";
	switch (_object->_header_type) {
	case HEADER_1:
		dump += "HEADER_1\n";
		break;
	case HEADER_2:
		encrypted = false;
		dump += "HEADER_2\n";
		break;
	default:
		std::to_string(_object->_header_type) + "\n";
	}
	//dump += "  transport_type:   " + std::to_string(_object->_transport_type) + "\n";
	dump += "  transport_type:   ";
	switch (_object->_transport_type) {
	case Type::Transport::BROADCAST:
		dump += "BROADCAST\n";
		break;
	case Type::Transport::TRANSPORT:
		dump += "TRANSPORT\n";
		break;
	case Type::Transport::RELAY:
		dump += "RELAY\n";
		break;
	case Type::Transport::TUNNEL:
		dump += "TUNNEL\n";
		break;
	case Type::Transport::NONE:
		dump += "NONE\n";
		break;
	default:
		std::to_string(_object->_transport_type) + "\n";
	}
	//dump += "  destination_type: " + std::to_string(_object->_destination_type) + "\n";
	dump += "  destination_type: ";
	switch (_object->_destination_type) {
	case Type::Destination::SINGLE:
		dump += "SINGLE\n";
		break;
	case Type::Destination::GROUP:
		dump += "GROUP\n";
		break;
	case Type::Destination::PLAIN:
		dump += "PLAIN\n";
		break;
	case Type::Destination::LINK:
		dump += "LINK\n";
		break;
	default:
		std::to_string(_object->_destination_type) + "\n";
	}
	//dump += "  packet_type:      " + std::to_string(_object->_packet_type) + "\n";
	dump += "  packet_type:      ";
	switch (_object->_packet_type) {
	case DATA:
		dump += "DATA\n";
		break;
	case ANNOUNCE:
		encrypted = false;
		dump += "ANNOUNCE\n";
		break;
	case LINKREQUEST:
		encrypted = false;
		dump += "LINKREQUEST\n";
		break;
	case PROOF:
		dump += "PROOF\n";
		if (_object->_context == RESOURCE_PRF) {
			encrypted = false;
		}
		if (_object->_destination && _object->_destination.type() == Type::Destination::LINK) {
			encrypted = false;
		}
		break;
	default:
		std::to_string(_object->_packet_type) + "\n";
	}
	dump += "hops:         " + std::to_string(_object->_hops) + "\n";
	dump += "transport:    " + _object->_transport_id.toHex() + "\n";
	dump += "destination:  " + _object->_destination_hash.toHex() + "\n";
	//dump += "context:      " + std::to_string(_object->_context) + "\n";
	dump += "context:      ";
	switch (_object->_context) {
	case CONTEXT_NONE:
		dump += "CONTEXT_NONE\n";
		break;
	case RESOURCE:
		encrypted = false;
		dump += "RESOURCE\n";
		break;
	case RESOURCE_ADV:
		dump += "RESOURCE_ADV\n";
		break;
	case RESOURCE_REQ:
		dump += "RESOURCE_REQ\n";
		break;
	case RESOURCE_HMU:
		dump += "RESOURCE_HMU\n";
		break;
	case RESOURCE_PRF:
		encrypted = false;
		dump += "RESOURCE_PRF\n";
		break;
	case RESOURCE_ICL:
		dump += "RESOURCE_ICL\n";
		break;
	case RESOURCE_RCL:
		dump += "RESOURCE_RCL\n";
		break;
	case CACHE_REQUEST:
		encrypted = false;
		dump += "CACHE_REQUEST\n";
		break;
	case REQUEST:
		dump += "REQUEST\n";
		break;
	case RESPONSE:
		dump += "RESPONSE\n";
		break;
	case PATH_RESPONSE:
		dump += "PATH_RESPONSE\n";
		break;
	case COMMAND:
		dump += "COMMAND\n";
		break;
	case COMMAND_STATUS:
		dump += "COMMAND_STATUS\n";
		break;
	case CHANNEL:
		dump += "CHANNEL\n";
		break;
	case KEEPALIVE:
		encrypted = false;
		dump += "KEEPALIVE\n";
		break;
	case LINKIDENTIFY:
		dump += "LINKIDENTIFY\n";
		break;
	case LINKCLOSE:
		dump += "LINKCLOSE\n";
		break;
	case LINKPROOF:
		dump += "LINKPROOF\n";
		break;
	case LRRTT:
		dump += "LRRTT\n";
		break;
	case LRPROOF:
		encrypted = false;
		dump += "LRPROOF\n";
		break;
	default:
		std::to_string(_object->_context) + "\n";
	}
	dump += "raw:          " + _object->_raw.toHex() + "\n";
	dump += "  length:           " + std::to_string(_object->_raw.size()) + "\n";
	dump += "data:         " + _object->_data.toHex() + "\n";
	dump += "  length:           " + std::to_string(_object->_data.size()) + "\n";
	//if ((encrypted || _object->_encrypted) && _object->_raw.size() > 0) {
	if (false) {
		size_t header_len = Type::Reticulum::HEADER_MINSIZE;
		if (_object->_header_type == HEADER_2) {
			header_len = Type::Reticulum::HEADER_MAXSIZE;
		}
		dump += "encrypted:\n";
		dump += "  header:           " + _object->_raw.left(header_len).toHex() + "\n";
		dump += "  key:              " + _object->_raw.mid(header_len, Type::Identity::KEYSIZE/8/2).toHex() + "\n";
		Bytes ciphertext(_object->_raw.mid(header_len+Type::Identity::KEYSIZE/8/2));
		dump += "  ciphertext:       " + ciphertext.toHex() + "\n";
		dump += "    length:           " + std::to_string(ciphertext.size()) + "\n";
		dump += "    iv:               " + ciphertext.left(16).toHex() + "\n";
		dump += "    sig:              " + ciphertext.right(32).toHex() + "\n";
		if (ciphertext.size() >= 48) {
			dump += "    aes ciphertext:   " + ciphertext.mid(16, ciphertext.size()-48).toHex() + "\n";
			dump += "      length:           " + std::to_string(ciphertext.size()-48) + "\n";
		}
	}
	dump += "------------------------------------------------------------------------------\n";
	return dump;
}
#endif

PacketReceipt::PacketReceipt(const Packet& packet) : _object(new Object()) {

	if (!packet.destination()) {
		throw std::invalid_argument("Packet with destination is required");
	}
	_object->_hash           = packet.get_hash();
	_object->_truncated_hash = packet.getTruncatedHash();
	_object->_destination    = packet.destination();

	if (packet.destination().type() == Type::Destination::LINK) {
		// CBA BUG? Destination does not have rtt or traffic_timeout_factor memebers
		//z _object->_timeout    = packet.destination().rtt() * packet.destination().traffic_timeout_factor();
		_object->_timeout    = TIMEOUT_PER_HOP * Transport::hops_to(_object->_destination.hash());
	}
	else {
		_object->_timeout    = TIMEOUT_PER_HOP * Transport::hops_to(_object->_destination.hash());
	}
}

// Validate a proof packet
bool PacketReceipt::validate_proof_packet(const Packet& proof_packet) {
	if (proof_packet.link()) {
		return validate_link_proof(proof_packet.data(), proof_packet.link(), proof_packet);
	}
	else {
		return validate_proof(proof_packet.data(), proof_packet);
	}
}

// Validate a raw proof for a link
//bool PacketReceipt::validate_link_proof(const Bytes& proof, const Link& link, const Packet& proof_packet /*= {Type::NONE}*/) {
bool PacketReceipt::validate_link_proof(const Bytes& proof, const Link& link) {
	return validate_link_proof(proof, link, {Type::NONE});
}
bool PacketReceipt::validate_link_proof(const Bytes& proof, const Link& link, const Packet& proof_packet) {
	assert(_object);
	TRACE("PacketReceipt::validate_link_proof: validating link proof...");
	// TODO: Hardcoded as explicit proofs for now
	if (true || proof.size() == EXPL_LENGTH) {
		// This is an explicit proof
		Bytes proof_hash = proof.left(Type::Identity::HASHLENGTH/8);
		Bytes signature = proof.mid(Type::Identity::HASHLENGTH/8, Type::Identity::SIGLENGTH/8);
		if (proof_hash == _object->_hash) {
			//z if (link.validate(signature, _object->_hash)) {
			if (false) {
				_object->_status = DELIVERED;
				_object->_proved = true;
				_object->_concluded_at = OS::time();
				//z _object->_proof_packet = proof_packet;
				//z link.last_proof(_object->_concluded_at);

				if (_object->_callbacks._delivery) {
					try {
						_object->_callbacks._delivery(*this);
					}
					catch (std::exception& e) {
						ERROR("An error occurred while evaluating external delivery callback for " + link.toString());
						ERROR("The contained exception was: "  + std::string(e.what()));
					}
				}
				return true;
			}
			else {
				return false;
			}
		}
		else {
			return false;
		}
	}
	else if (proof.size() == IMPL_LENGTH) {
		// TODO: Why is this disabled?
		// signature = proof[:RNS.Identity.SIGLENGTH//8]
		// proof_valid = self.link.validate(signature, self.hash)
		// if proof_valid:
		//       self.status = PacketReceipt.DELIVERED
		//       self.proved = True
		//       self.concluded_at = time.time()
		//       if self.callbacks.delivery != None:
		//           self.callbacks.delivery(self)
		//       RNS.log("valid")
		//       return True
		// else:
		//   RNS.log("invalid")
		//   return False
	}
	else {
		return false;
	}
	return false;
}

// Validate a raw proof
//bool PacketReceipt::validate_proof(const Bytes& proof, const Packet& proof_packet /*= {Type::NONE}*/) {
bool PacketReceipt::validate_proof(const Bytes& proof) {
	return validate_proof(proof, {Type::NONE});
}
bool PacketReceipt::validate_proof(const Bytes& proof, const Packet& proof_packet) {
	assert(_object);
	TRACE("PacketReceipt::validate_proof: validating proof...");
	if (proof.size() == EXPL_LENGTH) {
		// This is an explicit proof
		Bytes proof_hash = proof.left(Type::Identity::HASHLENGTH/8);
		Bytes signature = proof.mid(Type::Identity::HASHLENGTH/8, Type::Identity::SIGLENGTH/8);
		if (proof_hash == _object->_hash) {
			if (_object->_destination.identity().validate(signature, _object->_hash)) {
				_object->_status = DELIVERED;
				_object->_proved = true;
				_object->_concluded_at = OS::time();
				//z _object->_proof_packet = proof_packet;

				if (_object->_callbacks._delivery) {
					try {
						_object->_callbacks._delivery(*this);
					}
					catch (std::exception& e) {
						ERROR("Error while executing proof validated callback. The contained exception was: " + std::string(e.what()));
					}
				}
				return true;
			}
			else {
				return false;
			}
		}
		else {
			return false;
		}
	}
	else if (proof.size() == IMPL_LENGTH) {
		// This is an implicit proof
		if (!_object->_destination.identity()) {
			return false;
		}

		Bytes signature = proof.left(Type::Identity::SIGLENGTH/8);
		if (_object->_destination.identity().validate(signature, _object->_hash)) {
			_object->_status = DELIVERED;
			_object->_proved = true;
			_object->_concluded_at = OS::time();
			//z _object->_proof_packet = proof_packet;

			if (_object->_callbacks._delivery) {
				try {
					_object->_callbacks._delivery(*this);
				}
				catch (std::exception& e) {
					ERROR("Error while executing proof validated callback. The contained exception was: " + std::string(e.what()));
				}
			}
			return true;
		}
		else {
			return false;
		}
	}
	else {
		return false;
	}
	return false;
}

void PacketReceipt::check_timeout() {
	assert(_object);
	if (_object->_status == SENT && is_timed_out()) {
		if (_object->_timeout == -1) {
			_object->_status = CULLED;
		}
		else {
			_object->_status = FAILED;
		}

		_object->_concluded_at = Utilities::OS::time();

		if (_object->_callbacks._timeout) {
			//z thread = threading.Thread(target=self.callbacks.timeout, args=(self,))
			//z thread.daemon = True
			//z thread.start();
		}
	}
}

/*
void ArduinoJson::convertFromJson(JsonVariantConst src, RNS::Packet& dst) {
	if (!src.isNull()) {
		RNS::Bytes hash;
		hash.assignHex(src.as<const char*>());
		// Query transport for matching interface
		dst = Transport::get_cached_packet(hash);
	}
	else {
		dst = {RNS::Type::NONE};
	}
}
*/