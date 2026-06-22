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

#include "Types.h"
#include "Registry.h"
#include "Storage.h"
#include "Value.h"
#include "Field.h"
#include "Ops.h"

#include "../Bytes.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace RNS { namespace Provisioning {

	class Provisioner;

	// Fluent builder for app-side namespace registration.
	//
	//   Provisioner::instance()
	//       .register_namespace("radio", 100)
	//       .field_float("frequency", 1, FF_REBOOT_REQUIRED, 915.0e6, 100e6, 1e9,
	//                    [&](const Value& v) { radio.frequency(v.as_float()); return true; })
	//       .field_int("sf", 4, FF_REBOOT_REQUIRED, 8, 7, 12)
	//       .end();
	class NamespaceBuilder {

	public:
		// Thin handle into the Provisioner's registration scope. Every fluent
		// call operates on Provisioner::current_build_scope() — which is the
		// most-recently-opened namespace, including nested ones.
		explicit NamespaceBuilder(Provisioner* mgr) : _mgr(mgr) {}

		// Open a child namespace under the current scope. Subsequent
		// field_*() / metric_*() / command_*() calls on this builder
		// will target the child until .end() is called.
		NamespaceBuilder& register_namespace(const char* name, nid_t id);

		// Close the current scope, returning the builder so chaining can
		// continue at the parent level (or beyond, if .end() pops the
		// root scope).
		NamespaceBuilder& end();

		// Each field_* overload optionally accepts a typed getter that
		// returns the field's native C++ type. When provided, Provisioner::field()
		// queries it for the live runtime value instead of returning a
		// cached working-map entry. Internally wrapped to produce a Value.
		NamespaceBuilder& field_bool(const char* name, fid_t id, fflags_t flags,
			fbool_t default_value,
			SetterFn setter = nullptr,
			std::function<fbool_t()> getter = nullptr);

		NamespaceBuilder& field_int(const char* name, fid_t id, fflags_t flags,
			fint_t default_value, fint_t imin, fint_t imax,
			SetterFn setter = nullptr,
			std::function<fint_t()> getter = nullptr);

		NamespaceBuilder& field_int(const char* name, fid_t id, fflags_t flags,
			fint_t default_value,
			SetterFn setter = nullptr,
			std::function<fint_t()> getter = nullptr);

		NamespaceBuilder& field_float(const char* name, fid_t id, fflags_t flags,
			ffloat_t default_value, ffloat_t fmin, ffloat_t fmax,
			SetterFn setter = nullptr,
			std::function<ffloat_t()> getter = nullptr);

		NamespaceBuilder& field_float(const char* name, fid_t id, fflags_t flags,
			ffloat_t default_value,
			SetterFn setter = nullptr,
			std::function<ffloat_t()> getter = nullptr);

		NamespaceBuilder& field_string(const char* name, fid_t id, fflags_t flags,
			const char* default_value, flen_t max_len = 0,
			SetterFn setter = nullptr,
			std::function<fstring_t()> getter = nullptr);

		NamespaceBuilder& field_bytes(const char* name, fid_t id, fflags_t flags,
			const fbytes_t& default_value, flen_t max_len = 0,
			SetterFn setter = nullptr,
			std::function<fbytes_t()> getter = nullptr);

		// List of byte arrays — e.g. a set of allowed destination hashes.
		// element_size = required size per entry (0 = variable),
		// max_count    = maximum number of entries (0 = unlimited).
		NamespaceBuilder& field_bytes_list(const char* name, fid_t id, fflags_t flags,
			fbytes_list_t default_value,
			flen_t element_size = 0, flen_t max_count = 0,
			SetterFn setter = nullptr,
			std::function<fbytes_list_t()> getter = nullptr);

		NamespaceBuilder& field_enum(const char* name, fid_t id, fflags_t flags,
			fenum_t default_value,
			std::vector<fenum_t> values,
			std::vector<fstring_t> labels,
			SetterFn setter = nullptr,
			std::function<fenum_t()> getter = nullptr);

		// -- Sugar for the two read-only / write-only patterns --
		//
		// metric_*: read-only field surfaced through a live getter. Useful
		// for counters, status, packet rates, sensor readings — anything
		// the app reports as instrumentation. Not persisted; SET_STATE
		// rejected at validation; getter polled on every read.
		//
		// command_*: write-only one-shot action. SET_STATE+COMMIT fires
		// the setter exactly once with the supplied argument. Not persisted,
		// not exposed in GET_STATE, not replayed on reboot. Reads return
		// None. Use these for "reboot now", "rotate identity", "ping",
		// "factory reset", and similar imperative gestures.

		NamespaceBuilder& metric_bool(const char* name, fid_t id,
			std::function<fbool_t()> getter);
		NamespaceBuilder& metric_int(const char* name, fid_t id,
			std::function<fint_t()> getter);
		NamespaceBuilder& metric_float(const char* name, fid_t id,
			std::function<ffloat_t()> getter);
		NamespaceBuilder& metric_string(const char* name, fid_t id,
			std::function<fstring_t()> getter);
		NamespaceBuilder& metric_bytes(const char* name, fid_t id,
			std::function<fbytes_t()> getter);

		NamespaceBuilder& command_bool(const char* name, fid_t id, SetterFn setter);
		NamespaceBuilder& command_int(const char* name, fid_t id,
			fint_t imin, fint_t imax, SetterFn setter);
		NamespaceBuilder& command_float(const char* name, fid_t id,
			ffloat_t fmin, ffloat_t fmax, SetterFn setter);
		NamespaceBuilder& command_string(const char* name, fid_t id,
			flen_t max_len, SetterFn setter);
		NamespaceBuilder& command_bytes(const char* name, fid_t id,
			flen_t max_len, SetterFn setter);

		// Argument-less command. SET_STATE fires the supplied no-arg setter
		// on commit; the wire entry is a nil placeholder and any value is
		// discarded. Useful for imperative gestures that take no parameter:
		// "reboot now", "factory reset", "rotate identity", "ping".
		NamespaceBuilder& command_void(const char* name, fid_t id,
			std::function<bool()> setter);

		// Register a pre-commit hook for the current namespace scope. Fires
		// once per commit pass on this namespace, only when at least one
		// draft entry exists, and BEFORE any field setter runs — so the
		// callback can validate the pending drafts and revert (clear_draft)
		// or amend (set_draft) them before commit_one promotes them into
		// working state. See Namespace::CommitCallback for semantics.
		NamespaceBuilder& on_commit(CommitCallback cb);

	private:
		Provisioner* _mgr;
	};

	// Singleton orchestrating schema, storage, working/draft, wire codec.
	class Provisioner {

	public:
		enum class Source : uint8_t {
			Working   = 0,	// current effective value
			Draft     = 1,	// pending edit (none if no draft entry)
			Effective = 2,	// working overlaid with draft
		};

		using RebootRequiredCallback = std::function<void()>;
		using RebootCallback         = std::function<void()>;
		using FactoryResetCallback   = std::function<void()>;

		static Provisioner& instance();

		// Idempotent: must be called once after the filesystem is registered.
		void begin(const char* storage_root = nullptr);
		void end();
		bool started() const { return _started; }

		// Register a namespace.
		NamespaceBuilder register_namespace(const char* name, nid_t id);

		// -- Wire entry point --------------------------------------------
		Bytes handle_message(const Bytes& request);

		// -- Direct accessors, by id -------------------------------------
		Value field(nid_t ns_id, fid_t field_id, Source = Source::Working) const;
		bool  field(nid_t ns_id, fid_t field_id, const Value& v);
		bool  commit(nid_t ns_id = 0);
		bool  discard(nid_t ns_id = 0);

		// -- Direct accessors, by name -----------------------------------
		Value field(const char* ns_name, const char* field_name, Source = Source::Working) const;
		bool  field(const char* ns_name, const char* field_name, const Value& v);
		bool  commit(const char* ns_name);
		bool  discard(const char* ns_name);

		bool  factory_reset();

		// Remove every persisted namespace file under the storage root,
		// without touching in-memory state (working maps, drafts) or the
		// runtime (no setter calls). Returns true if all files were removed
		// (a missing file counts as success); false if any removal failed.
		// No-op (returns true) when storage is disabled via RNS_USE_FS.
		bool  clear_storage();

		// -- State queries -----------------------------------------------
		bool needs_reboot() const { return _needs_reboot; }
		bool draft_has_reboot() const;

		// Passive: fires when a commit flips needs_reboot false->true (a
		// reboot-required field was committed). App typically surfaces a
		// "reboot needed" badge; the user triggers the actual reboot.
		void on_reboot_required(RebootRequiredCallback cb) { _on_reboot_required = std::move(cb); }

		// Active: fires when the client sent the explicit Reboot wire op.
		// App is expected to actually reboot the device. microReticulum
		// itself does not reboot; if no callback is registered the op
		// still ack's, but nothing happens.
		void on_reboot(RebootCallback cb) { _on_reboot = std::move(cb); }

		// Fires after factory_reset() has completed its internal reset
		// (working values restored, setters fired, storage cleared). Lets
		// the app perform additional cleanup outside Provisioning's
		// storage root.
		void on_factory_reset(FactoryResetCallback cb) { _on_factory_reset = std::move(cb); }

		// -- Introspection (for tests, debug, host bindings) --------------
		Registry& registry() { return _registry; }
		const Registry& registry() const { return _registry; }
		Storage* storage() { return _storage.get(); }

		// -- Schema/version constants ------------------------------------
		// v2 adds the parent_id element to each namespace entry in the
		// GET_SCHEMA response (4-tuple instead of 3-tuple). v1 clients that
		// stop after reading the first three elements remain compatible.
		static constexpr nid_t SCHEMA_VERSION = 2;

		// CRC32 of the serialized GetSchema payload, computed once at the
		// end of begin(). Surfaced in GetInfo as Key::SchemaHash so clients
		// can cache the schema by hash and skip re-fetching when unchanged.
		uint32_t schema_hash() const { return _schema_hash; }

	private:
		Provisioner() = default;
		Provisioner(const Provisioner&) = delete;
		Provisioner& operator=(const Provisioner&) = delete;

		Registry _registry;
		std::unique_ptr<Storage> _storage;
		bool _started = false;
		bool _needs_reboot = false;
		uint32_t _schema_hash = 0;
		RebootRequiredCallback _on_reboot_required;
		RebootCallback         _on_reboot;
		FactoryResetCallback   _on_factory_reset;

		// Active namespace registration scope. Pushed by register_namespace() (incl.
		// the nested-builder form) and popped by NamespaceBuilder::end().
		// Must be empty by the time begin() is called; assert there.
		std::vector<Namespace*> _build_scope;

	public:
		// Accessors used by NamespaceBuilder during fluent registration.
		// Public so the builder doesn't need friend access; they're cheap
		// and don't need to be hidden.
		Namespace* current_build_scope() {
			return _build_scope.empty() ? nullptr : _build_scope.back();
		}
		void pop_build_scope() {
			if (!_build_scope.empty()) _build_scope.pop_back();
		}
		void push_build_scope(Namespace* ns) { _build_scope.push_back(ns); }
		bool build_scope_empty() const { return _build_scope.empty(); }

	private:

		// Internal helpers for the wire dispatch path.
		Bytes encode_error(opid_t op_id, seq_t seq, ErrorCode code, const char* msg = nullptr);
		Bytes encode_ack(opid_t op_id, seq_t seq);

		// Per-op response builders. The unpacker is positioned at the
		// envelope's payload value (which may be nil / map / array depending
		// on op), or null if the envelope had no payload element. The
		// returned Bytes is a fully framed response. Each handler reads its
		// payload (including the optional ReqCompress flag) and calls
		// pack_response with the resolved compress decision.
		class Unpacker;	// forward; defined in .cpp
		Bytes op_get_schema(seq_t seq, void* unpacker);
		Bytes op_get_info(seq_t seq, void* unpacker);
		Bytes op_get_capabilities(seq_t seq, void* unpacker);
		Bytes op_get_state(seq_t seq, void* unpacker);
		Bytes op_set_state(seq_t seq, void* unpacker);
		Bytes op_commit(seq_t seq, void* unpacker);
		Bytes op_discard(seq_t seq, void* unpacker);
		Bytes op_factory_reset(seq_t seq, void* unpacker);
		Bytes op_reboot(seq_t seq, void* unpacker);

		void  set_reboot_flag(bool any_reboot_applied);

		// After Storage::load_all() has overlaid disk values into the
		// working map, walk every field with a non-null setter and push
		// the working value through. Fires for both FF_LIVE_APPLY and
		// FF_REBOOT_REQUIRED fields — at boot the reboot has by definition
		// already happened, so REBOOT_REQUIRED's deferred-apply is
		// satisfied here. Post-boot commits keep the original behaviour
		// (REBOOT_REQUIRED setters do not fire on commit).
		void  apply_loaded_to_runtime();
	};

} }
