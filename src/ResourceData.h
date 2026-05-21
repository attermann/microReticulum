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

#include "Resource.h"

#include "Link.h"
#include "Packet.h"
#include "Interface.h"
#include "Destination.h"
#include "Bytes.h"
#include "Type.h"
#include "Cryptography/Fernet.h"

#include <vector>

namespace RNS {

	class ResourceData {
	public:
		ResourceData(const Link& link) : _link(link) {
			MEM("ResourceData object created");
		}
		virtual ~ResourceData() {
			MEM("ResourceData object destroyed");
		}
	private:
		// Link this resource is being transferred on
		Link _link;

		// Initiator/receiver role
		bool _initiator = false;

		// Status & flow control
		Type::Resource::status _status = Type::Resource::NONE;
		uint8_t _flags = 0x00;
		bool _encrypted = false;
		bool _compressed = false;
		bool _split = false;
		bool _is_response = false;
		bool _has_metadata = false;

		// Hashes
		Bytes _hash;
		Bytes _truncated_hash;
		Bytes _expected_proof;
		Bytes _original_hash;
		Bytes _random_hash;
		Bytes _request_id;

		// Sizes
		size_t _size = 0;
		size_t _total_size = 0;
		size_t _uncompressed_size = 0;
		size_t _compressed_size = 0;
		size_t _metadata_size = 0;

		// Payload (initiator: data to send; receiver: assembled data)
		Bytes _data;
		Bytes _metadata;
		Bytes _uncompressed_data;
		Bytes _compressed_data;

		// SDU and MTU
		uint16_t _sdu = Type::Resource::SDU;

		// Parts (transferred packets) and hashmap
		std::vector<Packet> _parts;
		Bytes _hashmap;             // packed N x MAPHASH_LEN bytes
		Bytes _hashmap_raw;         // raw hashmap as received in advertisement
		uint32_t _hashmap_height = 0;
		uint32_t _total_parts = 0;
		uint32_t _sent_parts = 0;
		uint32_t _received_count = 0;
		uint16_t _outstanding_parts = 0;
		int32_t _consecutive_completed_height = -1;
		int32_t _receiver_min_consecutive_height = 0;

		// Segmentation (large resources are split across segments)
		uint16_t _segment_index = 1;
		uint16_t _total_segments = 1;
		Resource _next_segment = {Type::NONE};
		bool _preparing_next_segment = false;

		// Window control
		uint16_t _window = Type::Resource::WINDOW;
		uint16_t _window_max = Type::Resource::WINDOW_MAX_SLOW;
		uint16_t _window_min = Type::Resource::WINDOW_MIN;
		uint16_t _window_flexibility = Type::Resource::WINDOW_FLEXIBILITY;
		uint8_t _fast_rate_rounds = 0;
		uint8_t _very_slow_rate_rounds = 0;

		// Retries
		uint8_t _max_retries = Type::Resource::MAX_RETRIES;
		uint8_t _max_adv_retries = Type::Resource::MAX_ADV_RETRIES;
		uint8_t _retries_left = Type::Resource::MAX_RETRIES;

		// Timing
		double _rtt = 0.0;
		double _timeout = 0.0;
		uint8_t _timeout_factor = Type::Link::TRAFFIC_TIMEOUT_FACTOR;
		uint8_t _part_timeout_factor = Type::Resource::PART_TIMEOUT_FACTOR;
		double _sender_grace_time = Type::Resource::SENDER_GRACE_TIME;
		double _last_activity = 0.0;
		double _started_transferring = 0.0;
		double _adv_sent = 0.0;
		double _last_part_sent = 0.0;
		double _req_sent = 0.0;
		double _req_resp = 0.0;

		// Rate / EIFR (effective inbound flow rate) tracking
		size_t _rtt_rxd_bytes = 0;
		size_t _rtt_rxd_bytes_at_part_req = 0;
		size_t _req_sent_bytes = 0;
		double _req_resp_rtt_rate = 0.0;
		double _req_data_rtt_rate = 0.0;
		double _eifr = 0.0;
		double _previous_eifr = 0.0;

		// Compression
		bool _auto_compress = false;
		bool _auto_compress_option = false;
		size_t _auto_compress_limit = Type::Resource::AUTO_COMPRESS_MAX_SIZE;
		size_t _max_decompressed_size = Type::Resource::AUTO_COMPRESS_MAX_SIZE;

		// State guards (single-threaded equivalents of Python's Locks/flags)
		bool _watchdog_lock = false;
		bool _receive_lock = false;
		bool _assembly_lock = false;
		bool _waiting_for_hmu = false;
		bool _receiving_part = false;
		bool _hmu_retry_ok = false;
		uint32_t _watchdog_job_id = 0;

		// Packets
		Packet _advertisement_packet = {Type::NONE};

		// Per-resource dedupe of incoming RESOURCE_REQ packets so a
		// retransmitted request does not cause duplicate part sends.
		std::vector<Bytes> _req_hashlist;

		// Progress tracking
		uint32_t _processed_parts = 0;
		uint32_t _progress_total_parts = 0;

		// Callbacks
		Resource::Callbacks _callbacks;

	friend class Resource;
	friend class ResourceAdvertisement;
	};

}
