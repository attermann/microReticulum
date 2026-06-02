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

#include "Namespace.h"

namespace RNS { namespace Provisioning {

	bool Namespace::add_field(Field f) {
		if (_id_index.count(f.id) != 0) return false;
		if (!f.name.empty() && _name_index.count(f.name) != 0) return false;
		size_t idx = _fields.size();
		_id_index[f.id] = idx;
		if (!f.name.empty()) _name_index[f.name] = idx;
		// Seed working with the field's declared default.
		_working[f.id] = f.default_value;
		_fields.push_back(std::move(f));
		return true;
	}

	const Field* Namespace::find_field(uint16_t id) const {
		auto it = _id_index.find(id);
		if (it == _id_index.end()) return nullptr;
		return &_fields[it->second];
	}

	const Field* Namespace::find_field(const char* name) const {
		if (!name) return nullptr;
		auto it = _name_index.find(name);
		if (it == _name_index.end()) return nullptr;
		return &_fields[it->second];
	}

	Value Namespace::working(uint16_t field_id) const {
		auto it = _working.find(field_id);
		if (it == _working.end()) return {};
		return it->second;
	}

	Value Namespace::effective(uint16_t field_id) const {
		const Field* f = find_field(field_id);
		if (!f) return {};
		// Write-only command fields have no readable state — the value is
		// consumed by the setter at commit and not retained anywhere.
		if (f->has_flag(FF_WRITE_ONLY)) return {};
		if (f->getter) return f->getter();
		return working(field_id);
	}

	bool Namespace::draft(uint16_t field_id, Value& out) const {
		auto it = _draft.find(field_id);
		if (it == _draft.end()) return false;
		out = it->second;
		return true;
	}

	bool Namespace::has_draft(uint16_t field_id) const {
		return _draft.count(field_id) != 0;
	}

	bool Namespace::set_draft(uint16_t field_id, const Value& v) {
		const Field* f = find_field(field_id);
		if (!f) return false;
		if (f->has_flag(FF_READ_ONLY)) return false;
		if (!f->validate(v)) return false;
		// For stateful fields, if the new draft value equals the current
		// working value, drop the draft entry — keeps draft_has_reboot()
		// honest about whether anything is actually pending.
		//
		// WRITE_ONLY fields skip this dedup: every set is a new command
		// invocation, even if the argument value happens to match a prior
		// invocation.
		if (!f->has_flag(FF_WRITE_ONLY)) {
			Value cur = working(field_id);
			if (cur == v) {
				_draft.erase(field_id);
				return true;
			}
		}
		_draft[field_id] = v;
		return true;
	}

	void Namespace::clear_draft(uint16_t field_id) {
		_draft.erase(field_id);
	}

	void Namespace::clear_draft() {
		_draft.clear();
	}

	bool Namespace::draft_has_reboot() const {
		for (const auto& kv : _draft) {
			const Field* f = find_field(kv.first);
			if (f && f->has_flag(FF_REBOOT_REQUIRED)) return true;
		}
		return false;
	}

	void Namespace::put_working(uint16_t field_id, const Value& v) {
		_working[field_id] = v;
		_dirty_for_persist = true;
	}

	Namespace::CommitOutcome Namespace::commit_one(uint16_t field_id) {
		const Field* f = find_field(field_id);
		if (!f) return CommitOutcome::Unknown;
		auto it = _draft.find(field_id);
		if (it == _draft.end()) return CommitOutcome::NoChange;
		Value v = it->second;
		if (f->has_flag(FF_READ_ONLY)) {
			_draft.erase(it);
			return CommitOutcome::ReadOnly;
		}
		if (!f->validate(v)) {
			return CommitOutcome::InvalidValue;
		}
		_draft.erase(it);

		// Write-only command field: fire the setter as a one-shot action.
		// Do not promote to working, do not mark dirty, do not persist.
		// Subsequent reads return None and the action does not replay on
		// reboot (working map stays at the seeded default).
		if (f->has_flag(FF_WRITE_ONLY)) {
			if (f->setter && !f->setter(v)) return CommitOutcome::SetterFailed;
			return CommitOutcome::AppliedLive;
		}

		// Stateful field: promote into working and persist-dirty regardless
		// of live/reboot flag — the file on disk is the source of truth
		// for next boot.
		_working[field_id] = v;
		_dirty_for_persist = true;

		if (f->has_flag(FF_LIVE_APPLY) && f->setter) {
			if (!f->setter(v)) return CommitOutcome::SetterFailed;
			return CommitOutcome::AppliedLive;
		}
		if (f->has_flag(FF_REBOOT_REQUIRED)) {
			return CommitOutcome::AppliedReboot;
		}
		// Field has neither LIVE nor REBOOT — value is stored but no runtime
		// effect declared. Treat as applied-live (no-op).
		return CommitOutcome::AppliedLive;
	}

} }
