#pragma once

#include "Destination.h"
#include "Link.h"
#include "Interface.h"
#include "Bytes.h"
#include "Log.h"
#include "Type.h"
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
			using delivery = void(*)(const PacketReceipt& packet_receipt);
			using timeout = void(*)(const PacketReceipt& packet_receipt);
		public:
			delivery _delivery = nullptr;
			timeout _timeout = nullptr;
		friend class PacketReceipt;
		};

	public:
		PacketReceipt(Type::NoneConstructor none) {}
		PacketReceipt(const PacketReceipt& packet_receipt) : _object(packet_receipt._object) {}
		PacketReceipt() : _object(new Object()) {}
		PacketReceipt(const Packet& packet);

		inline PacketReceipt& operator = (const PacketReceipt& packet_receipt) {
			_object = packet_receipt._object;
			return *this;
		}
		inline operator bool() const {
			return _object.get() != nullptr;
		}
		inline bool operator < (const PacketReceipt& packet_receipt) const {
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

		// getters/setters
		inline const Bytes& hash() const { assert(_object); return _object->_hash; }
		inline Type::PacketReceipt::Status status() const { assert(_object); return _object->_status; }

	private:
		class Object {
		public:
			Object() {}
			virtual ~Object() {}
		private:
			Bytes _hash;
			Bytes _truncated_hash;
			bool _sent = true;
			double _sent_at = Utilities::OS::time();
			bool _proved = false;
			Type::PacketReceipt::Status _status = Type::PacketReceipt::SENT;
			Destination _destination = {Type::NONE};
			Callbacks _callbacks;
			double _concluded_at = 0;
			//z Packet _proof_packet;
			int16_t _timeout = 0;
		friend class PacketReceipt;
		};
		std::shared_ptr<Object> _object;

	};


	class Packet {

	public:
		//static constexpr const uint8_t EMPTY_DESTINATION[Type::Reticulum::DESTINATION_LENGTH] = {0};
		uint8_t EMPTY_DESTINATION[Type::Reticulum::DESTINATION_LENGTH] = {0};

	public:
		Packet(Type::NoneConstructor none) {
			mem("Packet NONE object created, this: " + std::to_string((uintptr_t)this) + ", data: " + std::to_string((uintptr_t)_object.get()));
		}
		Packet(const Packet& packet) : _object(packet._object) {
			mem("Packet object copy created, this: " + std::to_string((uintptr_t)this) + ", data: " + std::to_string((uintptr_t)_object.get()));
		}
		Packet(
			const Destination& destination,
			const Interface& attached_interface,
			const Bytes& data,
			Type::Packet::types packet_type = Type::Packet::DATA,
			Type::Packet::context_types context = Type::Packet::CONTEXT_NONE,
			Type::Transport::types transport_type = Type::Transport::BROADCAST,
			Type::Packet::header_types header_type = Type::Packet::HEADER_1,
			const Bytes& transport_id = {Bytes::NONE},
			bool create_receipt = true
		);
		Packet(
			const Destination& destination,
			const Bytes& data,
			Type::Packet::types packet_type = Type::Packet::DATA,
			Type::Packet::context_types context = Type::Packet::CONTEXT_NONE,
			Type::Transport::types transport_type = Type::Transport::BROADCAST,
			Type::Packet::header_types header_type = Type::Packet::HEADER_1,
			const Bytes& transport_id = {Bytes::NONE},
			bool create_receipt = true
		) : Packet(destination, {Type::NONE}, data, packet_type, context, transport_type, header_type, transport_id, create_receipt) {}
		virtual ~Packet() {
			mem("Packet object destroyed, this: " + std::to_string((uintptr_t)this) + ", data: " + std::to_string((uintptr_t)_object.get()));
		}			

		inline Packet& operator = (const Packet& packet) {
			_object = packet._object;
			mem("Packet object copy created by assignment, this: " + std::to_string((uintptr_t)this) + ", data: " + std::to_string((uintptr_t)_object.get()));
			return *this;
		}
		inline operator bool() const {
			return _object.get() != nullptr;
		}
		inline bool operator < (const Packet& packet) const {
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
		void prove(const Destination& destination = {Type::NONE});
		void update_hash();
		const Bytes get_hash() const;
		const Bytes getTruncatedHash() const;
		const Bytes get_hashable_part() const;
		//z ProofDestination& generate_proof_destination();

		// getters/setters
		inline const Destination& destination() const { assert(_object); return _object->_destination; }
		inline void destination(const Destination& destination) { assert(_object); _object->_destination = destination; }
		inline const Link& link() const { assert(_object); return _object->_link; }
		inline void link(const Link& link) { assert(_object); _object->_link = link; }
		inline const Interface& attached_interface() const { assert(_object); return _object->_attached_interface; }
		inline const Interface& receiving_interface() const { assert(_object); return _object->_receiving_interface; }
		inline void receiving_interface(const Interface& receiving_interface) { assert(_object); _object->_receiving_interface = receiving_interface; }
		inline Type::Packet::header_types header_type() const { assert(_object); return _object->_header_type; }
		inline Type::Transport::types transport_type() const { assert(_object); return _object->_transport_type; }
		inline Type::Destination::types destination_type() const { assert(_object); return _object->_destination_type; }
		inline Type::Packet::types packet_type() const { assert(_object); return _object->_packet_type; }
		inline Type::Packet::context_types context() const { assert(_object); return _object->_context; }
		inline bool sent() const { assert(_object); return _object->_sent; }
		inline void sent(bool sent) { assert(_object); _object->_sent = sent; }
		inline double sent_at() const { assert(_object); return _object->_sent_at; }
		inline void sent_at(double sent_at) { assert(_object); _object->_sent_at = sent_at; }
		inline bool create_receipt() const { assert(_object); return _object->_create_receipt; }
		inline const PacketReceipt& receipt() const { assert(_object); return _object->_receipt; }
		inline void receipt(const PacketReceipt& receipt) { assert(_object); _object->_receipt = receipt; }
		inline uint8_t flags() const { assert(_object); return _object->_flags; }
		inline uint8_t hops() const { assert(_object); return _object->_hops; }
		inline void hops(uint8_t hops) { assert(_object); _object->_hops = hops; }
		inline const Bytes& packet_hash() const { assert(_object); return _object->_packet_hash; }
		inline const Bytes& destination_hash() const { assert(_object); return _object->_destination_hash; }
		inline const Bytes& transport_id() const { assert(_object); return _object->_transport_id; }
		inline void transport_id(const Bytes& transport_id) { assert(_object); _object->_transport_id = transport_id; }
		inline const Bytes& raw() const { assert(_object); return _object->_raw; }
		inline const Bytes& data() const { assert(_object); return _object->_data; }

		inline std::string toString() const { if (!_object) return ""; return "{Packet:" + _object->_packet_hash.toHex() + "}"; }

#ifndef NDEBUG
		std::string debugString() const;
		std::string dumpString() const;
#endif

	private:
		class Object {
		public:
			Object(const Destination& destination, const Interface& attached_interface) : _destination(destination), _attached_interface(attached_interface) { mem("Packet::Data object created, this: " + std::to_string((uintptr_t)this)); }
			virtual ~Object() { mem("Identity::Data object destroyed, this: " + std::to_string((uintptr_t)this)); }
		private:
			Destination _destination = {Type::NONE};
			Link _link = {Type::NONE};

			Interface _attached_interface = {Type::NONE};
			Interface _receiving_interface = {Type::NONE};

			Type::Packet::header_types _header_type = Type::Packet::HEADER_1;
			Type::Transport::types _transport_type = Type::Transport::BROADCAST;
			Type::Destination::types _destination_type = Type::Destination::SINGLE;
			Type::Packet::types _packet_type = Type::Packet::DATA;
			Type::Packet::context_types _context = Type::Packet::CONTEXT_NONE;

			uint8_t _flags = 0;
			uint8_t _hops = 0;

			bool _packed = false;
			bool _sent = false;
			bool _create_receipt = false;
			bool _fromPacked = false;
			bool _truncated = false;	// whether data was truncated
			bool _encrypted = false;	// whether data is encrytpted
			PacketReceipt _receipt;

			uint16_t _mtu = Type::Reticulum::MTU;
			double _sent_at = 0;

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

/*
namespace ArduinoJson {
	inline bool convertToJson(const RNS::Packet& src, JsonVariant dst) {
		if (!src) {
			return dst.set(nullptr);
		}
		return dst.set(src.get_hash().toHex());
	}
	void convertFromJson(JsonVariantConst src, RNS::Packet& dst);
	inline bool canConvertFromJson(JsonVariantConst src, const RNS::Packet&) {
		return src.is<const char*>() && strlen(src.as<const char*>()) == 64;
	}
}
*/
/*
namespace ArduinoJson {
	template <>
	struct Converter<RNS::Packet> {
		static bool toJson(const RNS::Packet& src, JsonVariant dst) {
			if (!src) {
				return dst.set(nullptr);
			}
			RNS::extreme("<<< Serializing packet hash " + src.get_hash().toHex());
			return dst.set(src.get_hash().toHex());
		}
		static RNS::Packet fromJson(JsonVariantConst src) {
			if (!src.isNull()) {
				RNS::Bytes hash;
				hash.assignHex(src.as<const char*>());
				RNS::extreme(">>> Deserialized packet hash " + hash.toHex());
				RNS::extreme(">>> Querying transport for cached packet");
				// Query transport for matching interface
				return RNS::Packet::get_cached_packet(hash);
			}
			else {
				return {RNS::Type::NONE};
			}
		}
		static bool checkJson(JsonVariantConst src) {
			return src.is<const char*>() && strlen(src.as<const char*>()) == 64;
		}
	};
}
*/
