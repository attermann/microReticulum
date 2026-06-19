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

	static void warn_if_dup(Namespace* ns, bool ok, fid_t id, const char* name) {
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

	// Helper to grab the current scope namespace; if registration is
	// outside any scope, emit a warning and return nullptr.
	static Namespace* scope(Provisioner* mgr, const char* op) {
		Namespace* ns = mgr ? mgr->current_build_scope() : nullptr;
		if (!ns) {
			WARNINGF("NamespaceBuilder::%s called with no open namespace scope", op);
		}
		return ns;
	}

	// -- Scope control -------------------------------------------------------

	NamespaceBuilder& NamespaceBuilder::namespace_(const char* name, nid_t id) {
		// Delegates to Provisioner::namespace_, which pushes the new namespace
		// onto the scope stack. The returned builder value is discarded —
		// it's identical to this one (both reference the same Provisioner).
		if (_mgr) _mgr->namespace_(name, id);
		return *this;
	}

	NamespaceBuilder& NamespaceBuilder::end() {
		if (_mgr) _mgr->pop_build_scope();
		return *this;
	}

	// -- Stateful field builders --------------------------------------------

	NamespaceBuilder& NamespaceBuilder::field_bool(const char* name, fid_t id, fflags_t flags,
		bool default_value, SetterFn setter,
		std::function<fbool_t()> getter) {
		Namespace* ns = scope(_mgr, "field_bool");
		if (!ns) return *this;
		Field f;
		f.id = id;
		f.name = name ? name : "";
		f.type = Type::Bool;
		f.flags = flags;
		f.default_value = Value(default_value);
		f.setter = std::move(setter);
		f.getter = wrap_getter(std::move(getter));
		warn_if_dup(ns, ns->add_field(std::move(f)), id, name);
		return *this;
	}

	NamespaceBuilder& NamespaceBuilder::field_int(const char* name, fid_t id, fflags_t flags,
		fint_t default_value, fint_t imin, fint_t imax, SetterFn setter,
		std::function<fint_t()> getter) {
		Namespace* ns = scope(_mgr, "field_int");
		if (!ns) return *this;
		Field f;
		f.id = id;
		f.name = name ? name : "";
		f.type = Type::Int;
		f.flags = flags;
		f.constraint.has_range = true;
		f.constraint.imin = imin;
		f.constraint.imax = imax;
		f.default_value = Value((fint_t)default_value);
		f.setter = std::move(setter);
		f.getter = wrap_getter(std::move(getter));
		warn_if_dup(ns, ns->add_field(std::move(f)), id, name);
		return *this;
	}

	NamespaceBuilder& NamespaceBuilder::field_int(const char* name, fid_t id, fflags_t flags,
		fint_t default_value, SetterFn setter,
		std::function<fint_t()> getter) {
		Namespace* ns = scope(_mgr, "field_int");
		if (!ns) return *this;
		Field f;
		f.id = id;
		f.name = name ? name : "";
		f.type = Type::Int;
		f.flags = flags;
		f.default_value = Value((fint_t)default_value);
		f.setter = std::move(setter);
		f.getter = wrap_getter(std::move(getter));
		warn_if_dup(ns, ns->add_field(std::move(f)), id, name);
		return *this;
	}

	NamespaceBuilder& NamespaceBuilder::field_float(const char* name, fid_t id, fflags_t flags,
		ffloat_t default_value, ffloat_t fmin, ffloat_t fmax, SetterFn setter,
		std::function<ffloat_t()> getter) {
		Namespace* ns = scope(_mgr, "field_float");
		if (!ns) return *this;
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
		warn_if_dup(ns, ns->add_field(std::move(f)), id, name);
		return *this;
	}

	NamespaceBuilder& NamespaceBuilder::field_float(const char* name, fid_t id, fflags_t flags,
		ffloat_t default_value, SetterFn setter,
		std::function<ffloat_t()> getter) {
		Namespace* ns = scope(_mgr, "field_float");
		if (!ns) return *this;
		Field f;
		f.id = id;
		f.name = name ? name : "";
		f.type = Type::Float;
		f.flags = flags;
		f.default_value = Value(default_value);
		f.setter = std::move(setter);
		f.getter = wrap_getter(std::move(getter));
		warn_if_dup(ns, ns->add_field(std::move(f)), id, name);
		return *this;
	}

	NamespaceBuilder& NamespaceBuilder::field_string(const char* name, fid_t id, fflags_t flags,
		const char* default_value, flen_t max_len, SetterFn setter,
		std::function<fstring_t()> getter) {
		Namespace* ns = scope(_mgr, "field_string");
		if (!ns) return *this;
		Field f;
		f.id = id;
		f.name = name ? name : "";
		f.type = Type::String;
		f.flags = flags;
		f.constraint.max_len = max_len;
		f.default_value = Value(default_value ? default_value : "");
		f.setter = std::move(setter);
		f.getter = wrap_getter(std::move(getter));
		warn_if_dup(ns, ns->add_field(std::move(f)), id, name);
		return *this;
	}

	NamespaceBuilder& NamespaceBuilder::field_bytes(const char* name, fid_t id, fflags_t flags,
		const fbytes_t& default_value, flen_t max_len, SetterFn setter,
		std::function<fbytes_t()> getter) {
		Namespace* ns = scope(_mgr, "field_bytes");
		if (!ns) return *this;
		Field f;
		f.id = id;
		f.name = name ? name : "";
		f.type = Type::Bytes;
		f.flags = flags;
		f.constraint.max_len = max_len;
		f.default_value = Value(default_value);
		f.setter = std::move(setter);
		f.getter = wrap_getter(std::move(getter));
		warn_if_dup(ns, ns->add_field(std::move(f)), id, name);
		return *this;
	}

	NamespaceBuilder& NamespaceBuilder::field_bytes_list(const char* name, fid_t id, fflags_t flags,
		fbytes_list_t default_value,
		flen_t element_size, flen_t max_count,
		SetterFn setter,
		std::function<fbytes_list_t()> getter) {
		Namespace* ns = scope(_mgr, "field_bytes_list");
		if (!ns) return *this;
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
		warn_if_dup(ns, ns->add_field(std::move(f)), id, name);
		return *this;
	}

	NamespaceBuilder& NamespaceBuilder::field_enum(const char* name, fid_t id, fflags_t flags,
		fenum_t default_value,
		std::vector<fenum_t> values,
		std::vector<fstring_t> labels,
		SetterFn setter,
		std::function<fenum_t()> getter) {
		Namespace* ns = scope(_mgr, "field_enum");
		if (!ns) return *this;
		Field f;
		f.id = id;
		f.name = name ? name : "";
		f.type = Type::Enum;
		f.flags = flags;
		f.constraint.enum_values = std::move(values);
		f.constraint.enum_labels = std::move(labels);
		f.default_value = Value::from_int_as(Type::Enum, default_value);
		f.setter = std::move(setter);
		if (getter) {
			f.getter = [g = std::move(getter)]() { return Value::from_int_as(Type::Enum, g()); };
		}
		warn_if_dup(ns, ns->add_field(std::move(f)), id, name);
		return *this;
	}

	// -- metric_* helpers ----------------------------------------------------

	NamespaceBuilder& NamespaceBuilder::metric_bool(const char* name, fid_t id,
		std::function<fbool_t()> getter) {
		return field_bool(name, id, FF_READ_ONLY, false, nullptr, std::move(getter));
	}
	NamespaceBuilder& NamespaceBuilder::metric_int(const char* name, fid_t id,
		std::function<fint_t()> getter) {
		return field_int(name, id, FF_READ_ONLY, 0, nullptr, std::move(getter));
	}
	NamespaceBuilder& NamespaceBuilder::metric_float(const char* name, fid_t id,
		std::function<ffloat_t()> getter) {
		return field_float(name, id, FF_READ_ONLY, 0.0, nullptr, std::move(getter));
	}
	NamespaceBuilder& NamespaceBuilder::metric_string(const char* name, fid_t id,
		std::function<fstring_t()> getter) {
		return field_string(name, id, FF_READ_ONLY, "", 0, nullptr, std::move(getter));
	}
	NamespaceBuilder& NamespaceBuilder::metric_bytes(const char* name, fid_t id,
		std::function<fbytes_t()> getter) {
		return field_bytes(name, id, FF_READ_ONLY, Bytes(), 0, nullptr, std::move(getter));
	}

	// -- command_* helpers ---------------------------------------------------

	NamespaceBuilder& NamespaceBuilder::command_bool(const char* name, fid_t id, SetterFn setter) {
		return field_bool(name, id, FF_WRITE_ONLY, false, std::move(setter));
	}
	NamespaceBuilder& NamespaceBuilder::command_int(const char* name, fid_t id,
		fint_t imin, fint_t imax, SetterFn setter) {
		return field_int(name, id, FF_WRITE_ONLY, 0, imin, imax, std::move(setter));
	}
	NamespaceBuilder& NamespaceBuilder::command_float(const char* name, fid_t id,
		ffloat_t fmin, ffloat_t fmax, SetterFn setter) {
		return field_float(name, id, FF_WRITE_ONLY, 0.0, fmin, fmax, std::move(setter));
	}
	NamespaceBuilder& NamespaceBuilder::command_string(const char* name, fid_t id,
		flen_t max_len, SetterFn setter) {
		return field_string(name, id, FF_WRITE_ONLY, "", max_len, std::move(setter));
	}
	NamespaceBuilder& NamespaceBuilder::command_bytes(const char* name, fid_t id,
		flen_t max_len, SetterFn setter) {
		return field_bytes(name, id, FF_WRITE_ONLY, Bytes(), max_len, std::move(setter));
	}

	NamespaceBuilder& NamespaceBuilder::command_void(const char* name, fid_t id,
		std::function<bool()> setter) {
		Namespace* ns = scope(_mgr, "command_void");
		if (!ns) return *this;
		Field f;
		f.id = id;
		f.name = name ? name : "";
		f.type = Type::Void;
		f.flags = FF_WRITE_ONLY;
		f.default_value = Value::make_void();
		if (setter) {
			// Adapt no-arg user callback to the SetterFn(Value) contract.
			f.setter = [s = std::move(setter)](const Value&) -> bool { return s(); };
		}
		warn_if_dup(ns, ns->add_field(std::move(f)), id, name);
		return *this;
	}

	NamespaceBuilder& NamespaceBuilder::on_commit(CommitCallback cb) {
		Namespace* ns = scope(_mgr, "on_commit");
		if (!ns) return *this;
		ns->on_commit(std::move(cb));
		return *this;
	}

} }
