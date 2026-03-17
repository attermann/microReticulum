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
//TRACEF("Adding %zu byte timestamp...", sizeof(entry._timestamp));
	append(&entry._timestamp, sizeof(entry._timestamp));

	// hops
//TRACEF("Adding %zu byte hops...", sizeof(entry._hops));
	append(&entry._hops, sizeof(entry._hops));

	// expires
//TRACEF("Adding %zu byte expires...", sizeof(entry._expires));
	append(&entry._expires, sizeof(entry._expires));

	// received_from
//TRACEF("Adding %zu byte received_from...", entry._received_from.collection().size());
	out.insert(out.end(), entry._received_from.collection().begin(), entry._received_from.collection().end());

	// random_blobs
	uint16_t blob_count = entry._random_blobs.size();
//TRACEF("Adding %zu byte blob_count...", sizeof(blob_count));
	append(&blob_count, sizeof(blob_count));
	for (auto& blob : entry._random_blobs) {
		uint16_t blob_size = blob.collection().size();
//TRACEF("Adding %zu byte blob_size...", sizeof(blob_size));
		append(&blob_size, sizeof(blob_size));
//TRACEF("Adding %zu byte blob...", blob.collection().size());
		out.insert(out.end(), blob.collection().begin(), blob.collection().end());
	}

	// receiving_interface
	Bytes interface_hash(entry._receiving_interface.get_hash());
//TRACEF("Adding %zu byte receiving_interface hash...", interface_hash.collection().size());
	out.insert(out.end(), interface_hash.collection().begin(), interface_hash.collection().end());

	// announce_packet
	uint16_t packet_size = entry._announce_packet.raw().size();
//TRACEF("Adding %zu byte packet_size...", sizeof(packet_size));
	append(&packet_size, sizeof(packet_size));
//TRACEF("Adding %zu byte packet...", entry._announce_packet.raw().collection().size());
	out.insert(out.end(), entry._announce_packet.raw().collection().begin(), entry._announce_packet.raw().collection().end());

TRACEF("Encoded %zu byte DestinationEntry", out.size());

/*
	append(&entry.nextHop, sizeof(entry.nextHop));
	append(&entry.cost, sizeof(entry.cost));

	uint32_t nameLen=entry.interfaceName.size();
	append(&nameLen, sizeof(nameLen));
	append(entry.interfaceName.data(), nameLen);

	uint32_t metaLen=entry.metadata.size();
	append(&metaLen, sizeof(metaLen));

	if(metaLen)
		append(entry.metadata.data(), metaLen);
*/

	return out;
}

/*static*/ bool DestinationEntryCodec::decode(const std::vector<uint8_t>& data, DestinationEntry& entry)
{
	size_t pos = 0;

TRACEF("Decoding %zu byte DestinationEntry", data.size());

	auto read=[&](void* dst, size_t len)->bool
	{
		if(pos+len > data.size()) return false;
		memcpy(dst, &data[pos], len);
		pos+=len;
		return true;
	};

	// timestamp
	if(!read(&entry._timestamp, sizeof(entry._timestamp))) return false;

	// hops
	if(!read(&entry._hops, sizeof(entry._hops))) return false;

	// expires
	if(!read(&entry._expires, sizeof(entry._expires))) return false;

	// received_from
	if(!read((void*)entry._received_from.writable(Type::Reticulum::DESTINATION_LENGTH), Type::Reticulum::DESTINATION_LENGTH)) return false;
	entry._received_from.resize(Type::Reticulum::DESTINATION_LENGTH);

	// random_blobs
	uint16_t blob_count;
	if(!read(&blob_count, sizeof(blob_count))) return false;
	for (int i = 0; i < blob_count; i++) {
		uint16_t blob_size;
		if(!read(&blob_size, sizeof(blob_size))) return false;
		Bytes blob(blob_size);
		if(!read((void*)blob.writable(blob_size), blob_size)) return false;
		blob.resize(blob_size);
		entry._random_blobs.insert(blob);
	}

	// receiving_interface
	Bytes interface_hash(Type::Reticulum::HASHLENGTH);
	if(!read((void*)interface_hash.writable(Type::Reticulum::HASHLENGTH), Type::Reticulum::HASHLENGTH)) return false;
	interface_hash.resize(Type::Reticulum::HASHLENGTH);
	entry._receiving_interface = Transport::find_interface_from_hash(interface_hash);

	// announce_packet
	uint16_t packet_size;
	if(!read(&packet_size, sizeof(packet_size))) return false;
	Bytes packet_data(packet_size);
	if(!read((void*)packet_data.writable(packet_size), packet_size)) return false;
	packet_data.resize(packet_size);
	entry._announce_packet = Packet(packet_data);

/*
	if(!read(&entry.nextHop, sizeof(entry.nextHop))) return false;
	if(!read(&entry.cost, sizeof(entry.cost))) return false;

	uint32_t nameLen;
	if(!read(&nameLen, sizeof(nameLen))) return false;

	if(pos+nameLen>data.size()) return false;

	entry.interfaceName.assign((char*)&data[pos], nameLen);
	pos+=nameLen;

	uint32_t metaLen;
	if(!read(&metaLen, sizeof(metaLen))) return false;

	if(pos+metaLen>data.size()) return false;

	entry.metadata.assign(data.begin()+pos, data.begin()+pos+metaLen);
*/

	if (entry._announce_packet) {
		// Announce packet is cached in packed state
		// so we need to unpack it before accessing.
		entry._announce_packet.unpack();
		// We increase the hops, since reading a packet
		// from cache is equivalent to receiving it again
		// over an interface. It is cached with it's non-
		// increased hop-count.
		entry._announce_packet.hops(entry._announce_packet.hops() + 1);
	}

	return true;
}
