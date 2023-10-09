#include "Packet.h"

#include "Identity.h"
#include "Log.h"

#include <string.h>
#include <stdexcept>

using namespace RNS;

Packet::Packet(const Destination &destination, const Interface &attached_interface, const Bytes &data, types packet_type /*= DATA*/, context_types context /*= CONTEXT_NONE*/, Transport::types transport_type /*= Transport::BROADCAST*/, header_types header_type /*= HEADER_1*/, const Bytes &transport_id /*= Bytes::NONE*/, bool create_receipt /*= true*/) : _object(new Object(destination, attached_interface)) {

	if (_object->_destination) {
		extreme("Creating packet with detination...");
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

		_transport_id = transport_id;

		_data = data;
		if (_data.size() > MDU) {
			_truncated = true;
			_data.resize(MDU);
		}
/*
		if (data) {
			// data is plaintext
			if (data.size() > MDU) {
				_truncated = true;
				// CBA TODO add method to truncate
				//zdata_len = MDU;
			}
			_data = _raw + Reticulum::HEADER_MAXSIZE;
			memcpy(_data, data.data(), data.size());
		}
*/
		_flags = get_packed_flags();

		_create_receipt = create_receipt;
	}
	else {
		extreme("Creating packet without detination...");
		_raw = data;
/*
		if (data) {
			memcpy(_raw, data.data(), data.size());
		}
*/
		_packed = true;
		_fromPacked = true;
		_create_receipt = false;
	}
	extreme("Packet object created");
}

Packet::~Packet() {
	extreme("Packet object destroyed");
}


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

void Packet::unpack_flags(uint8_t flags) {
	_header_type      = static_cast<header_types>((flags & 0b01000000) >> 6);
	_transport_type   = static_cast<Transport::types>((flags & 0b00110000) >> 4);
	_destination_type = static_cast<Destination::types>((flags & 0b00001100) >> 2);
	_packet_type      = static_cast<types>(flags & 0b00000011);
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

/*
void Packet::pack() {
	assert(_object);
	debug("Packet::pack: packing packet...");
	extreme("Packet::pack: pre hops: " + std::to_string(_hops));

	//memcpy(_destination_hash, _destination->_hash.data(), Reticulum::DESTINATION_LENGTH);
	memcpy(_destination_hash, _object->_destination.hash().data(), _object->_destination.hash().size());

	//uint8_t *ciphertext;
	if (_context == LRPROOF) {
		// write header
		_header = _data - Reticulum::HEADER_MINSIZE;
		_header[0] = _flags;
		_header[1] = _hops;
		//memcpy(header+2, _destination->_link_id, Reticulum::DESTINATION_LENGTH);
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
			_header = _data - Reticulum::HEADER_MINSIZE;
			_header[0] = _flags;
			_header[1] = _hops;
			//memcpy(header+2, _destination->_hash.data(), Reticulum::DESTINATION_LENGTH);
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
				Bytes ciphertext = _object->_destination.encrypt(plaintext);
				memcpy(_data, ciphertext.data(), ciphertext.size());
				_data_len = ciphertext.size();
			}
		}
		else if (_header_type == HEADER_2) {
			if (memcmp(_transport_id, EMPTY_DESTINATION, Reticulum::DESTINATION_LENGTH) == 0) {
                throw std::invalid_argument("Packet with header type 2 must have a transport ID");
			}
			// write header
			_header = _data - Reticulum::HEADER_MAXSIZE;
			_header[0] = _flags;
			_header[1] = _hops;
			memcpy(_header+2, _transport_id, Reticulum::DESTINATION_LENGTH);
			//memcpy(header+Reticulum::DESTINATION_LENGTH+2, _destination->_hash.data(), Reticulum::DESTINATION_LENGTH);
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

	extreme("Packet::pack: post hops: " + std::to_string(_hops));
}
*/
void Packet::pack() {
	assert(_object);
	debug("Packet::pack: packing packet...");
	extreme("Packet::pack: pre hops: " + std::to_string(_hops));

	_destination_hash = _object->_destination.hash();

	_raw.clear();

	_raw << _flags;
	_raw << _hops;

	if (_context == LRPROOF) {
		debug("Packet::pack: destination link id: " + _object->_destination.link_id().toHex() );
		_raw << _object->_destination.link_id();
		_raw << (uint8_t)_context;
		_raw << _data;
	}
	else {
		if (_header_type == HEADER_1) {
			debug("Packet::pack: destination hash: " + _object->_destination.hash().toHex() );
			_raw << _object->_destination.hash();
			_raw << (uint8_t)_context;

			if (_packet_type == ANNOUNCE) {
				// Announce packets are not encrypted
				_raw << _data;
			}
			else if (_packet_type == LINKREQUEST) {
				// Link request packets are not encrypted
				_raw << _data;
			}
			else if (_packet_type == PROOF && _context == RESOURCE_PRF) {
				// Resource proofs are not encrypted
				_raw << _data;
			}
			else if (_packet_type == PROOF && _object->_destination.type() == Destination::LINK) {
				// Packet proofs over links are not encrypted
				_raw << _data;
			}
			else if (_context == RESOURCE) {
				// A resource takes care of encryption
				// by itself
				_raw << _data;
			}
			else if (_context == KEEPALIVE) {
				// Keepalive packets contain no actual
				// data
				_raw << _data;
			}
			else if (_context == CACHE_REQUEST) {
				// Cache-requests are not encrypted
				_raw << _data;
			}
			else {
				// In all other cases, we encrypt the packet
				// with the destination's encryption method
				_raw << _object->_destination.encrypt(_data);
			}
		}
		else if (_header_type == HEADER_2) {
			if (!_transport_id) {
                throw std::invalid_argument("Packet with header type 2 must have a transport ID");
			}
			debug("Packet::pack: transport id: " + _transport_id.toHex() );
			debug("Packet::pack: destination hash: " + _object->_destination.hash().toHex() );
			_raw << _transport_id;
			_raw << _object->_destination.hash();
			_raw << (uint8_t)_context;

			if (_packet_type == ANNOUNCE) {
				// Announce packets are not encrypted
				_raw << _data;
			}
		}
	}

	if (_raw.size() > _mtu) {
		throw std::length_error("Packet size of " + std::to_string(_raw.size()) + " exceeds MTU of " + std::to_string(_mtu) +" bytes");
	}

	_packed = true;
	update_hash();

	extreme("Packet::pack: post hops: " + std::to_string(_hops));
}

/*
bool Packet::unpack() {
	assert(_object);
	debug("Packet::unpack: unpacking packet...");
	extreme("Packet::unpack: pre hops: " + std::to_string(_hops));
	try {

		// read header
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

	extreme("Packet::unpack: post hops: " + std::to_string(_hops));
	return true;
}
*/
bool Packet::unpack() {
	assert(_object);
	debug("Packet::unpack: unpacking packet...");
	extreme("Packet::unpack: pre hops: " + std::to_string(_hops));
	try {
		if (_raw.size() < Reticulum::HEADER_MINSIZE) {
			throw std::length_error("Packet size of " + std::to_string(_raw.size()) + " does not meet minimum header size of " + std::to_string(Reticulum::HEADER_MINSIZE) +" bytes");
		}

		const uint8_t *raw = _raw.data();

		// read header
		_flags = raw[0];
		_hops  = raw[1];

		unpack_flags(_flags);

		// CBA TODO detect invalid flags and throw error
		if (false) {
			log("Received malformed packet, dropping it.");
			return false;
		}

		if (_header_type == HEADER_2) {
			if (_raw.size() < Reticulum::HEADER_MAXSIZE) {
				throw std::length_error("Packet size of " + std::to_string(_raw.size()) + " does not meet minimum header size of " + std::to_string(Reticulum::HEADER_MAXSIZE) +" bytes");
			}
			_transport_id.assign(raw+2, Reticulum::DESTINATION_LENGTH);
			_destination_hash.assign(raw+Reticulum::DESTINATION_LENGTH+2, Reticulum::DESTINATION_LENGTH);
			_context = static_cast<context_types>(raw[2*Reticulum::DESTINATION_LENGTH+2]);
			_data.assign(raw+2*Reticulum::DESTINATION_LENGTH+3, _raw.size()-(2*Reticulum::DESTINATION_LENGTH+3));
		}
		else {
			_transport_id.clear();
			_destination_hash.assign(raw+2, Reticulum::DESTINATION_LENGTH);
			_context = static_cast<context_types>(raw[Reticulum::DESTINATION_LENGTH+2]);
			_data.assign(raw+Reticulum::DESTINATION_LENGTH+3, _raw.size()-(Reticulum::DESTINATION_LENGTH+3));
		}

		_packed = false;
		update_hash();
	}
	catch (std::exception& e) {
		error(std::string("Received malformed packet, dropping it. The contained exception was: ") + e.what());
		return false;
	}

	extreme("Packet::unpack: post hops: " + std::to_string(_hops));
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
	hashable_part << (uint8_t)(_raw.data()[0] & 0b00001111);
	if (_header_type == Packet::HEADER_2) {
		//hashable_part += self.raw[(RNS.Identity.TRUNCATED_HASHLENGTH//8)+2:]
		hashable_part << _raw.mid((Identity::TRUNCATED_HASHLENGTH/8)+2);
	}
	else {
		//hashable_part += self.raw[2:];
		hashable_part << _raw.mid(2);
	}
	return hashable_part;
}

// Generates a special destination that allows Reticulum
// to direct the proof back to the proved packet's sender
//ProofDestination &Packet::generate_proof_destination() {
//	return ProofDestination();
//}


std::string Packet::toString() {
	if (_packed) {
		//unpack();
	}
	std::string dump;
	dump = "\n--------------------\n";
	dump += "flags:        " + hexFromByte(_flags) + "\n";
	dump += "  header_type:      " + std::to_string(_header_type) + "\n";
	dump += "  transport_type:   " + std::to_string(_transport_type) + "\n";
	dump += "  destination_type: " + std::to_string(_destination_type) + "\n";
	dump += "  packet_type:      " + std::to_string(_packet_type) + "\n";
	dump += "hops:         " + std::to_string(_hops) + "\n";
	dump += "transport:    " + _transport_id.toHex() + "\n";
	dump += "destination:  " + _destination_hash.toHex() + "\n";
	dump += "context_type: " + std::to_string(_header_type) + "\n";
	dump += "plaintext:    " + _data.toHex() + "\n";
	dump += "  length:           " + std::to_string(_data.size()) + "\n";
	dump += "raw:          " + _raw.toHex() + "\n";
	dump += "  length:           " + std::to_string(_raw.size()) + "\n";
	if (_raw.size() > 0) {
		size_t header_len = Reticulum::HEADER_MINSIZE;
		if (_header_type == HEADER_2) {
			header_len = Reticulum::HEADER_MAXSIZE;
		}
		dump += "  header:           " + _raw.left(header_len).toHex() + "\n";
		dump += "  key:              " + _raw.mid(header_len, Identity::KEYSIZE/8/2).toHex() + "\n";
		Bytes ciphertext(_raw.mid(header_len+Identity::KEYSIZE/8/2));
		dump += "  ciphertext:       " + ciphertext.toHex() + "\n";
		dump += "    length:           " + std::to_string(ciphertext.size()) + "\n";
		dump += "    iv:               " + ciphertext.left(16).toHex() + "\n";
		dump += "    sig:              " + ciphertext.right(32).toHex() + "\n";
		dump += "    aes ciphertext:   " + ciphertext.mid(16, ciphertext.size()-48).toHex() + "\n";
		dump += "      length:           " + std::to_string(ciphertext.size()-48) + "\n";
	}
	dump += "--------------------\n";
	return dump;
}
