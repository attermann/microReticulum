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

#include "../Bytes.h"

#include <stdint.h>
#include <string>
#include <vector>

namespace RNS { namespace Provisioning {

	// Centralised type aliases used throughout the Provisioning subsystem.
	// Integrators should reference these names rather than the underlying
	// raw types — that keeps call sites readable and lets the underlying
	// widths evolve without touching downstream code.

	// -- Wire envelope and addressing ---------------------------------------
	using opid_t        = uint8_t;              // Op-code (handle_message envelope)
	using seq_t         = uint64_t;             // Request/response sequence number
	using nid_t         = uint16_t;             // Namespace id
	using fid_t         = uint16_t;             // Field id
	using ferror_t      = uint16_t;             // Error code (ErrorCode enum + wire)

	// -- Field metadata -----------------------------------------------------
	using fflags_t      = uint8_t;              // FieldFlags bitfield
	using flen_t        = size_t;               // Constraint length / count

	// -- Field-value storage types ------------------------------------------
	// One alias per declared field Type. fbool_t / fint_t / fenum_t / ffloat_t
	// match the natural C++ type for that field kind; fstring_t / fbytes_t /
	// fbytes_list_t name the variable-sized containers.
	using fbool_t       = bool;
	using fint_t        = int64_t;
	using fenum_t       = int64_t;              // wire-equivalent to fint_t; schema carries labels
	using ffloat_t      = double;
	using fstring_t     = std::string;
	using fbytes_t      = RNS::Bytes;
	using fbytes_list_t = std::vector<fbytes_t>;

} }
