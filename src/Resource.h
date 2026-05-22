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

#pragma once

#include "Destination.h"
#include "Link.h"
#include "Type.h"
#include "Bytes.h"

#include <memory>
#include <cassert>

namespace RNS {

	class ResourceData;
	class Packet;
	class Destination;
	class Resource;
	class ResourceAdvertisement;

/*
	The Resource class allows transferring arbitrary amounts of data over a link.
	It will automatically handle sequencing, compression, coordination and
	checksumming.

	:param data: The data to be transferred. Can be *bytes* or an open file handle.
	:param link: The :ref:`RNS.Link<api-link>` instance on which to transfer the data.
	:param advertise: Optional. Whether to automatically advertise the resource.
	:param auto_compress: Optional. Whether to auto-compress the resource.
	:param callback: An optional callback called when the resource transfer concludes.
	:param progress_callback: An optional callback called whenever transfer progress is updated.
*/
	class Resource {

	public:
		class Callbacks {
		public:
			// CBA std::function apparently not implemented in NRF52 framework
			//typedef std::function<void(const Resource& resource)> concluded;
			using concluded = void(*)(const Resource& resource);
			using progress = void(*)(const Resource& resource);
		public:
			concluded _concluded = nullptr;
			progress _progress = nullptr;
		friend class Resource;
		};

	public:
		Resource(Type::NoneConstructor none) {
			MEM("Resource NONE object created");
		}
		Resource(const Resource& resource) : _object(resource._object) {
			MEM("Resource object copy created");
		}
		Resource(const Bytes& data, const Link& link, const Bytes& request_id, bool is_response, double timeout);
		Resource(const Bytes& data, const Link& link, bool advertise = true, bool auto_compress = true, Callbacks::concluded callback = nullptr, Callbacks::progress progress_callback = nullptr, double timeout = 0.0, uint16_t segment_index = 1, const Bytes& original_hash = {Bytes::NONE}, const Bytes& request_id = {Bytes::NONE}, bool is_response = false, size_t sent_metadata_size = 0);
		virtual ~Resource(){
			MEM("Resource object destroyed");
		}

		Resource& operator = (const Resource& resource) {
			_object = resource._object;
			return *this;
		}
		operator bool() const {
			return _object.get() != nullptr;
		}
		bool operator < (const Resource& resource) const {
			return _object.get() < resource._object.get();
		}

	public:
		// Static creation entry points
		static void reject(const Packet& advertisement_packet);
		static Resource accept(const Packet& advertisement_packet, Callbacks::concluded callback = nullptr, Callbacks::progress progress_callback = nullptr, const Bytes& request_id = {Bytes::NONE});

	public:
		// Methods in roughly the same order as Python RNS.Resource
		void hashmap_update_packet(const Bytes& plaintext);
		void hashmap_update(uint16_t segment, const Bytes& hashmap);
		Bytes get_map_hash(const Bytes& data);
		void advertise();
		void advertise_job();
		void update_eifr();
		void watchdog_job();
		void __watchdog_job();
		void assemble();
		void prove();
		void prepare_next_segment();
		void validate_proof(const Bytes& proof_data);
		void receive_part(const Packet& packet);
		void request_next();
		void request(const Bytes& request_data);
		void cancel();
		void rejected();
		void set_callback(Callbacks::concluded callback);
		void progress_callback(Callbacks::progress callback);
		float get_progress() const;
		float get_segment_progress() const;
		size_t get_transfer_size() const;
		size_t get_data_size() const;
		uint32_t get_parts() const;
		uint16_t get_segments() const;
		const Bytes& get_hash() const;
		bool is_compressed() const;

		// CBA legacy names kept for backwards compatibility
		void set_concluded_callback(Callbacks::concluded callback);
		void set_progress_callback(Callbacks::progress callback);

		std::string toString() const;

		// getters
		const Bytes& hash() const;
		const Bytes& request_id() const;
		const Bytes& data() const;
		Type::Resource::status status() const;
		size_t size() const;
		size_t total_size() const;
		bool is_response() const;
		bool initiator() const;

		// Per-resource RESOURCE_REQ packet-hash dedupe (used by the Link
		// dispatcher to drop retransmitted requests). Mirror Python
		// Resource.req_hashlist.
		bool has_request_hash(const Bytes& packet_hash) const;
		void note_request_hash(const Bytes& packet_hash);

	protected:
		std::shared_ptr<ResourceData> _object;

	friend class ResourceAdvertisement;
	};


/*
	ResourceAdvertisement carries the metadata describing a resource that an
	initiator wishes to send to a peer. Its packed form is sent in a
	RESOURCE_ADV-context packet; the receiver parses it via accept().
*/
	class ResourceAdvertisement {

	public:
		ResourceAdvertisement() = default;
		ResourceAdvertisement(const Resource& resource, const Bytes& request_id = {Bytes::NONE}, bool is_response = false);

	public:
		// Static helpers that operate on an advertisement-context packet
		static bool is_request(const Packet& advertisement_packet);
		static bool is_response(const Packet& advertisement_packet);
		static Bytes read_request_id(const Packet& advertisement_packet);
		static size_t read_transfer_size(const Packet& advertisement_packet);
		static size_t read_size(const Packet& advertisement_packet);

		// Wire-format codec
		Bytes pack(uint16_t segment = 0) const;
		static ResourceAdvertisement unpack(const Bytes& data);

		// Getters mirroring Python
		size_t get_transfer_size() const { return _t; }
		size_t get_data_size() const { return _d; }
		uint32_t get_parts() const { return _n; }
		uint16_t get_segments() const { return _l; }
		const Bytes& get_hash() const { return _h; }
		bool is_compressed() const { return _c; }
		bool has_metadata() const { return _x; }
		const Link& get_link() const { return _link; }

	public:
		// Public fields named to match Python single-letter attributes for
		// readability against the reference implementation.
		Link _link = {Type::NONE};
		size_t _t = 0;            // Transfer size
		size_t _d = 0;            // Total uncompressed data size
		uint32_t _n = 0;          // Number of parts
		Bytes _h;                 // Resource hash
		Bytes _r;                 // Resource random hash
		Bytes _o;                 // First-segment hash
		Bytes _m;                 // Resource hashmap (segment)
		bool _c = false;          // Compression flag
		bool _e = false;          // Encryption flag
		bool _s = false;          // Split flag
		bool _x = false;          // Metadata flag
		uint16_t _i = 1;          // Segment index
		uint16_t _l = 1;          // Total segments
		Bytes _q;                 // Associated request ID (may be empty)
		bool _u = false;          // Is-request flag
		bool _p = false;          // Is-response flag
		uint8_t _f = 0x00;        // Packed flag byte
	};

}
