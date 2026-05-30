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

#include "Provisioning.h"
#include "Ids.h"

#include "../Reticulum.h"
#include "../Transport.h"

namespace RNS { namespace Provisioning {

	// Idempotent: re-calling does nothing because Registry rejects duplicate
	// namespace ids. Called automatically from Manager::begin().
	void register_builtin_namespaces(Manager& p) {

		// reticulum
		if (!p.registry().find(Ns::Reticulum::Id)) {
			p.register_namespace("reticulum", Ns::Reticulum::Id)
				.field_bool("transport_enabled", Ns::Reticulum::Field::TransportEnabled,
					FF_LIVE_APPLY, Reticulum::transport_enabled(),
					[](const Value& v) { Reticulum::transport_enabled(v.as_bool()); return true; })
				.field_bool("link_mtu_discovery", Ns::Reticulum::Field::LinkMtuDiscovery,
					FF_REBOOT_REQUIRED, Reticulum::link_mtu_discovery(),
					[](const Value& v) { Reticulum::link_mtu_discovery(v.as_bool()); return true; })
				.field_bool("remote_management_enabled", Ns::Reticulum::Field::RemoteManagementEnabled,
					FF_LIVE_APPLY, Reticulum::remote_management_enabled(),
					[](const Value& v) { Reticulum::remote_management_enabled(v.as_bool()); return true; })
				.field_bool("probe_destination_enabled", Ns::Reticulum::Field::ProbeDestinationEnabled,
					FF_LIVE_APPLY, Reticulum::probe_destination_enabled(),
					[](const Value& v) { Reticulum::probe_destination_enabled(v.as_bool()); return true; })
				.field_int("persist_interval", Ns::Reticulum::Field::PersistInterval,
					FF_LIVE_APPLY, (int64_t)Reticulum::persist_interval(), 30, 86400,
					[](const Value& v) { Reticulum::persist_interval((uint16_t)v.as_int()); return true; })
				.field_int("clean_interval", Ns::Reticulum::Field::CleanInterval,
					FF_LIVE_APPLY, (int64_t)Reticulum::clean_interval(), 60, 86400,
					[](const Value& v) { Reticulum::clean_interval((uint16_t)v.as_int()); return true; })
				.end();
		}

		// transport
		if (!p.registry().find(Ns::Transport::Id)) {
			p.register_namespace("transport", Ns::Transport::Id)
				.field_int("path_table_maxsize", Ns::Transport::Field::PathTableMaxsize,
					FF_LIVE_APPLY, (int64_t)RNS::Transport::path_table_maxsize(), 1, 65535,
					[](const Value& v) { RNS::Transport::path_table_maxsize((uint16_t)v.as_int()); return true; })
				.field_int("announce_table_maxsize", Ns::Transport::Field::AnnounceTableMaxsize,
					FF_LIVE_APPLY, (int64_t)RNS::Transport::announce_table_maxsize(), 1, 65535,
					[](const Value& v) { RNS::Transport::announce_table_maxsize((uint16_t)v.as_int()); return true; })
				.field_int("hashlist_maxsize", Ns::Transport::Field::HashlistMaxsize,
					FF_LIVE_APPLY, (int64_t)RNS::Transport::hashlist_maxsize(), 1, 65535,
					[](const Value& v) { RNS::Transport::hashlist_maxsize((uint16_t)v.as_int()); return true; })
				.field_int("max_pr_tags", Ns::Transport::Field::MaxPrTags,
					FF_LIVE_APPLY, (int64_t)RNS::Transport::max_pr_tags(), 1, 65535,
					[](const Value& v) { RNS::Transport::max_pr_tags((uint16_t)v.as_int()); return true; })
				.field_int("path_table_maxpersist", Ns::Transport::Field::PathTableMaxpersist,
					FF_LIVE_APPLY, (int64_t)RNS::Transport::path_table_maxpersist(), 1, 65535,
					[](const Value& v) { RNS::Transport::path_table_maxpersist((uint16_t)v.as_int()); return true; })
				.end();
		}

		// Note: an "identity" built-in namespace (id 3) was prototyped earlier
		// but dropped because the library has no canonical identity to expose.
		// Apps that want to surface an identity-like field should register
		// their own namespace using an id in the 100-199 (official-app) or
		// 200+ (vendor) range. Id 3 is permanently reserved.

	}

} }
