#include "Packet.h"

#include "Identity.h"
#include "Log.h"

#include <string.h>
#include <stdexcept>

using namespace RNS;

//Packet::Packet(const Destination &destination, const Bytes &data, types packet_type, context_types context, Transport::types transport_type, header_types header_type, const uint8_t *transport_id, Interface *attached_interface, bool create_receipt) : _object(new Object(destination)) {
Packet::Packet(const Destination &destination, const Interface &attached_interface, const Bytes &data, types packet_type /*= DATA*/, context_types context /*= CONTEXT_NONE*/, Transport::types transport_type /*= Transport::BROADCAST*/, header_types header_type /*= HEADER_1*/, const uint8_t *transport_id /*= nullptr*/, bool create_receipt /*= true*/) : _object(new Object(destination, attached_interface)) {

	if (_object->_destination) {
		// CBA TODO handle NONE
		if (transport_type == -1) {
			transport_type = Transport::BROADCAST;
		}
		// following moved to object constructor to avoid extra NONE object
		//_destination = destination;
		_header_type = header_type;
		_packet_type = packet_type;
		_transport_type = transport_type;
		_context = context;

		//transport_id = transport_id;
		//setTransportId(transport_id);
		if (transport_id != nullptr) {
			memcpy(_transport_id, transport_id, Reticulum::DESTINATION_LENGTH);
		}

		//data = data;
		//setData(data);
		if (data) {
			if (data.size() > MDU) {
				_truncated = true;
				// CBA TODO add method to truncate
				//zdata_len = MDU;
			}
			memcpy(_data, data.data(), data.size());
		}
		_flags = get_packed_flags();

		_create_receipt = create_receipt;
	}
	else {
		//_raw = data;
		if (data) {
			memcpy(_raw, data.data(), data.size());
		}
		_packed = true;
		_fromPacked = true;
		_create_receipt = false;
	}
	extreme("Packet object created");
}

Packet::~Packet() {
	extreme("Packet object destroyed");
}


/*
void Packet::setTransportId(const uint8_t* transport_id) {
	if (_transport_id == nullptr) {
		delete[] _transport_id;
		_transport_id = nullptr;
	}
	if (transport_id != nullptr) {
		_transport_id = new uint8_t[Reticulum::ADDRESS_LENGTH];
		memcpy(_transport_id, transport_id, Reticulum::ADDRESS_LENGTH);
	}
}

void Packet::setTransportId(const uint8_t* header) {
	if (_header == nullptr) {
		delete[] _header;
		_header = nullptr;
	}
	if (header != nullptr) {
		_header = new uint8_t[HEADER_MAXSIZE];
		memcpy(_header, header, HEADER_MAXSIZE);
	}
}

void Packet::setRaw(const uint8_t* raw, uint16_t len) {
	if (_raw == nullptr) {
		delete[] _raw;
		_raw = nullptr;
		_raw_len = 0;
	}
	if (raw != nullptr) {
		_raw = new uint8_t[_MTU];
		memcpy(_raw, raw, len);
		_raw_len = len;
	}
}

void Packet::setData(const uint8_t* data, uint16_t len) {
	if (_data == nullptr) {
		delete[] _data;
		_data = nullptr;
		_data_len = 0;
	}
	if (data != nullptr) {
		_data = new uint8_t[MDU];
		memcpy(_data, data, len);
		_data_len = len;
	}
}
*/


uint8_t Packet::get_packed_flags() {
	uint8_t packed_flags = 0;
	if (_context == LRPROOF) {
		packed_flags = (_header_type << 6) | (_transport_type << 4) | (Destination::LINK << 2) | _packet_type;
	}
	else {
		packed_flags = (_header_type << 6) | (_transport_type << 4) | (_object->_destination.type() << 2) | _packet_type;
	}
	return packed_flags;
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
	debug("Packet::pack: packing packet...");

	//memcpy(_destination_hash, _destination->_hash.data(), Reticulum::DESTINATION_LENGTH);
	memcpy(_destination_hash, _object->_destination.hash().data(), _object->_destination.hash().size());

	_header[0] = _flags;
	_header[1] = _hops;

	//uint8_t *ciphertext;
	if (_context == LRPROOF) {
		// write header
		//memcpy(_header+2, _destination->_link_id, Reticulum::DESTINATION_LENGTH);
		debug("Packet::pack: destination link id: " + _object->_destination.link_id().toHex() );
		memcpy(_header+2, _object->_destination.link_id().data(), _object->_destination.link_id().size());
		_header[Reticulum::DESTINATION_LENGTH+2] = _context;
		// prepend header to _data in _raw bytes
		memcpy(_data-Reticulum::HEADER_MINSIZE, _header, Reticulum::HEADER_MINSIZE);
		//ciphertext = _data;
	}
	else {
		if (_header_type == HEADER_1) {
			// write header
			//memcpy(_header+2, _destination->_hash.data(), Reticulum::DESTINATION_LENGTH);
			debug("Packet::pack: destination hash: " + _object->_destination.hash().toHex() );
			memcpy(_header+2, _object->_destination.hash().data(), _object->_destination.hash().size());
			_header[Reticulum::DESTINATION_LENGTH+2] = _context;
			// prepend header to _data in _raw bytes
			memcpy(_data-Reticulum::HEADER_MINSIZE, _header, Reticulum::HEADER_MINSIZE);

			if (_packet_type == ANNOUNCE) {
				// Announce packets are not encrypted
				//ciphertext = _data;
			}
			else if (_packet_type == LINKREQUEST) {
				// Link request packets are not encrypted
				//ciphertext = _data;
			}
			else if (_packet_type == PROOF && _context == RESOURCE_PRF) {
				// Resource proofs are not encrypted
				//ciphertext = _data;
			}
			else if (_packet_type == PROOF && _object->_destination.type() == Destination::LINK) {
				// Packet proofs over links are not encrypted
				//ciphertext = _data;
			}
			else if (_context == RESOURCE) {
				// A resource takes care of encryption
				// by itself
				//ciphertext = _data;
			}
			else if (_context == KEEPALIVE) {
				// Keepalive packets contain no actual
				// data
				//ciphertext = _data;
			}
			else if (_context == CACHE_REQUEST) {
				// Cache-requests are not encrypted
				//ciphertext = _data;
			}
			else {
				// In all other cases, we encrypt the packet
				// with the destination's encryption method
				// CBA TODO Figure out how to most efficiently pass in data and receive encrypted data back into _raw bytes
				// CBA TODO Ensure that encrypted data does not exceed ENCRYPTED_MDU
				// CBA TODO Determine if encrypt method can read from and write to the same bytes
				//_data_len = _destination->encrypt(_data, _data, _data_len);
				//uint8_t data[_data_len];
				//memcpy(data, _data, _data_len);
				//_data_len = _destination->encrypt(_data, data, _data_len);
				Bytes plaintext(_data, _data_len);
				Bytes bytes = _object->_destination.encrypt(plaintext);
				_data_len = bytes.size();
			}
		}
		else if (_header_type == HEADER_2) {
			if (memcmp(_transport_id, EMPTY_DESTINATION, Reticulum::DESTINATION_LENGTH) == 0) {
                throw std::invalid_argument("Packet with header type 2 must have a transport ID");
			}
			// write header
			memcpy(_header+2, _transport_id, Reticulum::DESTINATION_LENGTH);
			//memcpy(_header+Reticulum::DESTINATION_LENGTH+2, _destination->_hash.data(), Reticulum::DESTINATION_LENGTH);
			debug("Packet::pack: destination hash: " + _object->_destination.hash().toHex() );
			memcpy(_header+Reticulum::DESTINATION_LENGTH+2, _object->_destination.hash().data(), _object->_destination.hash().size());
			_header[2*Reticulum::DESTINATION_LENGTH+2] = _context;
			// prepend header to _data in _raw bytes
			memcpy(_data-Reticulum::HEADER_MAXSIZE, _header, Reticulum::HEADER_MAXSIZE);

			if (_packet_type == ANNOUNCE) {
				// Announce packets are not encrypted
				//ciphertext = _data;
			}
		}
	}

	if (_data_len > _mtu) {
		throw std::length_error("Packet size of " + std::to_string(_data_len) + " exceeds MTU of " + std::to_string(_mtu) +" bytes");
	}

	_packed = true;
	update_hash();

}

bool Packet::unpack() {
	assert(_object);
	debug("Packet::unpack: unpacking packet...");
	try {
		_flags = _raw[0];
		_hops  = _raw[1];

		_header_type      = static_cast<header_types>((_flags & 0b01000000) >> 6);
		_transport_type   = static_cast<Transport::types>((_flags & 0b00110000) >> 4);
		_destination_type = static_cast<Destination::types>((_flags & 0b00001100) >> 2);
		_packet_type      = static_cast<types>(_flags & 0b00000011);

		// CBA TODO detect invalid flags and throw error
		if (false) {
			log("Received malformed packet, dropping it.");
			return false;
		}

		if (_header_type == HEADER_2) {
			memcpy(_transport_id, _raw+2, Reticulum::DESTINATION_LENGTH);
			memcpy(_destination_hash, _raw+Reticulum::DESTINATION_LENGTH+2, Reticulum::DESTINATION_LENGTH);
			_context = static_cast<context_types>(_raw[2*Reticulum::DESTINATION_LENGTH+2]);
			_data = _raw+2*Reticulum::DESTINATION_LENGTH+3;
		}
		else {
			//memcpy(_transport_id, EMPTY_DESTINATION, Reticulum::DESTINATION_LENGTH);
			memcpy(_destination_hash, _raw+2, Reticulum::DESTINATION_LENGTH);
			_context = static_cast<context_types>(_raw[Reticulum::DESTINATION_LENGTH+2]);
			_data = _raw+Reticulum::DESTINATION_LENGTH+3;
		}

		_packed = false;
		update_hash();
	}
	catch (std::exception& e) {
		error(std::string("Received malformed packet, dropping it. The contained exception was: ") + e.what());
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
	debug("Packet::send: sending packet...");
	if (_sent) {
        throw std::logic_error("Packet was already sent");
	}
/*
	if (_destination->type == RNS::Destination::LINK) {
		if (_destination->status == RNS::Link::CLOSED) {
            throw std::runtime_error("Attempt to transmit over a closed link");
		}
		else {
			_destination->last_outbound = time();
			_destination->tx += 1;
			_destination->txbytes += _data_len;
		}
	}
*/

	if (!_packed) {
		pack();
	}

	if (RNS::Transport::outbound(*this)) {
		//zreturn self.receipt
		debug("Packet::send: successfully sent packet!!!");
		// MOCK
		return true;
	}
	else {
		error("No interfaces could process the outbound packet");
		_sent = false;
		//z_receipt = None;
		return false;
	}
}

/*
Re-sends the packet.

:returns: A :ref:`RNS.PacketReceipt<api-packetreceipt>` instance if *create_receipt* was set to *True* when the packet was instantiated, if not returns *None*. If the packet could not be sent *False* is returned.
*/
bool Packet::resend() {
	assert(_object);
	debug("Packet::resend: re-sending packet...");
	if (!_sent) {
		throw std::logic_error("Packet was not sent yet");
	}
	// Re-pack the packet to obtain new ciphertext for
	// encrypted destinations
	pack();

	if (RNS::Transport::outbound(*this)) {
		//zreturn self.receipt
		debug("Packet::resend: successfully sent packet!!!");
		// MOCK
		return true;
	}
	else {
		error("No interfaces could process the outbound packet");
		_sent = false;
		//zself.receipt = None;
		return false;
	}
}

void Packet::update_hash() {
	assert(_object);
	_packet_hash = get_hash();
}

Bytes Packet::get_hash() {
	assert(_object);
	Bytes hashable_part = get_hashable_part();
	return Identity::full_hash(hashable_part);
}

Bytes Packet::getTruncatedHash() {
	assert(_object);
	Bytes hashable_part = get_hashable_part();
	return Identity::truncated_hash(hashable_part);
}

Bytes Packet::get_hashable_part() {
	assert(_object);
	Bytes hashable_part;
	hashable_part << (uint8_t)(_raw[0] & 0b00001111);
	hashable_part.append(_data-Reticulum::DESTINATION_LENGTH-1, _data_len+Reticulum::DESTINATION_LENGTH+1);
	return hashable_part;
}

// Generates a special destination that allows Reticulum
// to direct the proof back to the proved packet's sender
//ProofDestination &Packet::generate_proof_destination() {
//	return ProofDestination();
//}

