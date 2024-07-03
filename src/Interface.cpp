#include "Interface.h"

#include "Identity.h"
#include "Transport.h"

using namespace RNS;
using namespace RNS::Type::Interface;

/*static*/ uint8_t Interface::DISCOVER_PATHS_FOR = MODE_ACCESS_POINT | MODE_GATEWAY;

Interface::Interface() : _object(new Object(this)), _creator(true) {
	// CBA MCU
	//_object->_hash = Identity::full_hash({toString()});
	_object->_hash = Identity::truncated_hash({toString()});
	MEM("Interface object created, this: " + std::to_string((uintptr_t)this) + ", data: " + std::to_string((uintptr_t)_object.get()));
}

Interface::Interface(const char* name) : _object(new Object(this, name)), _creator(true) {
	// CBA MCU
	//_object->_hash = Identity::full_hash({toString()});
	_object->_hash = Identity::truncated_hash({toString()});
	MEM("Interface object created, this: " + std::to_string((uintptr_t)this) + ", data: " + std::to_string((uintptr_t)_object.get()));
}

/*virtual*/ inline void Interface::on_incoming(const Bytes& data) {
	//TRACE("Interface.on_incoming: data: " + data.toHex());
	TRACE("Interface.on_incoming");
	assert(_object);
	_object->_rxb += data.size();
	// CBA TODO implement concept of owner or a callback mechanism for incoming data
	//_object->_owner.inbound(data, *this);
	Transport::inbound(data, *this);
}

/*virtual*/ inline void Interface::on_outgoing(const Bytes& data) {
	//TRACE("Interface.on_outgoing: data: " + data.toHex());
	TRACE("Interface.on_outgoing");
	assert(_object);
	_object->_txb += data.size();
}

const Bytes Interface::get_hash() const {
	assert(_object);
	// CBA MCU
	////return Identity::full_hash({toString()});
	//return Identity::truncated_hash({toString()});
	return _object->_hash;
}

void Interface::process_announce_queue() {
/*
	if not hasattr(self, "announce_cap"):
		self.announce_cap = RNS.Reticulum.ANNOUNCE_CAP

	if hasattr(self, "announce_queue"):
		try:
			now = time.time()
			stale = []
			for a in self.announce_queue:
				if now > a["time"]+RNS.Reticulum.QUEUED_ANNOUNCE_LIFE:
					stale.append(a)

			for s in stale:
				if s in self.announce_queue:
					self.announce_queue.remove(s)

			if len(self.announce_queue) > 0:
				min_hops = min(entry["hops"] for entry in self.announce_queue)
				entries = list(filter(lambda e: e["hops"] == min_hops, self.announce_queue))
				entries.sort(key=lambda e: e["time"])
				selected = entries[0]

				double now = OS::time();
				uint32_t wait_time = 0;
				if (_object->_bitrate > 0 && _object->_announce_cap > 0) {
					uint32_t tx_time = (len(selected["raw"])*8) / _object->_bitrate;
					wait_time = (tx_time / _object->_announce_cap);
				}
				_object->_announce_allowed_at = now + wait_time;

				self.on_outgoing(selected["raw"])

				if selected in self.announce_queue:
					self.announce_queue.remove(selected)

				if len(self.announce_queue) > 0:
					timer = threading.Timer(wait_time, self.process_announce_queue)
					timer.start()

		except Exception as e:
			self.announce_queue = []
			RNS.log("Error while processing announce queue on "+str(self)+". The contained exception was: "+str(e), RNS.LOG_ERROR)
			RNS.log("The announce queue for this interface has been cleared.", RNS.LOG_ERROR)
*/
}

/*
void ArduinoJson::convertFromJson(JsonVariantConst src, RNS::Interface& dst) {
	TRACE(">>> Deserializing Interface");
TRACE(">>> Interface pre: " + dst.debugString());
	if (!src.isNull()) {
		RNS::Bytes hash;
		hash.assignHex(src.as<const char*>());
		TRACE(">>> Querying Transport for Interface hash " + hash.toHex());
		// Query transport for matching interface
		dst = Transport::find_interface_from_hash(hash);
TRACE(">>> Interface post: " + dst.debugString());
	}
	else {
		dst = {RNS::Type::NONE};
TRACE(">>> Interface post: " + dst.debugString());
	}
}
*/
