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

#include <stdint.h>

// IMPORTANT: ids in this file are *permanent*. See docs/provisioning_id_registry.md.
// Append new fields only. To retire a field, leave its slot reserved.

namespace RNS { namespace Provisioning { namespace Ns {

	// Reticulum namespace — core stack-level toggles.
	namespace Reticulum {
		constexpr uint16_t Id = 1;
		namespace Field {
			constexpr uint16_t SchemaVersion           = 0;	// reserved across all namespaces
			constexpr uint16_t TransportEnabled        = 1;
			constexpr uint16_t LinkMtuDiscovery        = 2;
			constexpr uint16_t RemoteManagementEnabled = 3;
			constexpr uint16_t ProbeDestinationEnabled = 4;
			constexpr uint16_t UseImplicitProof        = 5;
			constexpr uint16_t PersistInterval         = 6;
			constexpr uint16_t CleanInterval           = 7;
			// next-id: 8
		}
	}

	// Transport namespace — routing-table sizing tunables.
	namespace Transport {
		constexpr uint16_t Id = 2;
		namespace Field {
			constexpr uint16_t SchemaVersion		= 0;	// reserved
			constexpr uint16_t PathTableMaxsize     = 1;
			constexpr uint16_t AnnounceTableMaxsize = 2;
			constexpr uint16_t HashlistMaxsize      = 3;
			constexpr uint16_t MaxPrTags            = 4;
			constexpr uint16_t PathTableMaxpersist  = 5;
			// next-id: 6
		}
	}

	// Namespace id 3 was used for an "identity" prototype with a single
	// read-only IdentityHash field. Dropped because the library has no
	// canonical identity to expose. Per the id-stability rules above,
	// id 3 is permanently reserved and must not be re-used. Apps that
	// want their own identity-style namespace should pick an id in the
	// 100-199 (official-app) or 200+ (vendor) range.

} } }
