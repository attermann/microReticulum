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

// IMPORTANT: ids in this file are *permanent*. See docs/provisioning_id_registry.md.
// Append new fields only. To retire a field, leave its slot reserved.

namespace RNS { namespace Provisioning { namespace Ns {

	// General Config namespace — core stack-level toggles.
	namespace GeneralConfig {
		constexpr nid_t Id = 1;
		namespace Field {
			constexpr fid_t SchemaVersion           = 0;	// reserved across all namespaces
			constexpr fid_t TransportEnabled        = 1;
			constexpr fid_t LinkMtuDiscovery        = 2;
			constexpr fid_t RemoteManagementEnabled = 3;
			constexpr fid_t ProbeDestinationEnabled = 4;
			constexpr fid_t UseImplicitProof        = 5;
			constexpr fid_t PersistInterval         = 6;
			constexpr fid_t CleanInterval           = 7;
			constexpr fid_t RemoteManagementAllowed = 8;	// BytesList of 16-byte dest hashes
			constexpr fid_t TransportIdentity       = 9;	// Bytes (64) — private key; SECRET
			constexpr fid_t ClearStorage            = 10;	// command (write-only): wipe persisted provisioning files
		}
	}

	// Transport Config namespace — routing-table sizing tunables.
	namespace TransportConfig {
		constexpr nid_t Id = 2;
		namespace Field {
			constexpr fid_t SchemaVersion		 = 0;	// reserved
			constexpr fid_t PathTableMaxsize     = 1;
			constexpr fid_t AnnounceTableMaxsize = 2;
			constexpr fid_t HashlistMaxsize      = 3;
			constexpr fid_t MaxPrTags            = 4;
			constexpr fid_t PathTableMaxpersist  = 5;
			constexpr fid_t ClearStorage         = 6;	// command (write-only): Transport::clear_storage()
		}
	}

	// Memory namespace - heap, flash, allocators, etc.
	namespace Memory {
		constexpr nid_t Id = 50;
		namespace Field {
			constexpr fid_t SchemaVersion		     = 0;	// reserved
		}
		namespace Heap {
			constexpr nid_t Id = 51;
			namespace Field {
				constexpr fid_t SchemaVersion		 = 0;	// reserved
				constexpr fid_t Size                 = 1;
				constexpr fid_t Free                 = 2;
				constexpr fid_t FreePct              = 3;
				constexpr fid_t MinFree              = 4;
				constexpr fid_t MaxAlloc             = 5;
				constexpr fid_t Fragmented           = 6;
			}
		}
		namespace Psram {
			constexpr nid_t Id = 52;
			namespace Field {
				constexpr fid_t SchemaVersion		 = 0;	// reserved
				constexpr fid_t Size                 = 1;
				constexpr fid_t Free                 = 2;
				constexpr fid_t FreePct              = 3;
				constexpr fid_t MinFree              = 4;
				constexpr fid_t MaxAlloc             = 5;
				constexpr fid_t Fragmented           = 6;
			}
		}
		namespace Flash {
			constexpr nid_t Id = 53;
			namespace Field {
				constexpr fid_t SchemaVersion		 = 0;	// reserved
				constexpr fid_t Size                 = 1;
				constexpr fid_t Free                 = 2;
				constexpr fid_t FreePct              = 3;
			}
		}
	}

	// Storage namespace — memory and flash consumers
	namespace Storage {
		constexpr nid_t Id = 54;
		namespace Field {
			constexpr fid_t SchemaVersion		     = 0;	// reserved
			constexpr fid_t Paths                    = 1;
			constexpr fid_t Destinations             = 2;
			constexpr fid_t Announces                = 3;
			constexpr fid_t HeldAnnounces            = 4;

			constexpr fid_t PathRequests             = 5;
			constexpr fid_t DiscoveryPathRequests    = 6;
			constexpr fid_t PendingLocalPathRequests = 7;
			constexpr fid_t DiscoveryPrTags          = 8;
			constexpr fid_t ControlDestinations      = 9;
			constexpr fid_t ControlHashes            = 10;

			constexpr fid_t PacketHashes             = 11;
			constexpr fid_t ReverseHashes            = 12;
			constexpr fid_t Receipts                 = 13;

			constexpr fid_t Links                    = 14;
			constexpr fid_t PendingLinks             = 15;
			constexpr fid_t ActiveLinks              = 16;
			constexpr fid_t Tunnels                  = 17;

			constexpr fid_t KnownDestinations        = 18;
			constexpr fid_t DestinationPathResponses = 19;
			constexpr fid_t QueuedAnnounces          = 20;
		}
	}

	// Metrics namespace - usage counts
	namespace Metrics {
		constexpr nid_t Id = 55;
		namespace Field {
			constexpr fid_t SchemaVersion		     = 0;	// reserved
			constexpr fid_t PacketsSent              = 1;
			constexpr fid_t PacketsReceived          = 2;
			constexpr fid_t AnnouncesSent            = 3;
			constexpr fid_t AnnouncesReceived        = 4;
			constexpr fid_t PathRequestSent          = 5;
			constexpr fid_t PathRequestsReceived     = 6;
			constexpr fid_t PathsAdded               = 7;
			constexpr fid_t PathsUpdated             = 8;
			constexpr fid_t PathsFailed              = 9;
		}
	}

} } }
