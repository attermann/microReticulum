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
#include "Codec.h"

#include "../Log.h"

#include <unordered_set>

namespace RNS { namespace Provisioning {

	// Defined in BuiltinNamespaces.cpp.
	void register_builtin_namespaces(Manager& p);

	// ---------------------------------------------------------------------
	// Singleton
	// ---------------------------------------------------------------------

	Manager& Manager::instance() {
		static Manager inst;
		return inst;
	}

	void Manager::begin(const char* storage_root) {
		if (_started) return;
		// Register library-side namespaces before mounting storage so that
		// load_all() has a Registry to overlay onto.
		register_builtin_namespaces(*this);
		// If app code or a builtin forgot to call .end() somewhere, the
		// scope stack is left non-empty. That would silently nest the next
		// registration under the wrong parent. Warn and clear so subsequent
		// builds aren't poisoned.
		if (!build_scope_empty()) {
			WARNINGF("Provisioning::begin: namespace scope stack non-empty (%zu) "
				"after registration — missing .end() call. Clearing.",
				_build_scope.size());
			_build_scope.clear();
		}
#ifdef RNS_USE_FS
		_storage.reset(new Storage(storage_root ? storage_root : "/config", &_registry));
		_storage->load_all(_registry);
		// Push the just-loaded working values into the runtime statics via
		// each field's setter. Without this step, persistence is decorative —
		// committed flash values would reload into the Provisioning map but
		// never reach consumers like Reticulum::link_mtu_discovery().
		apply_loaded_to_runtime();
#else
		(void)storage_root;
#endif
		_needs_reboot = false;
		_started = true;
	}

	void Manager::apply_loaded_to_runtime() {
		for (const auto& ns_ptr : _registry.namespaces()) {
			for (const Field& f : ns_ptr->fields()) {
				if (f.has_flag(FF_READ_ONLY)) continue;
				// Write-only fields are one-shot commands — replaying them
				// on every boot would be incorrect, and they're never
				// persisted anyway so working has no useful value to push.
				if (f.has_flag(FF_WRITE_ONLY)) continue;
				if (!f.setter) continue;
				Value v = ns_ptr->working(f.id);
				if (v.is_none()) continue;
				// Skip if the runtime already has this value — avoids
				// redundant setter invocations at boot. For fields with a
				// getter we can ask the live runtime directly, which is
				// accurate even when the loaded value happens to equal the
				// declared default (the previous "v == default_value"
				// heuristic silently dropped legitimate disk overrides in
				// that case — see the lora_interface_mode bug where saving
				// MODE_GATEWAY, the declared default, never re-applied at
				// boot because the runtime started at MODE_NONE). For
				// fields without a getter we fall back to the declared
				// default, which is the safe assumption for built-ins whose
				// statics are initialised to the schema's declared default.
				const Value runtime_v = f.getter ? f.getter() : f.default_value;
				if (v == runtime_v) continue;
				try {
					f.setter(v);
				}
				catch (const std::exception& e) {
					ERRORF("Manager::apply_loaded_to_runtime: setter for "
						"namespace %u field %u threw: %s",
						ns_ptr->id(), f.id, e.what());
				}
			}
		}
	}

	void Manager::end() {
		_storage.reset();
		_registry.clear();
		_build_scope.clear();
		_needs_reboot = false;
		_started = false;
		// Drop the reboot-requested callback. If the caller captured stack
		// state by reference (very common in tests), keeping the std::function
		// alive past end() turns the next set_reboot_flag(true) into a
		// write-through-dangling-reference.
		_on_reboot = nullptr;
	}

	NamespaceBuilder Manager::namespace_(const char* name, nid_t id) {
		// Parent comes from the current top of the registration scope stack
		// (set up by previous .namespace_(...) calls in the same chain).
		// If the scope is empty, this is a root namespace.
		nid_t parent_id = build_scope_empty() ? 0 : current_build_scope()->id();
		Namespace* ns = _registry.add_namespace(id, name, parent_id);
		if (!ns) {
			// Recovery: existing namespace by id, or by name at root level.
			// We still push it onto the scope so nested chaining works.
			ns = _registry.find(id);
			if (!ns && name) ns = _registry.find(name);
			WARNINGF("Provisioning::namespace_: namespace id=%u name=\"%s\" already exists or invalid; appending to it",
				id, name ? name : "");
		}
		if (ns) push_build_scope(ns);
		return NamespaceBuilder(this);
	}

	// ---------------------------------------------------------------------
	// Direct accessors
	// ---------------------------------------------------------------------

	Value Manager::field(nid_t ns_id, fid_t field_id, Source src) const {
		const Namespace* ns = _registry.find(ns_id);
		if (!ns) return {};
		switch (src) {
			case Source::Working:
				// Returns the field's getter value when present (live
				// runtime), else the cached working-map value. This is
				// what eliminates drift when direct setters mutate the
				// runtime outside of Provisioning's commit path.
				return ns->effective(field_id);
			case Source::Draft: {
				Value v;
				if (ns->draft(field_id, v)) return v;
				return {};
			}
			case Source::Effective: {
				Value v;
				if (ns->draft(field_id, v)) return v;
				return ns->effective(field_id);
			}
		}
		return {};
	}

	bool Manager::field(nid_t ns_id, fid_t field_id, const Value& v) {
		Namespace* ns = _registry.find(ns_id);
		if (!ns) return false;
		return ns->set_draft(field_id, v);
	}

	Value Manager::field(const char* ns_name, const char* field_name, Source src) const {
		const Namespace* ns = _registry.find(ns_name);
		if (!ns) return {};
		const Field* f = ns->find_field(field_name);
		if (!f) return {};
		return field(ns->id(), f->id, src);
	}

	bool Manager::field(const char* ns_name, const char* field_name, const Value& v) {
		Namespace* ns = _registry.find(ns_name);
		if (!ns) return false;
		const Field* f = ns->find_field(field_name);
		if (!f) return false;
		return ns->set_draft(f->id, v);
	}

	bool Manager::commit(nid_t ns_id) {
		auto do_one = [&](Namespace& ns) -> bool {
			bool any_reboot = false;
			// Collect ids first; commit_one() mutates _draft.
			std::vector<fid_t> ids;
			for (const Field& f : ns.fields()) {
				if (ns.has_draft(f.id)) ids.push_back(f.id);
			}
			for (fid_t id : ids) {
				auto outcome = ns.commit_one(id);
				if (outcome == Namespace::CommitOutcome::AppliedReboot) any_reboot = true;
			}
			bool ok = true;
			if (_storage) ok = _storage->save_namespace(ns);
			if (any_reboot) set_reboot_flag(true);
			return ok;
		};
		if (ns_id == 0) {
			bool ok = true;
			for (const auto& ns_ptr : _registry.namespaces()) ok = do_one(*ns_ptr) && ok;
			return ok;
		}
		Namespace* ns = _registry.find(ns_id);
		if (!ns) return false;
		return do_one(*ns);
	}

	bool Manager::discard(nid_t ns_id) {
		if (ns_id == 0) {
			for (const auto& ns_ptr : _registry.namespaces()) ns_ptr->clear_draft();
			return true;
		}
		Namespace* ns = _registry.find(ns_id);
		if (!ns) return false;
		ns->clear_draft();
		return true;
	}

	bool Manager::commit(const char* ns_name) {
		Namespace* ns = _registry.find(ns_name);
		if (!ns) return false;
		return commit(ns->id());
	}

	bool Manager::discard(const char* ns_name) {
		Namespace* ns = _registry.find(ns_name);
		if (!ns) return false;
		return discard(ns->id());
	}

	bool Manager::factory_reset() {
		// Drop drafts, reset working to defaults, AND push defaults into
		// the runtime via each field's setter — otherwise the live state
		// (and any field that's read via a getter) would keep whatever was
		// last committed, defeating the purpose of factory-reset.
		for (const auto& ns_ptr : _registry.namespaces()) {
			ns_ptr->clear_draft();
			for (const Field& f : ns_ptr->fields()) {
				ns_ptr->put_working(f.id, f.default_value);
				// Don't fire setters for read-only (no setter) or write-only
				// (firing on factory_reset would trigger the command with
				// the default argument every time — wrong).
				if (!f.has_flag(FF_READ_ONLY)
					&& !f.has_flag(FF_WRITE_ONLY)
					&& f.setter
					&& !f.default_value.is_none()) {
					try { f.setter(f.default_value); }
					catch (const std::exception& e) {
						ERRORF("Manager::factory_reset: setter for ns %u field %u threw: %s",
							ns_ptr->id(), f.id, e.what());
					}
				}
			}
			ns_ptr->mark_clean();
		}
		bool ok = true;
		if (_storage) ok = _storage->factory_reset(_registry);
		_needs_reboot = false;
		return ok;
	}

	bool Manager::clear_storage() {
		TRACE("Provisioning::clear_storage()");
		if (!_storage) return true;
		return _storage->factory_reset(_registry);
	}

	bool Manager::draft_has_reboot() const {
		for (const auto& ns_ptr : _registry.namespaces()) {
			if (ns_ptr->draft_has_reboot()) return true;
		}
		return false;
	}

	void Manager::set_reboot_flag(bool any_reboot_applied) {
		const bool was = _needs_reboot;
		if (any_reboot_applied) _needs_reboot = true;
		if (!was && _needs_reboot && _on_reboot) {
			try { _on_reboot(); }
			catch (const std::exception& e) {
				ERRORF("Provisioning::on_reboot_requested callback threw: %s", e.what());
			}
		}
	}

	// ---------------------------------------------------------------------
	// Wire codec
	// ---------------------------------------------------------------------

	// Each request and response is a MsgPack array of three elements:
	//   [ op_id (uint), seq (uint), payload ]
	// where payload is op-specific (often a map; nil if not used).

	static Bytes pack_response(opid_t op_id, seq_t seq, const std::function<void(MsgPack::Packer&)>& pack_payload) {
		MsgPack::Packer packer;
		packer.serialize(MsgPack::arr_size_t(3));
		packer.serialize((opid_t)op_id);
		packer.serialize((seq_t)seq);
		if (pack_payload) pack_payload(packer);
		else { MsgPack::object::nil_t n; packer.serialize(n); }
		return Bytes(packer.data(), packer.size());
	}

	Bytes Manager::encode_error(opid_t op_id, seq_t seq, ErrorCode code, const char* msg) {
		return pack_response((opid_t)Op::Error, seq, [&](MsgPack::Packer& p) {
			p.serialize(MsgPack::map_size_t(msg ? 2 : 1));
			p.serialize((uint16_t)Key::ErrorCodeKey);
			p.serialize((ferror_t)code);
			if (msg) {
				p.serialize((uint16_t)Key::ErrorMessage);
				p.serialize(msg);
			}
		});
	}

	Bytes Manager::encode_ack(opid_t op_id, seq_t seq) {
		return pack_response(op_id, seq, nullptr);
	}

	// Read a uint key into 'key'. Returns false if cursor isn't on a uint.
	static bool read_uint_key(MsgPack::Unpacker& u, nid_t& key) {
		if (!(u.isUInt() || u.isInt())) return false;
		fint_t v = 0;
		u.deserialize(v);
		key = (nid_t)v;
		return true;
	}

	// Skip the next value in the unpacker. Forward-declared/duplicate of the
	// helper in Codec.cpp so this TU is self-contained.
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
	}

	Bytes Manager::op_get_info(seq_t seq) {
		return pack_response((opid_t)Op::GetInfo, seq, [&](MsgPack::Packer& p) {
			p.serialize(MsgPack::map_size_t(3));
			p.serialize((uint16_t)Key::FirmwareVersion); p.serialize("microReticulum");
			p.serialize((uint16_t)Key::SchemaVersion);   p.serialize((nid_t)Manager::SCHEMA_VERSION);
			p.serialize((uint16_t)Key::NeedsRebootInfo); p.serialize((bool)_needs_reboot);
		});
	}

	Bytes Manager::op_get_capabilities(seq_t seq) {
		return pack_response((opid_t)Op::GetCapabilities, seq, [&](MsgPack::Packer& p) {
			const auto& nss = _registry.namespaces();
			p.serialize(MsgPack::arr_size_t(nss.size()));
			for (const auto& ns_ptr : nss) {
				p.serialize((nid_t)ns_ptr->id());
			}
		});
	}

	static void pack_field_default(MsgPack::Packer& p, const Field& f) {
		switch (f.type) {
			case Type::None:   { MsgPack::object::nil_t n; p.serialize(n); return; }
			case Type::Bool:    p.serialize((bool)f.default_value.as_bool()); return;
			case Type::Int:
			case Type::Enum:    p.serialize((fint_t)f.default_value.as_int()); return;
			case Type::Float:   p.serialize((double)f.default_value.as_float()); return;
			case Type::String:  p.serialize(f.default_value.as_string().c_str()); return;
			case Type::Bytes: {
				const Bytes& b = f.default_value.as_bytes();
				MsgPack::bin_t<uint8_t> bin;
				if (b.size() > 0) { bin.resize(b.size()); memcpy(bin.data(), b.data(), b.size()); }
				p.serialize(bin);
				return;
			}
			case Type::BytesList: {
				const auto& list = f.default_value.as_bytes_list();
				p.serialize(MsgPack::arr_size_t(list.size()));
				for (const Bytes& e : list) {
					MsgPack::bin_t<uint8_t> bin;
					if (e.size() > 0) { bin.resize(e.size()); memcpy(bin.data(), e.data(), e.size()); }
					p.serialize(bin);
				}
				return;
			}
			case Type::Void: { MsgPack::object::nil_t n; p.serialize(n); return; }
		}
	}

	static size_t schema_field_entries(const Field& f) {
		// id, name, type, flags, default
		size_t n = 5;
		if (f.type == Type::Int && f.constraint.has_range)   n += 2;
		if (f.type == Type::Float && f.constraint.has_range) n += 2;
		if ((f.type == Type::String || f.type == Type::Bytes) && f.constraint.max_len > 0) n += 1;
		if (f.type == Type::Enum) {
			if (!f.constraint.enum_values.empty()) n += 1;
			if (!f.constraint.enum_labels.empty()) n += 1;
		}
		if (f.type == Type::BytesList) {
			if (f.constraint.element_size > 0) n += 1;
			if (f.constraint.max_count    > 0) n += 1;
		}
		return n;
	}

	Bytes Manager::op_get_schema(seq_t seq) {
		return pack_response((opid_t)Op::GetSchema, seq, [&](MsgPack::Packer& p) {
			const auto& nss = _registry.namespaces();
			p.serialize(MsgPack::arr_size_t(nss.size()));
			for (const auto& ns_ptr : nss) {
				const Namespace& ns = *ns_ptr;
				// Each namespace is [id, name, parent_id_or_zero, [field-maps]].
				// parent_id of 0 means root (no parent). Schema v2 layout —
				// v1 clients reading the first three elements still parse
				// the rest of the response correctly.
				p.serialize(MsgPack::arr_size_t(4));
				p.serialize((nid_t)ns.id());
				p.serialize(ns.name().c_str());
				p.serialize((nid_t)ns.parent_id());
				const auto& fields = ns.fields();
				p.serialize(MsgPack::arr_size_t(fields.size()));
				for (const Field& f : fields) {
					p.serialize(MsgPack::map_size_t(schema_field_entries(f)));
					p.serialize((uint16_t)Key::FieldId);    p.serialize((fid_t)f.id);
					p.serialize((uint16_t)Key::FieldName);  p.serialize(f.name.c_str());
					p.serialize((uint16_t)Key::FieldType);  p.serialize((uint8_t)f.type);
					p.serialize((uint16_t)Key::FieldFlags); p.serialize((fflags_t)f.flags);
					p.serialize((uint16_t)Key::FieldDefault); pack_field_default(p, f);
					if (f.type == Type::Int && f.constraint.has_range) {
						p.serialize((uint16_t)Key::FieldMinI); p.serialize((fint_t)f.constraint.imin);
						p.serialize((uint16_t)Key::FieldMaxI); p.serialize((fint_t)f.constraint.imax);
					}
					if (f.type == Type::Float && f.constraint.has_range) {
						p.serialize((uint16_t)Key::FieldMinF); p.serialize((double)f.constraint.fmin);
						p.serialize((uint16_t)Key::FieldMaxF); p.serialize((double)f.constraint.fmax);
					}
					if ((f.type == Type::String || f.type == Type::Bytes) && f.constraint.max_len > 0) {
						p.serialize((uint16_t)Key::FieldMaxLen); p.serialize((uint64_t)f.constraint.max_len);
					}
					if (f.type == Type::Enum) {
						if (!f.constraint.enum_values.empty()) {
							p.serialize((uint16_t)Key::FieldEnumValues);
							p.serialize(MsgPack::arr_size_t(f.constraint.enum_values.size()));
							for (fenum_t v : f.constraint.enum_values) p.serialize(v);
						}
						if (!f.constraint.enum_labels.empty()) {
							p.serialize((uint16_t)Key::FieldEnumLabels);
							p.serialize(MsgPack::arr_size_t(f.constraint.enum_labels.size()));
							for (const auto& s : f.constraint.enum_labels) p.serialize(s.c_str());
						}
					}
					if (f.type == Type::BytesList) {
						if (f.constraint.element_size > 0) {
							p.serialize((uint16_t)Key::FieldElementSize);
							p.serialize((uint64_t)f.constraint.element_size);
						}
						if (f.constraint.max_count > 0) {
							p.serialize((uint16_t)Key::FieldMaxCount);
							p.serialize((uint64_t)f.constraint.max_count);
						}
					}
				}
			}
		});
	}

	static void pack_state_value(MsgPack::Packer& p, Type t, const Value& v) {
		(void)t;
		Codec::pack_value(p, v);
	}

	Bytes Manager::op_get_state(seq_t seq, void* unpacker_v) {
		MsgPack::Unpacker* up = (MsgPack::Unpacker*)unpacker_v;
		std::unordered_set<nid_t> ns_filter;
		bool has_filter = false;
		bool pending = false;
		// Optional payload map: {1: [ns_filter], 2: pending}
		if (up && up->isMap()) {
			const size_t n = up->unpackMapSize();
			for (size_t i = 0; i < n; ++i) {
				nid_t key;
				if (!read_uint_key(*up, key)) { skip_value(*up); continue; }
				if (key == Key::NamespaceFilter) {
					if (up->isArray()) {
						const size_t m = up->unpackArraySize();
						for (size_t j = 0; j < m; ++j) {
							if (up->isUInt() || up->isInt()) {
								fint_t v; up->deserialize(v);
								ns_filter.insert((nid_t)v);
							}
							else skip_value(*up);
						}
						has_filter = true;
					}
					else skip_value(*up);
				}
				else if (key == Key::Pending) {
					if (up->isBool()) up->deserialize(pending);
					else skip_value(*up);
				}
				else skip_value(*up);
			}
		}
		else if (up) skip_value(*up);

		return pack_response((opid_t)Op::GetState, seq, [&](MsgPack::Packer& p) {
			std::vector<const Namespace*> ns_list;
			for (const auto& ns_ptr : _registry.namespaces()) {
				if (has_filter && ns_filter.count(ns_ptr->id()) == 0) continue;
				ns_list.push_back(ns_ptr.get());
			}
			p.serialize(MsgPack::map_size_t(ns_list.size()));
			for (const Namespace* ns : ns_list) {
				p.serialize((nid_t)ns->id());
				// Count entries first (skip SECRET fields).
				size_t entries = 0;
				for (const Field& f : ns->fields()) {
					if (f.has_flag(FF_SECRET)) continue;
					if (f.has_flag(FF_WRITE_ONLY)) continue;
					Value v = ns->effective(f.id);
					if (pending) {
						Value d;
						if (ns->draft(f.id, d)) v = d;
					}
					if (!v.is_none()) ++entries;
				}
				p.serialize(MsgPack::map_size_t(entries));
				for (const Field& f : ns->fields()) {
					if (f.has_flag(FF_SECRET)) continue;
					Value v = ns->effective(f.id);
					if (pending) {
						Value d;
						if (ns->draft(f.id, d)) v = d;
					}
					if (v.is_none()) continue;
					p.serialize((fid_t)f.id);
					pack_state_value(p, f.type, v);
				}
			}
		});
	}

	Bytes Manager::op_set_state(seq_t seq, void* unpacker_v) {
		MsgPack::Unpacker* up = (MsgPack::Unpacker*)unpacker_v;
		if (!up || !up->isMap()) {
			return encode_error((opid_t)Op::SetState, seq, ErrorCode::MalformedRequest, "expected map payload");
		}
		const size_t n_ns = up->unpackMapSize();
		size_t applied = 0;
		struct Err { nid_t ns_id; fid_t field_id; ferror_t code; };
		std::vector<Err> errors;
		for (size_t i = 0; i < n_ns; ++i) {
			if (!(up->isUInt() || up->isInt())) { skip_value(*up); skip_value(*up); continue; }
			fint_t k1 = 0; up->deserialize(k1);
			const nid_t ns_id = (nid_t)k1;
			Namespace* ns = _registry.find(ns_id);
			if (!up->isMap()) {
				// Inner must be a map of {field_id: value}
				skip_value(*up);
				if (!ns) errors.push_back({ns_id, 0, (ferror_t)ErrorCode::UnknownNamespace});
				else errors.push_back({ns_id, 0, (ferror_t)ErrorCode::MalformedRequest});
				continue;
			}
			const size_t n_fields = up->unpackMapSize();
			for (size_t j = 0; j < n_fields; ++j) {
				if (!(up->isUInt() || up->isInt())) { skip_value(*up); skip_value(*up); continue; }
				fint_t k2 = 0; up->deserialize(k2);
				const fid_t fid = (fid_t)k2;
				if (!ns) {
					skip_value(*up);
					errors.push_back({ns_id, fid, (ferror_t)ErrorCode::UnknownNamespace});
					continue;
				}
				const Field* f = ns->find_field(fid);
				if (!f) {
					skip_value(*up);
					errors.push_back({ns_id, fid, (ferror_t)ErrorCode::UnknownField});
					continue;
				}
				Value v;
				if (!Codec::unpack_value(*up, f->type, v)) {
					errors.push_back({ns_id, fid, (ferror_t)ErrorCode::InvalidValue});
					continue;
				}
				if (f->has_flag(FF_READ_ONLY)) {
					errors.push_back({ns_id, fid, (ferror_t)ErrorCode::ReadOnly});
					continue;
				}
				if (!ns->set_draft(fid, v)) {
					errors.push_back({ns_id, fid, (ferror_t)ErrorCode::ConstraintViolation});
					continue;
				}
				++applied;
			}
		}
		return pack_response((opid_t)Op::SetState, seq, [&](MsgPack::Packer& p) {
			p.serialize(MsgPack::map_size_t(errors.empty() ? 2 : 3));
			p.serialize((uint16_t)Key::Applied);        p.serialize((uint64_t)applied);
			p.serialize((uint16_t)Key::DraftHasReboot); p.serialize((bool)draft_has_reboot());
			if (!errors.empty()) {
				p.serialize((uint16_t)Key::FieldErrors);
				p.serialize(MsgPack::arr_size_t(errors.size()));
				for (const Err& e : errors) {
					p.serialize(MsgPack::arr_size_t(3));
					p.serialize((nid_t)e.ns_id);
					p.serialize((fid_t)e.field_id);
					p.serialize((ferror_t)e.code);
				}
			}
		});
	}

	Bytes Manager::op_commit(seq_t seq, void* unpacker_v) {
		MsgPack::Unpacker* up = (MsgPack::Unpacker*)unpacker_v;
		std::vector<nid_t> filter;
		bool has_filter = false;
		if (up && up->isArray()) {
			const size_t n = up->unpackArraySize();
			has_filter = true;
			for (size_t i = 0; i < n; ++i) {
				if (up->isUInt() || up->isInt()) {
					fint_t v; up->deserialize(v);
					filter.push_back((nid_t)v);
				}
				else skip_value(*up);
			}
		}
		else if (up) skip_value(*up);

		size_t applied_total = 0;
		bool any_reboot = false;

		auto do_one = [&](Namespace& ns) {
			std::vector<fid_t> ids;
			for (const Field& f : ns.fields()) if (ns.has_draft(f.id)) ids.push_back(f.id);
			for (fid_t id : ids) {
				auto outcome = ns.commit_one(id);
				switch (outcome) {
					case Namespace::CommitOutcome::AppliedLive:
						++applied_total; break;
					case Namespace::CommitOutcome::AppliedReboot:
						++applied_total; any_reboot = true; break;
					default: break;
				}
			}
			if (_storage) _storage->save_namespace(ns);
		};

		if (has_filter) {
			for (nid_t id : filter) {
				Namespace* ns = _registry.find(id);
				if (ns) do_one(*ns);
			}
		}
		else {
			for (const auto& ns_ptr : _registry.namespaces()) do_one(*ns_ptr);
		}
		set_reboot_flag(any_reboot);

		return pack_response((opid_t)Op::Commit, seq, [&](MsgPack::Packer& p) {
			p.serialize(MsgPack::map_size_t(2));
			p.serialize((uint16_t)Key::Applied);     p.serialize((uint64_t)applied_total);
			p.serialize((uint16_t)Key::NeedsReboot); p.serialize((bool)_needs_reboot);
		});
	}

	Bytes Manager::op_discard(seq_t seq, void* unpacker_v) {
		MsgPack::Unpacker* up = (MsgPack::Unpacker*)unpacker_v;
		std::vector<nid_t> filter;
		bool has_filter = false;
		if (up && up->isArray()) {
			const size_t n = up->unpackArraySize();
			has_filter = true;
			for (size_t i = 0; i < n; ++i) {
				if (up->isUInt() || up->isInt()) {
					fint_t v; up->deserialize(v);
					filter.push_back((nid_t)v);
				}
				else skip_value(*up);
			}
		}
		else if (up) skip_value(*up);

		size_t cleared = 0;
		auto count_and_clear = [&](Namespace& ns) {
			for (const Field& f : ns.fields()) if (ns.has_draft(f.id)) ++cleared;
			ns.clear_draft();
		};
		if (has_filter) {
			for (nid_t id : filter) {
				Namespace* ns = _registry.find(id);
				if (ns) count_and_clear(*ns);
			}
		}
		else {
			for (const auto& ns_ptr : _registry.namespaces()) count_and_clear(*ns_ptr);
		}

		return pack_response((opid_t)Op::Discard, seq, [&](MsgPack::Packer& p) {
			p.serialize(MsgPack::map_size_t(1));
			p.serialize((uint16_t)Key::Applied); p.serialize((uint64_t)cleared);
		});
	}

	Bytes Manager::op_factory_reset(seq_t seq) {
		factory_reset();
		return pack_response((opid_t)Op::FactoryReset, seq, [&](MsgPack::Packer& p) {
			p.serialize(MsgPack::map_size_t(1));
			p.serialize((uint16_t)Key::NeedsReboot); p.serialize((bool)_needs_reboot);
		});
	}

	Bytes Manager::handle_message(const Bytes& request) {
		if (!_started) {
			return encode_error(0, 0, ErrorCode::NotInitialized, "Provisioning::begin not called");
		}
		MsgPack::Unpacker up;
		up.feed(request.data(), request.size());

		if (!up.isArray()) {
			return encode_error(0, 0, ErrorCode::MalformedRequest, "envelope must be array");
		}
		const size_t arr_size = up.unpackArraySize();
		if (arr_size < 1) {
			return encode_error(0, 0, ErrorCode::MalformedRequest, "envelope empty");
		}

		// Element 1: op id
		if (!(up.isUInt() || up.isInt())) {
			return encode_error(0, 0, ErrorCode::MalformedRequest, "op id must be uint");
		}
		fint_t op_raw = 0;
		up.deserialize(op_raw);
		const opid_t op_id = (opid_t)op_raw;

		// Element 2: seq (optional)
		seq_t seq = 0;
		if (arr_size >= 2) {
			if (up.isUInt() || up.isInt()) {
				fint_t s = 0; up.deserialize(s);
				seq = (seq_t)s;
			}
			else skip_value(up);
		}

		// Element 3: payload (optional). May be nil.
		const bool has_payload = (arr_size >= 3);

		switch ((Op)op_id) {
			case Op::GetSchema:       if (has_payload) skip_value(up); return op_get_schema(seq);
			case Op::GetInfo:         if (has_payload) skip_value(up); return op_get_info(seq);
			case Op::GetCapabilities: if (has_payload) skip_value(up); return op_get_capabilities(seq);
			case Op::GetState:        return op_get_state(seq, has_payload ? &up : nullptr);
			case Op::SetState:        return op_set_state(seq, has_payload ? &up : nullptr);
			case Op::Commit:          return op_commit(seq, has_payload ? &up : nullptr);
			case Op::Discard:         return op_discard(seq, has_payload ? &up : nullptr);
			case Op::FactoryReset:    if (has_payload) skip_value(up); return op_factory_reset(seq);
			default:
				return encode_error(op_id, seq, ErrorCode::UnknownOp, "unrecognised op id");
		}
	}

} }
