#pragma once

#include "Log.h"

#include <memory>
#include <stdint.h>
#include <vector>

namespace RNS {

	class Reticulum {

	private:
		class Object {
		private:
		friend class Reticulum;
		};

	public:
		enum NoneConstructor {
			NONE
		};

		// Future minimum will probably be locked in at 251 bytes to support
		// networks with segments of different MTUs. Absolute minimum is 219.
		static const uint16_t MTU            = 500;
		/*
		The MTU that Reticulum adheres to, and will expect other peers to
		adhere to. By default, the MTU is 507 bytes. In custom RNS network
		implementations, it is possible to change this value, but doing so will
		completely break compatibility with all other RNS networks. An identical
		MTU is a prerequisite for peers to communicate in the same network.

		Unless you really know what you are doing, the MTU should be left at
		the default value.
		*/

		static const uint16_t MAX_QUEUED_ANNOUNCES = 16384;
		static const uint32_t QUEUED_ANNOUNCE_LIFE = 60*60*24;

		static const uint8_t ANNOUNCE_CAP = 2;
		/*
		The maximum percentage of interface bandwidth that, at any given time,
		may be used to propagate announces. If an announce was scheduled for
		broadcasting on an interface, but doing so would exceed the allowed
		bandwidth allocation, the announce will be queued for transmission
		when there is bandwidth available.

		Reticulum will always prioritise propagating announces with fewer
		hops, ensuring that distant, large networks with many peers on fast
		links don't overwhelm the capacity of smaller networks on slower
		mediums. If an announce remains queued for an extended amount of time,
		it will eventually be dropped.

		This value will be applied by default to all created interfaces,
		but it can be configured individually on a per-interface basis.
		*/

		static const uint16_t MINIMUM_BITRATE = 500;

		// TODO: To reach the 300bps level without unreasonably impacting
		// performance on faster links, we need a mechanism for setting
		// this value more intelligently. One option could be inferring it
		// from interface speed, but a better general approach would most
		// probably be to let Reticulum somehow continously build a map of
		// per-hop latencies and use this map for the timeout calculation. 
		static const uint8_t DEFAULT_PER_HOP_TIMEOUT = 6;

		static const uint16_t HASHLENGTH = 256;	// In bits
		// Length of truncated hashes in bits.
		static const uint16_t TRUNCATED_HASHLENGTH = 128;	// In bits

		static const uint16_t HEADER_MINSIZE   = 2+1+(TRUNCATED_HASHLENGTH/8)*1;	// In bytes
		static const uint16_t HEADER_MAXSIZE   = 2+1+(TRUNCATED_HASHLENGTH/8)*2;	// In bytes
		static const uint16_t IFAC_MIN_SIZE    = 1;
		//zIFAC_SALT        = bytes.fromhex("adf54d882c9a9b80771eb4995d702d4a3e733391b2a0f53f416d9f907e55cff8")

		static const uint16_t MDU              = MTU - HEADER_MAXSIZE - IFAC_MIN_SIZE;

		static const uint32_t RESOURCE_CACHE   = 24*60*60;
		static const uint16_t JOB_INTERVAL     = 5*60;
		static const uint16_t CLEAN_INTERVAL   = 15*60;
		static const uint16_t PERSIST_INTERVAL = 60*60*12;
		static const uint16_t GRACIOUS_PERSIST_INTERVAL = 60*5;

		static const uint8_t DESTINATION_LENGTH = TRUNCATED_HASHLENGTH/8;	// In bytes

	public:
		Reticulum();
		Reticulum(NoneConstructor none) {
			extreme("Reticulum NONE object created");
		}
		Reticulum(const Reticulum &reticulum) : _object(reticulum._object) {
			extreme("Reticulum object copy created");
		}
		~Reticulum();

		inline Reticulum& operator = (const Reticulum &reticulum) {
			_object = reticulum._object;
			extreme("Reticulum object copy created by assignment, this: " + std::to_string((ulong)this) + ", data: " + std::to_string((uint32_t)_object.get()));
			return *this;
		}
		inline operator bool() const {
			return _object.get() != nullptr;
		}

	private:
		std::shared_ptr<Object> _object;

	};

}
