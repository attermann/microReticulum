/*
 * Copyright (c) 2023 Chad Attermann
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */

#include "Interface.h"

#include "Identity.h"
#include "Transport.h"
#include "Utilities/OS.h"

using namespace RNS;
using namespace RNS::Type::Interface;
using namespace RNS::Utilities;

/*static*/ uint8_t Interface::DISCOVER_PATHS_FOR = MODE_ACCESS_POINT | MODE_GATEWAY | MODE_ROAMING;

void InterfaceImpl::handle_outgoing(const Bytes& data) {
	//TRACEF("InterfaceImpl.handle_outgoing: data: %s", data.toHex().c_str());
	//TRACE("InterfaceImpl.handle_outgoing");
	_tx += 1;
	_txbytes += data.size();
}

void InterfaceImpl::handle_incoming(const Bytes& data) {
	//TRACEF("InterfaceImpl.handle_incoming: data: %s", data.toHex().c_str());
	//TRACE("InterfaceImpl.handle_incoming");
	_rx += 1;
	_rxbytes += data.size();
	// Create temporary Interface encapsulating our own shared impl
	std::shared_ptr<InterfaceImpl> self = shared_from_this();
	Interface interface(self);
	// Pass data on to transport for handling
	Transport::inbound(data, interface);
}

bool Interface::send_outgoing(const Bytes& data) {
	assert(_impl);
	//TRACEF("Interface.send_outgoing: data: %s", data.toHex().c_str());
	//TRACE("Interface.send_outgoing");
	// Catch exceptions from calls into Interface implementation
	try {
		return _impl->send_outgoing(data);
    }
    catch (const std::bad_alloc&) {
		ERROR("Interface::send_outgoing: bad_alloc - OUT OF MEMORY");
		// Critical OOM, restarting
#if defined(ESP32)
		ESP.restart();
#elif defined(ARDUINO_ARCH_NRF52) || defined(ARDUINO_NRF52_ADAFRUIT)
		NVIC_SystemReset();
#endif
    }
    catch (const std::exception& e) {
		ERRORF("Interface::send_outgoing: %s", e.what());
    }
	return false;
}

void Interface::handle_incoming(const Bytes& data) {
	assert(_impl);
	//TRACEF("Interface.handle_incoming: data: %s", data.toHex().c_str());
	//TRACE("Interface.handle_incoming");
	// Catch exceptions from calls into Interface implementation
	try {
		_impl->handle_incoming(data);
    }
    catch (const std::bad_alloc&) {
		ERROR("Interface::handle_incoming: bad_alloc - OUT OF MEMORY");
		// Critical OOM, restarting
#if defined(ESP32)
		ESP.restart();
#elif defined(ARDUINO_ARCH_NRF52) || defined(ARDUINO_NRF52_ADAFRUIT)
		NVIC_SystemReset();
#endif
    }
    catch (const std::exception& e) {
		ERRORF("Interface::handle_incoming: %s", e.what());
    }
}

// Send an announce that Transport::outbound had to queue, as often as the
// interface's announce cap allows one out.
//
// RNS arms a timer for the cap's wait and sends the next announce when it
// fires. There are no timers here, so Transport::jobs calls this on every
// interface and the cap is kept by _announce_allowed_at instead: the interval
// between calls decides only how promptly a queue drains, never how fast
// announces leave. One per call is what the timer does per firing, and is far
// more than the arrival rate a queue forms from.
//
// While this had no body nothing ever drained the queue, and outbound()
// refuses to transmit an announce onto an interface that has any queued: the
// first announce an interface ever queued stopped it forwarding announces for
// the rest of the uptime, silently and with no way back short of a reboot.
void Interface::process_announce_queue() {
	assert(_impl);
	if (_impl->_announce_queue.empty()) return;

	try {
		double now = OS::time();

		// An announce that has been waiting this long is not worth the airtime
		// any more: whoever sent it has almost certainly announced again since.
		size_t held = _impl->_announce_queue.size();
		_impl->_announce_queue.remove_if([now](const AnnounceEntry& entry) {
			return now > entry._time + Type::Reticulum::QUEUED_ANNOUNCE_LIFE;
		});
		if (_impl->_announce_queue.size() < held) {
			DEBUGF("Dropped %u stale queued announce(s) on %s",
				(unsigned)(held - _impl->_announce_queue.size()), toString().c_str());
		}
		if (_impl->_announce_queue.empty() || now < _impl->_announce_allowed_at) return;

		// Fewest hops first, and among those the one that has waited longest,
		// so a queue draining slowly still carries the nearest path onwards
		// first and nothing in it can be starved by later arrivals.
		auto selected = _impl->_announce_queue.begin();
		for (auto entry = _impl->_announce_queue.begin(); entry != _impl->_announce_queue.end(); ++entry) {
			if (entry->_hops < selected->_hops ||
				(entry->_hops == selected->_hops && entry->_time < selected->_time)) {
				selected = entry;
			}
		}

		double wait_time = announce_wait_time(selected->_raw.size());
		_impl->_announce_allowed_at = now + wait_time;

		Bytes raw(selected->_raw);
		_impl->_announce_queue.erase(selected);
		DEBUGF("Sending queued announce on %s, %u still queued, next allowed in %.1f s",
			toString().c_str(), (unsigned)_impl->_announce_queue.size(), wait_time);

		if (Transport::transmit(*this, raw)) {
			sent_announce();
		}
	}
	catch (const std::exception& e) {
		_impl->_announce_queue.clear();
		ERRORF("Error while processing the announce queue on %s. The contained exception was: %s",
			toString().c_str(), e.what());
		ERROR("The announce queue for this interface has been cleared.");
	}
}

/*
void ArduinoJson::convertFromJson(JsonVariantConst src, RNS::Interface& dst) {
	TRACE(">>> Deserializing Interface");
TRACEF(">>> Interface pre: %s", dst.debugString().c_str());
	if (!src.isNull()) {
		RNS::Bytes hash;
		hash.assignHex(src.as<const char*>());
		TRACEF(">>> Querying Transport for Interface hash %s", hash.toHex().c_str());
		// Query transport for matching interface
		dst = Transport::find_interface_from_hash(hash);
TRACEF(">>> Interface post: %s", dst.debugString().c_str());
	}
	else {
		dst = {RNS::Type::NONE};
TRACEF(">>> Interface post: %s", dst.debugString().c_str());
	}
}
*/
