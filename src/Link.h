#pragma once

#include "Destination.h"
#include "Bytes.h"
#include "Type.h"

#include <memory>
#include <cassert>

namespace RNS {

	class Packet;

	class Link {

	public:
		class Callbacks {
		public:
			typedef void (*established)(Link *link);
			typedef void (*closed)(Link *link);
		};

	public:
		Link(Type::NoneConstructor none) {
			MEM("Link NONE object created");
		}
		Link(const Link& link) : _object(link._object) {
			MEM("Link object copy created");
		}
		Link(const Destination& destination);
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

		inline std::string toString() const { if (!_object) return "";
		 return "{Link: unknown}"; }

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
		friend class Link;
		};
		std::shared_ptr<Object> _object;

	};

}