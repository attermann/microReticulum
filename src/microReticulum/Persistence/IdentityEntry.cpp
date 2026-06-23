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

#include "IdentityEntry.h"

#include <MsgPack.h>

using namespace RNS;
using namespace RNS::Persistence;

// Encodes an IdentityEntry as a 4-element MsgPack array:
//   [timestamp, packet_hash, public_key, app_data]
//
// New fields are appended by bumping the array length; old decoders see the
// extra elements as trailing array members they can ignore.
/*static*/ std::vector<uint8_t> microStore::Codec<IdentityEntry>::encode(const IdentityEntry& entry) {

	if (!entry) return {};

	MsgPack::Packer p;
	p.packArraySize(4);

	// timestamp
	p.packFloat64(entry._timestamp);

	// packet_hash
	p.packBinary(entry._packet_hash.data(), entry._packet_hash.size());

	// public_key
	p.packBinary(entry._public_key.data(), entry._public_key.size());

	// app_data
	p.packBinary(entry._app_data.data(), entry._app_data.size());

	return std::vector<uint8_t>(p.data(), p.data() + p.size());
}

/*static*/ bool microStore::Codec<IdentityEntry>::decode(const std::vector<uint8_t>& data, IdentityEntry& entry) {
	if (data.empty()) return false;

	MsgPack::Unpacker u;
	u.feed(data.data(), data.size());

	if (!u.isArray()) return false;
	const size_t n = u.unpackArraySize();
	if (n < 4) return false;

	// timestamp
	if (!u.deserialize(entry._timestamp)) return false;

	// packet_hash
	{
		MsgPack::bin_t<uint8_t> b;
		if (!u.deserialize(b)) return false;
		entry._packet_hash = Bytes(b.data(), b.size());
	}

	// public_key
	{
		MsgPack::bin_t<uint8_t> b;
		if (!u.deserialize(b)) return false;
		entry._public_key = Bytes(b.data(), b.size());
	}

	// app_data
	{
		MsgPack::bin_t<uint8_t> b;
		if (!u.deserialize(b)) return false;
		entry._app_data = Bytes(b.data(), b.size());
	}

	return true;
}
