#pragma once

#include "Packet.h"
#include "Destination.h"
#include "Bytes.h"
#include "Type.h"

#include <memory>
#include <cassert>

namespace RNS {

	class Packet;


	class RequestReceiptCallbacks {
	private:
		//z self.response = None
		//z self.failed   = None
		//z self.progress = None
	};

	class Link {

/*
		This class is used to establish and manage links to other peers. When a
		link instance is created, Reticulum will attempt to establish verified
		and encrypted connectivity with the specified destination.

		:param destination: A :ref:`RNS.Destination<api-destination>` instance which to establish a link to.
		:param established_callback: An optional function or method with the signature *callback(link)* to be called when the link has been established.
		:param closed_callback: An optional function or method with the signature *callback(link)* to be called when the link is closed.
*/

		static uint8_t resource_strategies;

	public:
		class Callbacks {
		public:
			using established = void(*)(const Link& link);
			using closed = void(*)(const Link& link);
			using packet = void(*)(const Bytes& plaintext, const Packet& packet);
			using remote_identified = void(*)(const Link& link, const Identity& remote_identity);
			//using resource = void(*)(const ResourceAdvertisement& resource_advertisement);
			//using resource_started = void(*)(const Resource& resource);
			//using resource_concluded = void(*)(const Resource& resource);
		public:
			established _established = nullptr;
			closed _closed = nullptr;
			packet _packet = nullptr;
			remote_identified _remote_identified = nullptr;
			//resource _resource = nullptr;
			//resource_started _resource_started = nullptr;
			//resource_concluded _resource_concluded = nullptr;

		friend class Link;
		};

	public:
		Link(Type::NoneConstructor none) {
			MEM("Link NONE object created");
		}
		Link(const Link& link) : _object(link._object) {
			MEM("Link object copy created");
		}
		Link(const Destination& destination = {Type::NONE}, Callbacks::established established_callback = nullptr, Callbacks::closed closed_callback = nullptr, const Destination& owner = {Type::NONE}, const Bytes& peer_pub_bytes = {Bytes::NONE}, const Bytes& peer_sig_pub_bytes = {Bytes::NONE});
		virtual ~Link(){
			MEM("Link object destroyed");
		}

		inline Link& operator = (const Link& link) {
			_object = link._object;
			return *this;
		}
		inline operator bool() const {
			return _object.get() != nullptr;
		}
		inline bool operator < (const Link& link) const {
			return _object.get() < link._object.get();
		}

	public:
		void set_link_id(const Packet& packet);
		void receive(const Packet& packet);

		void prove();
		void prove_packet(const Packet& packet);

		// getters/setters
		inline const Destination& destination() const { assert(_object); return _object->_destination; }
		inline const Bytes& link_id() const { assert(_object); return _object->_link_id; }
		inline const Bytes& hash() const { assert(_object); return _object->_hash; }
		inline Type::Link::status status() const { assert(_object); return _object->_status; }

		inline std::string toString() const { if (!_object) return ""; return "{Link: unknown}"; }

	private:
		class Object {
		public:
			Object(const Destination& destination) : _destination(destination) {}
			virtual ~Object() {}
		private:
			Destination _destination = {Type::NONE};
			Bytes _link_id;
			Bytes _hash;
			Type::Link::status _status = Type::Link::PENDING;

			//z _rtt = None
			//z _establishment_cost = 0
			Callbacks _callbacks;
			Type::Link::resource_strategy _resource_strategy = Type::Link::ACCEPT_NONE;
			//z _outgoing_resources = []
			//z _incoming_resources = []
			//z _pending_requests   = []
			double _last_inbound = 0.0;
			double _last_outbound = 0.0;
			double _last_proof = 0.0;
			double _last_data = 0.0;
			uint16_t _tx = 0;
			uint16_t _rx = 0;
			uint16_t _txbytes = 0;
			uint16_t _rxbytes = 0;
			float _rssi = 0.0;
			float _snr = 0.0;
			float _q = 0.0;
			uint16_t _traffic_timeout_factor = Type::Link::TRAFFIC_TIMEOUT_FACTOR;
			uint16_t _keepalive_timeout_factor = Type::Link::KEEPALIVE_TIMEOUT_FACTOR;
			uint16_t _keepalive = Type::Link::KEEPALIVE;
			uint16_t _stale_time = Type::Link::STALE_TIME;
			bool _watchdog_lock = false;
			double _activated_at = 0.0;
			Type::Destination::types _type = Type::Destination::LINK;
			const Destination _destination = {Type::NONE};
			Destination _owner = {Type::NONE};
			bool _initiator = false;
			uint8_t _expected_hops = 0;
			const Interface _attached_interface = {Type::NONE};
			const Identity ___remote_identity = {Type::NONE};
			bool ___track_phy_stats = false;
			//z const Channel __channel = {Type::NONE};

			Cryptography::X25519PrivateKey::Ptr _prv;
			Bytes _prv_bytes;

			Cryptography::Ed25519PrivateKey::Ptr _sig_prv;
			Bytes _sig_prv_bytes;

			Cryptography::X25519PublicKey::Ptr _pub;
			Bytes _pub_bytes;

			Cryptography::Ed25519PublicKey::Ptr _sig_pub;
			Bytes _sig_pub_bytes;

		friend class Link;
		};
		std::shared_ptr<Object> _object;

	};

}