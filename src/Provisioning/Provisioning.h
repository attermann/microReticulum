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

	class Manager;

	// Fluent builder for app-side namespace registration.
	//
	//   Manager::instance()
	//       .namespace_("radio", 100)
	//       .field_float("frequency", 1, FF_REBOOT_REQUIRED, 915.0e6, 100e6, 1e9,
	//                    [&](const Value& v) { radio.frequency(v.as_float()); return true; })
	//       .field_int("sf", 4, FF_REBOOT_REQUIRED, 8, 7, 12)
	//       .end();
	class NamespaceBuilder {

	public:
		NamespaceBuilder(Namespace* ns) : _ns(ns) {}

		NamespaceBuilder& field_bool(const char* name, uint16_t id, uint8_t flags,
			bool default_value, SetterFn setter = nullptr);

		NamespaceBuilder& field_int(const char* name, uint16_t id, uint8_t flags,
			int64_t default_value, int64_t imin, int64_t imax, SetterFn setter = nullptr);

		NamespaceBuilder& field_int(const char* name, uint16_t id, uint8_t flags,
			int64_t default_value, SetterFn setter = nullptr);

		NamespaceBuilder& field_float(const char* name, uint16_t id, uint8_t flags,
			double default_value, double fmin, double fmax, SetterFn setter = nullptr);

		NamespaceBuilder& field_float(const char* name, uint16_t id, uint8_t flags,
			double default_value, SetterFn setter = nullptr);

		NamespaceBuilder& field_string(const char* name, uint16_t id, uint8_t flags,
			const char* default_value, size_t max_len = 0, SetterFn setter = nullptr);

		NamespaceBuilder& field_bytes(const char* name, uint16_t id, uint8_t flags,
			const Bytes& default_value, size_t max_len = 0, SetterFn setter = nullptr);

		NamespaceBuilder& field_enum(const char* name, uint16_t id, uint8_t flags,
			int64_t default_value,
			std::vector<int64_t> values,
			std::vector<std::string> labels,
			SetterFn setter = nullptr);

		// Optional terminator for readability; the chain is also fine
		// without it (NamespaceBuilder is a value, not a guard).
		void end() {}

	private:
		Namespace* _ns;
	};

	// Singleton orchestrating schema, storage, working/draft, wire codec.
	class Manager {

	public:
		enum class Source : uint8_t {
			Working   = 0,	// current effective value
			Draft     = 1,	// pending edit (none if no draft entry)
			Effective = 2,	// working overlaid with draft
		};

		using RebootRequestedCallback = std::function<void()>;

		static Manager& instance();

		// Idempotent: must be called once after the filesystem is registered.
		// storage_root defaults to "/config".
		void begin(const char* storage_root = "/config");
		void end();
		bool started() const { return _started; }

		// Register an app-defined namespace.
		NamespaceBuilder namespace_(const char* name, uint16_t id);

		// Register a built-in namespace (same backing call as namespace_
		// but named for symmetry with the RNS_PROVISION_NAMESPACE macros).
		NamespaceBuilder register_namespace(const char* name, uint16_t id) {
			return namespace_(name, id);
		}

		// -- Wire entry point --------------------------------------------
		Bytes handle_message(const Bytes& request);

		// -- Direct accessors, by id -------------------------------------
		Value field(uint16_t ns_id, uint16_t field_id, Source = Source::Working) const;
		bool  field(uint16_t ns_id, uint16_t field_id, const Value& v);
		bool  commit(uint16_t ns_id = 0);
		bool  discard(uint16_t ns_id = 0);

		// -- Direct accessors, by name -----------------------------------
		Value field(const char* ns_name, const char* field_name, Source = Source::Working) const;
		bool  field(const char* ns_name, const char* field_name, const Value& v);
		bool  commit(const char* ns_name);
		bool  discard(const char* ns_name);

		bool  factory_reset();

		// -- State queries -----------------------------------------------
		bool needs_reboot() const { return _needs_reboot; }
		bool draft_has_reboot() const;

		void on_reboot_requested(RebootRequestedCallback cb) { _on_reboot = std::move(cb); }

		// -- Introspection (for tests, debug, host bindings) --------------
		Registry& registry() { return _registry; }
		const Registry& registry() const { return _registry; }
		Storage* storage() { return _storage.get(); }

		// -- Schema/version constants ------------------------------------
		static constexpr uint16_t SCHEMA_VERSION = 1;

	private:
		Manager() = default;
		Manager(const Manager&) = delete;
		Manager& operator=(const Manager&) = delete;

		Registry _registry;
		std::unique_ptr<Storage> _storage;
		bool _started = false;
		bool _needs_reboot = false;
		RebootRequestedCallback _on_reboot;

		// Internal helpers for the wire dispatch path.
		Bytes encode_error(uint8_t op_id, uint64_t seq, ErrorCode code, const char* msg = nullptr);
		Bytes encode_ack(uint8_t op_id, uint64_t seq);

		// Per-op response builders. The unpacker is positioned at the
		// envelope's payload value (which may be nil / map / array depending
		// on op). The returned Bytes is a fully framed response.
		class Unpacker;	// forward; defined in .cpp
		Bytes op_get_schema(uint64_t seq);
		Bytes op_get_info(uint64_t seq);
		Bytes op_get_capabilities(uint64_t seq);
		Bytes op_get_state(uint64_t seq, void* unpacker);
		Bytes op_set_state(uint64_t seq, void* unpacker);
		Bytes op_commit(uint64_t seq, void* unpacker);
		Bytes op_discard(uint64_t seq, void* unpacker);
		Bytes op_factory_reset(uint64_t seq);

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
