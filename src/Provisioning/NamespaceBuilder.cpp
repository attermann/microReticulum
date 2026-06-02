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

	// Helper to wrap a typed getter as a Value-returning GetterFn.
	template <typename TypedGetter>
	static GetterFn wrap_getter(TypedGetter typed_getter) {
		if (!typed_getter) return nullptr;
		return [typed_getter]() { return Value(typed_getter()); };
	}

	NamespaceBuilder& NamespaceBuilder::field_bool(const char* name, uint16_t id, uint8_t flags,
		bool default_value, SetterFn setter,
		std::function<bool()> getter) {
		if (!_ns) return *this;
		Field f;
		f.id = id;
		f.name = name ? name : "";
		f.type = Type::Bool;
		f.flags = flags;
		f.default_value = Value(default_value);
		f.setter = std::move(setter);
		f.getter = wrap_getter(std::move(getter));
		warn_if_dup(_ns, _ns->add_field(std::move(f)), id, name);
		return *this;
	}

	NamespaceBuilder& NamespaceBuilder::field_int(const char* name, uint16_t id, uint8_t flags,
		int64_t default_value, int64_t imin, int64_t imax, SetterFn setter,
		std::function<int64_t()> getter) {
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
		f.getter = wrap_getter(std::move(getter));
		warn_if_dup(_ns, _ns->add_field(std::move(f)), id, name);
		return *this;
	}

	NamespaceBuilder& NamespaceBuilder::field_int(const char* name, uint16_t id, uint8_t flags,
		int64_t default_value, SetterFn setter,
		std::function<int64_t()> getter) {
		if (!_ns) return *this;
		Field f;
		f.id = id;
		f.name = name ? name : "";
		f.type = Type::Int;
		f.flags = flags;
		f.default_value = Value((int64_t)default_value);
		f.setter = std::move(setter);
		f.getter = wrap_getter(std::move(getter));
		warn_if_dup(_ns, _ns->add_field(std::move(f)), id, name);
		return *this;
	}

	NamespaceBuilder& NamespaceBuilder::field_float(const char* name, uint16_t id, uint8_t flags,
		double default_value, double fmin, double fmax, SetterFn setter,
		std::function<double()> getter) {
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
		f.getter = wrap_getter(std::move(getter));
		warn_if_dup(_ns, _ns->add_field(std::move(f)), id, name);
		return *this;
	}

	NamespaceBuilder& NamespaceBuilder::field_float(const char* name, uint16_t id, uint8_t flags,
		double default_value, SetterFn setter,
		std::function<double()> getter) {
		if (!_ns) return *this;
		Field f;
		f.id = id;
		f.name = name ? name : "";
		f.type = Type::Float;
		f.flags = flags;
		f.default_value = Value(default_value);
		f.setter = std::move(setter);
		f.getter = wrap_getter(std::move(getter));
		warn_if_dup(_ns, _ns->add_field(std::move(f)), id, name);
		return *this;
	}

	NamespaceBuilder& NamespaceBuilder::field_string(const char* name, uint16_t id, uint8_t flags,
		const char* default_value, size_t max_len, SetterFn setter,
		std::function<std::string()> getter) {
		if (!_ns) return *this;
		Field f;
		f.id = id;
		f.name = name ? name : "";
		f.type = Type::String;
		f.flags = flags;
		f.constraint.max_len = max_len;
		f.default_value = Value(default_value ? default_value : "");
		f.setter = std::move(setter);
		f.getter = wrap_getter(std::move(getter));
		warn_if_dup(_ns, _ns->add_field(std::move(f)), id, name);
		return *this;
	}

	NamespaceBuilder& NamespaceBuilder::field_bytes(const char* name, uint16_t id, uint8_t flags,
		const Bytes& default_value, size_t max_len, SetterFn setter,
		std::function<Bytes()> getter) {
		if (!_ns) return *this;
		Field f;
		f.id = id;
		f.name = name ? name : "";
		f.type = Type::Bytes;
		f.flags = flags;
		f.constraint.max_len = max_len;
		f.default_value = Value(default_value);
		f.setter = std::move(setter);
		f.getter = wrap_getter(std::move(getter));
		warn_if_dup(_ns, _ns->add_field(std::move(f)), id, name);
		return *this;
	}

	NamespaceBuilder& NamespaceBuilder::field_bytes_list(const char* name, uint16_t id, uint8_t flags,
		std::vector<Bytes> default_value,
		size_t element_size, size_t max_count,
		SetterFn setter,
		std::function<std::vector<Bytes>()> getter) {
		if (!_ns) return *this;
		Field f;
		f.id = id;
		f.name = name ? name : "";
		f.type = Type::BytesList;
		f.flags = flags;
		f.constraint.element_size = element_size;
		f.constraint.max_count    = max_count;
		f.default_value = Value(std::move(default_value));
		f.setter = std::move(setter);
		if (getter) {
			f.getter = [g = std::move(getter)]() { return Value(g()); };
		}
		warn_if_dup(_ns, _ns->add_field(std::move(f)), id, name);
		return *this;
	}

	// -- metric_* helpers ----------------------------------------------------

	NamespaceBuilder& NamespaceBuilder::metric_bool(const char* name, uint16_t id,
		std::function<bool()> getter) {
		return field_bool(name, id, FF_READ_ONLY, false, nullptr, std::move(getter));
	}
	NamespaceBuilder& NamespaceBuilder::metric_int(const char* name, uint16_t id,
		std::function<int64_t()> getter) {
		return field_int(name, id, FF_READ_ONLY, 0, nullptr, std::move(getter));
	}
	NamespaceBuilder& NamespaceBuilder::metric_float(const char* name, uint16_t id,
		std::function<double()> getter) {
		return field_float(name, id, FF_READ_ONLY, 0.0, nullptr, std::move(getter));
	}
	NamespaceBuilder& NamespaceBuilder::metric_string(const char* name, uint16_t id,
		std::function<std::string()> getter) {
		return field_string(name, id, FF_READ_ONLY, "", 0, nullptr, std::move(getter));
	}
	NamespaceBuilder& NamespaceBuilder::metric_bytes(const char* name, uint16_t id,
		std::function<Bytes()> getter) {
		return field_bytes(name, id, FF_READ_ONLY, Bytes(), 0, nullptr, std::move(getter));
	}

	// -- command_* helpers ---------------------------------------------------

	NamespaceBuilder& NamespaceBuilder::command_bool(const char* name, uint16_t id, SetterFn setter) {
		return field_bool(name, id, FF_WRITE_ONLY, false, std::move(setter));
	}
	NamespaceBuilder& NamespaceBuilder::command_int(const char* name, uint16_t id,
		int64_t imin, int64_t imax, SetterFn setter) {
		return field_int(name, id, FF_WRITE_ONLY, 0, imin, imax, std::move(setter));
	}
	NamespaceBuilder& NamespaceBuilder::command_float(const char* name, uint16_t id,
		double fmin, double fmax, SetterFn setter) {
		return field_float(name, id, FF_WRITE_ONLY, 0.0, fmin, fmax, std::move(setter));
	}
	NamespaceBuilder& NamespaceBuilder::command_string(const char* name, uint16_t id,
		size_t max_len, SetterFn setter) {
		return field_string(name, id, FF_WRITE_ONLY, "", max_len, std::move(setter));
	}
	NamespaceBuilder& NamespaceBuilder::command_bytes(const char* name, uint16_t id,
		size_t max_len, SetterFn setter) {
		return field_bytes(name, id, FF_WRITE_ONLY, Bytes(), max_len, std::move(setter));
	}

	NamespaceBuilder& NamespaceBuilder::field_enum(const char* name, uint16_t id, uint8_t flags,
		int64_t default_value,
		std::vector<int64_t> values,
		std::vector<std::string> labels,
		SetterFn setter,
		std::function<int64_t()> getter) {
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
		// Enum uses int64 storage but the typed getter returns an int64
		// that we want tagged as Enum on read.
		if (getter) {
			f.getter = [g = std::move(getter)]() { return Value::from_int_as(Type::Enum, g()); };
		}
		warn_if_dup(_ns, _ns->add_field(std::move(f)), id, name);
		return *this;
	}

} }
