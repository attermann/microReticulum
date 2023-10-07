#pragma once

#include "../Log.h"

#include <memory>

namespace RNS {

	class Interface {

	public:
		enum NoneConstructor {
			NONE
		};

	public:
		// Interface mode definitions
		enum modes {
		MODE_FULL           = 0x01,
		MODE_POINT_TO_POINT = 0x02,
		MODE_ACCESS_POINT   = 0x03,
		MODE_ROAMING        = 0x04,
		MODE_BOUNDARY       = 0x05,
		MODE_GATEWAY        = 0x06,
		};

		// Which interface modes a Transport Node
		// should actively discover paths for.
		//zDISCOVER_PATHS_FOR  = [MODE_ACCESS_POINT, MODE_GATEWAY]

		public:
		Interface(NoneConstructor none) {
			extreme("Interface object NONE created");
		}
		Interface(const Interface &interface) : _object(interface._object) {
			extreme("Interface object copy created");
		}
		Interface();
		~Interface();

		inline Interface& operator = (const Interface &interface) {
			_object = interface._object;
			extreme("Interface object copy created by assignment, this: " + std::to_string((ulong)this) + ", data: " + std::to_string((uint32_t)_object.get()));
			return *this;
		}
		inline operator bool() const {
			return _object.get() != nullptr;
		}

	private:
		class Object {
		private:


		friend class Interface;
		};
		std::shared_ptr<Object> _object;

	};

}
