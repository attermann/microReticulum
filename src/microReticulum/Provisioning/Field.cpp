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

#include "Field.h"

namespace RNS { namespace Provisioning {

	bool Field::validate(const Value& v) const {
		if (v.type() != type) return false;
		switch (type) {
			case Type::None:
				return false;
			case Type::Bool:
				return true;
			case Type::Int: {
				if (constraint.has_range) {
					fint_t iv = v.as_int();
					if (iv < constraint.imin || iv > constraint.imax) return false;
				}
				return true;
			}
			case Type::Float: {
				if (constraint.has_range) {
					ffloat_t fv = v.as_float();
					if (fv < constraint.fmin || fv > constraint.fmax) return false;
				}
				return true;
			}
			case Type::String:
				if (constraint.max_len > 0 && v.as_string().size() > constraint.max_len) return false;
				return true;
			case Type::Bytes:
				if (constraint.max_len > 0 && v.as_bytes().size() > constraint.max_len) return false;
				return true;
			case Type::Enum: {
				fenum_t iv = v.as_int();
				if (constraint.enum_values && constraint.enum_count > 0) {
					for (flen_t i = 0; i < constraint.enum_count; ++i) {
						if (constraint.enum_values[i] == iv) return true;
					}
					return false;
				}
				return true;
			}
			case Type::BytesList: {
				const auto& list = v.as_bytes_list();
				if (constraint.max_count > 0 && list.size() > constraint.max_count) return false;
				if (constraint.element_size > 0) {
					for (const fbytes_t& e : list) {
						if (e.size() != constraint.element_size) return false;
					}
				}
				return true;
			}
			case Type::Void:
				// Argument-less command marker — nothing to validate.
				return v.type() == Type::Void;
		}
		return false;
	}

} }
