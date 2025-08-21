#pragma once

#include "Destination.h"
#include "Type.h"

#include <memory>
#include <cassert>

namespace RNS {

	class Channel {

	public:
		Channel(Type::NoneConstructor none) {
			MEM("Channel NONE object created");
		}
		Channel(const Channel& resource) : _object(resource._object) {
			MEM("Channel object copy created");
		}
		virtual ~Channel(){
			MEM("Channel object destroyed");
		}

		Channel& operator = (const Channel& resource) {
			_object = resource._object;
			return *this;
		}
		operator bool() const {
			return _object.get() != nullptr;
		}
		bool operator < (const Channel& resource) const {
			return _object.get() < resource._object.get();
			//return _object->_hash < resource._object->_hash;
		}

	public:
		void _shutdown() {}

	private:
		class Object {
		public:
			Object() { MEM("Channel::Data object created, this: " + std::to_string((uintptr_t)this)); }
			virtual ~Object() { MEM("Channel::Data object destroyed, this: " + std::to_string((uintptr_t)this)); }
		private:

		friend class Channel;
		};
		std::shared_ptr<Object> _object;

	};


	class ChannelAdvertisement {

	};

}
