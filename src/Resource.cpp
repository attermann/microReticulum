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

void Resource::reject(const Packet& advertisement_packet) {
	// TODO 1.4 — port Python Resource.reject() (Resource.py:155)
}

Resource Resource::accept(const Packet& advertisement_packet, Callbacks::concluded callback /*= nullptr*/, Callbacks::progress progress_callback /*= nullptr*/, const Bytes& request_id /*= {Bytes::NONE}*/) {
	// TODO 1.4 — port Python Resource.accept() (Resource.py:167)
	return {Type::NONE};
}


// ============================================================================
// Constructors
// ============================================================================

Resource::Resource(const Bytes& data, const Link& link, const Bytes& request_id, bool is_response, double timeout) :
	_object(new ResourceData(link))
{
	assert(_object);
	MEM("Resource object created");
	// TODO 1.3 — delegate to the main constructor once it is implemented
}

Resource::Resource(const Bytes& data, const Link& link, bool advertise /*= true*/, bool auto_compress /*= true*/, Callbacks::concluded callback /*= nullptr*/, Callbacks::progress progress_callback /*= nullptr*/, double timeout /*= 0.0*/, uint16_t segment_index /*= 1*/, const Bytes& original_hash /*= {Bytes::NONE}*/, const Bytes& request_id /*= {Bytes::NONE}*/, bool is_response /*= false*/, size_t sent_metadata_size /*= 0*/) :
	_object(new ResourceData(link))
{
	assert(_object);
	MEM("Resource object created");
	// TODO 1.3 — port Python Resource.__init__() initiator branch (Resource.py:248)
	// TODO 1.4 — port Python Resource.__init__() receiver branch (data == None case)
}


// ============================================================================
// Hashmap handling
// ============================================================================

void Resource::hashmap_update_packet(const Bytes& plaintext) {
	// TODO 1.4 — port Python hashmap_update_packet() (Resource.py:483)
}

void Resource::hashmap_update(uint16_t segment, const Bytes& hashmap) {
	// TODO 1.4 — port Python hashmap_update() (Resource.py:492)
}

Bytes Resource::get_map_hash(const Bytes& data) {
	// TODO 1.3 — port Python get_map_hash() (Resource.py:505)
	return {Bytes::NONE};
}


// ============================================================================
// Advertise / advertise job
// ============================================================================

void Resource::advertise() {
	// TODO 1.3 — port Python advertise() (Resource.py:508)
}

void Resource::advertise_job() {
	// TODO 1.3 — port Python __advertise_job() (Resource.py:520)
	// Driven cooperatively from Resource::__watchdog_job() / Reticulum::loop().
}


// ============================================================================
// EIFR (effective inbound flow rate) tracking
// ============================================================================

void Resource::update_eifr() {
	// TODO 1.3 — port Python update_eifr() (Resource.py:543)
}


// ============================================================================
// Watchdog
// ============================================================================

void Resource::watchdog_job() {
	// TODO 1.3 — port Python watchdog_job() (Resource.py:560)
	// In Python this spawns a thread; in C++ we run a single cooperative
	// state-machine tick (__watchdog_job) from Reticulum::loop().
}

void Resource::__watchdog_job() {
	// TODO 1.3 — port Python __watchdog_job() (Resource.py:564)
}


// ============================================================================
// Assemble & prove (receiver side)
// ============================================================================

void Resource::assemble() {
	// TODO 1.4 — port Python assemble() (Resource.py:672)
}

void Resource::prove() {
	// TODO 1.4 — port Python prove() (Resource.py:752)
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

void Resource::validate_proof(const Bytes& proof_data) {
	// TODO 1.3 — port Python validate_proof() (Resource.py:782)
}


// ============================================================================
// Receive part / request next (receiver side)
// ============================================================================

void Resource::receive_part(const Packet& packet) {
	// TODO 1.4 — port Python receive_part() (Resource.py:828)
}

void Resource::request_next() {
	// TODO 1.4 — port Python request_next() (Resource.py:931)
}


// ============================================================================
// Request (initiator side)
// ============================================================================

void Resource::request(const Bytes& request_data) {
	// TODO 1.3 — port Python request() (Resource.py:982)
}


// ============================================================================
// Cancel & rejected
// ============================================================================

void Resource::cancel() {
	// TODO 1.5 — port Python cancel() (Resource.py:1075)
}

void Resource::rejected() {
	// TODO 1.5 — port Python _rejected() (Resource.py:1106)
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
	// TODO 1.5 — port Python get_progress() (Resource.py:1126)
	return 0.0f;
}

float Resource::get_segment_progress() const {
	// TODO 1.5 — port Python get_segment_progress() (Resource.py:1183)
	return 0.0f;
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
	if (!_object) {
		return "";
	}
	// TODO 1.5 — port Python __str__() (Resource.py:1230)
	// Format: "<"+hexrep(hash)+"/"+hexrep(link.link_id)+">"
	return "{Resource: " + _object->_hash.toHex() + "}";
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
