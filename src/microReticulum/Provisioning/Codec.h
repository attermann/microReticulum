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

#include "Value.h"
#include "Field.h"
#include "Namespace.h"

#define MSGPACK_DEBUGLOG_ENABLE 0
#include <MsgPack.h>

namespace RNS { namespace Provisioning {

	// Low-level MsgPack helpers shared by Storage (on-disk) and Provisioning
	// (wire). Keep all type-dispatch logic here so the two callers don't
	// drift apart.
	namespace Codec {

		// Portable str_t -> std::string conversion. On native MsgPack::str_t
		// IS std::string; on Arduino targets it's Arduino::String. Both have
		// c_str()/length(), so this works without #ifdef.
		inline std::string to_std_string(const MsgPack::str_t& s) {
			return std::string(s.c_str(), s.length());
		}

		// Pack a Value as MsgPack at the cursor. The Value's declared type
		// determines the wire encoding; bool/int/enum all collapse to int.
		// Returns false if the value carries Type::None.
		bool pack_value(MsgPack::Packer& packer, const Value& v);

		// Read the next MsgPack element into a Value coerced to `declared`.
		// Mismatched types (e.g. wire says str but field declares int) fail.
		// Unknown / nil values produce Type::None.
		bool unpack_value(MsgPack::Unpacker& unpacker, Type declared, Value& out);

		// Serialize the persistable subset of a namespace's working map
		// (excludes FF_READ_ONLY fields, which are never written to flash).
		// Output is a single MsgPack map: { field_id: value, ... }.
		bool pack_namespace_working(MsgPack::Packer& packer, const Namespace& ns);

		// Inverse of pack_namespace_working. Validates each value against
		// the field's constraint; invalid values are skipped silently
		// (forward-compat with renamed fields whose types changed).
		// Unknown field ids are also skipped silently.
		bool unpack_namespace_working(MsgPack::Unpacker& unpacker, Namespace& ns);

	}

} }
