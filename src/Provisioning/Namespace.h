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

#include "Field.h"

#include <stdint.h>
#include <string>
#include <unordered_map>
#include <vector>

namespace RNS { namespace Provisioning {

	class Namespace {

	public:
		Namespace(nid_t id, const char* name, nid_t parent_id = 0)
			: _id(id), _name(name ? name : ""), _parent_id(parent_id) {}

		nid_t id() const { return _id; }
		const fstring_t& name() const { return _name; }
		// 0 = root (no parent). Non-zero values reference another namespace
		// id in the same Registry, forming a tree used for UI grouping and
		// disk filename construction.
		nid_t parent_id() const { return _parent_id; }

		// Append a field. Returns false if id or name is already taken.
		bool add_field(Field f);

		const Field* find_field(fid_t id) const;
		const Field* find_field(const char* name) const;

		const std::vector<Field>& fields() const { return _fields; }

		// Effective working value for a field (returns default if not set).
		// Returns a none-typed Value if the field id is unknown.
		Value working(fid_t field_id) const;

		// "Effective" value for a field — queries the field's getter callback
		// if it has one (so the value reflects the live runtime), falling
		// back to the cached working map otherwise. Read- and save-side
		// callers should prefer this over working() to avoid drift when
		// direct setters mutate the runtime out-of-band.
		Value effective(fid_t field_id) const;

		// Returns true and writes into 'out' if a draft entry exists for this field.
		bool draft(fid_t field_id, Value& out) const;
		bool has_draft(fid_t field_id) const;

		// Write to draft after validating against the field's constraint.
		// Returns false if the field is unknown, read-only, or invalid.
		bool set_draft(fid_t field_id, const Value& v);

		// Drop a single draft entry, or all of them.
		void clear_draft(fid_t field_id);
		void clear_draft();

		// True iff the current draft touches any FF_REBOOT_REQUIRED field.
		bool draft_has_reboot() const;

		// Commit a single draft entry into working state and invoke its
		// setter when FF_LIVE_APPLY. Removes the draft entry afterward.
		enum class CommitOutcome {
			NoChange,        // no draft for this field
			AppliedLive,     // promoted and live-applied
			AppliedReboot,   // promoted; reboot required for effect
			InvalidValue,    // draft value failed constraint
			Unknown,         // field id not in this namespace
			ReadOnly,        // field is read-only
			SetterFailed,    // FF_LIVE_APPLY setter returned false
		};
		CommitOutcome commit_one(fid_t field_id);

		// True iff any working value differs from the initial default
		// (used by storage to decide whether to write a file).
		bool dirty_for_persist() const { return _dirty_for_persist; }
		void mark_clean() { _dirty_for_persist = false; }

		// Directly set working value (used by Storage on load and by
		// commit_one). No validation, no setter invocation.
		void put_working(fid_t field_id, const Value& v);

	private:
		nid_t _id;
		fstring_t _name;
		nid_t _parent_id = 0;
		std::vector<Field> _fields;
		std::unordered_map<fid_t, size_t> _id_index;
		std::unordered_map<fstring_t, size_t> _name_index;
		std::unordered_map<fid_t, Value> _working;
		std::unordered_map<fid_t, Value> _draft;
		bool _dirty_for_persist = false;
	};

} }
