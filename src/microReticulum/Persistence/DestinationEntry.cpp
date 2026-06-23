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

#include "../Transport.h"

#include <MsgPack.h>

using namespace RNS;
using namespace RNS::Persistence;

// Encodes a DestinationEntry as a 7-element MsgPack array:
//   [timestamp, hops, expires, received_from, random_blobs, receiving_interface, announce_packet]
//
// random_blobs is itself an array of length-prefixed binary blobs, tail-trimmed
// to PERSIST_RANDOM_BLOBS to bound on-disk size. Adding new fields in the future
// is a matter of bumping the outer array length and appending; old decoders see
// the extra elements as trailing array members they can ignore.
/*static*/ std::vector<uint8_t> microStore::Codec<DestinationEntry>::encode(const DestinationEntry& entry) {

	// If invalid/empty entry then return empty
	if (!entry) return {};

	MsgPack::Packer p;
	p.packArraySize(7);

	// timestamp
	p.packFloat64(entry._timestamp);

	// hops
	p.serialize(entry._hops);

	// expires
	p.packFloat64(entry._expires);

	// received_from
	p.packBinary(entry._received_from.data(), entry._received_from.size());

	// random_blobs -- write only the tail-newest PERSIST_RANDOM_BLOBS entries
	const size_t persist_cap = Type::Transport::PERSIST_RANDOM_BLOBS;
	const size_t total_blobs = entry._random_blobs.size();
	const size_t persist_n   = (total_blobs > persist_cap) ? persist_cap : total_blobs;
	const size_t start_idx   = total_blobs - persist_n;
	p.packArraySize(persist_n);
	for (size_t i = start_idx; i < total_blobs; i++) {
		const auto& blob = entry._random_blobs[i];
		p.packBinary(blob.data(), blob.size());
	}

	// receiving_interface (hash only; the live Interface is re-bound on decode)
	Bytes interface_hash(entry._receiving_interface.get_hash());
	p.packBinary(interface_hash.data(), interface_hash.size());

	// announce_packet (raw bytes, including header)
	const Bytes& raw = entry._announce_packet.raw();
	p.packBinary(raw.data(), raw.size());

	return std::vector<uint8_t>(p.data(), p.data() + p.size());
}

/*static*/ bool microStore::Codec<DestinationEntry>::decode(const std::vector<uint8_t>& data, DestinationEntry& entry)
{
	if (data.empty()) return false;

	MsgPack::Unpacker u;
	u.feed(data.data(), data.size());

	if (!u.isArray()) return false;
	const size_t n = u.unpackArraySize();
	if (n < 7) return false;

	// timestamp
	if (!u.deserialize(entry._timestamp)) return false;

	// hops
	if (!u.deserialize(entry._hops)) return false;

	// expires
	if (!u.deserialize(entry._expires)) return false;

	// received_from
	{
		MsgPack::bin_t<uint8_t> b;
		if (!u.deserialize(b)) return false;
		entry._received_from = Bytes(b.data(), b.size());
	}

	// random_blobs
	{
		if (!u.isArray()) return false;
		const size_t blob_n = u.unpackArraySize();
		entry._random_blobs.clear();
		entry._random_blobs.reserve(blob_n);
		for (size_t i = 0; i < blob_n; i++) {
			MsgPack::bin_t<uint8_t> b;
			if (!u.deserialize(b)) return false;
			entry._random_blobs.push_back(Bytes(b.data(), b.size()));
		}
	}

	// receiving_interface (rebind to live Interface by hash)
	{
		MsgPack::bin_t<uint8_t> b;
		if (!u.deserialize(b)) return false;
		Bytes interface_hash(b.data(), b.size());
		entry._receiving_interface = Transport::find_interface_from_hash(interface_hash);
		if (!entry._receiving_interface) {
			WARNINGF("Path Interface %s not found", interface_hash.toHex().c_str());
		}
	}

	// announce_packet
	{
		MsgPack::bin_t<uint8_t> b;
		if (!u.deserialize(b)) return false;
		Bytes packet_data(b.data(), b.size());
		if (packet_data.size() > 0) {
			entry._announce_packet = Packet(packet_data);
		}
	}

	if (entry._announce_packet) {
		// Announce packet is cached in packed state
		// so we need to unpack it before accessing.
		if (entry._announce_packet.unpack()) {
			// We increase the hops, since reading a packet
			// from cache is equivalent to receiving it again
			// over an interface. It is cached with its non-
			// increased hop-count.
			entry._announce_packet.hops(entry._announce_packet.hops() + 1);
		}
	}

	return true;
}
