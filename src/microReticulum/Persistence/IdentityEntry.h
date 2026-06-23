/*
 * Copyright (c) 2026 Chad Attermann
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

#include "../Bytes.h"
#include "../Type.h"
#include "../Utilities/Memory.h"

#if defined(RNS_USE_FS) && RNS_PERSIST_KNOWN_DESTINATIONS
#include <microStore/FileStore.h>
#else
#include <microStore/HeapStore.h>
#endif
#include <microStore/TypedStore.h>
#include <microStore/Codec.h>

namespace RNS { namespace Persistence {

class IdentityEntry {
public:
	IdentityEntry() {}
	IdentityEntry(double timestamp, const RNS::Bytes& packet_hash, const RNS::Bytes& public_key, const RNS::Bytes& app_data) :
		_timestamp(timestamp),
		_packet_hash(packet_hash),
		_public_key(public_key),
		_app_data(app_data)
	{
	}
	inline explicit operator bool() const {
		// Treat presence of a public key as the validity test — callers that today
		// compare find() against end() are really asking "do we have keys for this".
		return _public_key.size() > 0;
	}
	inline bool operator < (const IdentityEntry& entry) const {
		return _timestamp < entry._timestamp;
	}
public:
	double _timestamp = 0;
	RNS::Bytes _packet_hash;
	RNS::Bytes _public_key;
	RNS::Bytes _app_data;
public:
#ifndef NDEBUG
	inline std::string debugString() const {
		return "IdentityEntry: timestamp=" + std::to_string(_timestamp) +
			" packet_hash=" + _packet_hash.toHex() +
			" public_key=" + _public_key.toHex() +
			" app_data=" + _app_data.toHex();
	}
#endif
};

#if defined(RNS_USE_FS) && RNS_PERSIST_KNOWN_DESTINATIONS
using KnownStore = microStore::BasicFileStore<Utilities::Memory::ContainerAllocator<uint8_t>>;
#else
using KnownStore = microStore::BasicHeapStore<Utilities::Memory::ContainerAllocator<uint8_t>>;
#endif
using KnownDestinations = microStore::TypedStore<Bytes, IdentityEntry, KnownStore>;

} }

namespace microStore {
template<>
struct Codec<RNS::Persistence::IdentityEntry>
{
	static std::vector<uint8_t> encode(const RNS::Persistence::IdentityEntry& entry);
	static bool decode(const std::vector<uint8_t>& data, RNS::Persistence::IdentityEntry& entry);
};
}
