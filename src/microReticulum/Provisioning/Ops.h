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
		Reboot          = 9,
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

	// Envelope and response keys. Same set on both directions.
	namespace Key {
		constexpr uint16_t Op             = 1;
		constexpr uint16_t Seq            = 2;
		constexpr uint16_t Payload        = 3;

		// Error payload keys
		constexpr uint16_t ErrorCodeKey   = 1;
		constexpr uint16_t ErrorMessage   = 2;
		constexpr uint16_t ErrorNamespace = 3;
		constexpr uint16_t ErrorField     = 4;

		// Commit/SetState response keys
		constexpr uint16_t Applied        = 1;
		constexpr uint16_t NeedsReboot    = 2;
		constexpr uint16_t DraftHasReboot = 2;	// alias for SetState
		constexpr uint16_t FieldErrors    = 3;
		// Post-op state returned in Commit and SetState responses when
		// IncludeState was requested. Slots 4-6 avoid collision with
		// Applied(1) / NeedsReboot(2) / FieldErrors(3) in the enclosing
		// response body. Commit only emits Values+Hash (drafts are cleared
		// by commit); SetState emits Values+Drafts+Hash (drafts persist).
		constexpr uint16_t PostOpValues   = 4;
		constexpr uint16_t PostOpDrafts   = 5;
		constexpr uint16_t PostOpHash     = 6;

		// GetState / SetState / Commit request flags
		constexpr uint16_t NamespaceFilter = 1;
		constexpr uint16_t Draft           = 2;	// GetState request: true = also include drafts alongside working
		constexpr uint16_t State           = 3;	// the {ns: {field: value}} map
		constexpr uint16_t PriorHash       = 4;	// GetState request: client-supplied CRC32 for cache short-circuit
		constexpr uint16_t IncludeState    = 5;	// Commit request: return post-commit Values in the response

		// GetState response body
		constexpr uint16_t Values          = 1;	// {ns_id: {field_id: value}} map (working values)
		constexpr uint16_t Drafts          = 2;	// sparse {ns_id: {field_id: value}} — only ns/fields with drafts
		constexpr uint16_t Hash            = 3;	// CRC32 over the response body; echoed back as PriorHash
		constexpr uint16_t Unchanged       = 4;	// present iff the client's PriorHash matched

		// GetInfo
		constexpr uint16_t FirmwareVersion = 1;
		constexpr uint16_t SchemaVersion   = 2;
		constexpr uint16_t NeedsRebootInfo = 3;
		constexpr uint16_t SchemaHash      = 4;	// CRC32 of serialized GetSchema response bytes

		// GetCapabilities
		constexpr uint16_t Namespaces      = 1;

		// GetSchema field metadata
		constexpr uint16_t SchemaNamespaces = 1;
		constexpr uint16_t NsId             = 1;
		constexpr uint16_t NsName           = 2;
		constexpr uint16_t NsFields         = 3;
		constexpr uint16_t NsParent         = 4;	// optional parent ns id (0 = root)
		constexpr uint16_t NsFieldCount     = 5;	// GetCapabilities map: number of fields
		constexpr uint16_t NsSchemaHash     = 6;	// GetCapabilities map: per-namespace CRC32
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

		// Per-request compression negotiation. Clients set ReqCompress=true
		// in the request payload map to ask the server to compress the
		// response. When the server complies, the response payload becomes
		// a single-entry map { CompressedPayload: <bin> } whose value is
		// heatshrink-compressed bytes that decompress to the original
		// MsgPack-encoded payload. Servers always pick the smaller wire
		// form and may send an uncompressed payload even when compression
		// was requested (tiny payloads where the wrapper overhead exceeds
		// the savings).
		constexpr uint16_t ReqCompress       = 100;
		constexpr uint16_t CompressedPayload = 101;
	}

} }
