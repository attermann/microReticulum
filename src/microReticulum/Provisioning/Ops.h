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

#include <stdint.h>

namespace RNS { namespace Provisioning {

	// Operation IDs. Identical ids are used in both requests and responses
	// (response of op X always echoes X unless an error occurred, in which
	// case the response op is Error).
	enum class Op : opid_t {
		GetSchema       = 1,
		GetInfo         = 2,
		GetCapabilities = 3,
		GetState        = 4,
		SetState        = 5,
		Commit          = 6,
		Discard         = 7,
		FactoryReset    = 8,
		Ack             = 100,
		Error           = 101,
	};

	// Error codes returned in Error responses.
	enum class ErrorCode : ferror_t {
		Ok                  = 0,
		MalformedRequest    = 1,
		UnknownOp           = 2,
		UnknownNamespace    = 3,
		UnknownField        = 4,
		InvalidValue        = 5,
		ConstraintViolation = 6,
		ReadOnly            = 7,
		StorageError        = 8,
		NotInitialized      = 9,
		Internal            = 99,
	};

	// Map-key constants used inside MsgPack payloads. Envelopes themselves
	// are positional arrays ([op, seq, payload]) and don't need keys;
	// GetSchema / GetCapabilities also use positional arrays for ns entries.
	// Only keys actually referenced by the wire codec are listed here.
	namespace Key {
		// Error payload keys
		constexpr uint16_t ErrorCodeKey   = 1;
		constexpr uint16_t ErrorMessage   = 2;

		// Commit/SetState response keys
		constexpr uint16_t Applied        = 1;
		constexpr uint16_t NeedsReboot    = 2;
		constexpr uint16_t DraftHasReboot = 2;	// alias for SetState
		constexpr uint16_t FieldErrors    = 3;

		// GetState / SetState request flags
		constexpr uint16_t NamespaceFilter = 1;
		constexpr uint16_t Pending         = 2;

		// GetInfo
		constexpr uint16_t FirmwareVersion = 1;
		constexpr uint16_t SchemaVersion   = 2;
		constexpr uint16_t NeedsRebootInfo = 3;

		// GetSchema field metadata (per-field map keys)
		constexpr uint16_t FieldId          = 1;
		constexpr uint16_t FieldName        = 2;
		constexpr uint16_t FieldType        = 3;
		constexpr uint16_t FieldFlags       = 4;
		constexpr uint16_t FieldMinI        = 5;
		constexpr uint16_t FieldMaxI        = 6;
		constexpr uint16_t FieldMinF        = 7;
		constexpr uint16_t FieldMaxF        = 8;
		constexpr uint16_t FieldMaxLen      = 9;
		constexpr uint16_t FieldEnumValues  = 10;
		constexpr uint16_t FieldEnumLabels  = 11;
		constexpr uint16_t FieldDefault     = 12;
		constexpr uint16_t FieldElementSize = 13;	// BytesList: required size per entry
		constexpr uint16_t FieldMaxCount    = 14;	// BytesList: max number of entries
	}

} }
