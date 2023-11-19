#pragma once

#include "Reticulum.h"
#include "Identity.h"
// CBA TODO resolve circular dependency with following header file
//#include "Packet.h"
#include "Bytes.h"
#include "None.h"

#include <memory>

namespace RNS {

	class Link {

	public:
		class Callbacks {
		public:
			typedef void (*established)(Link *link);
			typedef void (*closed)(Link *link);
		};

		enum NoneConstructor {
			NONE
		};

		static constexpr const char* CURVE = Identity::CURVE;
		// The curve used for Elliptic Curve DH key exchanges

		static const uint16_t ECPUBSIZE         = 32+32;
		static const uint8_t KEYSIZE           = 32;

		//static const uint16_t MDU = floor((Reticulum::MTU-Reticulum::IFAC_MIN_SIZE-Reticulum::HEADER_MINSIZE-Identity::FERNET_OVERHEAD)/Identity::AES128_BLOCKSIZE)*Identity::AES128_BLOCKSIZE - 1;
		static const uint16_t MDU = ((Reticulum::MTU-Reticulum::IFAC_MIN_SIZE-Reticulum::HEADER_MINSIZE-Identity::FERNET_OVERHEAD)/Identity::AES128_BLOCKSIZE)*Identity::AES128_BLOCKSIZE - 1;

		static const uint8_t ESTABLISHMENT_TIMEOUT_PER_HOP = Reticulum::DEFAULT_PER_HOP_TIMEOUT;
		// Timeout for link establishment in seconds per hop to destination.

		static const uint16_t TRAFFIC_TIMEOUT_FACTOR = 6;
		static const uint16_t KEEPALIVE_TIMEOUT_FACTOR = 4;
		// RTT timeout factor used in link timeout calculation.
		static const uint8_t STALE_GRACE = 2;
		// Grace period in seconds used in link timeout calculation.
		static const uint16_t KEEPALIVE = 360;
		// Interval for sending keep-alive packets on established links in seconds.
		static const uint16_t STALE_TIME = 2*KEEPALIVE;
		/*
		If no traffic or keep-alive packets are received within this period, the
		link will be marked as stale, and a final keep-alive packet will be sent.
		If after this no traffic or keep-alive packets are received within ``RTT`` *
		``KEEPALIVE_TIMEOUT_FACTOR`` + ``STALE_GRACE``, the link is considered timed out,
		and will be torn down.
		*/

		enum status {
			PENDING   = 0x00,
			HANDSHAKE = 0x01,
			ACTIVE    = 0x02,
			STALE     = 0x03,
			CLOSED    = 0x04
		};

		enum teardown_reasons {
			TIMEOUT            = 0x01,
			INITIATOR_CLOSED   = 0x02,
			DESTINATION_CLOSED = 0x03,
		};

		enum resource_strategies {
			ACCEPT_NONE = 0x00,
			ACCEPT_APP  = 0x01,
			ACCEPT_ALL  = 0x02,
		};

	public:
		Link(NoneConstructor none) {
			extreme("Link NONE object created");
		}
		Link(RNS::NoneConstructor none) {
			extreme("Link NONE object created");
		}
		Link(const Link &link) : _object(link._object) {
			extreme("Link object copy created");
		}
		Link();
		virtual ~Link(){
			extreme("Link object destroyed");
		}

		inline Link& operator = (const Link &link) {
			_object = link._object;
			return *this;
		}
		inline operator bool() const {
			return _object.get() != nullptr;
		}
		inline bool operator < (const Link &link) const {
			return _object.get() < link._object.get();
		}

	public:
		void set_link_id(const Packet &packet);
		void receive(const Packet &packet);

		// getters/setters
		inline const Bytes &link_id() const { assert(_object); return _object->_link_id; }
		inline const Bytes &hash() const { assert(_object); return _object->_hash; }

		inline std::string toString() const { assert(_object); return "{Link: unknown}"; }

	private:
		class Object {
		public:
			Object() {}
			virtual ~Object() {}
		private:
			Bytes _link_id;
			Bytes _hash;
		friend class Link;
		};
		std::shared_ptr<Object> _object;

	};

}