#pragma once

#include "Reticulum.h"
#include "Identity.h"
#include "Transport.h"
#include "Destination.h"
#include "Interfaces/Interface.h"

#include <memory>
#include <stdint.h>
#include <time.h>

namespace RNS {

	class Packet;
	class PacketProof;
	class ProofDestination;
	class PacketReceipt;
	class PacketReceiptCallbacks;

	class Packet {

	public:
		enum NoneConstructor {
			NONE
		};

		// Packet types
		enum types {
			DATA         = 0x00,     // Data packets
			ANNOUNCE     = 0x01,     // Announces
			LINKREQUEST  = 0x02,     // Link requests
			PROOF        = 0x03,      // Proofs
		};

		// Header types
		enum header_types {
			HEADER_1     = 0x00,     // Normal header format
			HEADER_2     = 0x01,      // Header format used for packets in transport
		};

		// Packet context types
		enum context_types {
			CONTEXT_NONE           = 0x00,   // Generic data packet
			RESOURCE       = 0x01,   // Packet is part of a resource
			RESOURCE_ADV   = 0x02,   // Packet is a resource advertisement
			RESOURCE_REQ   = 0x03,   // Packet is a resource part request
			RESOURCE_HMU   = 0x04,   // Packet is a resource hashmap update
			RESOURCE_PRF   = 0x05,   // Packet is a resource proof
			RESOURCE_ICL   = 0x06,   // Packet is a resource initiator cancel message
			RESOURCE_RCL   = 0x07,   // Packet is a resource receiver cancel message
			CACHE_REQUEST  = 0x08,   // Packet is a cache request
			REQUEST        = 0x09,   // Packet is a request
			RESPONSE       = 0x0A,   // Packet is a response to a request
			PATH_RESPONSE  = 0x0B,   // Packet is a response to a path request
			COMMAND        = 0x0C,   // Packet is a command
			COMMAND_STATUS = 0x0D,   // Packet is a status of an executed command
			CHANNEL        = 0x0E,   // Packet contains link channel data
			KEEPALIVE      = 0xFA,   // Packet is a keepalive packet
			LINKIDENTIFY   = 0xFB,   // Packet is a link peer identification proof
			LINKCLOSE      = 0xFC,   // Packet is a link close message
			LINKPROOF      = 0xFD,   // Packet is a link packet proof
			LRRTT          = 0xFE,   // Packet is a link request round-trip time measurement
			LRPROOF        = 0xFF,    // Packet is a link request proof
		};

		// This is used to calculate allowable
		// payload sizes
		static const uint16_t HEADER_MAXSIZE = Reticulum::HEADER_MAXSIZE;
		static const uint16_t MDU            = Reticulum::MDU;

		// With an MTU of 500, the maximum of data we can
		// send in a single encrypted packet is given by
		// the below calculation; 383 bytes.
		//static const uint16_t ENCRYPTED_MDU  = floor((Reticulum::MDU-Identity::FERNET_OVERHEAD-Identity::KEYSIZE/16)/Identity::AES128_BLOCKSIZE)*Identity::AES128_BLOCKSIZE - 1;
		//static const uint16_t ENCRYPTED_MDU;
		static const uint16_t ENCRYPTED_MDU  = ((Reticulum::MDU-Identity::FERNET_OVERHEAD-Identity::KEYSIZE/16)/Identity::AES128_BLOCKSIZE)*Identity::AES128_BLOCKSIZE - 1;
		// The maximum size of the payload data in a single encrypted packet 
		static const uint16_t PLAIN_MDU      = MDU;
		// The maximum size of the payload data in a single unencrypted packet

		static const uint8_t TIMEOUT_PER_HOP = Reticulum::DEFAULT_PER_HOP_TIMEOUT;

		//static constexpr const uint8_t EMPTY_DESTINATION[Reticulum::DESTINATION_LENGTH] = {0};
		uint8_t EMPTY_DESTINATION[Reticulum::DESTINATION_LENGTH] = {0};

	public:
		Packet(const Destination &destination, const Interface &attached_interface, const Bytes &data, types packet_type = DATA, context_types context = CONTEXT_NONE, Transport::types transport_type = Transport::BROADCAST, header_types header_type = HEADER_1, const uint8_t *transport_id = nullptr, bool create_receipt = true);
		Packet(const Destination &destination, const Bytes &data, types packet_type = DATA, context_types context = CONTEXT_NONE, Transport::types transport_type = Transport::BROADCAST, header_types header_type = HEADER_1, const uint8_t *transport_id = nullptr, bool create_receipt = true) : Packet(destination, Interface::NONE, data, DATA, CONTEXT_NONE, Transport::BROADCAST, HEADER_1, nullptr, create_receipt) {
		}
		Packet(NoneConstructor none) {
			extreme("Packet NONE object created");
		}
		Packet(const Packet &packet) : _object(packet._object) {
			extreme("Packet object copy created");
		}
		~Packet();

		inline Packet& operator = (const Packet &packet) {
			_object = packet._object;
			extreme("Packet object copy created by assignment, this: " + std::to_string((ulong)this) + ", data: " + std::to_string((uint32_t)_object.get()));
			return *this;
		}
		inline operator bool() const {
			return _object.get() != nullptr;
		}

	private:
	/*
		void setTransportId(const uint8_t* transport_id);
		void setHeader(const uint8_t* header);
		void setRaw(const uint8_t* raw, uint16_t len);
		void setData(const uint8_t* rata, uint16_t len);
	*/

	public:
		uint8_t get_packed_flags();
		void pack();
		bool unpack();
		bool send();
		bool resend();
		void update_hash();
		Bytes get_hash();
		Bytes getTruncatedHash();
		Bytes get_hashable_part();
		//zProofDestination &generate_proof_destination();

		// getters/setters
		inline const Interface& receiving_interface() const { assert(_object); return _object->_receiving_interface; }

	public:
		types _packet_type;
		header_types _header_type;
		context_types _context;
		Transport::types _transport_type;
		Destination::types _destination_type;

		uint8_t _flags = 0;
		uint8_t _hops = 0;

		bool _packed = false;
		bool _sent = false;
		bool _create_receipt = false;
		bool _fromPacked = false;
		bool _truncated = false;
		//z_receipt = nullptr;

		uint16_t _mtu = Reticulum::MTU;
		time_t _sent_at = 0;

		float _rssi = 0.0;
		float _snr = 0.0;

		//uint8_t _packet_hash[Reticulum::HASHLENGTH] = {0};
		Bytes _packet_hash;
		uint8_t _destination_hash[Reticulum::DESTINATION_LENGTH] = {0};
		uint8_t _transport_id[Reticulum::DESTINATION_LENGTH] = {0};

		uint8_t _raw[Reticulum::MTU];
		uint8_t _header[Reticulum::HEADER_MAXSIZE];
		uint8_t *_data = _raw + Reticulum::HEADER_MAXSIZE;
		uint16_t _data_len = 0;

	private:
		class Object {
		public:
			Object(const Destination &destination, const Interface &attached_interface) : _destination(destination), _attached_interface(attached_interface) {}
		private:

			Destination _destination;

			Interface _attached_interface;
			Interface _receiving_interface;

		friend class Packet;
		};
		std::shared_ptr<Object> _object;
	};


	class ProofDestination {
	};


	class PacketReceipt {

	public:
		class Callbacks {
		public:
			typedef void (*delivery)(PacketReceipt *packet_receipt);
			typedef void (*timeout)(PacketReceipt *packet_receipt);
		};

	};

}
