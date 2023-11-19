#pragma once

#include "../Log.h"
#include "../Bytes.h"
#include "../Type.h"

#include <list>
#include <memory>
#include <stdint.h>

namespace RNS {

	class Interface {

	public:
	class AnnounceEntry {
	public:
		AnnounceEntry() {}
		AnnounceEntry(const Bytes &destination, uint64_t time, uint8_t hops, uint64_t emitted, const Bytes &raw) :
			_destination(destination),
			_time(time),
			_hops(hops),
			_emitted(emitted),
			_raw(raw) {}
	public:
		Bytes _destination;
		uint64_t _time = 0;
		uint8_t _hops = 0;
		uint64_t _emitted = 0;
		Bytes _raw;
	};

	public:
		// Which interface modes a Transport Node
		// should actively discover paths for.
		uint8_t DISCOVER_PATHS_FOR = Type::Interface::MODE_ACCESS_POINT | Type::Interface::MODE_GATEWAY;

	public:
		Interface(Type::NoneConstructor none) {
			extreme("Interface object NONE created");
		}
		Interface(const Interface &interface) : _object(interface._object) {
			extreme("Interface object copy created");
		}
		Interface() : _object(new Object()) {
			extreme("Interface object created");
		}
		Interface(const char *name) : _object(new Object(name)) {
			extreme("Interface object created");
		}
		virtual ~Interface() {
			extreme("Interface object destroyed");
		}

		inline Interface& operator = (const Interface &interface) {
			_object = interface._object;
			extreme("Interface object copy created by assignment, this: " + std::to_string((uintptr_t)this) + ", data: " + std::to_string((uintptr_t)_object.get()));
			return *this;
		}
		inline operator bool() const {
			return _object.get() != nullptr;
		}
		inline bool operator < (const Interface &interface) const {
			return _object.get() < interface._object.get();
		}

	public:
		inline Bytes get_hash() const { /*return Identity::full_hash();*/ return {}; }
		void process_announce_queue();
		inline void detach() {}

	    virtual void processIncoming(const Bytes &data);
		virtual void processOutgoing(const Bytes &data);

		inline void add_announce(AnnounceEntry &entry) { assert(_object); _object->_announce_queue.push_back(entry); }

		// getters/setters
	protected:
		inline void IN(bool IN) { assert(_object); _object->_IN = IN; }
		inline void OUT(bool OUT) { assert(_object); _object->_OUT = OUT; }
		inline void FWD(bool FWD) { assert(_object); _object->_FWD = FWD; }
		inline void RPT(bool RPT) { assert(_object); _object->_RPT = RPT; }
		inline void name(const char *name) { assert(_object); _object->_name = name; }
	public:
		inline bool IN() const { assert(_object); return _object->_IN; }
		inline bool OUT() const { assert(_object); return _object->_OUT; }
		inline bool FWD() const { assert(_object); return _object->_FWD; }
		inline bool RPT() const { assert(_object); return _object->_RPT; }
		inline std::string name() const { assert(_object); return _object->_name; }
		inline Bytes ifac_identity() const { assert(_object); return _object->_ifac_identity; }
		inline Type::Interface::modes mode() const { assert(_object); return _object->_mode; }
		inline uint32_t bitrate() const { assert(_object); return _object->_bitrate; }
		inline uint64_t announce_allowed_at() const { assert(_object); return _object->_announce_allowed_at; }
		inline void announce_allowed_at(uint64_t announce_allowed_at) { assert(_object); _object->_announce_allowed_at = announce_allowed_at; }
		inline float announce_cap() const { assert(_object); return _object->_announce_cap; }
		inline std::list<AnnounceEntry> announce_queue() const { assert(_object); return _object->_announce_queue; }

		virtual inline std::string toString() const { assert(_object); return "Interface[" + _object->_name + "]"; }

	private:
		class Object {
		public:
			Object() {}
			Object(const char *name) : _name(name) {}
			virtual ~Object() {}
		private:
			bool _IN  = false;
			bool _OUT = false;
			bool _FWD = false;
			bool _RPT = false;
			std::string _name;
			size_t _rxb = 0;
			size_t _txb = 0;
			bool online = false;
			Bytes _ifac_identity;
			Type::Interface::modes _mode = Type::Interface::MODE_NONE;
			uint32_t _bitrate = 0;
			uint64_t _announce_allowed_at = 0;
			float _announce_cap = 0.0;
			std::list<AnnounceEntry> _announce_queue;
			//Transport &_owner;
		friend class Interface;
		};
		std::shared_ptr<Object> _object;

	};

}
