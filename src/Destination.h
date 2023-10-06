#pragma once

#include "Reticulum.h"
#include "Identity.h"
#include "Bytes.h"

#include <stdint.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <map>

namespace RNS {

	class Interface;
	class Packet;
	class Link;

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
			using link_established = void(*)(Link *link);
			//using packet = void(*)(uint8_t *data, uint16_t data_len, Packet *packet);
			using packet = void(*)(const Bytes &data, Packet *packet);
			using proof_requested = void(*)(Packet *packet);

			link_established _link_established = nullptr;
			packet _packet = nullptr;
			proof_requested _proof_requested = nullptr;
		};

		//typedef std::pair<time_t, std::string> Response;
		using Response = std::pair<time_t, Bytes>;
		//using Response = std::pair<time_t, std::vector<uint8_t>>;

		enum NoneConstructor {
			NONE
		};

		// Constants
		enum types {
			SINGLE     = 0x00,
			GROUP      = 0x01,
			PLAIN      = 0x02,
			LINK       = 0x03,
		};

		enum proof_strategies {
			PROVE_NONE = 0x21,
			PROVE_APP  = 0x22,
			PROVE_ALL  = 0x23,
		};

		enum request_policies {
			ALLOW_NONE = 0x00,
			ALLOW_ALL  = 0x01,
			ALLOW_LIST = 0x02,
		};

		enum directions {
			IN         = 0x11,
			OUT        = 0x12,
		};

		const uint8_t PR_TAG_WINDOW = 30;

	public:
		Destination(NoneConstructor none) {
			extreme("Destination NONE object created");
		}
		Destination(const Destination &destination) : _object(destination._object) {
			extreme("Destination object copy created");
		}
		Destination(const Identity &identity, const directions direction, const types type, const char* app_name, const char *aspects);
		~Destination();

		inline Destination& operator = (const Destination &destination) {
			_object = destination._object;
			extreme("Destination object copy created by assignment, this: " + std::to_string((ulong)this) + ", data: " + std::to_string((uint32_t)_object.get()));
			return *this;
		}
		inline operator bool() const {
			return _object.get() != nullptr;
		}

	public:
		static Bytes hash(const Identity &identity, const char *app_name, const char *aspects);
		static std::string expand_name(const Identity &identity, const char *app_name, const char *aspects);

	public:
		Packet announce(const Bytes &app_data = {}, bool path_response = false, Interface *attached_interface = nullptr, const Bytes &tag = {}, bool send = true);
		inline void set_accepts_links(bool accepts) { assert(_object); _object->_accept_link_requests = accepts; }
		inline bool get_accepts_links() { assert(_object); return _object->_accept_link_requests; }
		void set_link_established_callback(Callbacks::link_established callback);
		void set_packet_callback(Callbacks::packet callback);
		void set_proof_requested_callback(Callbacks::proof_requested callback);
		Bytes encrypt(const Bytes &data);
		Bytes decrypt(const Bytes &data);
		Bytes sign(const Bytes &message);

		// getters/setters
		inline types type() const { assert(_object); return _object->_type; }
		inline directions _direction() const { assert(_object); return _object->_direction; }
		inline proof_strategies _proof_strategy() const { assert(_object); return _object->_proof_strategy; }
		inline Bytes hash() const { assert(_object); return _object->_hash; }
		inline Bytes link_id() const { assert(_object); return _object->_link_id; }
		inline uint16_t mtu() const { assert(_object); return _object->_mtu; }
		inline void mtu(uint16_t mtu) { assert(_object); _object->_mtu = mtu; }

	private:
		class Object {
		public:
			Object(const Identity &identity) : _identity(identity) {}
		private:
			bool _accept_link_requests = true;
			Callbacks _callbacks;
			//z_request_handlers = {}
			types _type;
			directions _direction;
			proof_strategies _proof_strategy = PROVE_NONE;
			uint16_t _mtu = 0;

			//std::vector<Response> _path_responses;
			std::map<Bytes, Response> _path_responses;
			//z_links = []

			Identity _identity;
			std::string _name;

			// Generate the destination address hash
			Bytes _hash;
			Bytes _name_hash;
			std::string _hexhash;

			// CBA TODO when is _default_app_data a "callable"?
			Bytes _default_app_data;
			//z_callback = None
			//z_proofcallback = None

			// CBA _link_id is expected by packet but only present in Link
			// CBA TODO determine if Link needs to inherit from Destination or vice-versa
			Bytes _link_id;
		friend class Destination;
		};
		std::shared_ptr<Object> _object;

	};

}