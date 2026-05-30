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

#include "Provisioning.h"

#include "../Log.h"

namespace RNS { namespace Provisioning {

	static void warn_if_dup(Namespace* ns, bool ok, uint16_t id, const char* name) {
		if (!ok) {
			WARNINGF("NamespaceBuilder: duplicate field id=%u name=\"%s\" in namespace %s",
				id, name ? name : "", ns ? ns->name().c_str() : "?");
		}
	}

	NamespaceBuilder& NamespaceBuilder::field_bool(const char* name, uint16_t id, uint8_t flags,
		bool default_value, SetterFn setter) {
		if (!_ns) return *this;
		Field f;
		f.id = id;
		f.name = name ? name : "";
		f.type = Type::Bool;
		f.flags = flags;
		f.default_value = Value(default_value);
		f.setter = std::move(setter);
		warn_if_dup(_ns, _ns->add_field(std::move(f)), id, name);
		return *this;
	}

	NamespaceBuilder& NamespaceBuilder::field_int(const char* name, uint16_t id, uint8_t flags,
		int64_t default_value, int64_t imin, int64_t imax, SetterFn setter) {
		if (!_ns) return *this;
		Field f;
		f.id = id;
		f.name = name ? name : "";
		f.type = Type::Int;
		f.flags = flags;
		f.constraint.has_range = true;
		f.constraint.imin = imin;
		f.constraint.imax = imax;
		f.default_value = Value((int64_t)default_value);
		f.setter = std::move(setter);
		warn_if_dup(_ns, _ns->add_field(std::move(f)), id, name);
		return *this;
	}

	NamespaceBuilder& NamespaceBuilder::field_int(const char* name, uint16_t id, uint8_t flags,
		int64_t default_value, SetterFn setter) {
		if (!_ns) return *this;
		Field f;
		f.id = id;
		f.name = name ? name : "";
		f.type = Type::Int;
		f.flags = flags;
		f.default_value = Value((int64_t)default_value);
		f.setter = std::move(setter);
		warn_if_dup(_ns, _ns->add_field(std::move(f)), id, name);
		return *this;
	}

	NamespaceBuilder& NamespaceBuilder::field_float(const char* name, uint16_t id, uint8_t flags,
		double default_value, double fmin, double fmax, SetterFn setter) {
		if (!_ns) return *this;
		Field f;
		f.id = id;
		f.name = name ? name : "";
		f.type = Type::Float;
		f.flags = flags;
		f.constraint.has_range = true;
		f.constraint.fmin = fmin;
		f.constraint.fmax = fmax;
		f.default_value = Value(default_value);
		f.setter = std::move(setter);
		warn_if_dup(_ns, _ns->add_field(std::move(f)), id, name);
		return *this;
	}

	NamespaceBuilder& NamespaceBuilder::field_float(const char* name, uint16_t id, uint8_t flags,
		double default_value, SetterFn setter) {
		if (!_ns) return *this;
		Field f;
		f.id = id;
		f.name = name ? name : "";
		f.type = Type::Float;
		f.flags = flags;
		f.default_value = Value(default_value);
		f.setter = std::move(setter);
		warn_if_dup(_ns, _ns->add_field(std::move(f)), id, name);
		return *this;
	}

	NamespaceBuilder& NamespaceBuilder::field_string(const char* name, uint16_t id, uint8_t flags,
		const char* default_value, size_t max_len, SetterFn setter) {
		if (!_ns) return *this;
		Field f;
		f.id = id;
		f.name = name ? name : "";
		f.type = Type::String;
		f.flags = flags;
		f.constraint.max_len = max_len;
		f.default_value = Value(default_value ? default_value : "");
		f.setter = std::move(setter);
		warn_if_dup(_ns, _ns->add_field(std::move(f)), id, name);
		return *this;
	}

	NamespaceBuilder& NamespaceBuilder::field_bytes(const char* name, uint16_t id, uint8_t flags,
		const Bytes& default_value, size_t max_len, SetterFn setter) {
		if (!_ns) return *this;
		Field f;
		f.id = id;
		f.name = name ? name : "";
		f.type = Type::Bytes;
		f.flags = flags;
		f.constraint.max_len = max_len;
		f.default_value = Value(default_value);
		f.setter = std::move(setter);
		warn_if_dup(_ns, _ns->add_field(std::move(f)), id, name);
		return *this;
	}

	NamespaceBuilder& NamespaceBuilder::field_enum(const char* name, uint16_t id, uint8_t flags,
		int64_t default_value,
		std::vector<int64_t> values,
		std::vector<std::string> labels,
		SetterFn setter) {
		if (!_ns) return *this;
		Field f;
		f.id = id;
		f.name = name ? name : "";
		f.type = Type::Enum;
		f.flags = flags;
		f.constraint.enum_values = std::move(values);
		f.constraint.enum_labels = std::move(labels);
		f.default_value = Value::from_int_as(Type::Enum, default_value);
		f.setter = std::move(setter);
		warn_if_dup(_ns, _ns->add_field(std::move(f)), id, name);
		return *this;
	}

} }
