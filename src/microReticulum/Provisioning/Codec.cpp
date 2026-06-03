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

#include "Codec.h"

#include "../Log.h"

namespace RNS { namespace Provisioning { namespace Codec {

	bool pack_value(MsgPack::Packer& packer, const Value& v) {
		switch (v.type()) {
			case Type::None:
				return false;
			case Type::Bool:
				packer.serialize((fbool_t)v.as_bool());
				return true;
			case Type::Int:
			case Type::Enum:
				packer.serialize((fint_t)v.as_int());
				return true;
			case Type::Float:
				packer.serialize((ffloat_t)v.as_float());
				return true;
			case Type::String:
				packer.serialize(v.as_string().c_str());
				return true;
			case Type::Bytes: {
				const fbytes_t& b = v.as_bytes();
				MsgPack::bin_t<uint8_t> bin;
				if (b.size() > 0) {
					bin.resize(b.size());
					memcpy(bin.data(), b.data(), b.size());
				}
				packer.serialize(bin);
				return true;
			}
			case Type::BytesList: {
				const auto& list = v.as_bytes_list();
				packer.serialize(MsgPack::arr_size_t(list.size()));
				for (const fbytes_t& e : list) {
					MsgPack::bin_t<uint8_t> bin;
					if (e.size() > 0) {
						bin.resize(e.size());
						memcpy(bin.data(), e.data(), e.size());
					}
					packer.serialize(bin);
				}
				return true;
			}
			case Type::Void: {
				// Argument-less command marker — pack nil so the wire entry
				// has a value slot (MsgPack maps require value pairs). Never
				// hit in practice because Void is always FF_WRITE_ONLY and
				// neither persisted nor surfaced in GET_STATE.
				MsgPack::object::nil_t nil;
				packer.serialize(nil);
				return true;
			}
		}
		return false;
	}

	bool unpack_value(MsgPack::Unpacker& unpacker, Type declared, Value& out) {
		switch (declared) {
			case Type::None:
				return false;
			case Type::Bool: {
				if (!unpacker.isBool()) return false;
				fbool_t b = false;
				unpacker.deserialize(b);
				out = Value(b);
				return true;
			}
			case Type::Int:
			case Type::Enum: {
				if (!unpacker.isInt() && !unpacker.isUInt()) return false;
				fint_t iv = 0;
				unpacker.deserialize(iv);
				out = Value::from_int_as(declared, iv);
				return true;
			}
			case Type::Float: {
				// Floats may arrive as int on wire; accept either.
				if (unpacker.isFloat32() || unpacker.isFloat64()
					|| unpacker.isInt() || unpacker.isUInt()) {
					ffloat_t d = 0.0;
					unpacker.deserialize(d);
					out = Value(d);
					return true;
				}
				return false;
			}
			case Type::String: {
				if (!unpacker.isStr()) return false;
				MsgPack::str_t s;
				unpacker.deserialize(s);
				out = Value(to_std_string(s));
				return true;
			}
			case Type::Bytes: {
				if (!unpacker.isBin()) return false;
				MsgPack::bin_t<uint8_t> bin;
				unpacker.deserialize(bin);
				fbytes_t b(bin.data(), bin.size());
				out = Value(b);
				return true;
			}
			case Type::BytesList: {
				if (!unpacker.isArray()) return false;
				const size_t n = unpacker.unpackArraySize();
				fbytes_list_t list;
				list.reserve(n);
				for (size_t i = 0; i < n; ++i) {
					if (!unpacker.isBin()) {
						// Bad element type — skip and abort the read.
						return false;
					}
					MsgPack::bin_t<uint8_t> bin;
					unpacker.deserialize(bin);
					list.emplace_back(bin.data(), bin.size());
				}
				out = Value(std::move(list));
				return true;
			}
			case Type::Void: {
				// Argument-less command — accept nil on the wire (canonical
				// form) and discard the value. The presence of the field id
				// in the SET_STATE map is itself the trigger.
				if (!unpacker.isNil()) return false;
				MsgPack::object::nil_t nil;
				unpacker.deserialize(nil);
				out = Value::make_void();
				return true;
			}
		}
		return false;
	}

	// Pick the value to persist for a field. The choice depends on the
	// field's apply semantics:
	//
	//  - FF_LIVE_APPLY: prefer effective() (live runtime via getter, else
	//    working). Captures direct-setter mutations on commit and keeps
	//    persistence in lockstep with what's actually running.
	//
	//  - FF_REBOOT_REQUIRED: must use working(). After commit_one(), the
	//    new value lives in the working map but the runtime still holds
	//    the *old* value (setter intentionally not fired). Saving via
	//    effective() would persist the stale runtime — the opposite of
	//    what the user just asked for.
	//
	//  - Neither flag set: behave like LIVE_APPLY (use effective).
	static Value persist_value(const Namespace& ns, const Field& f) {
		if (f.has_flag(FF_REBOOT_REQUIRED)) return ns.working(f.id);
		return ns.effective(f.id);
	}

	bool pack_namespace_working(MsgPack::Packer& packer, const Namespace& ns) {
		// Skip read-only (no state to persist beyond defaults) and write-only
		// (commands are one-shot; replaying on reboot would be wrong).
		auto skip = [](const Field& f) {
			return f.has_flag(FF_READ_ONLY) || f.has_flag(FF_WRITE_ONLY);
		};
		size_t n = 0;
		for (const Field& f : ns.fields()) {
			if (skip(f)) continue;
			Value v = persist_value(ns, f);
			if (v.is_none()) continue;
			++n;
		}
		packer.serialize(MsgPack::map_size_t(n));
		for (const Field& f : ns.fields()) {
			if (skip(f)) continue;
			Value v = persist_value(ns, f);
			if (v.is_none()) continue;
			packer.serialize((fid_t)f.id);
			pack_value(packer, v);
		}
		return true;
	}

	// Skip whatever value the cursor points at. Hideakitai MsgPack doesn't
	// expose a generic skip(); we deserialize into a throwaway typed local.
	static void skip_value(MsgPack::Unpacker& u) {
		if (u.isNil())                            { MsgPack::object::nil_t n; u.deserialize(n); }
		else if (u.isBool())                      { bool b; u.deserialize(b); }
		else if (u.isUInt() || u.isInt())         { fint_t i; u.deserialize(i); }
		else if (u.isFloat32() || u.isFloat64())  { double d; u.deserialize(d); }
		else if (u.isStr())                       { MsgPack::str_t s; u.deserialize(s); }
		else if (u.isBin())                       { MsgPack::bin_t<uint8_t> b; u.deserialize(b); }
		else if (u.isArray()) {
			const size_t n = u.unpackArraySize();
			for (size_t i = 0; i < n; ++i) skip_value(u);
		}
		else if (u.isMap()) {
			const size_t n = u.unpackMapSize();
			for (size_t i = 0; i < n; ++i) { skip_value(u); skip_value(u); }
		}
		else {
			WARNING("Codec::skip_value: unknown MsgPack type encountered");
		}
	}

	bool unpack_namespace_working(MsgPack::Unpacker& unpacker, Namespace& ns) {
		if (!unpacker.isMap()) return false;
		const size_t n = unpacker.unpackMapSize();
		for (size_t i = 0; i < n; ++i) {
			if (!(unpacker.isUInt() || unpacker.isInt())) {
				// Map key must be an integer field id; skip both halves and
				// keep going (forward-compat with future key types).
				skip_value(unpacker);
				skip_value(unpacker);
				continue;
			}
			fint_t key = 0;
			unpacker.deserialize(key);
			fid_t field_id = (fid_t)key;
			const Field* f = ns.find_field(field_id);
			if (!f) {
				skip_value(unpacker);
				continue;
			}
			Value v;
			if (!unpack_value(unpacker, f->type, v)) {
				// Type mismatch — skip but don't fail the whole file.
				continue;
			}
			if (!f->validate(v)) {
				// Out-of-range or otherwise invalid — leave default in place.
				continue;
			}
			ns.put_working(field_id, v);
		}
		return true;
	}

} } }
