#pragma once

#include "Log.h"
#include "Type.h"

#include <vector>
#include <memory>
#include <stdint.h>

namespace RNS {

	class Reticulum {

	public:
		static bool __transport_enabled;
		static bool __use_implicit_proof;
		static bool __allow_probes;
		static bool panic_on_interface_error;

	public:
		Reticulum(Type::NoneConstructor none) {
			mem("Reticulum NONE object created, this: " + std::to_string((uintptr_t)this) + ", data: " + std::to_string((uintptr_t)_object.get()));
		}
		Reticulum(const Reticulum& reticulum) : _object(reticulum._object) {
			mem("Reticulum object copy created, this: " + std::to_string((uintptr_t)this) + ", data: " + std::to_string((uintptr_t)_object.get()));
		}
		Reticulum();
		virtual ~Reticulum() {
			mem("Reticulum object destroyed, this: " + std::to_string((uintptr_t)this) + ", data: " + std::to_string((uintptr_t)_object.get()));
		}

		inline Reticulum& operator = (const Reticulum& reticulum) {
			_object = reticulum._object;
			mem("Reticulum object copy created by assignment, this: " + std::to_string((uintptr_t)this) + ", data: " + std::to_string((uintptr_t)_object.get()));
			return *this;
		}
		inline operator bool() const {
			return _object.get() != nullptr;
		}
		inline bool operator < (const Reticulum& reticulum) const {
			return _object.get() < reticulum._object.get();
		}

	public:
		void loop();

		/*
		Returns whether proofs sent are explicit or implicit.

		:returns: True if the current running configuration specifies to use implicit proofs. False if not.
		*/
		inline static bool should_use_implicit_proof() { return __use_implicit_proof; }

		/*
		Returns whether Transport is enabled for the running
		instance.

		When Transport is enabled, Reticulum will
		route traffic for other peers, respond to path requests
		and pass announces over the network.

		:returns: True if Transport is enabled, False if not.
		*/
		inline static bool transport_enabled() { return __transport_enabled; }

		inline static bool probe_destination_enabled() { return __allow_probes; }

		// getters/setters
		inline bool is_connected_to_shared_instance() const { assert(_object); return _object->_is_connected_to_shared_instance; }

	private:
		class Object {
		public:
			Object() {
				mem("Reticulum data object created, this: " + std::to_string((uintptr_t)this));
			}
			virtual ~Object() {
				mem("Reticulum data object destroyed, this: " + std::to_string((uintptr_t)this));
			}
		private:
			bool _is_connected_to_shared_instance = false;
		friend class Reticulum;
		};
		std::shared_ptr<Object> _object;

	};

}
