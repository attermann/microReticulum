#pragma once

#include "Interface.h"
#include "Packet.h"
#include "Bytes.h"

// CBA microStore
#include <microStore/FileStore.h>
#include <microStore/TypedStore.h>
#include <microStore/Codec.h>

#include <set>
#include <map>

namespace RNS { namespace Persistence {

class DestinationEntry {
public:
	DestinationEntry() {}
	DestinationEntry(double timestamp, const RNS::Bytes& received_from, uint8_t announce_hops, double expires, const std::set<RNS::Bytes>& random_blobs, const Interface& receiving_interface, const Packet& announce_packet) :
		_timestamp(timestamp),
		_received_from(received_from),
		_hops(announce_hops),
		_expires(expires),
		_random_blobs(random_blobs),
		_receiving_interface(receiving_interface),
		_announce_packet(announce_packet)
	{
	}
	inline operator bool() const {
		return (_receiving_interface && _announce_packet);
	}
	inline bool operator < (const DestinationEntry& entry) const {
		// sort by ascending timestamp (oldest entries at the top)
		return _timestamp < entry._timestamp;
	}
public:
	inline Interface& receiving_interface() { return _receiving_interface; }
	inline Packet& announce_packet() { return _announce_packet; }
	// Getters and setters for receiving_interface_hash and announce_packet_hash
	inline RNS::Bytes receiving_interface_hash() const { if (_receiving_interface) return _receiving_interface.get_hash(); return {RNS::Type::NONE}; }
	inline RNS::Bytes announce_packet_hash() const { if (_announce_packet) return _announce_packet.get_hash(); return {RNS::Type::NONE}; }
public:
	double _timestamp = 0;
	RNS::Bytes _received_from;
	uint8_t _hops = 0;
	double _expires = 0;
	std::set<RNS::Bytes> _random_blobs;
	Interface _receiving_interface = {Type::NONE};
	Packet _announce_packet = {Type::NONE};
public:
#ifndef NDEBUG
	inline std::string debugString() const {
		std::string dump;
		dump = "DestinationEntry: timestamp=" + std::to_string(_timestamp) +
			" received_from=" + _received_from.toHex() +
			" hops=" + std::to_string(_hops) +
			" expires=" + std::to_string(_expires) +
			//" random_blobs=" + _random_blobs +
			" receiving_interface=" + receiving_interface_hash().toHex() +
			" announce_packet=" + announce_packet_hash().toHex();
		dump += " random_blobs=(";
		for (auto& blob : _random_blobs) {
			dump += blob.toHex() + ",";
		}
		dump += ")";
		return dump;
	}
#endif
};
//using PathTable = std::map<RNS::Bytes, DestinationEntry>;
using PathTable = std::map<RNS::Bytes, DestinationEntry, std::less<RNS::Bytes>, Utilities::Memory::ContainerAllocator<std::pair<const RNS::Bytes, DestinationEntry>>>;

//using PathStore = microStore::FileStore;
//using NewPathTable = microStore::TypedStore<Bytes, DestinationEntry, PathStore>;
using PathStore = microStore::BasicFileStore<Utilities::Memory::ContainerAllocator<uint8_t>>;
using NewPathTable = microStore::TypedStore<Bytes, DestinationEntry, PathStore>;

} }

namespace microStore {
template<>
struct Codec<RNS::Persistence::DestinationEntry>
{
	static std::vector<uint8_t> encode(const RNS::Persistence::DestinationEntry& entry);
	static bool decode(const std::vector<uint8_t>& data, RNS::Persistence::DestinationEntry& entry);
};
}
