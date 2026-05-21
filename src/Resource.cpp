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

#include "Resource.h"

#include "ResourceData.h"
#include "Reticulum.h"
#include "Transport.h"
#include "Packet.h"
#include "Identity.h"
#include "Link.h"
#include "Log.h"

#include <MsgPack.h>

#include <algorithm>

using namespace RNS;
using namespace RNS::Utilities;


// ============================================================================
// Static creation entry points
// ============================================================================

/*
Reject a resource advertisement by sending a RESOURCE_RCL packet back to the
initiator carrying the advertised resource hash. Mirror Python Resource.py:155.
*/
void Resource::reject(const Packet& advertisement_packet) {
	try {
		ResourceAdvertisement adv = ResourceAdvertisement::unpack(advertisement_packet.plaintext());
		Bytes resource_hash = adv._h;
		Packet reject_packet(advertisement_packet.link(), resource_hash, Type::Packet::DATA, Type::Packet::RESOURCE_RCL);
		reject_packet.send();
	}
	catch (const std::exception& e) {
		ERRORF("An error occurred while rejecting advertised resource: %s", e.what());
	}
}

/*
Accept an incoming resource advertisement, instantiate a receiver-side
Resource and register it with the link. Mirror Python Resource.py:167.

Returns a Resource (or NONE on rejection / duplicate).
*/
Resource Resource::accept(const Packet& advertisement_packet, Callbacks::concluded callback /*= nullptr*/, Callbacks::progress progress_callback /*= nullptr*/, const Bytes& request_id /*= {Bytes::NONE}*/) {
	ResourceAdvertisement adv = ResourceAdvertisement::unpack(advertisement_packet.plaintext());
	if (!adv._h) {
		DEBUG("Could not decode resource advertisement, dropping resource");
		return {Type::NONE};
	}

	// Allocate a receiver-side Resource via the main constructor with
	// data == empty (triggers the receiver branch of __init__).
	Link link = advertisement_packet.link();
	Resource resource({Bytes::NONE}, link, /*advertise=*/false, /*auto_compress=*/false,
	                  callback, progress_callback,
	                  /*timeout=*/0.0, /*segment_index=*/1, /*original_hash=*/adv._o,
	                  request_id);
	assert(resource._object);

	resource._object->_status            = Type::Resource::TRANSFERRING;
	resource._object->_flags             = adv._f;
	resource._object->_size              = adv._t;
	resource._object->_total_size        = adv._d;
	resource._object->_uncompressed_size = adv._d;
	resource._object->_hash              = adv._h;
	resource._object->_original_hash     = adv._o;
	resource._object->_random_hash       = adv._r;
	resource._object->_hashmap_raw       = adv._m;
	resource._object->_encrypted         = adv._e;
	resource._object->_compressed        = adv._c;
	resource._object->_initiator         = false;
	resource._object->_callbacks._concluded = callback;
	resource._object->_callbacks._progress  = progress_callback;
	resource._object->_total_parts       = static_cast<uint32_t>((adv._t + resource._object->_sdu - 1) / resource._object->_sdu);
	resource._object->_received_count    = 0;
	resource._object->_outstanding_parts = 0;
	resource._object->_window            = Type::Resource::WINDOW;
	resource._object->_window_max        = Type::Resource::WINDOW_MAX_SLOW;
	resource._object->_window_min        = Type::Resource::WINDOW_MIN;
	resource._object->_window_flexibility = Type::Resource::WINDOW_FLEXIBILITY;
	resource._object->_last_activity     = Utilities::OS::time();
	resource._object->_started_transferring = resource._object->_last_activity;
	resource._object->_advertisement_packet = advertisement_packet;
	resource._object->_segment_index     = adv._i;
	resource._object->_total_segments    = adv._l;
	resource._object->_split             = (adv._l > 1);
	resource._object->_has_metadata      = adv._x;
	resource._object->_consecutive_completed_height = -1;
	resource._object->_waiting_for_hmu   = false;
	resource._object->_receiving_part    = false;
	resource._object->_request_id        = request_id;
	// Mirror Python: the advertisement carries the request/response flags;
	// these are used by Link::resource_concluded() to dispatch on completion.
	resource._object->_is_response       = adv._p;

	// Pre-allocate the parts buffer with empty slots; receive_part() fills
	// each slot as the corresponding packet arrives.
	resource._object->_parts.clear();
	resource._object->_parts.reserve(resource._object->_total_parts);
	for (uint32_t i = 0; i < resource._object->_total_parts; ++i) {
		resource._object->_parts.push_back({Type::NONE});
	}
	// Hashmap holds N x MAPHASH_LEN bytes; pre-zero so missing entries are
	// detectable by comparing to a zero-block.
	resource._object->_hashmap = Bytes();
	for (size_t i = 0; i < static_cast<size_t>(resource._object->_total_parts) * Type::Resource::MAPHASH_LEN; ++i) {
		resource._object->_hashmap.append(uint8_t(0));
	}
	resource._object->_hashmap_height = 0;

	if (link.has_incoming_resource(resource)) {
		DEBUGF("Ignoring resource advertisement for %s, resource already transferring", resource._object->_hash.toHex().c_str());
		return {Type::NONE};
	}

	link.register_incoming_resource(resource);
	DEBUGF("Accepting resource advertisement for %s. Transfer size is %lu in %u parts.",
	       resource._object->_hash.toHex().c_str(),
	       static_cast<unsigned long>(resource._object->_size),
	       resource._object->_total_parts);

	if (link.callbacks()._resource_started != nullptr) {
		try {
			link.callbacks()._resource_started(resource);
		}
		catch (const std::exception& e) {
			ERRORF("Error while executing resource started callback from %s. The contained exception was: %s",
			       resource.toString().c_str(), e.what());
		}
	}

	resource.hashmap_update(0, resource._object->_hashmap_raw);
	resource.watchdog_job();
	return resource;
}


// ============================================================================
// Constructors
// ============================================================================

// Compact form used by Link::request() at Link.cpp:601 for the
// "request as resource" pattern. Delegates to the main constructor.
Resource::Resource(const Bytes& data, const Link& link, const Bytes& request_id, bool is_response, double timeout) :
	Resource(data, link, /*advertise=*/true, /*auto_compress=*/true, /*callback=*/nullptr,
	         /*progress_callback=*/nullptr, timeout, /*segment_index=*/1,
	         /*original_hash=*/{Bytes::NONE}, request_id, is_response, /*sent_metadata_size=*/0)
{
}

Resource::Resource(const Bytes& data, const Link& link, bool advertise /*= true*/, bool auto_compress /*= true*/, Callbacks::concluded callback /*= nullptr*/, Callbacks::progress progress_callback /*= nullptr*/, double timeout /*= 0.0*/, uint16_t segment_index /*= 1*/, const Bytes& original_hash /*= {Bytes::NONE}*/, const Bytes& request_id /*= {Bytes::NONE}*/, bool is_response /*= false*/, size_t sent_metadata_size /*= 0*/) :
	_object(new ResourceData(link))
{
	assert(_object);
	MEM("Resource object created");

	// Metadata handling — auto-compress and metadata serialisation are
	// not yet supported on the C++ port (see plan: "no bz2 on MCU";
	// metadata support deferred). Always emit uncompressed, no metadata.
	_object->_metadata        = {Bytes::NONE};
	_object->_has_metadata    = (sent_metadata_size > 0);
	_object->_metadata_size   = sent_metadata_size;

	_object->_status               = Type::Resource::NONE;
	_object->_link                 = link;
	_object->_callbacks._concluded = callback;
	_object->_callbacks._progress  = progress_callback;
	_object->_request_id           = request_id;
	_object->_is_response          = is_response;

	// SDU: mirror Python Resource.__init__ (Resource.py:337-340)
	// Use the non-const Link stored in _object->_link since Link::get_mtu/
	// get_mdu are not const-qualified (they assert _object via pimpl).
	if (_object->_link.get_mtu()) {
		_object->_sdu = static_cast<uint16_t>(_object->_link.get_mtu() - Type::Reticulum::HEADER_MAXSIZE - Type::Reticulum::IFAC_MIN_SIZE);
	}
	else if (_object->_link.get_mdu()) {
		_object->_sdu = _object->_link.get_mdu();
	}
	else {
		_object->_sdu = Type::Resource::SDU;
	}

	_object->_max_retries        = Type::Resource::MAX_RETRIES;
	_object->_max_adv_retries    = Type::Resource::MAX_ADV_RETRIES;
	_object->_retries_left       = _object->_max_retries;
	_object->_timeout_factor     = link.traffic_timeout_factor();
	_object->_part_timeout_factor = Type::Resource::PART_TIMEOUT_FACTOR;
	_object->_sender_grace_time  = Type::Resource::SENDER_GRACE_TIME;
	_object->_auto_compress_option = auto_compress;
	_object->_auto_compress      = false;  // bz2 not available on MCU; see plan
	_object->_auto_compress_limit = Type::Resource::AUTO_COMPRESS_MAX_SIZE;
	_object->_max_decompressed_size = Type::Resource::AUTO_COMPRESS_MAX_SIZE;
	_object->_receiver_min_consecutive_height = 0;

	_object->_timeout = (timeout != 0.0) ? timeout
	                                     : link.rtt() * link.traffic_timeout_factor();

	// Always single-segment, no compression, no metadata on the C++ port.
	_object->_total_segments = 1;
	_object->_segment_index  = 1;
	_object->_split          = false;

	if (data && data.size() > 0) {
		// ====================================================================
		// Initiator branch
		// ====================================================================
		_object->_initiator = true;

		const size_t data_size = data.size();
		_object->_uncompressed_size = data_size;
		_object->_total_size        = data_size + _object->_metadata_size;

		// Build the payload: prefix_random_hash || data. (No bz2 compression.)
		// Mirror Python Resource.py:411-413 — the prefix is a random hash
		// included in the encrypted stream so identical user payloads still
		// produce different ciphertexts.
		_object->_random_hash = Identity::get_random_hash().left(Type::Resource::RANDOM_HASH_SIZE);

		Bytes payload;
		payload.append(_object->_random_hash);
		payload.append(data);
		_object->_uncompressed_data = payload;

		_object->_compressed       = false;
		_object->_compressed_data  = {Bytes::NONE};

		// The hash and expected_proof are computed over (user data || random_hash),
		// NOT over the prefixed payload — see Python Resource.py:441-443.
		// The receiver's assemble() decrypts, strips the leading random_hash,
		// then computes full_hash(self.data + self.random_hash) which is the
		// user payload followed by the advertised random_hash. We must match
		// that exactly for byte-level interop.
		Bytes hash_input = data + _object->_random_hash;
		_object->_hash            = Identity::full_hash(hash_input);
		_object->_truncated_hash  = Identity::truncated_hash(hash_input);
		_object->_expected_proof  = Identity::full_hash(data + _object->_hash);
		_object->_original_hash   = (original_hash.size() > 0) ? original_hash : _object->_hash;

		// Resources encrypt their payload themselves so each chunk sits in
		// the cipher stream contiguously (Python Resource.py:423-428).
		_object->_data       = _object->_link.encrypt(payload);
		_object->_encrypted  = true;
		_object->_size       = _object->_data.size();

		_object->_uncompressed_data = {Bytes::NONE};

		// Slice the encrypted stream into SDU-sized parts and compute the
		// hashmap. Mirror Python Resource.py:432-473 — minus the collision
		// guard, which is not strictly necessary for byte-exact interop
		// (random_hash already provides per-resource entropy).
		const uint32_t hashmap_entries = static_cast<uint32_t>((_object->_size + _object->_sdu - 1) / _object->_sdu);
		_object->_total_parts = hashmap_entries;
		_object->_sent_parts  = 0;

		_object->_parts.clear();
		_object->_parts.reserve(hashmap_entries);
		_object->_hashmap = {Bytes::NONE};

		for (uint32_t i = 0; i < hashmap_entries; ++i) {
			const size_t offset = static_cast<size_t>(i) * _object->_sdu;
			size_t chunk_size = _object->_sdu;
			if (offset + chunk_size > _object->_data.size()) {
				chunk_size = _object->_data.size() - offset;
			}
			Bytes chunk = _object->_data.mid(offset, chunk_size);

			Packet part(link, chunk, Type::Packet::DATA, Type::Packet::RESOURCE);
			part.pack();

			Bytes map_hash = get_map_hash(chunk);
			_object->_hashmap.append(map_hash);
			_object->_parts.push_back(part);
		}

		// Drop the buffered stream now that parts and hashmap are computed.
		_object->_data = {Bytes::NONE};

		if (advertise) {
			this->advertise();
		}
	}
	else {
		// ====================================================================
		// Receiver branch — TODO 1.4: full receiver initialisation lives in
		// Resource::accept(). This constructor is the no-op shell used by
		// accept() to allocate the underlying ResourceData.
		// ====================================================================
		_object->_initiator = false;
	}
}


// ============================================================================
// Hashmap handling
// ============================================================================

/*
Process an incoming RESOURCE_HMU packet carrying the next slice of the
hashmap. The packet layout is hash || msgpack([segment, hashmap]). Mirror
Python Resource.py:483.
*/
void Resource::hashmap_update_packet(const Bytes& plaintext) {
	assert(_object);
	if (_object->_status == Type::Resource::FAILED) return;

	_object->_last_activity = Utilities::OS::time();
	_object->_retries_left  = _object->_max_retries;

	const size_t hash_bytes = Type::Identity::HASHLENGTH / 8;
	if (plaintext.size() <= hash_bytes) return;

	Bytes packed = plaintext.mid(hash_bytes);
	MsgPack::Unpacker u;
	u.feed(packed.data(), packed.size());
	if (!u.isArray()) return;
	const size_t n = u.unpackArraySize();
	if (n < 2) return;

	uint32_t segment = 0;
	u.deserialize(segment);

	MsgPack::bin_t<uint8_t> hashmap_bin;
	u.deserialize(hashmap_bin);
	Bytes hashmap(hashmap_bin.data(), hashmap_bin.size());

	hashmap_update(static_cast<uint16_t>(segment), hashmap);
}

/*
Apply a slice of hashmap entries to the local hashmap, advancing the
hashmap_height by the number of newly-known entries. Mirror Python
Resource.py:492.
*/
void Resource::hashmap_update(uint16_t segment, const Bytes& hashmap) {
	assert(_object);
	if (_object->_status == Type::Resource::FAILED) return;
	_object->_status = Type::Resource::TRANSFERRING;

	const size_t seg_len       = Type::Resource::ResourceAdvertisement::HASHMAP_MAX_LEN;
	const size_t maphash_len   = Type::Resource::MAPHASH_LEN;
	const size_t hashes        = hashmap.size() / maphash_len;
	const size_t base          = static_cast<size_t>(segment) * seg_len;

	// Zero block used to detect "entry not yet populated".
	Bytes zero;
	for (size_t k = 0; k < maphash_len; ++k) zero.append(uint8_t(0));

	for (size_t i = 0; i < hashes; ++i) {
		const size_t slot = base + i;
		if (slot >= _object->_total_parts) break;

		Bytes current = _object->_hashmap.mid(slot * maphash_len, maphash_len);
		if (current == zero) {
			_object->_hashmap_height++;
		}
		// Overwrite the slot's bytes in-place. Bytes lacks an in-place
		// replace, so reconstruct around the slot.
		Bytes head = _object->_hashmap.left(slot * maphash_len);
		Bytes tail = _object->_hashmap.mid((slot + 1) * maphash_len);
		Bytes new_hash = hashmap.mid(i * maphash_len, maphash_len);
		_object->_hashmap = head + new_hash + tail;
	}

	_object->_waiting_for_hmu = false;
	request_next();
}

Bytes Resource::get_map_hash(const Bytes& data) {
	// Python Resource.py:505 — full_hash(data + random_hash)[:MAPHASH_LEN]
	assert(_object);
	return Identity::full_hash(data + _object->_random_hash).left(Type::Resource::MAPHASH_LEN);
}


// ============================================================================
// Advertise / advertise job
// ============================================================================

/*
Advertise the resource. If the other end of the link accepts the resource
advertisement it will begin transferring.
*/
void Resource::advertise() {
	assert(_object);
	// Python uses a background thread; the C++ port runs the job inline.
	// Cooperative ticking of the subsequent watchdog is driven from
	// Reticulum::loop() once the watchdog_job pump is wired up (sub-phase 1.5+).
	advertise_job();
}

void Resource::advertise_job() {
	assert(_object);

	// Build the advertisement packet. Mirror Python Resource.py:521.
	ResourceAdvertisement adv(*this, _object->_request_id, _object->_is_response);
	_object->_advertisement_packet = Packet(_object->_link, adv.pack(), Type::Packet::DATA, Type::Packet::RESOURCE_ADV);

	// Python loops until the link is ready (Resource.py:522-524). On the
	// cooperative C++ runtime we cannot busy-wait; if the link is not ready
	// we mark the resource QUEUED and let the next watchdog tick retry.
	if (!_object->_link.ready_for_new_resource()) {
		_object->_status = Type::Resource::QUEUED;
		return;
	}

	try {
		_object->_advertisement_packet.send();
		_object->_last_activity        = Utilities::OS::time();
		_object->_started_transferring = _object->_last_activity;
		_object->_adv_sent             = _object->_last_activity;
		_object->_rtt                  = 0.0;
		_object->_status               = Type::Resource::ADVERTISED;
		_object->_retries_left         = _object->_max_adv_retries;
		_object->_link.register_outgoing_resource(*this);
		TRACEF("Sent resource advertisement for %s", _object->_hash.toHex().c_str());
	}
	catch (const std::exception& e) {
		ERRORF("Could not advertise resource, the contained exception was: %s", e.what());
		cancel();
		return;
	}

	watchdog_job();
}


// ============================================================================
// EIFR (effective inbound flow rate) tracking
// ============================================================================

/*
Recomputes the link's expected in-flight data rate. Mirror Python Resource.py:543.
*/
void Resource::update_eifr() {
	assert(_object);

	const double rtt = (_object->_rtt == 0.0) ? _object->_link.rtt() : _object->_rtt;
	if (rtt <= 0.0) return;

	double expected_inflight_rate;
	if (_object->_req_data_rtt_rate != 0.0) {
		expected_inflight_rate = _object->_req_data_rtt_rate * 8.0;
	}
	else if (_object->_previous_eifr != 0.0) {
		expected_inflight_rate = _object->_previous_eifr;
	}
	else {
		expected_inflight_rate = static_cast<double>(_object->_link.establishment_cost()) * 8.0 / rtt;
	}

	_object->_eifr = expected_inflight_rate;
	// CBA: Link::expected_rate setter is exposed for future wiring; in the
	// current API only get_expected_rate() is available. The EIFR value
	// remains useful locally for the initiator's watchdog calculations.
}


// ============================================================================
// Watchdog
// ============================================================================

/*
Schedule the watchdog tick. In Python this spawns a thread; in the C++ port
ticking is cooperative (driven from Reticulum::loop() once wired up).
Mirror Python Resource.py:560.
*/
void Resource::watchdog_job() {
	assert(_object);
	_object->_watchdog_job_id++;
	// Cooperative ticking arrives via Resource::__watchdog_job() called from
	// the main loop; nothing more to do here on the C++ side.
}

/*
Single state-machine tick for an in-flight resource. Initiator-side branches
implemented in sub-phase 1.3; receiver-side branches added in sub-phase 1.4.
Mirror Python Resource.py:564.
*/
void Resource::__watchdog_job() {
	assert(_object);
	if (_object->_status >= Type::Resource::ASSEMBLING) return;
	if (_object->_watchdog_lock) return;

	const double now = Utilities::OS::time();

	if (_object->_status == Type::Resource::ADVERTISED) {
		const double sleep_time = (_object->_adv_sent + _object->_timeout + Type::Resource::PROCESSING_GRACE) - now;
		if (sleep_time < 0) {
			if (_object->_retries_left <= 0) {
				DEBUG("Resource transfer timeout after sending advertisement");
				cancel();
				return;
			}
			try {
				DEBUG("No part requests received, retrying resource advertisement...");
				_object->_retries_left--;
				ResourceAdvertisement adv(*this, _object->_request_id, _object->_is_response);
				_object->_advertisement_packet = Packet(_object->_link, adv.pack(), Type::Packet::DATA, Type::Packet::RESOURCE_ADV);
				_object->_advertisement_packet.send();
				_object->_last_activity = Utilities::OS::time();
				_object->_adv_sent      = _object->_last_activity;
			}
			catch (const std::exception& e) {
				DEBUGF("Could not resend advertisement packet, cancelling resource. The contained exception was: %s", e.what());
				cancel();
			}
		}
	}
	else if (_object->_status == Type::Resource::TRANSFERRING && _object->_initiator) {
		// Initiator-side transfer timeout — mirror Python Resource.py:630-637.
		// "max_extra_wait" is the sum of retry delays; combined with the
		// link RTT and traffic timeout factor it bounds the longest a
		// healthy resource transfer should take before we give up.
		double max_extra_wait = 0.0;
		for (uint8_t r = 0; r < Type::Resource::MAX_RETRIES; ++r) {
			max_extra_wait += (r + 1) * Type::Resource::PER_RETRY_DELAY;
		}
		const double max_wait = _object->_rtt * _object->_timeout_factor * _object->_max_retries
		                        + _object->_sender_grace_time + max_extra_wait;
		const double sleep_time = _object->_last_activity + max_wait - now;
		if (sleep_time < 0) {
			DEBUG("Resource timed out waiting for part requests");
			cancel();
		}
	}
	else if (_object->_status == Type::Resource::AWAITING_PROOF) {
		_object->_timeout_factor = Type::Resource::PROOF_TIMEOUT_FACTOR;
		const double sleep_time = _object->_last_part_sent
		                          + (_object->_rtt * _object->_timeout_factor + _object->_sender_grace_time)
		                          - now;
		if (sleep_time < 0) {
			if (_object->_retries_left <= 0) {
				DEBUG("Resource timed out waiting for proof");
				cancel();
				return;
			}
			DEBUG("All parts sent, but no resource proof received");
			_object->_retries_left--;
			_object->_last_part_sent = Utilities::OS::time();
			// Python additionally queries the network cache for a cached
			// proof here (Resource.py:651-657). That path uses
			// Transport.cache_request which is not yet plumbed for
			// resource proofs on the C++ port; we just refresh the
			// timer and let further proof packets arrive.
		}
	}
	else if (_object->_status == Type::Resource::TRANSFERRING && !_object->_initiator) {
		// Receiver-side transfer timeout — mirror Python Resource.py:594-629.
		const uint8_t retries_used = _object->_max_retries - _object->_retries_left;
		const double  extra_wait   = retries_used * Type::Resource::PER_RETRY_DELAY;

		update_eifr();
		const double eifr = (_object->_eifr > 0.0) ? _object->_eifr : 1.0;
		const double expected_hmu_wait = (_object->_waiting_for_hmu || _object->_outstanding_parts == 0)
		                                 ? (static_cast<double>(_object->_sdu) * 8.0 * Type::Resource::HMU_WAIT_FACTOR) / eifr
		                                 : 0.0;
		const double expected_tof = (static_cast<double>(_object->_outstanding_parts) * _object->_sdu * 8.0) / eifr;

		double sleep_time;
		if (_object->_req_resp_rtt_rate != 0.0) {
			sleep_time = _object->_last_activity
			           + _object->_part_timeout_factor * expected_tof
			           + expected_hmu_wait
			           + Type::Resource::RETRY_GRACE_TIME
			           + extra_wait
			           - now;
		}
		else {
			sleep_time = _object->_last_activity
			           + _object->_part_timeout_factor * ((3.0 * _object->_sdu) / eifr)
			           + Type::Resource::RETRY_GRACE_TIME
			           + extra_wait
			           - now;
		}

		if (sleep_time < 0) {
			if (_object->_retries_left > 0) {
				DEBUGF("Timed out waiting for %u part(s), requesting retry on %s",
				       _object->_outstanding_parts, toString().c_str());
				if (_object->_window > _object->_window_min) {
					_object->_window--;
					if (_object->_window_max > _object->_window_min) {
						_object->_window_max--;
						if ((_object->_window_max - _object->_window) > (_object->_window_flexibility - 1)) {
							_object->_window_max--;
						}
					}
				}
				_object->_retries_left--;
				_object->_waiting_for_hmu = false;
				request_next();
			}
			else {
				cancel();
			}
		}
	}
}


// ============================================================================
// Assemble & prove (receiver side)
// ============================================================================

/*
Reassemble received parts into the final payload, validate the hash, and fire
the resource_concluded callback. Mirror Python Resource.py:672.

C++-port differences vs. Python:
- No bz2 decompression. If a peer flagged the resource as compressed, mark it
  FAILED rather than attempting to decompress.
- No on-disk staging. The assembled payload sits in _data in RAM; the
  caller-provided callback receives the in-memory Resource (the legacy
  Resource::data() getter exposes the payload).
- No metadata extraction. Metadata is not yet supported on the C++ port —
  the metadata flag bit is preserved on the wire but the payload is treated
  as the entire resource body.
*/
void Resource::assemble() {
	assert(_object);
	if (_object->_status == Type::Resource::FAILED) return;

	try {
		_object->_status = Type::Resource::ASSEMBLING;

		Bytes stream;
		for (auto& part : _object->_parts) {
			if (part) stream.append(part.data());
		}

		Bytes plaintext;
		if (_object->_encrypted) {
			plaintext = _object->_link.decrypt(stream);
		}
		else {
			plaintext = stream;
		}

		if (_object->_compressed) {
			ERRORF("Received resource %s flagged as compressed, but bz2 is not supported on the C++ port. Rejecting.", _object->_hash.toHex().c_str());
			_object->_status = Type::Resource::CORRUPT;
			cancel();
			return;
		}

		// Strip random_hash prefix (Python Resource.py:682).
		Bytes data = plaintext.mid(Type::Resource::RANDOM_HASH_SIZE);

		// Verify hash matches advertised hash.
		Bytes calculated_hash = Identity::full_hash(plaintext);
		if (calculated_hash != _object->_hash) {
			_object->_status = Type::Resource::CORRUPT;
		}
		else {
			// Metadata: not supported — pass the whole body through as data.
			// (Python Resource.py:696-710 extracts metadata + writes to disk.)
			_object->_data   = data;
			_object->_status = Type::Resource::COMPLETE;
			prove();
		}
	}
	catch (const std::exception& e) {
		ERRORF("Error while assembling received resource: %s", e.what());
		_object->_status = Type::Resource::CORRUPT;
	}

	_object->_link.resource_concluded(*this);

	if (_object->_segment_index == _object->_total_segments) {
		if (_object->_callbacks._concluded != nullptr) {
			try {
				_object->_callbacks._concluded(*this);
			}
			catch (const std::exception& e) {
				ERRORF("Error while executing resource assembled callback from %s. The contained exception was: %s",
				       toString().c_str(), e.what());
			}
		}
	}
	else {
		DEBUGF("Resource segment %u of %u received, waiting for next segment to be announced",
		       _object->_segment_index, _object->_total_segments);
	}
}

/*
Send a RESOURCE_PRF proof packet to the initiator confirming successful
receipt. Mirror Python Resource.py:752.
*/
void Resource::prove() {
	assert(_object);
	if (_object->_status == Type::Resource::FAILED) return;

	try {
		Bytes proof = Identity::full_hash(_object->_data + _object->_hash);
		Bytes proof_data;
		proof_data.append(_object->_hash);
		proof_data.append(proof);

		Packet proof_packet(_object->_link, proof_data, Type::Packet::PROOF, Type::Packet::RESOURCE_PRF);
		proof_packet.send();
		Transport::cache_packet(proof_packet, /*force_cache=*/true);
	}
	catch (const std::exception& e) {
		DEBUGF("Could not send proof packet, cancelling resource. The contained exception was: %s", e.what());
		cancel();
	}
}


// ============================================================================
// Segmentation
// ============================================================================

void Resource::prepare_next_segment() {
	// TODO 1.3 — port Python __prepare_next_segment() (Resource.py:765)
}


// ============================================================================
// Proof validation (initiator side)
// ============================================================================

/*
Called on the initiator when a proof packet arrives from the receiver. If the
proof matches the expected proof we transition to COMPLETE and fire the
concluded callback. Mirror Python Resource.py:782.
*/
void Resource::validate_proof(const Bytes& proof_data) {
	assert(_object);
	if (_object->_status == Type::Resource::FAILED) return;

	const size_t hash_bytes = Type::Identity::HASHLENGTH / 8;
	if (proof_data.size() != hash_bytes * 2) return;

	const Bytes proof = proof_data.mid(hash_bytes);
	if (proof != _object->_expected_proof) return;

	_object->_status = Type::Resource::COMPLETE;
	_object->_link.resource_concluded(*this);

	if (_object->_segment_index == _object->_total_segments) {
		// Final segment — signal completion to the application.
		if (_object->_callbacks._concluded != nullptr) {
			try {
				_object->_callbacks._concluded(*this);
			}
			catch (const std::exception& e) {
				ERRORF("Error while executing resource concluded callback from %s. The contained exception was: %s", toString().c_str(), e.what());
			}
		}
	}
	else {
		// Multi-segment transfers (Resource.py:804-821) — not supported in
		// the C++ port for sub-phase 1.3 since segmentation is disabled.
		TRACE("Multi-segment resource validate_proof reached: segmentation not yet supported on C++ port");
	}
}


// ============================================================================
// Receive part / request next (receiver side)
// ============================================================================

/*
Called for each incoming RESOURCE-context packet on the receiver side.
Looks the packet up in the in-flight window via its map_hash, files it into
parts[], and either kicks off assemble() when all parts have arrived or
requests the next window. Mirror Python Resource.py:828.
*/
void Resource::receive_part(const Packet& packet) {
	assert(_object);
	if (_object->_receive_lock) return;
	_object->_receive_lock = true;

	_object->_receiving_part = true;
	_object->_last_activity  = Utilities::OS::time();
	_object->_retries_left   = _object->_max_retries;

	if (_object->_req_resp == 0.0) {
		_object->_req_resp = _object->_last_activity;
		const double rtt = _object->_req_resp - _object->_req_sent;

		_object->_part_timeout_factor = Type::Resource::PART_TIMEOUT_FACTOR_AFTER_RTT;
		if (_object->_rtt == 0.0) {
			_object->_rtt = _object->_link.rtt();
			watchdog_job();
		}
		else if (rtt < _object->_rtt) {
			_object->_rtt = std::max(_object->_rtt - _object->_rtt * 0.05, rtt);
		}
		else if (rtt > _object->_rtt) {
			_object->_rtt = std::min(_object->_rtt + _object->_rtt * 0.05, rtt);
		}

		if (rtt > 0) {
			const double req_resp_cost = static_cast<double>(packet.raw().size() + _object->_req_sent_bytes);
			_object->_req_resp_rtt_rate = req_resp_cost / rtt;

			if (_object->_req_resp_rtt_rate > Type::Resource::RATE_FAST
			    && _object->_fast_rate_rounds < Type::Resource::FAST_RATE_THRESHOLD) {
				_object->_fast_rate_rounds++;
				if (_object->_fast_rate_rounds == Type::Resource::FAST_RATE_THRESHOLD) {
					_object->_window_max = Type::Resource::WINDOW_MAX_FAST;
				}
			}
		}
	}

	if (_object->_status == Type::Resource::FAILED) {
		_object->_receiving_part = false;
		_object->_receive_lock   = false;
		return;
	}

	_object->_status = Type::Resource::TRANSFERRING;
	const Bytes part_data = packet.data();
	const Bytes part_hash = get_map_hash(part_data);

	const int32_t cci    = (_object->_consecutive_completed_height >= 0) ? _object->_consecutive_completed_height : 0;
	const size_t maphash_len = Type::Resource::MAPHASH_LEN;
	int32_t i = cci;
	const size_t window_end = std::min(static_cast<size_t>(cci) + _object->_window,
	                                   static_cast<size_t>(_object->_total_parts));

	for (size_t j = static_cast<size_t>(cci); j < window_end; ++j) {
		Bytes map_hash_j = _object->_hashmap.mid(j * maphash_len, maphash_len);
		if (map_hash_j == part_hash) {
			if (!_object->_parts[i]) {
				// File this part into the slot.
				Packet stored(part_data);
				stored.pack();   // mirror Python: parts hold packed bytes
				_object->_parts[i] = stored;
				// Update via raw data (not the wrapper) — we want the actual data bytes.
				_object->_parts[i].data(part_data);

				_object->_rtt_rxd_bytes  += part_data.size();
				_object->_received_count += 1;
				if (_object->_outstanding_parts > 0) _object->_outstanding_parts--;

				if (i == _object->_consecutive_completed_height + 1) {
					_object->_consecutive_completed_height = i;
				}

				int32_t cp = _object->_consecutive_completed_height + 1;
				while (cp < static_cast<int32_t>(_object->_parts.size()) && _object->_parts[cp]) {
					_object->_consecutive_completed_height = cp;
					cp++;
				}

				if (_object->_callbacks._progress != nullptr) {
					try {
						_object->_callbacks._progress(*this);
					}
					catch (const std::exception& e) {
						ERRORF("Error while executing progress callback from %s. The contained exception was: %s",
						       toString().c_str(), e.what());
					}
				}
			}
		}
		i++;
	}

	_object->_receiving_part = false;

	if (_object->_received_count == _object->_total_parts && !_object->_assembly_lock) {
		_object->_assembly_lock = true;
		assemble();
	}
	else if (_object->_outstanding_parts == 0) {
		if (_object->_window < _object->_window_max) {
			_object->_window++;
			if ((_object->_window - _object->_window_min) > (_object->_window_flexibility - 1)) {
				_object->_window_min++;
			}
		}

		if (_object->_req_sent != 0.0) {
			const double rtt = Utilities::OS::time() - _object->_req_sent;
			const size_t req_transferred = _object->_rtt_rxd_bytes - _object->_rtt_rxd_bytes_at_part_req;
			if (rtt != 0.0) {
				_object->_req_data_rtt_rate = static_cast<double>(req_transferred) / rtt;
				update_eifr();
				_object->_rtt_rxd_bytes_at_part_req = _object->_rtt_rxd_bytes;

				if (_object->_req_data_rtt_rate > Type::Resource::RATE_FAST
				    && _object->_fast_rate_rounds < Type::Resource::FAST_RATE_THRESHOLD) {
					_object->_fast_rate_rounds++;
					if (_object->_fast_rate_rounds == Type::Resource::FAST_RATE_THRESHOLD) {
						_object->_window_max = Type::Resource::WINDOW_MAX_FAST;
					}
				}
				if (_object->_fast_rate_rounds == 0
				    && _object->_req_data_rtt_rate < Type::Resource::RATE_VERY_SLOW
				    && _object->_very_slow_rate_rounds < Type::Resource::VERY_SLOW_RATE_THRESHOLD) {
					_object->_very_slow_rate_rounds++;
					if (_object->_very_slow_rate_rounds == Type::Resource::VERY_SLOW_RATE_THRESHOLD) {
						_object->_window_max = Type::Resource::WINDOW_MAX_VERY_SLOW;
					}
				}
			}
		}

		request_next();
	}

	_object->_receive_lock = false;
}

/*
Send a RESOURCE_REQ packet asking the initiator for the next window of parts.
Mirror Python Resource.py:931.
*/
void Resource::request_next() {
	assert(_object);
	if (_object->_status == Type::Resource::FAILED) return;
	if (_object->_waiting_for_hmu) return;

	_object->_outstanding_parts = 0;
	uint8_t hashmap_exhausted = Type::Resource::HASHMAP_IS_NOT_EXHAUSTED;
	Bytes requested_hashes;

	uint16_t i = 0;
	int32_t pn = _object->_consecutive_completed_height + 1;
	const int32_t search_start = pn;
	const uint16_t search_size = _object->_window;

	const size_t maphash_len = Type::Resource::MAPHASH_LEN;

	// Zero block sentinel for "no map_hash yet".
	Bytes zero;
	for (size_t k = 0; k < maphash_len; ++k) zero.append(uint8_t(0));

	for (int32_t idx = search_start;
	     idx < search_start + static_cast<int32_t>(search_size)
	         && static_cast<size_t>(idx) < _object->_parts.size();
	     ++idx) {
		if (!_object->_parts[idx]) {
			Bytes part_hash = _object->_hashmap.mid(static_cast<size_t>(pn) * maphash_len, maphash_len);
			if (part_hash != zero) {
				requested_hashes.append(part_hash);
				_object->_outstanding_parts++;
				i++;
			}
			else {
				hashmap_exhausted = Type::Resource::HASHMAP_IS_EXHAUSTED;
			}
		}
		pn++;
		if (i >= _object->_window || hashmap_exhausted == Type::Resource::HASHMAP_IS_EXHAUSTED) break;
	}

	Bytes hmu_part;
	hmu_part.append(hashmap_exhausted);
	if (hashmap_exhausted == Type::Resource::HASHMAP_IS_EXHAUSTED) {
		if (_object->_hashmap_height > 0) {
			Bytes last_map_hash = _object->_hashmap.mid(
				static_cast<size_t>(_object->_hashmap_height - 1) * maphash_len, maphash_len);
			hmu_part.append(last_map_hash);
		}
		else {
			for (size_t k = 0; k < maphash_len; ++k) hmu_part.append(uint8_t(0));
		}
		_object->_waiting_for_hmu = true;
	}

	Bytes request_data;
	request_data.append(hmu_part);
	request_data.append(_object->_hash);
	request_data.append(requested_hashes);

	try {
		Packet request_packet(_object->_link, request_data, Type::Packet::DATA, Type::Packet::RESOURCE_REQ);
		request_packet.send();
		_object->_last_activity            = Utilities::OS::time();
		_object->_req_sent                 = _object->_last_activity;
		_object->_req_sent_bytes           = request_packet.raw().size();
		_object->_rtt_rxd_bytes_at_part_req = _object->_rtt_rxd_bytes;
		_object->_req_resp                 = 0.0;
	}
	catch (const std::exception& e) {
		DEBUGF("Could not send resource request packet, cancelling resource. The contained exception was: %s", e.what());
		cancel();
	}
}


// ============================================================================
// Request (initiator side)
// ============================================================================

/*
Called on the outgoing (initiator-side) resource when a RESOURCE_REQ packet
arrives from the receiver containing a list of map_hashes the receiver wants.
Mirror Python Resource.py:982.
*/
void Resource::request(const Bytes& request_data) {
	assert(_object);
	if (_object->_status == Type::Resource::FAILED) return;
	if (request_data.size() < 1) return;

	const double now = Utilities::OS::time();
	if (_object->_rtt == 0.0) {
		_object->_rtt = now - _object->_adv_sent;
	}

	if (_object->_status != Type::Resource::TRANSFERRING) {
		_object->_status = Type::Resource::TRANSFERRING;
		watchdog_job();
	}

	_object->_retries_left = _object->_max_retries;

	// Layout: [hmu_flag (1 byte) || last_map_hash (4 bytes, only if HMU)
	//         || resource_hash (HASHLENGTH/8 bytes) || requested_map_hashes...]
	const bool wants_more_hashmap = (request_data.data()[0] == Type::Resource::HASHMAP_IS_EXHAUSTED);
	const size_t pad = wants_more_hashmap ? (1 + Type::Resource::MAPHASH_LEN) : 1;
	const size_t hash_bytes = Type::Identity::HASHLENGTH / 8;

	if (request_data.size() < pad + hash_bytes) return;
	const Bytes requested_hashes = request_data.mid(pad + hash_bytes);

	// Build the set of requested map_hashes for membership testing.
	std::vector<Bytes> map_hashes;
	const size_t map_hashes_count = requested_hashes.size() / Type::Resource::MAPHASH_LEN;
	map_hashes.reserve(map_hashes_count);
	for (size_t i = 0; i < map_hashes_count; ++i) {
		map_hashes.push_back(requested_hashes.mid(i * Type::Resource::MAPHASH_LEN, Type::Resource::MAPHASH_LEN));
	}

	const size_t search_start = static_cast<size_t>(_object->_receiver_min_consecutive_height);
	const size_t collision_guard = Type::Resource::ResourceAdvertisement::COLLISION_GUARD_SIZE;
	const size_t search_end = std::min(search_start + collision_guard, static_cast<size_t>(_object->_parts.size()));

	auto map_hash_in_requested = [&](const Bytes& mh) -> bool {
		for (const auto& candidate : map_hashes) {
			if (candidate == mh) return true;
		}
		return false;
	};

	for (size_t i = search_start; i < search_end; ++i) {
		Bytes part_map_hash = _object->_hashmap.mid(i * Type::Resource::MAPHASH_LEN, Type::Resource::MAPHASH_LEN);
		if (!map_hash_in_requested(part_map_hash)) continue;

		try {
			Packet& part = _object->_parts[i];
			if (!part.sent()) {
				part.send();
				_object->_sent_parts++;
			}
			else {
				part.resend();
			}
			_object->_last_activity  = Utilities::OS::time();
			_object->_last_part_sent = _object->_last_activity;
		}
		catch (const std::exception& e) {
			DEBUGF("Resource could not send parts, cancelling transfer! The contained exception was: %s", e.what());
			cancel();
			return;
		}
	}

	if (wants_more_hashmap) {
		// Receiver is requesting the next slice of the hashmap.
		// Mirror Python Resource.py:1027-1064. Locate the last map_hash the
		// receiver saw and emit the next HASHMAP_MAX_LEN entries.
		const Bytes last_map_hash = request_data.mid(1, Type::Resource::MAPHASH_LEN);
		size_t part_index = static_cast<size_t>(_object->_receiver_min_consecutive_height);
		for (size_t i = search_start; i < search_end; ++i) {
			++part_index;
			Bytes mh = _object->_hashmap.mid(i * Type::Resource::MAPHASH_LEN, Type::Resource::MAPHASH_LEN);
			if (mh == last_map_hash) break;
		}

		const int32_t new_min = static_cast<int32_t>(part_index) - 1 - static_cast<int32_t>(Type::Resource::WINDOW_MAX);
		_object->_receiver_min_consecutive_height = std::max<int32_t>(new_min, 0);

		const size_t hm_max_len = Type::Resource::ResourceAdvertisement::HASHMAP_MAX_LEN;
		if (part_index % hm_max_len != 0) {
			ERROR("Resource sequencing error, cancelling transfer!");
			cancel();
			return;
		}
		const size_t segment = part_index / hm_max_len;

		const size_t hashmap_start = segment * hm_max_len;
		size_t hashmap_end = (segment + 1) * hm_max_len;
		if (hashmap_end > _object->_parts.size()) hashmap_end = _object->_parts.size();

		Bytes segment_hashmap = _object->_hashmap.mid(hashmap_start * Type::Resource::MAPHASH_LEN, (hashmap_end - hashmap_start) * Type::Resource::MAPHASH_LEN);

		// hmu = hash || msgpack([segment, hashmap]) — mirror Python Resource.py:1055.
		MsgPack::Packer hmu_packer;
		hmu_packer.packArraySize(2);
		hmu_packer.serialize(static_cast<uint32_t>(segment));
		hmu_packer.packBinary(segment_hashmap.data(), segment_hashmap.size());

		Bytes hmu_payload;
		hmu_payload.append(_object->_hash);
		hmu_payload.append(hmu_packer.data(), hmu_packer.size());

		try {
			Packet hmu_packet(_object->_link, hmu_payload, Type::Packet::DATA, Type::Packet::RESOURCE_HMU);
			hmu_packet.send();
			_object->_last_activity = Utilities::OS::time();
		}
		catch (const std::exception& e) {
			DEBUGF("Could not send resource HMU packet, cancelling resource. The contained exception was: %s", e.what());
			cancel();
			return;
		}
	}

	if (_object->_sent_parts == _object->_parts.size()) {
		_object->_status       = Type::Resource::AWAITING_PROOF;
		_object->_retries_left = 3;
	}

	if (_object->_callbacks._progress != nullptr) {
		try {
			_object->_callbacks._progress(*this);
		}
		catch (const std::exception& e) {
			ERRORF("Error while executing progress callback from %s. The contained exception was: %s", toString().c_str(), e.what());
		}
	}
}


// ============================================================================
// Cancel & rejected
// ============================================================================

/*
Cancels transferring the resource. Mirror Python Resource.py:1075.
*/
void Resource::cancel() {
	assert(_object);

	if (_object->_next_segment) {
		_object->_next_segment.cancel();
	}

	if (_object->_status == Type::Resource::CORRUPT) {
		_object->_link.cancel_incoming_resource(*this);
		Resource::reject(_object->_advertisement_packet);
		_object->_link.teardown();
		return;
	}

	if (_object->_status >= Type::Resource::COMPLETE) return;

	_object->_status = Type::Resource::FAILED;
	if (_object->_initiator) {
		if (_object->_link.status() == Type::Link::ACTIVE) {
			try {
				Packet cancel_packet(_object->_link, _object->_hash, Type::Packet::DATA, Type::Packet::RESOURCE_ICL);
				cancel_packet.send();
			}
			catch (const std::exception& e) {
				ERRORF("Could not send resource cancel packet, the contained exception was: %s", e.what());
			}
		}
		_object->_link.cancel_outgoing_resource(*this);
	}
	else {
		_object->_link.cancel_incoming_resource(*this);
	}

	if (_object->_callbacks._concluded != nullptr) {
		try {
			_object->_link.resource_concluded(*this);
			_object->_callbacks._concluded(*this);
		}
		catch (const std::exception& e) {
			ERRORF("Error while executing callbacks on resource cancel from %s. The contained exception was: %s", toString().c_str(), e.what());
		}
	}
}

/*
Called when a RESOURCE_RCL (receiver-side reject) arrives. Mirror Python _rejected().
*/
void Resource::rejected() {
	assert(_object);
	if (_object->_status >= Type::Resource::COMPLETE) return;
	if (!_object->_initiator) return;

	_object->_status = Type::Resource::REJECTED;
	_object->_link.cancel_outgoing_resource(*this);
	if (_object->_callbacks._concluded != nullptr) {
		try {
			_object->_link.resource_concluded(*this);
			_object->_callbacks._concluded(*this);
		}
		catch (const std::exception& e) {
			ERRORF("Error while executing callbacks on resource reject from %s. The contained exception was: %s", toString().c_str(), e.what());
		}
	}
}


// ============================================================================
// Callbacks
// ============================================================================

void Resource::set_callback(Callbacks::concluded callback) {
	assert(_object);
	_object->_callbacks._concluded = callback;
}

void Resource::progress_callback(Callbacks::progress callback) {
	assert(_object);
	_object->_callbacks._progress = callback;
}

void Resource::set_concluded_callback(Callbacks::concluded callback) {
	set_callback(callback);
}

void Resource::set_progress_callback(Callbacks::progress callback) {
	progress_callback(callback);
}


// ============================================================================
// Progress / introspection
// ============================================================================

/*
:returns: The current progress of the resource transfer as a *float* between 0.0 and 1.0.
*/
float Resource::get_progress() const {
	assert(_object);
	if (_object->_status == Type::Resource::COMPLETE
	    && _object->_segment_index == _object->_total_segments) {
		return 1.0f;
	}

	double processed = 0.0;
	double total     = 0.0;
	if (_object->_initiator) {
		if (!_object->_split) {
			processed = static_cast<double>(_object->_sent_parts);
			total     = static_cast<double>(_object->_total_parts);
		}
		else {
			const uint16_t processed_segments = _object->_segment_index - 1;
			const uint32_t max_parts_per_segment = static_cast<uint32_t>(
				(Type::Resource::MAX_EFFICIENT_SIZE + _object->_sdu - 1) / _object->_sdu);
			const double current_segment_factor = (_object->_total_parts < max_parts_per_segment)
				? (static_cast<double>(max_parts_per_segment) / static_cast<double>(_object->_total_parts))
				: 1.0;
			processed = static_cast<double>(processed_segments) * max_parts_per_segment
			          + static_cast<double>(_object->_sent_parts) * current_segment_factor;
			total     = static_cast<double>(_object->_total_segments) * max_parts_per_segment;
		}
	}
	else {
		if (!_object->_split) {
			processed = static_cast<double>(_object->_received_count);
			total     = static_cast<double>(_object->_total_parts);
		}
		else {
			const uint16_t processed_segments = _object->_segment_index - 1;
			const uint32_t max_parts_per_segment = static_cast<uint32_t>(
				(Type::Resource::MAX_EFFICIENT_SIZE + _object->_sdu - 1) / _object->_sdu);
			const double current_segment_factor = (_object->_total_parts < max_parts_per_segment)
				? (static_cast<double>(max_parts_per_segment) / static_cast<double>(_object->_total_parts))
				: 1.0;
			processed = static_cast<double>(processed_segments) * max_parts_per_segment
			          + static_cast<double>(_object->_received_count) * current_segment_factor;
			total     = static_cast<double>(_object->_total_segments) * max_parts_per_segment;
		}
	}

	if (total <= 0.0) return 0.0f;
	const double progress = processed / total;
	return static_cast<float>(progress > 1.0 ? 1.0 : progress);
}

float Resource::get_segment_progress() const {
	assert(_object);
	if (_object->_status == Type::Resource::COMPLETE
	    && _object->_segment_index == _object->_total_segments) {
		return 1.0f;
	}
	if (_object->_total_parts == 0) return 0.0f;
	const double processed = _object->_initiator ? _object->_sent_parts : _object->_received_count;
	const double progress  = processed / static_cast<double>(_object->_total_parts);
	return static_cast<float>(progress > 1.0 ? 1.0 : progress);
}

size_t Resource::get_transfer_size() const {
	assert(_object);
	return _object->_size;
}

size_t Resource::get_data_size() const {
	assert(_object);
	return _object->_total_size;
}

uint32_t Resource::get_parts() const {
	assert(_object);
	return _object->_total_parts;
}

uint16_t Resource::get_segments() const {
	assert(_object);
	return _object->_total_segments;
}

const Bytes& Resource::get_hash() const {
	assert(_object);
	return _object->_hash;
}

bool Resource::is_compressed() const {
	assert(_object);
	return _object->_compressed;
}


// ============================================================================
// String representation
// ============================================================================

std::string Resource::toString() const {
	if (!_object) return "";
	// Mirror Python __str__() (Resource.py:1230): "<hash/link_id>".
	std::string s = "<";
	s += _object->_hash.toHex();
	s += "/";
	if (_object->_link) s += _object->_link.link_id().toHex();
	s += ">";
	return s;
}


// ============================================================================
// Legacy getters (existing API, retained)
// ============================================================================

const Bytes& Resource::hash() const {
	assert(_object);
	return _object->_hash;
}

const Bytes& Resource::request_id() const {
	assert(_object);
	return _object->_request_id;
}

const Bytes& Resource::data() const {
	assert(_object);
	return _object->_data;
}

Type::Resource::status Resource::status() const {
	assert(_object);
	return _object->_status;
}

size_t Resource::size() const {
	assert(_object);
	return _object->_size;
}

size_t Resource::total_size() const {
	assert(_object);
	return _object->_total_size;
}

bool Resource::is_response() const {
	assert(_object);
	return _object->_is_response;
}

bool Resource::initiator() const {
	assert(_object);
	return _object->_initiator;
}

bool Resource::has_request_hash(const Bytes& packet_hash) const {
	assert(_object);
	for (const auto& seen : _object->_req_hashlist) {
		if (seen == packet_hash) return true;
	}
	return false;
}

void Resource::note_request_hash(const Bytes& packet_hash) {
	assert(_object);
	_object->_req_hashlist.push_back(packet_hash);
}


// ============================================================================
// ResourceAdvertisement
// ============================================================================

ResourceAdvertisement::ResourceAdvertisement(const Resource& resource, const Bytes& request_id /*= {Bytes::NONE}*/, bool is_response /*= false*/) {
	if (!resource) return;

	_link = resource._object->_link;
	_t    = resource._object->_size;
	_d    = resource._object->_total_size;
	_n    = static_cast<uint32_t>(resource._object->_parts.size());
	_h    = resource._object->_hash;
	_r    = resource._object->_random_hash;
	_o    = resource._object->_original_hash;
	_m    = resource._object->_hashmap;
	_c    = resource._object->_compressed;
	_e    = resource._object->_encrypted;
	_s    = resource._object->_split;
	_x    = resource._object->_has_metadata;
	_i    = resource._object->_segment_index;
	_l    = resource._object->_total_segments;
	_q    = resource._object->_request_id;
	_u    = false;
	_p    = false;

	if (_q.size() > 0) {
		if (!resource._object->_is_response) {
			_u = true;
			_p = false;
		} else {
			_u = false;
			_p = true;
		}
	}

	// Packed flag byte (mirror Python Resource.py:1307)
	_f = static_cast<uint8_t>(
		  (_x ? 1u : 0u) << 5
		| (_p ? 1u : 0u) << 4
		| (_u ? 1u : 0u) << 3
		| (_s ? 1u : 0u) << 2
		| (_c ? 1u : 0u) << 1
		| (_e ? 1u : 0u));
}

bool ResourceAdvertisement::is_request(const Packet& advertisement_packet) {
	ResourceAdvertisement adv = ResourceAdvertisement::unpack(advertisement_packet.plaintext());
	return adv._q.size() > 0 && adv._u;
}

bool ResourceAdvertisement::is_response(const Packet& advertisement_packet) {
	ResourceAdvertisement adv = ResourceAdvertisement::unpack(advertisement_packet.plaintext());
	return adv._q.size() > 0 && adv._p;
}

Bytes ResourceAdvertisement::read_request_id(const Packet& advertisement_packet) {
	ResourceAdvertisement adv = ResourceAdvertisement::unpack(advertisement_packet.plaintext());
	return adv._q;
}

size_t ResourceAdvertisement::read_transfer_size(const Packet& advertisement_packet) {
	ResourceAdvertisement adv = ResourceAdvertisement::unpack(advertisement_packet.plaintext());
	return adv._t;
}

size_t ResourceAdvertisement::read_size(const Packet& advertisement_packet) {
	ResourceAdvertisement adv = ResourceAdvertisement::unpack(advertisement_packet.plaintext());
	return adv._d;
}

Bytes ResourceAdvertisement::pack(uint16_t segment /*= 0*/) const {
	// Compute the slice of the hashmap belonging to this segment, matching
	// Python Resource.py:1334-1339.
	const size_t hm_max_len    = Type::Resource::ResourceAdvertisement::HASHMAP_MAX_LEN;
	const size_t hashmap_start = static_cast<size_t>(segment) * hm_max_len;
	size_t hashmap_end         = (static_cast<size_t>(segment) + 1) * hm_max_len;
	if (hashmap_end > _n) hashmap_end = _n;

	Bytes hashmap_slice;
	const size_t maphash_len = Type::Resource::MAPHASH_LEN;
	if (_m.size() >= hashmap_end * maphash_len && hashmap_end > hashmap_start) {
		hashmap_slice = _m.mid(hashmap_start * maphash_len, (hashmap_end - hashmap_start) * maphash_len);
	}

	// Pack as a msgpack map. Key insertion order matches Python (Resource.py:1341-1353)
	// so byte-output is identical for interop.
	MsgPack::Packer p;
	p.packMapSize(11);

	p.pack("t");
	p.serialize(static_cast<uint64_t>(_t));

	p.pack("d");
	p.serialize(static_cast<uint64_t>(_d));

	p.pack("n");
	p.serialize(static_cast<uint32_t>(_n));

	p.pack("h");
	p.packBinary(_h.data(), _h.size());

	p.pack("r");
	p.packBinary(_r.data(), _r.size());

	p.pack("o");
	p.packBinary(_o.data(), _o.size());

	p.pack("i");
	p.serialize(static_cast<uint32_t>(_i));

	p.pack("l");
	p.serialize(static_cast<uint32_t>(_l));

	p.pack("q");
	if (_q.size() > 0) {
		p.packBinary(_q.data(), _q.size());
	} else {
		p.packNil();
	}

	p.pack("f");
	p.serialize(static_cast<uint32_t>(_f));

	p.pack("m");
	p.packBinary(hashmap_slice.data(), hashmap_slice.size());

	return Bytes(p.data(), p.size());
}

ResourceAdvertisement ResourceAdvertisement::unpack(const Bytes& data) {
	ResourceAdvertisement adv;
	if (!data || data.size() == 0) return adv;

	MsgPack::Unpacker u;
	u.feed(data.data(), data.size());

	if (!u.isMap()) {
		TRACE("ResourceAdvertisement::unpack: data is not a msgpack map");
		return adv;
	}

	const size_t entries = u.unpackMapSize();
	for (size_t e = 0; e < entries; ++e) {
		MsgPack::str_t key;
		if (!u.deserialize(key)) {
			TRACE("ResourceAdvertisement::unpack: failed to deserialize key");
			return adv;
		}

		// Each branch must consume exactly one value from the stream.
		// MsgPack::str_t is Arduino String on MCU targets (no .size()) and
		// std::string on native. .length() / operator[] work on both.
		if (key.length() == 1) {
			const char k = key[0];
			if (k == 't') {
				uint64_t v = 0; u.deserialize(v); adv._t = static_cast<size_t>(v);
			} else if (k == 'd') {
				uint64_t v = 0; u.deserialize(v); adv._d = static_cast<size_t>(v);
			} else if (k == 'n') {
				uint32_t v = 0; u.deserialize(v); adv._n = v;
			} else if (k == 'h') {
				MsgPack::bin_t<uint8_t> b; u.deserialize(b);
				adv._h = Bytes(b.data(), b.size());
			} else if (k == 'r') {
				MsgPack::bin_t<uint8_t> b; u.deserialize(b);
				adv._r = Bytes(b.data(), b.size());
			} else if (k == 'o') {
				MsgPack::bin_t<uint8_t> b; u.deserialize(b);
				adv._o = Bytes(b.data(), b.size());
			} else if (k == 'm') {
				MsgPack::bin_t<uint8_t> b; u.deserialize(b);
				adv._m = Bytes(b.data(), b.size());
			} else if (k == 'i') {
				uint32_t v = 0; u.deserialize(v); adv._i = static_cast<uint16_t>(v);
			} else if (k == 'l') {
				uint32_t v = 0; u.deserialize(v); adv._l = static_cast<uint16_t>(v);
			} else if (k == 'q') {
				if (u.isNil()) {
					u.unpackNil();
				} else if (u.isBin()) {
					MsgPack::bin_t<uint8_t> b; u.deserialize(b);
					adv._q = Bytes(b.data(), b.size());
				} else {
					TRACE("ResourceAdvertisement::unpack: 'q' is neither nil nor bin");
					return adv;
				}
			} else if (k == 'f') {
				uint32_t v = 0; u.deserialize(v); adv._f = static_cast<uint8_t>(v);
			} else {
				TRACE("ResourceAdvertisement::unpack: unknown single-char key");
				return adv;
			}
		} else {
			TRACE("ResourceAdvertisement::unpack: unexpected multi-char key");
			return adv;
		}
	}

	// Derive booleans from the flag byte (mirror Python Resource.py:1374-1379).
	adv._e = (adv._f      & 0x01) != 0;
	adv._c = ((adv._f >> 1) & 0x01) != 0;
	adv._s = ((adv._f >> 2) & 0x01) != 0;
	adv._u = ((adv._f >> 3) & 0x01) != 0;
	adv._p = ((adv._f >> 4) & 0x01) != 0;
	adv._x = ((adv._f >> 5) & 0x01) != 0;

	return adv;
}
