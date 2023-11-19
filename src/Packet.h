#pragma once

#include "Transport.h"
#include "Reticulum.h"
#include "Link.h"
#include "Identity.h"
#include "Destination.h"
#include "None.h"
#include "Interfaces/Interface.h"
#include "Utilities/OS.h"

#include <memory>
#include <stdint.h>
#include <time.h>

namespace RNS {

	class ProofDestination;
	class PacketReceipt;
	class Packet;


	class ProofDestination {
	};


    /*
    The PacketReceipt class is used to receive notifications about
    :ref:`RNS.Packet<api-packet>` instances sent over the network. Instances
    of this class are never created manually, but always returned from
    the *send()* method of a :ref:`RNS.Packet<api-packet>` instance.
    */
	class PacketReceipt {

	public:
		class Callbacks {
		public:
			using delivery = void(*)(const PacketReceipt &packet_receipt);
			using timeout = void(*)(const PacketReceipt &packet_receipt);
		public:
			delivery _delivery = nullptr;
			timeout _timeout = nullptr;
		friend class PacketReceipt;
		};

		enum NoneConstructor {
			NONE
		};

		// Receipt status constants
		enum Status {
			FAILED    = 0x00,
			SENT      = 0x01,
			DELIVERED = 0x02,
			CULLED    = 0xFF
		};

		static const uint16_t EXPL_LENGTH = Identity::HASHLENGTH / 8 + Identity::SIGLENGTH / 8;
		static const uint16_t IMPL_LENGTH = Identity::SIGLENGTH / 8;

	public:
		PacketReceipt(NoneConstructor none) {}
		PacketReceipt(const PacketReceipt &packet_receipt) : _object(packet_receipt._object) {}
		PacketReceipt() : _object(new Object()) {}
		PacketReceipt(const Packet &packet) {}

		inline PacketReceipt& operator = (const PacketReceipt &packet_receipt) {
			_object = packet_receipt._object;
			return *this;
		}
		inline operator bool() const {
			return _object.get() != nullptr;
		}
		inline bool operator < (const PacketReceipt &packet_receipt) const {
			return _object.get() < packet_receipt._object.get();
		}

	public:
		inline bool is_timed_out() {
			assert(_object);
			return ((_object->_sent_at + _object->_timeout) < Utilities::OS::time());
		}

		void check_timeout();

		/*
		Sets a timeout in seconds
		
		:param timeout: The timeout in seconds.
		*/
		inline void set_timeout(int16_t timeout) {
			assert(_object);
			_object->_timeout = timeout;
		}

		/*
		Sets a function that gets called if a successfull delivery has been proven.

		:param callback: A *callable* with the signature *callback(packet_receipt)*
		*/
		inline void set_delivery_callback(Callbacks::delivery callback) {
			assert(_object);
			_object->_callbacks._delivery = callback;
		}

		/*
		Sets a function that gets called if the delivery times out.

		:param callback: A *callable* with the signature *callback(packet_receipt)*
		*/
		inline void set_timeout_callback(Callbacks::timeout callback) {
			assert(_object);
			_object->_callbacks._timeout = callback;
		}

	private:
		class Object {
		public:
			Object() {}
			virtual ~Object() {}
		private:
			bool _sent           = true;
			uint64_t _sent_at        = Utilities::OS::time();
			bool _proved         = false;
			Status _status         = SENT;
			Destination _destination    = Destination::NONE;
			Callbacks _callbacks;
			uint64_t _concluded_at   = 0;
			//zPacket _proof_packet;
			int16_t _timeout = 0;
		friend class PacketReceipt;
		};
		std::shared_ptr<Object> _object;

	};


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
		Packet(NoneConstructor none) {
			extreme("Packet NONE object created");
		}
		Packet(RNS::NoneConstructor none) {
			extreme("Packet NONE object created");
		}
		Packet(const Packet &packet) : _object(packet._object) {
			extreme("Packet object copy created");
		}
		Packet(const Destination &destination, const Interface &attached_interface, const Bytes &data, types packet_type = DATA, context_types context = CONTEXT_NONE, Transport::types transport_type = Transport::BROADCAST, header_types header_type = HEADER_1, const Bytes &transport_id = Bytes::NONE, bool create_receipt = true);
		Packet(const Destination &destination, const Bytes &data, types packet_type = DATA, context_types context = CONTEXT_NONE, Transport::types transport_type = Transport::BROADCAST, header_types header_type = HEADER_1, const Bytes &transport_id = Bytes::NONE, bool create_receipt = true) : Packet(destination, Interface::NONE, data, packet_type, context, transport_type, header_type, transport_id, create_receipt) {}
		virtual ~Packet();

		inline Packet& operator = (const Packet &packet) {
			_object = packet._object;
			extreme("Packet object copy created by assignment, this: " + std::to_string((uintptr_t)this) + ", data: " + std::to_string((uintptr_t)_object.get()));
			return *this;
		}
		inline operator bool() const {
			return _object.get() != nullptr;
		}
		inline bool operator < (const Packet &packet) const {
			return _object.get() < packet._object.get();
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
		void unpack_flags(uint8_t flags);
		void pack();
		bool unpack();
		bool send();
		bool resend();
		void prove(const Destination &destination = Destination::NONE);
		void update_hash();
		const Bytes get_hash() const;
		const Bytes getTruncatedHash() const;
		const Bytes get_hashable_part() const;
		//zProofDestination &generate_proof_destination();

		// getters/setters
		inline const Destination &destination() const { assert(_object); return _object->_destination; }
		inline void destination(const Destination &destination) { assert(_object); _object->_destination = destination; }
		inline const Link &link() const { assert(_object); return _object->_link; }
		inline void link(const Link &link) { assert(_object); _object->_link = link; }
		inline const Interface &attached_interface() const { assert(_object); return _object->_attached_interface; }
		inline const Interface &receiving_interface() const { assert(_object); return _object->_receiving_interface; }
		inline void receiving_interface(const Interface &receiving_interface) { assert(_object); _object->_receiving_interface = receiving_interface; }
		inline header_types header_type() const { assert(_object); return _object->_header_type; }
		inline Transport::types transport_type() const { assert(_object); return _object->_transport_type; }
		inline Destination::types destination_type() const { assert(_object); return _object->_destination_type; }
		inline types packet_type() const { assert(_object); return _object->_packet_type; }
		inline context_types context() const { assert(_object); return _object->_context; }
		inline bool sent() const { assert(_object); return _object->_sent; }
		inline void sent(bool sent) { assert(_object); _object->_sent = sent; }
		inline time_t sent_at() const { assert(_object); return _object->_sent_at; }
		inline void sent_at(time_t sent_at) { assert(_object); _object->_sent_at = sent_at; }
		inline bool create_receipt() const { assert(_object); return _object->_create_receipt; }
		inline const PacketReceipt &receipt() const { assert(_object); return _object->_receipt; }
		inline void receipt(const PacketReceipt &receipt) { assert(_object); _object->_receipt = receipt; }
		inline uint8_t flags() const { assert(_object); return _object->_flags; }
		inline uint8_t hops() const { assert(_object); return _object->_hops; }
		inline void hops(uint8_t hops) { assert(_object); _object->_hops = hops; }
		inline Bytes packet_hash() const { assert(_object); return _object->_packet_hash; }
		inline Bytes destination_hash() const { assert(_object); return _object->_destination_hash; }
		inline Bytes transport_id() const { assert(_object); return _object->_transport_id; }
		inline void transport_id(const Bytes &transport_id) { assert(_object); _object->_transport_id = transport_id; }
		inline const Bytes &raw() const { assert(_object); return _object->_raw; }
		inline const Bytes &data() const { assert(_object); return _object->_data; }

		inline std::string toString() const { assert(_object); return "{Packet:" + _object->_packet_hash.toHex() + "}"; }

		std::string debugString() const;

	private:
		class Object {
		public:
			Object(const Destination &destination, const Interface &attached_interface) : _destination(destination), _attached_interface(attached_interface) {}
			virtual ~Object() {}
		private:
			Destination _destination = {Destination::NONE};
			Link _link = {Link::NONE};

			Interface _attached_interface = {Interface::NONE};
			Interface _receiving_interface = {Interface::NONE};

			header_types _header_type = HEADER_1;
			Transport::types _transport_type = Transport::BROADCAST;
			Destination::types _destination_type = Destination::SINGLE;
			types _packet_type = DATA;
			context_types _context = CONTEXT_NONE;

			uint8_t _flags = 0;
			uint8_t _hops = 0;

			bool _packed = false;
			bool _sent = false;
			bool _create_receipt = false;
			bool _fromPacked = false;
			bool _truncated = false;	// whether data was truncated
			bool _encrypted = false;	// whether data is encrytpted
			PacketReceipt _receipt;

			uint16_t _mtu = Reticulum::MTU;
			time_t _sent_at = 0;

			float _rssi = 0.0;
			float _snr = 0.0;

			Bytes _packet_hash;
			Bytes _destination_hash;
			Bytes _transport_id;

			Bytes _raw;		// header + ( plaintext | ciphertext-token )
			Bytes _data;	// plaintext | ciphertext

		friend class Packet;
		};
		std::shared_ptr<Object> _object;
	};

}
