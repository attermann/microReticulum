#pragma once

//#include "Reticulum.h"
//#include "Link.h"
//#include "Interface.h"
#include "Identity.h"
#include "Bytes.h"
#include "Type.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <map>
#include <set>
#include <cassert>
#include <stdint.h>

namespace RNS {

	class Interface;
	class Link;
	class Packet;

	class RequestHandler {
	public:
		//p response_generator(path, data, request_id, link_id, remote_identity, requested_at)
		using response_generator = Bytes(*)(const Bytes& path, const Bytes& data, const Bytes& request_id, const Bytes& link_id, const Identity& remote_identity, double requested_at);
	public:
		RequestHandler(const RequestHandler& handler) {
			_path = handler._path;
			_response_generator = handler._response_generator;
			_allow = handler._allow;
			_allowed_list = handler._allowed_list;
		}
		Bytes _path;
		response_generator _response_generator = nullptr;
		Type::Destination::request_policies _allow = Type::Destination::ALLOW_NONE;
		std::set<Bytes> _allowed_list;
	};

    /**
     * @brief A class used to describe endpoints in a Reticulum Network. Destination
	 * instances are used both to create outgoing and incoming endpoints. The
	 * destination type will decide if encryption, and what type, is used in
	 * communication with the endpoint. A destination can also announce its
	 * presence on the network, which will also distribute necessary keys for
	 * encrypted communication with it.
	 * 
	 * @param identity An instance of :ref:`RNS.Identity<api-identity>`. Can hold only public keys for an outgoing destination, or holding private keys for an ingoing.
	 * @param direction ``RNS.Destination.IN`` or ``RNS.Destination.OUT``.
	 * @param type ``RNS.Destination.SINGLE``, ``RNS.Destination.GROUP`` or ``RNS.Destination.PLAIN``.
	 * @param app_name A string specifying the app name.
	 * @param aspects Any non-zero number of string arguments.
	 */
	class Destination {

	public:
		class Callbacks {
		public:
			using link_established = void(*)(const Link& link);
			//using packet = void(*)(uint8_t* data, uint16_t data_len, Packet *packet);
			using packet = void(*)(const Bytes& data, const Packet& packet);
			using proof_requested = bool(*)(const Packet& packet);
		public:
			link_established _link_established = nullptr;
			packet _packet = nullptr;
			proof_requested _proof_requested = nullptr;
		friend class Destination;
		};

		using PathResponse = std::pair<double, Bytes>;

	public:
		Destination(Type::NoneConstructor none) {
			MEM("Destination NONE object created, this: " + std::to_string((uintptr_t)this));
		}
		Destination(const Destination& destination) : _object(destination._object) {
			MEM("Destination object copy created, this: " + std::to_string((uintptr_t)this) + ", data: " + std::to_string((uintptr_t)_object.get()));
		}
		Destination(
			const Identity& identity,
			const Type::Destination::directions direction,
			const Type::Destination::types type,
			const char* app_name,
			const char* aspects
		);
		Destination(
			const Identity& identity,
			const Type::Destination::directions direction,
			const Type::Destination::types type,
			const Bytes& hash
		);
		virtual ~Destination();

		inline Destination& operator = (const Destination& destination) {
			_object = destination._object;
			MEM("Destination object copy created by assignment, this: " + std::to_string((uintptr_t)this) + ", data: " + std::to_string((uintptr_t)_object.get()));
			return *this;
		}
		inline operator bool() const {
			return _object.get() != nullptr;
		}
		inline bool operator < (const Destination& destination) const {
			return _object.get() < destination._object.get();
		}

	public:
		static std::string expand_name(const Identity& identity, const char* app_name, const char* aspects);
		static Bytes hash(const Identity& identity, const char* app_name, const char* aspects);
		static Bytes name_hash(const char* app_name, const char* aspects);
		static std::vector<std::string> app_and_aspects_from_name(const char* full_name);
		static Bytes hash_from_name_and_identity(const char* full_name, const Identity& identity);

	public:
		//Packet announce(const Bytes& app_data = {}, bool path_response = false, const Interface& attached_interface = {Type::NONE}, const Bytes& tag = {}, bool send = true);
		Packet announce(const Bytes& app_data, bool path_response, const Interface& attached_interface, const Bytes& tag = {}, bool send = true);
		Packet announce(const Bytes& app_data = {}, bool path_response = false);

		/*
		Set or query whether the destination accepts incoming link requests.

		:param accepts: If ``True`` or ``False``, this method sets whether the destination accepts incoming link requests. If not provided or ``None``, the method returns whether the destination currently accepts link requests.
		:returns: ``True`` or ``False`` depending on whether the destination accepts incoming link requests, if the *accepts* parameter is not provided or ``None``.
		*/
		inline void accepts_links(bool accepts) { assert(_object); _object->_accept_link_requests = accepts; }
		inline bool accepts_links() { assert(_object); return _object->_accept_link_requests; }

		/*
			Registers a function to be called when a link has been established to
			this destination.

			:param callback: A function or method with the signature *callback(link)* to be called when a new link is established with this destination.
		*/
		inline void set_link_established_callback(Callbacks::link_established callback) {
			assert(_object);
			_object->_callbacks._link_established = callback;
		}
		/*
			Registers a function to be called when a packet has been received by
			this destination.

			:param callback: A function or method with the signature *callback(data, packet)* to be called when this destination receives a packet.
		*/
		inline void set_packet_callback(Callbacks::packet callback)  {
			assert(_object);
			_object->_callbacks._packet = callback;
		}
		/*
			Registers a function to be called when a proof has been requested for
			a packet sent to this destination. Allows control over when and if
			proofs should be returned for received packets.

			:param callback: A function or method to with the signature *callback(packet)* be called when a packet that requests a proof is received. The callback must return one of True or False. If the callback returns True, a proof will be sent. If it returns False, a proof will not be sent.
		*/
		inline void set_proof_requested_callback(Callbacks::proof_requested callback)  {
			assert(_object);
			_object->_callbacks._proof_requested = callback;
		}

		/*
		Sets the destinations proof strategy.

		:param proof_strategy: One of ``RNS.Destination.PROVE_NONE``, ``RNS.Destination.PROVE_ALL`` or ``RNS.Destination.PROVE_APP``. If ``RNS.Destination.PROVE_APP`` is set, the `proof_requested_callback` will be called to determine whether a proof should be sent or not.
		*/
		inline void set_proof_strategy(Type::Destination::proof_strategies proof_strategy) {
			assert(_object);
			//if (proof_strategy <= PROOF_NONE) {
			//	throw throw std::invalid_argument("Unsupported proof strategy");
			//}
			_object->_proof_strategy = proof_strategy;
		}

		void receive(const Packet& packet);
		void incoming_link_request(const Bytes& data, const Packet& packet);

		virtual const Bytes encrypt(const Bytes& data);
		virtual const Bytes decrypt(const Bytes& data);
		virtual const Bytes sign(const Bytes& message);

		// CBA
		bool has_link(const Link& link);
		void remove_link(const Link& link);

		inline std::string toString() const { if (!_object) return ""; return "{Destination:" + _object->_hash.toHex() + "}"; }

		// getters
		inline Type::Destination::types type() const { assert(_object); return _object->_type; }
		inline Type::Destination::directions direction() const { assert(_object); return _object->_direction; }
		inline Type::Destination::proof_strategies proof_strategy() const { assert(_object); return _object->_proof_strategy; }
		inline const Bytes& hash() const { assert(_object); return _object->_hash; }
		inline const Bytes& link_id() const { assert(_object); return _object->_link_id; }
		inline uint16_t mtu() const { assert(_object); return _object->_mtu; }
		inline Type::Link::status status() const { assert(_object); return _object->_status; }
		inline const Callbacks& callbacks() const { assert(_object); return _object->_callbacks; }
		inline const Identity& identity() const { assert(_object); return _object->_identity; }
		inline const std::map<Bytes, PathResponse>& path_responses() const { assert(_object); return _object->_path_responses; }
		inline const std::map<Bytes, RequestHandler>& request_handlers() const { assert(_object); return _object->_request_handlers; }

		// setters
		// CBA Don't allow changing destination after construction since it's used as key in collections
		//inline void hash(const Bytes& hash) { assert(_object); _object->_hash = hash; _object->_hexhash = _object->_hash.toHex(); }
		inline void type(Type::Destination::types type) { assert(_object); _object->_type = type; }
		inline void link_id(const Bytes& id) { assert(_object); _object->_link_id = id; }
		inline void mtu(uint16_t mtu) { assert(_object); _object->_mtu = mtu; }
		inline void last_outbound(double time) { assert(_object); _object->_last_outbound = time; }
		inline void increment_tx() { assert(_object); ++_object->_tx; }
		inline void increment_txbytes(uint16_t bytes) { assert(_object); _object->_txbytes += bytes; }

	private:
		class Object {
		public:
			Object(const Identity& identity) : _identity(identity) { MEM("Destination::Data object created, this: " + std::to_string((uintptr_t)this)); }
			virtual ~Object() { MEM("Destination::Data object destroyed, this: " + std::to_string((uintptr_t)this)); }
		private:
			bool _accept_link_requests = true;
			Callbacks _callbacks;
			std::map<Bytes, RequestHandler> _request_handlers;
			Type::Destination::types _type;
			Type::Destination::directions _direction;
			Type::Destination::proof_strategies _proof_strategy = Type::Destination::PROVE_NONE;
			uint16_t _mtu = 0;

			std::map<Bytes, PathResponse> _path_responses;
			std::set<Link> _links;

			Identity _identity;
			std::string _name;

			// Generate the destination address hash
			Bytes _hash;
			Bytes _name_hash;
			std::string _hexhash;

			// CBA TODO when is _default_app_data a "callable"?
			Bytes _default_app_data;
			//z _callback = None
			//z _proofcallback = None

			// CBA _link_id is expected by packet but only present in Link
			// CBA TODO determine if Link needs to inherit from Destination or vice-versa
			Bytes _link_id;

			Type::Link::status _status;

			double _last_outbound = 0.0;
			uint16_t _tx = 0;
			uint32_t _txbytes = 0;

		friend class Destination;
		};
		std::shared_ptr<Object> _object;

	};

}