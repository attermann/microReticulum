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

#include "Value.h"

#include <stdint.h>
#include <functional>
#include <string>
#include <vector>

namespace RNS { namespace Provisioning {

	enum FieldFlags : uint8_t {
		FF_NONE            = 0,
		FF_LIVE_APPLY      = 1 << 0,	// setter called immediately on commit
		FF_REBOOT_REQUIRED = 1 << 1,	// committed value takes effect after reboot
		FF_READ_ONLY       = 1 << 2,	// no setter; not modifiable from wire; not persisted
		FF_SECRET          = 1 << 3,	// excluded from GET_STATE responses
		// Write-only command field. SET_STATE+COMMIT fires the setter as
		// a one-shot action — the value is consumed, not stored or persisted.
		// Excluded from GET_STATE (no state to report) and from apply-on-boot
		// (the action shouldn't replay on every reboot). Reads return None.
		FF_WRITE_ONLY      = 1 << 4,
	};

	struct Constraint {
		bool has_range  = false;
		int64_t imin    = 0;
		int64_t imax    = 0;
		double  fmin    = 0.0;
		double  fmax    = 0.0;
		size_t  max_len = 0;
		// BytesList-only: required exact byte size per element (0 = variable),
		// and maximum number of elements (0 = unlimited).
		size_t  element_size = 0;
		size_t  max_count    = 0;
		std::vector<int64_t>     enum_values;
		std::vector<std::string> enum_labels;
	};

	using SetterFn = std::function<bool(const Value&)>;

	// Returns the live runtime value of the field. When set, this is the
	// authoritative source for reads — Provisioning becomes a thin view over
	// the runtime static rather than a separate cached value. Optional; if
	// null, reads fall back to the per-namespace working map.
	using GetterFn = std::function<Value()>;

	struct Field {
		uint16_t      id      = 0;
		std::string   name;
		Type          type    = Type::None;
		uint8_t       flags   = FF_NONE;
		Constraint    constraint;
		Value         default_value;
		SetterFn      setter; // optional; invoked on commit for FF_LIVE_APPLY
		GetterFn      getter; // optional; queried on read/save when present

		bool has_flag(FieldFlags f) const { return (flags & f) != 0; }

		// Returns true iff v is type-compatible with this field and satisfies
		// any declared constraint. Does NOT apply the value anywhere.
		bool validate(const Value& v) const;
	};

} }
