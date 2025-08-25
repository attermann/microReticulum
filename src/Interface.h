#pragma once

#include "Identity.h"
#include "Log.h"
#include "Bytes.h"
#include "Type.h"

#include <ArduinoJson.h>

#include <list>
#include <memory>
#include <cassert>
#include <stdint.h>

namespace RNS {

	class Interface;
	using HInterface = std::shared_ptr<Interface>;

	class AnnounceEntry {
	public:
		AnnounceEntry() {}
		AnnounceEntry(const Bytes& destination, double time, uint8_t hops, double emitted, const Bytes& raw) :
			_destination(destination),
			_time(time),
			_hops(hops),
			_emitted(emitted),
			_raw(raw) {}
	public:
		Bytes _destination;
		double _time = 0;
		uint8_t _hops = 0;
		uint64_t _emitted = 0;
		Bytes _raw;
	};

	class InterfaceImpl : public std::enable_shared_from_this<InterfaceImpl> {

	protected:
		InterfaceImpl() { MEMF("InterfaceImpl object created, this: 0x%X", this); }
		InterfaceImpl(const char* name) : _name(name) { MEMF("InterfaceImpl object created, this: 0x%X", this); }
	public:
		virtual ~InterfaceImpl() { MEMF("InterfaceImpl object destroyed, this: 0x%X", this); }

	protected:
		virtual bool start() { return true; }
		virtual void stop() {}
		virtual void loop() {}

		// CBA Virtual override method for custom interface to send outgoing data
		virtual void send_outgoing(const Bytes& data) = 0;
		
		// CBA Internal method to handle housekeeping for data going out on interface
		void handle_outgoing(const Bytes& data);
		// CBA Internal method to handle data coming in on interface and pass on to transport
		void handle_incoming(const Bytes& data);

		virtual const Bytes get_hash() const {
			return Identity::full_hash({toString()});
		}

		virtual inline std::string toString() const { return "Interface[" + _name + "]"; }

	protected:
		Interface* _parent = nullptr;
		bool _IN  = false;
		bool _OUT = false;
		bool _FWD = false;
		bool _RPT = false;
		std::string _name;
		size_t _rxb = 0;
		size_t _txb = 0;
		bool _online = false;
		Bytes _ifac_identity;
		Type::Interface::modes _mode = Type::Interface::MODE_NONE;
		uint32_t _bitrate = 0;
		uint16_t _HW_MTU = 0;
		bool _AUTOCONFIGURE_MTU = false;
		bool _FIXED_MTU = false;
		double _announce_allowed_at = 0;
		float _announce_cap = 0.0;
		std::list<AnnounceEntry> _announce_queue;
		bool _is_connected_to_shared_instance = false;
		bool _is_local_shared_instance = false;
		//Bytes _hash;
		HInterface _parent_interface;
		//Transport& _owner;

	friend class Interface;
	};

	class Interface {

	public:
		// Which interface modes a Transport Node
		// should actively discover paths for.
		static uint8_t DISCOVER_PATHS_FOR;

	public:
		Interface(Type::NoneConstructor none) {
			MEMF("Interface NONE object created, this: 0x%X, impl: 0x%X", this, _impl.get());
		}
		Interface(const Interface& obj) : _impl(obj._impl) {
			MEMF("Interface object copy created, this: 0x%X, impl: 0x%X", this, _impl.get());
		}
		Interface(std::shared_ptr<InterfaceImpl>& impl) : _impl(impl) {
			MEMF("Interface object created with shared impl, this: 0x%X, impl: 0x%X", this, _impl.get());
		}
		Interface(InterfaceImpl* impl) : _impl(impl) {
			MEMF("Interface object created with new impl, this: 0x%X, impl: 0x%X", this, _impl.get());
		}
		virtual ~Interface() {
			MEMF("Interface object destroyed, this: 0x%X, impl: 0x%X", this, _impl.get());
		}

		inline Interface& operator = (const Interface& obj) {
			_impl = obj._impl;
			MEMF("Interface object copy created by assignment, this: 0x%X, impl: 0x%X", this, _impl.get());
			return *this;
		}
		inline Interface& operator = (InterfaceImpl* impl) {
			_impl.reset(impl);
			MEMF("Interface object copy created by assignment, this: 0x%X, impl: 0x%X", this, _impl.get());
			return *this;
		}
		inline operator bool() const {
			MEMF("Interface object bool, this: 0x%X, impl: 0x%X", this, _impl.get());
			return _impl.get() != nullptr;
		}
		inline bool operator < (const Interface& obj) const {
			MEMF("Interface object <, this: 0x%X, impl: 0x%X", this, _impl.get());
			return _impl.get() < obj._impl.get();
		}
		inline bool operator > (const Interface& obj) const {
			MEMF("Interface object <, this: 0x%X, impl: 0x%X", this, _impl.get());
			return _impl.get() > obj._impl.get();
		}
		inline bool operator == (const Interface& obj) const {
			MEMF("Interface object ==, this: 0x%X, impl: 0x%X", this, _impl.get());
			return _impl.get() == obj._impl.get();
		}
		inline bool operator != (const Interface& obj) const {
			MEMF("Interface object !=, this: 0x%X, impl: 0x%X", this, _impl.get());
			return _impl.get() != obj._impl.get();
		}
		inline InterfaceImpl* get() {
			return _impl.get();
		}
		inline void clear() {
			_impl.reset();
		}

	public:
		inline bool start() { assert(_impl); return _impl->start(); }
		inline void stop() { assert(_impl); return _impl->stop(); }
		inline void loop() { assert(_impl); return _impl->loop(); }
		inline const Bytes get_hash() const { assert(_impl); return _impl->get_hash(); }
		void process_announce_queue();

		// CBA ACCUMULATES
		inline void add_announce(AnnounceEntry& entry) { assert(_impl); _impl->_announce_queue.push_back(entry); }

	protected:
		inline void send_outgoing(const Bytes& data) { assert(_impl); _impl->send_outgoing(data); }
	public:
		//inline void handle_incoming(const Bytes& data) { assert(_impl); _impl->handle_incoming(data); }
		// Public method to handle data coming in on interface and pass on to impl
		void handle_incoming(const Bytes& data);

	protected:
		// setters
		inline void IN(bool IN) { assert(_impl); _impl->_IN = IN; }
		inline void OUT(bool OUT) { assert(_impl); _impl->_OUT = OUT; }
		inline void FWD(bool FWD) { assert(_impl); _impl->_FWD = FWD; }
		inline void RPT(bool RPT) { assert(_impl); _impl->_RPT = RPT; }
		inline void name(const char* name) { assert(_impl); _impl->_name = name; }
		inline void bitrate(uint32_t bitrate) { assert(_impl); _impl->_bitrate = bitrate; }
		inline void online(bool online) { assert(_impl); _impl->_online = online; }
		inline void announce_allowed_at(double announce_allowed_at) { assert(_impl); _impl->_announce_allowed_at = announce_allowed_at; }
	public:
		// getters
		inline bool IN() const { assert(_impl); return _impl->_IN; }
		inline bool OUT() const { assert(_impl); return _impl->_OUT; }
		inline bool FWD() const { assert(_impl); return _impl->_FWD; }
		inline bool RPT() const { assert(_impl); return _impl->_RPT; }
		inline bool online() const { assert(_impl); return _impl->_online; }
		inline std::string name() const { assert(_impl); return _impl->_name; }
		inline const Bytes& ifac_identity() const { assert(_impl); return _impl->_ifac_identity; }
		inline Type::Interface::modes mode() const { assert(_impl); return _impl->_mode; }
		inline void mode(Type::Interface::modes mode) { assert(_impl); _impl->_mode = mode; }
		inline uint32_t bitrate() const { assert(_impl); return _impl->_bitrate; }
		inline uint16_t HW_MTU() const { assert(_impl); return _impl->_HW_MTU; }
		inline bool AUTOCONFIGURE_MTU() const { assert(_impl); return _impl->_AUTOCONFIGURE_MTU; }
		inline bool FIXED_MTU() const { assert(_impl); return _impl->_FIXED_MTU; }
		inline double announce_allowed_at() const { assert(_impl); return _impl->_announce_allowed_at; }
		inline float announce_cap() const { assert(_impl); return _impl->_announce_cap; }
		inline std::list<AnnounceEntry>& announce_queue() const { assert(_impl); return _impl->_announce_queue; }
		inline bool is_connected_to_shared_instance() const { assert(_impl); return _impl->_is_connected_to_shared_instance; }
		inline bool is_local_shared_instance() const { assert(_impl); return _impl->_is_local_shared_instance; }
		inline HInterface parent_interface() const { assert(_impl); return _impl->_parent_interface; }

		virtual inline std::string toString() const { if (!_impl) return ""; return _impl->toString(); }

#ifndef NDEBUG
		inline std::string debugString() const {
			std::string dump;
			dump = "Interface object, this: " + std::to_string((uintptr_t)this) + ", data: " + std::to_string((uintptr_t)_impl.get());
			return dump;
		}
#endif

	protected:
		std::shared_ptr<InterfaceImpl> _impl;

	friend class Transport;
	};

}

/*
namespace ArduinoJson {
	inline bool convertToJson(const RNS::Interface& src, JsonVariant dst) {
		TRACE("<<< Serializing Interface");
		if (!src) {
			return dst.set(nullptr);
		}
		TRACE("<<< Interface hash " + src.get_hash().toHex());
		return dst.set(src.get_hash().toHex());
	}
	void convertFromJson(JsonVariantConst src, RNS::Interface& dst);
	inline bool canConvertFromJson(JsonVariantConst src, const RNS::Interface&) {
		return src.is<const char*>() && strlen(src.as<const char*>()) == 64;
	}
}
*/
/*
namespace ArduinoJson {
	template <>
	struct Converter<RNS::Interface> {
		static bool toJson(const RNS::Interface& src, JsonVariant dst) {
			if (!src) {
				return dst.set(nullptr);
			}
			TRACE("<<< Serializing interface hash " + src.get_hash().toHex());
			return dst.set(src.get_hash().toHex());
		}
		static RNS::Interface fromJson(JsonVariantConst src) {
			if (!src.isNull()) {
				RNS::Bytes hash;
				hash.assignHex(src.as<const char*>());
				TRACE(">>> Deserialized interface hash " + hash.toHex());
				TRACE(">>> Querying transport for interface");
				// Query transport for matching interface
				return RNS::Interface::find_interface_from_hash(hash);
			}
			else {
				return {RNS::Type::NONE};
			}
		}
		static bool checkJson(JsonVariantConst src) {
			return src.is<const char*>() && strlen(src.as<const char*>()) == 64;
		}
	};
}
*/