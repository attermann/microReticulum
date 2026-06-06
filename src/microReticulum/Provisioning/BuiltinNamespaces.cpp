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

#include <set>
#include <vector>

namespace RNS { namespace Provisioning {

	// Idempotent: re-calling does nothing because Registry rejects duplicate
	// namespace ids. Called automatically from Manager::begin().
	void register_builtin_namespaces(Manager& p) {

		// reticulum
		if (!p.registry().find(Ns::Reticulum::Id)) {
			p.register_namespace("Reticulum", Ns::Reticulum::Id)
				.field_bool("transport_enabled", Ns::Reticulum::Field::TransportEnabled,
					FF_REBOOT_REQUIRED, Reticulum::transport_enabled(),
					[](const Value& v) { Reticulum::transport_enabled(v.as_bool()); return true; },
					[]() { return Reticulum::transport_enabled(); })
				.field_bool("link_mtu_discovery", Ns::Reticulum::Field::LinkMtuDiscovery,
					FF_REBOOT_REQUIRED, Reticulum::link_mtu_discovery(),
					[](const Value& v) { Reticulum::link_mtu_discovery(v.as_bool()); return true; },
					[]() { return Reticulum::link_mtu_discovery(); })
				.field_bool("remote_management_enabled", Ns::Reticulum::Field::RemoteManagementEnabled,
					FF_LIVE_APPLY, Reticulum::remote_management_enabled(),
					[](const Value& v) { Reticulum::remote_management_enabled(v.as_bool()); return true; },
					[]() { return Reticulum::remote_management_enabled(); })
				.field_bool("probe_destination_enabled", Ns::Reticulum::Field::ProbeDestinationEnabled,
					FF_LIVE_APPLY, Reticulum::probe_destination_enabled(),
					[](const Value& v) { Reticulum::probe_destination_enabled(v.as_bool()); return true; },
					[]() { return Reticulum::probe_destination_enabled(); })
				.field_int("persist_interval", Ns::Reticulum::Field::PersistInterval,
					FF_LIVE_APPLY, (int64_t)Reticulum::persist_interval(), 30, 86400,
					[](const Value& v) { Reticulum::persist_interval((uint16_t)v.as_int()); return true; },
					[]() { return (int64_t)Reticulum::persist_interval(); })
				.field_int("clean_interval", Ns::Reticulum::Field::CleanInterval,
					FF_LIVE_APPLY, (int64_t)Reticulum::clean_interval(), 60, 86400,
					[](const Value& v) { Reticulum::clean_interval((uint16_t)v.as_int()); return true; },
					[]() { return (int64_t)Reticulum::clean_interval(); })
				.field_bytes("transport_identity",
					Ns::Reticulum::Field::TransportIdentity,
					(uint8_t)(FF_REBOOT_REQUIRED | FF_SECRET), Bytes(),
					64,	// max_len: 64 bytes (KEYSIZE/8 = 32 prv + 32 sig_prv)
					[](const Value& v) {
						// Empty input means "no change"; preserve whatever
						// Transport currently has. Only replace on a real key.
						if (v.as_bytes().size() == 0) return true;
						RNS::Transport::set_identity_prv(v.as_bytes());
						return true;
					},
					[]() {
						// get_identity_prv() calls Identity::get_private_key()
						// which dereferences the inner shared-object; guard
						// against an unset Identity (early-boot, pre-Transport::start).
						if (!RNS::Transport::identity()) return Bytes();
						return RNS::Transport::get_identity_prv();
					})
				.field_bytes_list("remote_management_allowed",
					Ns::Reticulum::Field::RemoteManagementAllowed,
					FF_REBOOT_REQUIRED,
					std::vector<Bytes>(),    // default: empty list
					16,                       // each entry is a 16-byte destination hash
					32,                       // cap the list at 32 entries
					[](const Value& v) {
						const auto& list = v.as_bytes_list();
						std::set<Bytes> s(list.begin(), list.end());
						RNS::Transport::remote_management_allowed(s);
						return true;
					},
					[]() {
						const std::set<Bytes>& s = RNS::Transport::remote_management_allowed();
						return std::vector<Bytes>(s.begin(), s.end());
					})
				.command_void("clear_provisioning", Ns::Reticulum::Field::ClearStorage,
					[]() { return Manager::instance().clear_storage(); })
				.end();
		}

		// transport
		if (!p.registry().find(Ns::Transport::Id)) {
			p.register_namespace("Transport", Ns::Transport::Id)
				.field_int("path_table_maxsize", Ns::Transport::Field::PathTableMaxsize,
					FF_LIVE_APPLY, (int64_t)RNS::Transport::path_table_maxsize(), 1, 65535,
					[](const Value& v) { RNS::Transport::path_table_maxsize((uint16_t)v.as_int()); return true; },
					[]() { return (int64_t)RNS::Transport::path_table_maxsize(); })
				.field_int("announce_table_maxsize", Ns::Transport::Field::AnnounceTableMaxsize,
					FF_LIVE_APPLY, (int64_t)RNS::Transport::announce_table_maxsize(), 1, 65535,
					[](const Value& v) { RNS::Transport::announce_table_maxsize((uint16_t)v.as_int()); return true; },
					[]() { return (int64_t)RNS::Transport::announce_table_maxsize(); })
				.field_int("hashlist_maxsize", Ns::Transport::Field::HashlistMaxsize,
					FF_LIVE_APPLY, (int64_t)RNS::Transport::hashlist_maxsize(), 1, 65535,
					[](const Value& v) { RNS::Transport::hashlist_maxsize((uint16_t)v.as_int()); return true; },
					[]() { return (int64_t)RNS::Transport::hashlist_maxsize(); })
				.field_int("max_pr_tags", Ns::Transport::Field::MaxPrTags,
					FF_LIVE_APPLY, (int64_t)RNS::Transport::max_pr_tags(), 1, 65535,
					[](const Value& v) { RNS::Transport::max_pr_tags((uint16_t)v.as_int()); return true; },
					[]() { return (int64_t)RNS::Transport::max_pr_tags(); })
				.field_int("path_table_maxpersist", Ns::Transport::Field::PathTableMaxpersist,
					FF_LIVE_APPLY, (int64_t)RNS::Transport::path_table_maxpersist(), 1, 65535,
					[](const Value& v) { RNS::Transport::path_table_maxpersist((uint16_t)v.as_int()); return true; },
					[]() { return (int64_t)RNS::Transport::path_table_maxpersist(); })
				.command_void("clear_storage", Ns::Transport::Field::ClearStorage,
					[]() { RNS::Transport::clear_storage(); return true; })
				.end();
		}

		// Note: an "identity" built-in namespace (id 3) was prototyped earlier
		// but dropped because the library has no canonical identity to expose.
		// Apps that want to surface an identity-like field should register
		// their own namespace using an id in the 100-199 (official-app) or
		// 200+ (vendor) range. Id 3 is permanently reserved.

	}

} }
