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
		FF_READ_ONLY       = 1 << 2,	// no setter; not modifiable from wire
		FF_SECRET          = 1 << 3,	// excluded from GET_STATE responses
	};

	struct Constraint {
		bool has_range  = false;
		int64_t imin    = 0;
		int64_t imax    = 0;
		double  fmin    = 0.0;
		double  fmax    = 0.0;
		size_t  max_len = 0;
		std::vector<int64_t>     enum_values;
		std::vector<std::string> enum_labels;
	};

	using SetterFn = std::function<bool(const Value&)>;

	struct Field {
		uint16_t      id      = 0;
		std::string   name;
		Type          type    = Type::None;
		uint8_t       flags   = FF_NONE;
		Constraint    constraint;
		Value         default_value;
		SetterFn      setter; // optional; invoked on commit for FF_LIVE_APPLY

		bool has_flag(FieldFlags f) const { return (flags & f) != 0; }

		// Returns true iff v is type-compatible with this field and satisfies
		// any declared constraint. Does NOT apply the value anywhere.
		bool validate(const Value& v) const;
	};

} }
