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
#include "../Utilities/Memory.h"
#include "../Utilities/tlsf/tlsf.h"

#include <set>
#include <vector>

namespace RNS { namespace Provisioning {

	// Idempotent: re-calling does nothing because Registry rejects duplicate
	// namespace ids. Called automatically from Provisioner::begin().
	void register_builtin_namespaces(Provisioner& p) {

		// General Config
		if (!p.registry().find(Ns::GeneralConfig::Id)) {
			p.register_namespace("uReticulum General Config", Ns::GeneralConfig::Id)
				.field_bool("Transport Enabled", Ns::GeneralConfig::Field::TransportEnabled,
					FF_REBOOT_REQUIRED, Reticulum::transport_enabled(),
					[](const Value& v) { Reticulum::transport_enabled(v.as_bool()); return true; },
					[]() { return Reticulum::transport_enabled(); })
				.field_bool("Link MTU Discovery Enabled", Ns::GeneralConfig::Field::LinkMtuDiscovery,
					FF_REBOOT_REQUIRED, Reticulum::link_mtu_discovery(),
					[](const Value& v) { Reticulum::link_mtu_discovery(v.as_bool()); return true; },
					[]() { return Reticulum::link_mtu_discovery(); })
				.field_bool("Remote Management Enabled", Ns::GeneralConfig::Field::RemoteManagementEnabled,
					FF_REBOOT_REQUIRED, Reticulum::remote_management_enabled(),
					[](const Value& v) { Reticulum::remote_management_enabled(v.as_bool()); return true; },
					[]() { return Reticulum::remote_management_enabled(); })
				.field_bool("Probe Destination Enabled", Ns::GeneralConfig::Field::ProbeDestinationEnabled,
					FF_REBOOT_REQUIRED, Reticulum::probe_destination_enabled(),
					[](const Value& v) { Reticulum::probe_destination_enabled(v.as_bool()); return true; },
					[]() { return Reticulum::probe_destination_enabled(); })
				.field_int("Persist Interval", Ns::GeneralConfig::Field::PersistInterval,
					FF_LIVE_APPLY, (int64_t)Reticulum::persist_interval(), 30, 86400,
					[](const Value& v) { Reticulum::persist_interval((uint16_t)v.as_int()); return true; },
					[]() { return (int64_t)Reticulum::persist_interval(); })
				.field_int("Clean Interval", Ns::GeneralConfig::Field::CleanInterval,
					FF_LIVE_APPLY, (int64_t)Reticulum::clean_interval(), 60, 86400,
					[](const Value& v) { Reticulum::clean_interval((uint16_t)v.as_int()); return true; },
					[]() { return (int64_t)Reticulum::clean_interval(); })
				.field_bytes("Transport Identity",
					Ns::GeneralConfig::Field::TransportIdentity,
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
				.field_bytes_list("Remote Management Allowed",
					Ns::GeneralConfig::Field::RemoteManagementAllowed,
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
				.command_void("Clear Provisioning", Ns::GeneralConfig::Field::ClearStorage,
					[]() { return Provisioner::instance().clear_storage(); })
				.end();
		}

		// Transport Config
		if (!p.registry().find(Ns::TransportConfig::Id)) {
			p.register_namespace("uReticulum Transport Config", Ns::TransportConfig::Id)
				.field_int("Path Table Max Size", Ns::TransportConfig::Field::PathTableMaxsize,
					FF_LIVE_APPLY, (int64_t)RNS::Transport::path_table_maxsize(), 1, 65535,
					[](const Value& v) { RNS::Transport::path_table_maxsize((uint16_t)v.as_int()); return true; },
					[]() { return (int64_t)RNS::Transport::path_table_maxsize(); })
				.field_int("Announce Table Max Size", Ns::TransportConfig::Field::AnnounceTableMaxsize,
					FF_LIVE_APPLY, (int64_t)RNS::Transport::announce_table_maxsize(), 1, 65535,
					[](const Value& v) { RNS::Transport::announce_table_maxsize((uint16_t)v.as_int()); return true; },
					[]() { return (int64_t)RNS::Transport::announce_table_maxsize(); })
				.field_int("Hashlist Max Size", Ns::TransportConfig::Field::HashlistMaxsize,
					FF_LIVE_APPLY, (int64_t)RNS::Transport::hashlist_maxsize(), 1, 65535,
					[](const Value& v) { RNS::Transport::hashlist_maxsize((uint16_t)v.as_int()); return true; },
					[]() { return (int64_t)RNS::Transport::hashlist_maxsize(); })
				.field_int("Max PR Tags", Ns::TransportConfig::Field::MaxPrTags,
					FF_LIVE_APPLY, (int64_t)RNS::Transport::max_pr_tags(), 1, 65535,
					[](const Value& v) { RNS::Transport::max_pr_tags((uint16_t)v.as_int()); return true; },
					[]() { return (int64_t)RNS::Transport::max_pr_tags(); })
				.field_int("Known Destinations Max Size", Ns::TransportConfig::Field::KnownDestinationsMaxsize,
					FF_LIVE_APPLY, (int64_t)RNS::Identity::known_destinations_maxsize(), 1, 65535,
					[](const Value& v) { RNS::Identity::known_destinations_maxsize((uint16_t)v.as_int()); return true; },
					[]() { return (int64_t)RNS::Identity::known_destinations_maxsize(); })
				.command_void("Clear Storage", Ns::TransportConfig::Field::ClearStorage,
					[]() { RNS::Transport::clear_storage(); return true; })
				.end();
		}

		// Storage
		if (!p.registry().find(Ns::Storage::Id)) {
			p.register_namespace("uReticulum Storage", Ns::Storage::Id)
				.metric_int("Paths", Ns::Storage::Field::Paths, []() { return RNS::Transport::new_path_table().size(); })
				.metric_int("Known Destinations", Ns::Storage::Field::KnownDestinations, []() { return RNS::Identity::known_destinations().size(); })
				.metric_int("Packet Hashes", Ns::Storage::Field::PacketHashes, []() { return RNS::Transport::packet_hashlist().size(); })
				.metric_int("Destinations", Ns::Storage::Field::Destinations, []() { return RNS::Transport::destinations().size(); })
				.metric_int("Announces", Ns::Storage::Field::Announces, []() { return RNS::Transport::announce_table().size(); })
				.metric_int("Held Announces", Ns::Storage::Field::HeldAnnounces, []() { return RNS::Transport::held_announces().size(); })

				.metric_int("Path Requests", Ns::Storage::Field::PathRequests, []() { return RNS::Transport::path_requests().size(); })
				.metric_int("Discovery Path Requests", Ns::Storage::Field::DiscoveryPathRequests, []() { return RNS::Transport::discovery_path_requests().size(); })
				.metric_int("Pending Local Path Requests", Ns::Storage::Field::PendingLocalPathRequests, []() { return RNS::Transport::pending_local_path_requests().size(); })
				.metric_int("Discovery PR Tags", Ns::Storage::Field::DiscoveryPrTags, []() { return RNS::Transport::discovery_pr_tags().size(); })
				.metric_int("Control Destinations", Ns::Storage::Field::ControlDestinations, []() { return RNS::Transport::control_destinations().size(); })
				.metric_int("Control Hashes", Ns::Storage::Field::ControlHashes, []() { return RNS::Transport::control_hashes().size(); })

				.metric_int("Reverse Hashes", Ns::Storage::Field::ReverseHashes, []() { return RNS::Transport::reverse_table().size(); })
				.metric_int("Receipts", Ns::Storage::Field::Receipts, []() { return RNS::Transport::receipts().size(); })

				.metric_int("Links", Ns::Storage::Field::Links, []() { return RNS::Transport::link_table().size(); })
				.metric_int("Pending Links", Ns::Storage::Field::PendingLinks, []() { return RNS::Transport::pending_links().size(); })
				.metric_int("Active Links", Ns::Storage::Field::ActiveLinks, []() { return RNS::Transport::active_links().size(); })
				.metric_int("Tunnels", Ns::Storage::Field::Tunnels, []() { return RNS::Transport::tunnels().size(); })

				.metric_int("Destination Path Responses", Ns::Storage::Field::DestinationPathResponses, []() {
					uint32_t destination_path_responses = 0;
					for (auto& [destination_hash, destination] : RNS::Transport::destinations()) {
						destination_path_responses += destination.path_responses().size();
					}
					return destination_path_responses;
				})
				.metric_int("Queued Announces", Ns::Storage::Field::QueuedAnnounces, []() {
					uint32_t queued_announces = 0;
					for (auto& interface : RNS::Transport::get_interfaces()) {
						queued_announces += interface.announce_queue().size();
					}
					return queued_announces;
				})

				.end();
		}

		// Metrics
		if (!p.registry().find(Ns::Info::Id)) {
			p.register_namespace("uReticulum Info", Ns::Info::Id)
				.register_namespace("Addresses", Ns::Info::Addresses::Id)
					.metric_bytes("Transport Identity", Ns::Info::Addresses::Field::TransportIdentity, []() { return RNS::Transport::identity() ? RNS::Transport::identity().hash() : RNS::Bytes{}; })
					.metric_bytes("Probe Destination", Ns::Info::Addresses::Field::ProbeDestination, []() { return RNS::Transport::probe_destination() ? RNS::Transport::probe_destination().hash() : RNS::Bytes{}; })
					.metric_bytes("Mgmt Destination", Ns::Info::Addresses::Field::MgmtDestination, []() { return RNS::Transport::remote_management_destination() ? RNS::Transport::remote_management_destination().hash() : RNS::Bytes{}; })
					.end()
				.register_namespace("Metrics", Ns::Info::Metrics::Id)
					.metric_int("Packets Sent", Ns::Info::Metrics::Field::PacketsSent, []() { return RNS::Transport::packets_sent(); })
					.metric_int("Packets Received", Ns::Info::Metrics::Field::PacketsReceived, []() { return RNS::Transport::packets_received(); })
					.metric_int("Announces Received", Ns::Info::Metrics::Field::AnnouncesReceived, []() { return RNS::Transport::announces_received(); })
					.metric_int("Path Requests Received", Ns::Info::Metrics::Field::PathRequestsReceived, []() { return RNS::Transport::path_requests_received(); })
					.metric_int("Paths Added", Ns::Info::Metrics::Field::PathsAdded, []() { return RNS::Transport::paths_added(); })
					.metric_int("Paths Updated", Ns::Info::Metrics::Field::PathsUpdated, []() { return RNS::Transport::paths_updated(); })
					.metric_int("Paths Failed", Ns::Info::Metrics::Field::PathsFailed, []() { return RNS::Transport::paths_failed(); })
					.end()
				.end();
		}

		// Memory
		if (!p.registry().find(Ns::Memory::Id)) {
			p.register_namespace("uReticulum Memory", Ns::Memory::Id)
				.register_namespace("Heap", Ns::Memory::Heap::Id)
					.metric_int("Size", Ns::Memory::Heap::Field::Size, []() { return RNS::Utilities::Memory::heap_size(); })
					.metric_int("Free", Ns::Memory::Heap::Field::Free, []() { return RNS::Utilities::Memory::heap_available(); })
					.metric_int("Free (%)", Ns::Memory::Heap::Field::FreePct, []() { return (unsigned)((double)RNS::Utilities::Memory::heap_available() / (double)RNS::Utilities::Memory::heap_size() * 100.0); })
#if defined(ESP32)
					.metric_int("Min Free", Ns::Memory::Heap::Field::MinFree, []() { return ESP.getMinFreeHeap(); })
					.metric_int("Max Alloc", Ns::Memory::Heap::Field::MaxAlloc, []() { return ESP.getMaxAllocHeap(); })
					.metric_int("Fragmented (%)", Ns::Memory::Heap::Field::Fragmented, []() { return (unsigned)(100.0 - (double)ESP.getMaxAllocHeap() / (double)ESP.getFreeHeap() * 100.0); })
#elif defined(ARDUINO_ARCH_NRF52) || defined(ARDUINO_NRF52_ADAFRUIT)
#endif
					.end()
#if defined(ESP32)
				.register_namespace("PSRAM", Ns::Memory::Psram::Id)
					.metric_int("Size", Ns::Memory::Psram::Field::Size, []() { return ESP.getPsramSize(); })
					.metric_int("Free", Ns::Memory::Psram::Field::Free, []() { return ESP.getFreePsram(); })
					.metric_int("Free (%)", Ns::Memory::Psram::Field::FreePct, []() { return (ESP.getPsramSize() > 0) ? (unsigned)((double)ESP.getFreePsram() / (double)ESP.getPsramSize() * 100.0) : 0; })
					.metric_int("Min Free", Ns::Memory::Psram::Field::MinFree, []() { return ESP.getMinFreePsram(); })
					.metric_int("Max Alloc", Ns::Memory::Psram::Field::MaxAlloc, []() { return ESP.getMaxAllocPsram(); })
					.metric_int("Fragmented (%)", Ns::Memory::Psram::Field::Fragmented, []() { return (ESP.getFreePsram() > 0) ? (unsigned)(100.0 - (double)ESP.getMaxAllocPsram() / (double)ESP.getFreePsram() * 100.0) : 0; })
					.end()
#elif defined(ARDUINO_ARCH_NRF52) || defined(ARDUINO_NRF52_ADAFRUIT)
#endif
				.register_namespace("Flash", Ns::Memory::Flash::Id)
					.metric_int("Size", Ns::Memory::Flash::Field::Size, []() { return RNS::Utilities::OS::storage_size(); })
					.metric_int("Free", Ns::Memory::Flash::Field::Free, []() { return RNS::Utilities::OS::storage_available(); })
					.metric_int("Free (%)", Ns::Memory::Flash::Field::FreePct, []() {
						size_t flash_size = RNS::Utilities::OS::storage_size();
						size_t flash_free = RNS::Utilities::OS::storage_available();
						uint8_t flash_freepct = 0;
						if (flash_size > 0) flash_freepct = (uint8_t)((double)flash_free / (double)flash_size * 100.0);
						return (fint_t)flash_freepct;
					})
					.end()

				.end();
		}

		// Allocator
		if (!p.registry().find(Ns::Allocator::Id)) {
			auto b = p.register_namespace("uReticulum Allocator", Ns::Allocator::Id);

			if (Utilities::Memory::heap_pool_info.tlsf != nullptr) {
				b.register_namespace("Heap Pool", Ns::Allocator::HeapPool::Id)
					.metric_int("Size", Ns::Allocator::HeapPool::Field::Size, []() { return RNS::Utilities::Memory::heap_pool_size(); })
					.metric_int("Free", Ns::Allocator::HeapPool::Field::Free, []() { return RNS::Utilities::Memory::heap_pool_free(); })
					.metric_int("Free (%)", Ns::Allocator::HeapPool::Field::FreePct, []() { return (uint8_t)((double)RNS::Utilities::Memory::heap_pool_free() / (double)RNS::Utilities::Memory::heap_pool_size() * 100.0); })
					.metric_int("Fragmented (%)", Ns::Allocator::HeapPool::Field::Fragmented, []() { return RNS::Utilities::Memory::heap_pool_fragmented(); })
					.end();
			}
			if (Utilities::Memory::psram_pool_info.tlsf != nullptr) {
				b.register_namespace("PSRAM Pool", Ns::Allocator::PsramPool::Id)
					.metric_int("Size", Ns::Allocator::PsramPool::Field::Size, []() { return RNS::Utilities::Memory::psram_pool_size(); })
					.metric_int("Free", Ns::Allocator::PsramPool::Field::Free, []() { return RNS::Utilities::Memory::psram_pool_free(); })
					.metric_int("Free (%)", Ns::Allocator::PsramPool::Field::FreePct, []() { return (uint8_t)((double)RNS::Utilities::Memory::psram_pool_free() / (double)RNS::Utilities::Memory::psram_pool_size() * 100.0); })
					.metric_int("Fragmented (%)", Ns::Allocator::PsramPool::Field::Fragmented, []() { return RNS::Utilities::Memory::heap_pool_fragmented(); })
					.end();
			}
			b.register_namespace("Default Allocator", Ns::Allocator::DefaultAllocator::Id)
				.metric_int("Active", Ns::Allocator::DefaultAllocator::Field::Active, []() { return RNS::Utilities::Memory::default_allocator_alloc() - RNS::Utilities::Memory::default_allocator_alloc(); })
				.end();
			b.register_namespace("Container Allocator", Ns::Allocator::ContainerAllocator::Id)
				.metric_int("Active", Ns::Allocator::ContainerAllocator::Field::Active, []() { return RNS::Utilities::Memory::container_allocator_alloc() - RNS::Utilities::Memory::container_allocator_alloc(); })
				.end();

			b.end();
		}

	}

} }
