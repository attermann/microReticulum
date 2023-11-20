#pragma once

#include "Bytes.h"
#include "Type.h"

#include <memory>

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
			extreme("Link NONE object created");
		}
		Link(const Link& link) : _object(link._object) {
			extreme("Link object copy created");
		}
		Link();
		virtual ~Link(){
			extreme("Link object destroyed");
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

		// getters/setters
		inline const Bytes& link_id() const { assert(_object); return _object->_link_id; }
		inline const Bytes& hash() const { assert(_object); return _object->_hash; }

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