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

#include "DestinationEntry.h"

#include "Transport.h"
#include "Type.h"

using namespace RNS;
using namespace RNS::Persistence;

/*static*/ std::vector<uint8_t> microStore::Codec<DestinationEntry>::encode(const DestinationEntry& entry) {

	// If invalid/empty entry then return empty
	if (!entry) return {};

	std::vector<uint8_t> out;

	auto write = [&](const void* ptr, size_t len)
	{
		const uint8_t* p=(const uint8_t*)ptr;
		out.insert(out.end(), p, p+len);
	};

	// timestamp
//TRACEF("Writing %lu byte timestamp: %f", sizeof(entry._timestamp), entry._timestamp);
	write(&entry._timestamp, sizeof(entry._timestamp));

	// hops
//TRACEF("Writing %lu byte hops: %u", sizeof(entry._hops), entry._hops);
	write(&entry._hops, sizeof(entry._hops));

	// expires
//TRACEF("Writing %lu byte expires: %f", sizeof(entry._expires), entry._expires);
	write(&entry._expires, sizeof(entry._expires));

	// received_from
//TRACEF("Writing %lu byte received_from", entry._received_from.collection().size());
	out.insert(out.end(), entry._received_from.collection().begin(), entry._received_from.collection().end());

	// random_blobs
	uint16_t blob_count = entry._random_blobs.size();
//TRACEF("Writing %lu byte blob_count: %u", sizeof(blob_count), blob_count);
	write(&blob_count, sizeof(blob_count));
	for (auto& blob : entry._random_blobs) {
		uint16_t blob_size = blob.collection().size();
//TRACEF("Writing %lu byte blob_size: %u", sizeof(blob_size), blob_size);
		write(&blob_size, sizeof(blob_size));
//TRACEF("Writing %lu byte blob", blob.collection().size());
		out.insert(out.end(), blob.collection().begin(), blob.collection().end());
	}

	// receiving_interface
	Bytes interface_hash(entry._receiving_interface.get_hash());
//TRACEF("Writing %lu byte receiving_interface hash", interface_hash.collection().size());
	out.insert(out.end(), interface_hash.collection().begin(), interface_hash.collection().end());

	// announce_packet
	uint16_t packet_size = entry._announce_packet.raw().size();
//TRACEF("Writing %lu byte packet_size: %u", sizeof(packet_size), packet_size);
	write(&packet_size, sizeof(packet_size));
//TRACEF("Writing %lu byte packet", entry._announce_packet.raw().collection().size());
	out.insert(out.end(), entry._announce_packet.raw().collection().begin(), entry._announce_packet.raw().collection().end());

//TRACEF("Encoded %lu byte DestinationEntry", out.size());

	return out;
}

/*static*/ bool microStore::Codec<DestinationEntry>::decode(const std::vector<uint8_t>& data, DestinationEntry& entry)
{
	size_t pos = 0;

//TRACEF("Decoding %lu byte DestinationEntry", data.size());

	auto read=[&](void* dst, size_t len)->bool
	{
		if(pos+len > data.size()) return false;
		memcpy(dst, &data[pos], len);
		pos+=len;
		return true;
	};

	// timestamp
	if(!read(&entry._timestamp, sizeof(entry._timestamp))) return false;
//TRACEF("Read %lu byte timestamp: %f", sizeof(entry._timestamp), entry._timestamp);

	// hops
	if(!read(&entry._hops, sizeof(entry._hops))) return false;
//TRACEF("Read %lu byte hops: %u", sizeof(entry._hops), entry._hops);

	// expires
	if(!read(&entry._expires, sizeof(entry._expires))) return false;
//TRACEF("Read %lu byte expires: %f", sizeof(entry._expires), entry._expires);

	// received_from
	if(!read((void*)entry._received_from.writable(Type::Reticulum::DESTINATION_LENGTH), Type::Reticulum::DESTINATION_LENGTH)) return false;
	entry._received_from.resize(Type::Reticulum::DESTINATION_LENGTH);
//TRACEF("Read %lu byte received_from", entry._received_from.size());

	// random_blobs
	uint16_t blob_count;
	if(!read(&blob_count, sizeof(blob_count))) return false;
//TRACEF("Read %lu byte blob_count: %u", sizeof(blob_count), blob_count);
	for (int i = 0; i < blob_count; i++) {
		uint16_t blob_size;
		if(!read(&blob_size, sizeof(blob_size))) return false;
//TRACEF("Read %lu byte blob_size: %u", sizeof(blob_size), blob_size);
		Bytes blob(blob_size);
		if(!read((void*)blob.writable(blob_size), blob_size)) return false;
		blob.resize(blob_size);
		entry._random_blobs.insert(blob);
//TRACEF("Read %lu byte blob", blob.size());
	}

	// receiving_interface
	Bytes interface_hash(Type::Reticulum::HASHLENGTH/8);
	if(!read((void*)interface_hash.writable(Type::Reticulum::HASHLENGTH/8), Type::Reticulum::HASHLENGTH/8)) return false;
	interface_hash.resize(Type::Reticulum::HASHLENGTH/8);
//TRACEF("Read %lu byte interface_hash", interface_hash.size());
	entry._receiving_interface = Transport::find_interface_from_hash(interface_hash);
	if (!entry._receiving_interface) {
		WARNINGF("Path Interface %s not found", interface_hash.toHex().c_str());
	}

	// announce_packet
	uint16_t packet_size;
	if(!read(&packet_size, sizeof(packet_size))) return false;
//TRACEF("Read %lu byte packet_size: %u", sizeof(packet_size), packet_size);
	Bytes packet_data(packet_size);
	if(!read((void*)packet_data.writable(packet_size), packet_size)) return false;
	packet_data.resize(packet_size);
	if (packet_data.size() > 0) {
		entry._announce_packet = Packet(packet_data);
	}
//TRACEF("Read %lu byte packet", packet_data.size());

	if (entry._announce_packet) {
		// Announce packet is cached in packed state
		// so we need to unpack it before accessing.
//TRACE("Unpacking packet...");
		if (entry._announce_packet.unpack()) {
//TRACEF("Packet: %s", entry._announce_packet.debugString().c_str());
			// We increase the hops, since reading a packet
			// from cache is equivalent to receiving it again
			// over an interface. It is cached with it's non-
			// increased hop-count.
//TRACE("Incrementing packet hop count...");
			entry._announce_packet.hops(entry._announce_packet.hops() + 1);
		}
	}

	return true;
}
