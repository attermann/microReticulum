#include "DestinationEntry.h"

#include "Transport.h"
#include "Type.h"

using namespace RNS;
using namespace RNS::Persistence;

/*static*/ std::vector<uint8_t> DestinationEntryCodec::encode(const DestinationEntry& entry) {

	// If invlaid/empty entry then return empty
	if (!entry) return {};

	std::vector<uint8_t> out;

	auto append = [&](const void* ptr, size_t len)
	{
		const uint8_t* p=(const uint8_t*)ptr;
		out.insert(out.end(), p, p+len);
	};

	// timestamp
//TRACEF("Adding %zu byte timestamp: %f", sizeof(entry._timestamp), entry._timestamp);
	append(&entry._timestamp, sizeof(entry._timestamp));

	// hops
//TRACEF("Adding %zu byte hops: %u", sizeof(entry._hops), entry._hops);
	append(&entry._hops, sizeof(entry._hops));

	// expires
//TRACEF("Adding %zu byte expires: %f", sizeof(entry._expires), entry._expires);
	append(&entry._expires, sizeof(entry._expires));

	// received_from
//TRACEF("Adding %zu byte received_from", entry._received_from.collection().size());
	out.insert(out.end(), entry._received_from.collection().begin(), entry._received_from.collection().end());

	// random_blobs
	uint16_t blob_count = entry._random_blobs.size();
//TRACEF("Adding %zu byte blob_count: %u", sizeof(blob_count), blob_count);
	append(&blob_count, sizeof(blob_count));
	for (auto& blob : entry._random_blobs) {
		uint16_t blob_size = blob.collection().size();
//TRACEF("Adding %zu byte blob_size: %u", sizeof(blob_size), blob_size);
		append(&blob_size, sizeof(blob_size));
//TRACEF("Adding %zu byte blob", blob.collection().size());
		out.insert(out.end(), blob.collection().begin(), blob.collection().end());
	}

	// receiving_interface
	Bytes interface_hash(entry._receiving_interface.get_hash());
//TRACEF("Adding %zu byte receiving_interface hash", interface_hash.collection().size());
	out.insert(out.end(), interface_hash.collection().begin(), interface_hash.collection().end());

	// announce_packet
	uint16_t packet_size = entry._announce_packet.raw().size();
//TRACEF("Adding %zu byte packet_size: %u", sizeof(packet_size), packet_size);
	append(&packet_size, sizeof(packet_size));
//TRACEF("Adding %zu byte packet", entry._announce_packet.raw().collection().size());
	out.insert(out.end(), entry._announce_packet.raw().collection().begin(), entry._announce_packet.raw().collection().end());

//TRACEF("Encoded %zu byte DestinationEntry", out.size());

	return out;
}

/*static*/ bool DestinationEntryCodec::decode(const std::vector<uint8_t>& data, DestinationEntry& entry)
{
	size_t pos = 0;

//TRACEF("Decoding %zu byte DestinationEntry", data.size());

	auto read=[&](void* dst, size_t len)->bool
	{
		if(pos+len > data.size()) return false;
		memcpy(dst, &data[pos], len);
		pos+=len;
		return true;
	};

	// timestamp
	if(!read(&entry._timestamp, sizeof(entry._timestamp))) return false;
//TRACEF("Read %zu byte timestamp: %f", sizeof(entry._timestamp), entry._timestamp);

	// hops
	if(!read(&entry._hops, sizeof(entry._hops))) return false;
//TRACEF("Read %zu byte hops: %u", sizeof(entry._hops), entry._hops);

	// expires
	if(!read(&entry._expires, sizeof(entry._expires))) return false;
//TRACEF("Read %zu byte expires: %f", sizeof(entry._expires), entry._expires);

	// received_from
	if(!read((void*)entry._received_from.writable(Type::Reticulum::DESTINATION_LENGTH), Type::Reticulum::DESTINATION_LENGTH)) return false;
	entry._received_from.resize(Type::Reticulum::DESTINATION_LENGTH);
//TRACEF("Read %zu byte received_from", entry._received_from.size());

	// random_blobs
	uint16_t blob_count;
	if(!read(&blob_count, sizeof(blob_count))) return false;
//TRACEF("Read %zu byte blob_count: %u", sizeof(blob_count), blob_count);
	for (int i = 0; i < blob_count; i++) {
		uint16_t blob_size;
		if(!read(&blob_size, sizeof(blob_size))) return false;
//TRACEF("Read %zu byte blob_size: %u", sizeof(blob_size), blob_size);
		Bytes blob(blob_size);
		if(!read((void*)blob.writable(blob_size), blob_size)) return false;
		blob.resize(blob_size);
		entry._random_blobs.insert(blob);
//TRACEF("Read %zu byte blob", blob.size());
	}

	// receiving_interface
	Bytes interface_hash(Type::Reticulum::HASHLENGTH/8);
	if(!read((void*)interface_hash.writable(Type::Reticulum::HASHLENGTH/8), Type::Reticulum::HASHLENGTH/8)) return false;
	interface_hash.resize(Type::Reticulum::HASHLENGTH/8);
//TRACEF("Read %zu byte interface_hash", interface_hash.size());
	entry._receiving_interface = Transport::find_interface_from_hash(interface_hash);
	if (!entry._receiving_interface) {
		WARNINGF("Path Interface %s not found", interface_hash.toHex().c_str());
	}

	// announce_packet
	uint16_t packet_size;
	if(!read(&packet_size, sizeof(packet_size))) return false;
//TRACEF("Read %zu byte packet_size: %u", sizeof(packet_size), packet_size);
	Bytes packet_data(packet_size);
	if(!read((void*)packet_data.writable(packet_size), packet_size)) return false;
	packet_data.resize(packet_size);
	entry._announce_packet = Packet(packet_data);
//TRACEF("Read %zu byte packet", packet_data.size());

	if (entry._announce_packet) {
		// Announce packet is cached in packed state
		// so we need to unpack it before accessing.
//TRACE("Unpacking packet...");
		entry._announce_packet.unpack();
//TRACEF("Packet: %s", entry._announce_packet.debugString().c_str());
		// We increase the hops, since reading a packet
		// from cache is equivalent to receiving it again
		// over an interface. It is cached with it's non-
		// increased hop-count.
//TRACE("Incrementing packet hop count...");
		entry._announce_packet.hops(entry._announce_packet.hops() + 1);
	}

	return true;
}
