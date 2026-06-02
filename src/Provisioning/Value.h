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

#include <stdint.h>
#include <string>
#include <vector>

namespace RNS { namespace Provisioning {

	// Field/value types aligned with MsgPack wire types.
	enum class Type : uint8_t {
		None      = 0,
		Bool      = 1,
		Int       = 2,
		Float     = 3,
		String    = 4,
		Bytes     = 5,
		Enum      = 6,	// integer on the wire; schema carries labels
		BytesList = 7,	// MsgPack array of bin; for sets/lists of hashes
	};

	class Value {

	public:
		Value() : _type(Type::None), _i(0), _d(0.0) {}
		Value(bool v) : _type(Type::Bool), _i(v ? 1 : 0), _d(0.0) {}
		Value(int v) : _type(Type::Int), _i(v), _d(0.0) {}
		Value(unsigned int v) : _type(Type::Int), _i((int64_t)v), _d(0.0) {}
		Value(long v) : _type(Type::Int), _i((int64_t)v), _d(0.0) {}
		Value(unsigned long v) : _type(Type::Int), _i((int64_t)v), _d(0.0) {}
		Value(long long v) : _type(Type::Int), _i((int64_t)v), _d(0.0) {}
		Value(unsigned long long v) : _type(Type::Int), _i((int64_t)v), _d(0.0) {}
		Value(double v) : _type(Type::Float), _i(0), _d(v) {}
		Value(float v) : _type(Type::Float), _i(0), _d((double)v) {}
		Value(const char* s) : _type(Type::String), _i(0), _d(0.0), _s(s ? s : "") {}
		Value(const std::string& s) : _type(Type::String), _i(0), _d(0.0), _s(s) {}
		Value(const Bytes& b) : _type(Type::Bytes), _i(0), _d(0.0), _b(b) {}
		Value(std::vector<Bytes> list) : _type(Type::BytesList), _i(0), _d(0.0), _bl(std::move(list)) {}

		// Enum is distinguished from Int only in schema; on the wire and in
		// storage it's a positive int. Use this factory when a field is declared
		// as Type::Enum so consumers know to look up labels.
		static Value make_enum(int64_t v) {
			Value out;
			out._type = Type::Enum;
			out._i = v;
			return out;
		}

		// Adopt an existing wire-side integer as the given declared type
		// (used by the wire codec when decoding into a field whose declared
		// type may be Bool / Enum / Int — all of which carry an int).
		static Value from_int_as(Type declared_type, int64_t v) {
			Value out;
			out._type = declared_type;
			out._i = v;
			return out;
		}

		Type type() const { return _type; }
		bool is_none() const { return _type == Type::None; }

		bool as_bool() const { return _i != 0; }
		int64_t as_int() const { return _i; }
		double as_float() const { return _d; }
		const std::string& as_string() const { return _s; }
		const Bytes& as_bytes() const { return _b; }
		const std::vector<Bytes>& as_bytes_list() const { return _bl; }

		bool operator==(const Value& o) const {
			if (_type != o._type) return false;
			switch (_type) {
				case Type::None:      return true;
				case Type::Bool:
				case Type::Int:
				case Type::Enum:      return _i == o._i;
				case Type::Float:     return _d == o._d;
				case Type::String:    return _s == o._s;
				case Type::Bytes:     return _b == o._b;
				case Type::BytesList: return _bl == o._bl;
			}
			return false;
		}
		bool operator!=(const Value& o) const { return !(*this == o); }

	private:
		Type _type;
		int64_t _i;
		double _d;
		std::string _s;
		Bytes _b;
		std::vector<Bytes> _bl;
	};

} }
