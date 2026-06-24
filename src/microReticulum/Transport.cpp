/*
 * Copyright (c) 2023 Chad Attermann
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

#include "Transport.h"

#include "Reticulum.h"
#include "Destination.h"
#include "Identity.h"
#include "Link.h"
#include "Packet.h"
#include "Interface.h"
#include "Log.h"
#include "Cryptography/Random.h"
#include "Utilities/OS.h"
#include "Utilities/Persistence.h"

#if defined(RNS_ENABLE_REMOTE_PROVISIONING) && defined(RNS_USE_PROVISIONING)
#include "Provisioning/Provisioning.h"
#endif

#include <MsgPack.h>

#include <algorithm>
#include <cmath>
#include <unistd.h>
#include <time.h>

using namespace RNS;
using namespace RNS::Type::Transport;
using namespace RNS::Utilities;
using namespace RNS::Persistence;

#ifndef RNS_PATH_TABLE_MAX
#define RNS_PATH_TABLE_MAX 100
#endif

#ifndef RNS_PATH_TABLE_SEGMENT_SIZE
#define RNS_PATH_TABLE_SEGMENT_SIZE 65536
#endif

#ifndef RNS_PATH_TABLE_SEGMENT_COUNT
#define RNS_PATH_TABLE_SEGMENT_COUNT 8
#endif

#ifndef RNS_ANNOUNCE_TABLE_MAX
#define RNS_ANNOUNCE_TABLE_MAX 100
#endif

#ifndef RNS_HASHLIST_MAX
#define RNS_HASHLIST_MAX 100
#endif

#ifndef RNS_HASHLIST_SEGMENT_SIZE
#define RNS_HASHLIST_SEGMENT_SIZE 32768
#endif

#ifndef RNS_HASHLIST_SEGMENT_COUNT
#define RNS_HASHLIST_SEGMENT_COUNT 2
#endif

#ifndef RNS_PR_TAGS_MAX
#define RNS_PR_TAGS_MAX	 32
#endif

#ifndef RNS_SAME_INTERFACE_PATH_REQUESTS
#define RNS_SAME_INTERFACE_PATH_REQUESTS 1
#endif

#ifndef RNS_REJECT_BLACKHOLED_ANNOUNCE
#define RNS_REJECT_BLACKHOLED_ANNOUNCE 1
#endif

#ifndef RNS_BLOCK_UNRESPONSIVE_ANNOUNCE
#define RNS_BLOCK_UNRESPONSIVE_ANNOUNCE 1
#endif

/*static*/ Transport::InterfaceTable Transport::_interfaces;
/*static*/ Transport::DestinationTable Transport::_destinations;
/*static*/ std::set<Link> Transport::_pending_links;
/*static*/ std::set<Link> Transport::_active_links;
#if defined(RNS_USE_FS) && RNS_PERSIST_HASHLIST
/*static*/ Transport::HashlistStore Transport::_packet_hashlist(RNS_HASHLIST_SEGMENT_SIZE, RNS_HASHLIST_SEGMENT_COUNT);
#else
/*static*/ Transport::HashlistStore Transport::_packet_hashlist;
#endif
/*static*/ std::list<PacketReceipt> Transport::_receipts;

/*static*/ Transport::AnnounceTable Transport::_announce_table;
/*static*/ PathTable Transport::_path_table;
/*static*/ std::map<Bytes, Transport::ReverseEntry> Transport::_reverse_table;
/*static*/ std::map<Bytes, Transport::LinkEntry> Transport::_link_table;
/*static*/ Transport::AnnounceTable Transport::_held_announces;
/*static*/ std::set<HAnnounceHandler> Transport::_announce_handlers;
/*static*/ std::map<Bytes, Transport::TunnelEntry> Transport::_tunnels;
/*static*/ std::map<Bytes, Transport::RateEntry> Transport::_announce_rate_table;
/*static*/ std::map<Bytes, double> Transport::_path_requests;

/*static*/ std::map<Bytes, Transport::PathRequestEntry> Transport::_discovery_path_requests;
/*static*/ Transport::BytesList Transport::_discovery_pr_tags;
/*static*/ Transport::PathStateTable Transport::_path_states;
/*static*/ Transport::PendingDiscoveryPRs Transport::_pending_discovery_prs;
/*static*/ double Transport::_pending_discovery_prs_last_tx = 0.0;
/*static*/ Transport::BlackholeTable Transport::_blackholed_identities;

/*static*/ std::set<Destination> Transport::_control_destinations;
/*static*/ std::set<Bytes> Transport::_control_hashes;
/*static*/ std::set<Destination> Transport::_mgmt_destinations;
/*static*/ std::set<Bytes> Transport::_mgmt_hashes;

///*static*/ std::set<Interface> Transport::_local_client_interfaces;
/*static*/ std::set<std::reference_wrapper<const Interface>, std::less<const Interface>> Transport::_local_client_interfaces;

/*static*/ std::map<Bytes, const Interface> Transport::_pending_local_path_requests;

// CBA
/*static*/ std::map<Bytes, Transport::PacketEntry> Transport::_packet_table;

/*static*/ uint16_t Transport::_LOCAL_CLIENT_CACHE_MAXSIZE = 512;

/*static*/ double Transport::_start_time				= 0.0;
/*static*/ bool Transport::_jobs_locked					= false;
/*static*/ bool Transport::_jobs_running				= false;
/*static*/ float Transport::_job_interval				= 0.250;
/*static*/ double Transport::_jobs_last_run				= 0.0;
/*static*/ double Transport::_links_last_checked		= 0.0;
/*static*/ float Transport::_links_check_interval		= 1.0;
/*static*/ double Transport::_receipts_last_checked		= 0.0;
/*static*/ float Transport::_receipts_check_interval	= 1.0;
/*static*/ double Transport::_announces_last_checked	= 0.0;
/*static*/ float Transport::_announces_check_interval	= 1.0;
/*static*/ double Transport::_pending_prs_last_checked	= 0.0;
/*static*/ float Transport::_pending_prs_check_interval	= 30.0;
/*static*/ double Transport::_tables_last_culled		= 0.0;
// CBA MCU
/*static*/ //float Transport::_tables_cull_interval		= 5.0;
/*static*/ float Transport::_tables_cull_interval		= 60.0;
/*static*/ double Transport::_traffic_last_checked		= 0.0;
/*static*/ float Transport::_traffic_check_interval		= 1.0;
/*static*/ double Transport::_interface_last_jobs		= 0.0;
/*static*/ float Transport::_interface_jobs_interval	= 5.0;
/*static*/ double Transport::_blackhole_last_checked	= 0.0;
/*static*/ float Transport::_blackhole_check_interval	= 60.0;
/*static*/ double Transport::_last_mgmt_announce		= 0.0;
/*static*/ float Transport::_mgmt_announce_interval		= 7200.0;
/*static*/ bool Transport::_saving_path_table			= false;
// CBA ACCUMULATES
// CBA MCU
/*static*/ uint16_t Transport::_hashlist_maxsize		= RNS_HASHLIST_MAX;
// CBA ACCUMULATES
// CBA MCU
/*static*/ uint16_t Transport::_max_pr_tags				= RNS_PR_TAGS_MAX;

// CBA
// CBA ACCUMULATES
/*static*/ uint16_t Transport::_path_table_maxsize		= RNS_PATH_TABLE_MAX;
// CBA ACCUMULATES
/*static*/ uint16_t Transport::_path_table_maxpersist	= RNS_PATH_TABLE_MAX;
/*static*/ double Transport::_last_saved				= 0.0;
/*static*/ float Transport::_save_interval				= 3600.0;
/*static*/ uint32_t Transport::_path_table_crc	= 0;
/*static*/ uint16_t Transport::_announce_table_maxsize	= RNS_ANNOUNCE_TABLE_MAX;

/*static*/ Reticulum Transport::_owner({Type::NONE});
/*static*/ Identity Transport::_identity({Type::NONE});
/*static*/ Identity Transport::_network_identity({Type::NONE});

/*static*/ std::set<Bytes> Transport::_remote_management_allowed;
/*static*/ Destination Transport::_probe_destination({Type::NONE});
/*static*/ Destination Transport::_remote_management_destination({Type::NONE});
/*static*/ Destination Transport::_blackhole_destination({Type::NONE});
/*static*/ Destination Transport::_instance_destination({Type::NONE});
/*static*/ Destination Transport::_network_destination({Type::NONE});

// CBA
/*static*/ Transport::Callbacks Transport::_callbacks;

// CBA Stats
/*static*/ uint32_t Transport::_packets_sent = 0;
/*static*/ uint32_t Transport::_packets_received = 0;
/*static*/ uint64_t Transport::_traffic_rxb = 0;
/*static*/ uint64_t Transport::_traffic_txb = 0;
/*static*/ double Transport::_speed_rx = 0.0;
/*static*/ double Transport::_speed_tx = 0.0;
/*static*/ uint32_t Transport::_announces_received = 0;
/*static*/ uint32_t Transport::_path_requests_received = 0;
/*static*/ uint32_t Transport::_paths_added = 0;
/*static*/ uint32_t Transport::_paths_updated = 0;
/*static*/ uint32_t Transport::_paths_failed = 0;
/*static*/ size_t Transport::_last_memory = 0;
/*static*/ size_t Transport::_last_psram = 0;
/*static*/ size_t Transport::_last_flash = 0;

/*static*/ bool Transport::cleaning_caches = false;

// CBA microStore
/*static*/ uint32_t Transport::_path_store_segment_size = 0;
/*static*/ uint8_t Transport::_path_store_segment_count = 0;
/*static*/ uint32_t Transport::_hashlist_segment_size = 0;
/*static*/ uint8_t Transport::_hashlist_segment_count = 0;
#if defined(RNS_USE_FS) && RNS_PERSIST_PATHS
/*static*/ PathStore Transport::_path_store(RNS_PATH_TABLE_SEGMENT_SIZE, RNS_PATH_TABLE_SEGMENT_COUNT);
#else
/*static*/ PathStore Transport::_path_store;
#endif
/*static*/ NewPathTable Transport::_new_path_table(Transport::_path_store);

DestinationEntry empty_destination_entry;

/*static*/ void Transport::start(const Reticulum& reticulum_instance) {
	INFO("Transport starting...");
	_owner = reticulum_instance;

	_jobs_running = true;

	// Wire size caps (no-op if already set via hashlist_maxsize()/max_pr_tags()
	// setters before start()).
	_packet_hashlist.set_max_recs(_hashlist_maxsize);
	_discovery_pr_tags.max_size(_max_pr_tags);

	try {
		// Initialize time-based variables *after* time offset update
		_jobs_last_run = OS::time();
		_links_last_checked = OS::time();
		_receipts_last_checked = OS::time();
		_announces_last_checked = OS::time();
		_tables_last_culled = OS::time();
		_traffic_last_checked = OS::time();
		_blackhole_last_checked = OS::time();
		_last_saved = OS::time();

		// Ensure required directories exist
		if (!OS::directory_exists(Reticulum::_cachepath)) {
			VERBOSE("No cache directory, creating...");
			OS::create_directory(Reticulum::_cachepath);
		}

		// Load transport identity
		if (!_identity) {
			char transport_identity_path[Type::Reticulum::FILEPATH_MAXSIZE];
			snprintf(transport_identity_path, Type::Reticulum::FILEPATH_MAXSIZE, "%s/transport_identity", Reticulum::_storagepath);
			DEBUG("Checking for transport identity...");
			try {
				// Attempt to load identity from file
				if (OS::file_exists(transport_identity_path)) {
					_identity = Identity::from_file(transport_identity_path);
				}

				// If identity could not be loaded then generate a new identity and save it
				if (!_identity) {
					VERBOSE("No valid Transport Identity in storage, creating...");
					_identity = Identity();
					_identity.to_file(transport_identity_path);
				}
				else {
					VERBOSE("Loaded Transport Identity from storage");
				}
			}
			catch (const std::exception& e) {
				ERRORF("Failed to check for transport identity, the contained exception was: %s", e.what());
			}
		}

		// Create transport-specific destination for path request
		Destination path_request_destination({Type::NONE}, Type::Destination::IN, Type::Destination::PLAIN, APP_NAME, "path.request");
		path_request_destination.set_packet_callback(path_request_handler);
		// CBA ACCUMULATES
		_control_destinations.insert(path_request_destination);
		// CBA ACCUMULATES
		_control_hashes.insert(path_request_destination.hash());
		DEBUGF("Created transport-specific path request destination %s", path_request_destination.hash().toHex().c_str());

		// Create transport-specific destination for tunnel synthesize
		Destination tunnel_synthesize_destination({Type::NONE}, Type::Destination::IN, Type::Destination::PLAIN, APP_NAME, "tunnel.synthesize");
		tunnel_synthesize_destination.set_packet_callback(tunnel_synthesize_handler);
		// CBA BUG?
		//p Transport.control_destinations.append(Transport.tunnel_synthesize_handler)
		// CBA ACCUMULATES
		_control_destinations.insert(tunnel_synthesize_destination);
		// CBA ACCUMULATES
		_control_hashes.insert(tunnel_synthesize_destination.hash());
		DEBUGF("Created transport-specific tunnel synthesize destination %s", tunnel_synthesize_destination.hash().toHex().c_str());

		// Create remote management destinations
		if (Reticulum::remote_management_enabled() && !_owner.is_connected_to_shared_instance()) {
			_remote_management_destination = {_identity, Type::Destination::IN, Type::Destination::SINGLE, APP_NAME, "remote.management"};
			_remote_management_destination.register_request_handler({"/status"}, remote_status_handler, Type::Destination::ALLOW_LIST, _remote_management_allowed);
			//_remote_management_destination.register_request_handler({"/status"}, remote_status_handler, Type::Destination::ALLOW_ALL);
			_remote_management_destination.register_request_handler("/path", remote_path_handler, Type::Destination::ALLOW_LIST, _remote_management_allowed);
			//_remote_management_destination.register_request_handler("/path", remote_path_handler, Type::Destination::ALLOW_ALL);
#if defined(RNS_ENABLE_REMOTE_PROVISIONING) && defined(RNS_USE_PROVISIONING)
			_remote_management_destination.register_request_handler("/provision", remote_provision_handler, Type::Destination::ALLOW_LIST, _remote_management_allowed);
#endif
			_mgmt_destinations.insert(_remote_management_destination);
			_mgmt_hashes.insert(_remote_management_destination.hash());
			NOTICEF("Enabled remote management on <%s>", _remote_management_destination.toString().c_str());
#if defined(RNS_ENABLE_REMOTE_PROVISIONING) && defined(RNS_USE_PROVISIONING)
			NOTICEF("Enabled remote provisioning on <%s>", _remote_management_destination.toString().c_str());
#endif
		}

		// Load any persisted blackhole list (always, regardless of publish flag).
		reload_blackhole();

		if (Reticulum::publish_blackhole_enabled() && !_owner.is_connected_to_shared_instance()) {
			_blackhole_destination = {_identity, Type::Destination::IN, Type::Destination::SINGLE, APP_NAME, "info.blackhole"};
			_blackhole_destination.register_request_handler({"/list"}, blackhole_list_handler, Type::Destination::ALLOW_ALL);
			_mgmt_destinations.insert(_blackhole_destination);
			_mgmt_hashes.insert(_blackhole_destination.hash());
			NOTICEF("Enabled blackhole list publishing for transport identity %s", _identity.hash().toHex().c_str());
		}

		// If a network identity has been set on this transport, register two
		// IN/SINGLE destinations under it: one specific to this instance
		// ("network.instance.<hash>") and one shared across the named network
		// ("network"). Both are added to the management announce rotation so
		// peers can discover members of the network overlay.
		//
		// Python additionally gates this on `not is_connected_to_shared_instance`
		// because a shared-instance master would own these destinations on the
		// client's behalf. microReticulum does not support being a shared-instance
		// client, so that guard collapses and we just check network_identity.
		if (has_network_identity()) {
			std::string instance_aspect = "network.instance." + _network_identity.hash().toHex();
			_instance_destination = {_network_identity, Type::Destination::IN, Type::Destination::SINGLE, APP_NAME, instance_aspect.c_str()};
			_network_destination  = {_network_identity, Type::Destination::IN, Type::Destination::SINGLE, APP_NAME, "network"};
			_mgmt_destinations.insert(_instance_destination);
			_mgmt_destinations.insert(_network_destination);
			_mgmt_hashes.insert(_instance_destination.hash());
			_mgmt_hashes.insert(_network_destination.hash());
			NOTICEF("Registered network identity destinations under %s", _network_identity.hash().toHex().c_str());
		}
	}
	catch (const std::exception& e) {
		ERROR("An exception occurred while starting Transport.");
		ERRORF("The contained exception was: %s", e.what());
	}

	_jobs_running = false;

	// Start job loops
	// CBA Threading
	//p thread = threading.Thread(target=Transport.jobloop, daemon=True)
	//p thread.start()

	// Load transport-related data
	if (Reticulum::transport_enabled()) {
		INFO("Transport mode is enabled");

		// Read in path table
		//read_path_table();
#if defined(RNS_USE_FS) && RNS_PERSIST_PATHS
		// CBA microStore
		if (Utilities::OS::get_filesystem()) {
			INFOF("FileSystem available: %lu bytes", Utilities::OS::get_filesystem().storageAvailable());
			// CBA Must pass time offset into microStore for accurate timestamps on devices without a real-time clock
#if defined(ARDUINO)
			microStore::set_time_offset(Utilities::OS::getTimeOffset() / 1000);
#endif
			TRACE("Initializing path table store...");
			_path_store.init(Utilities::OS::get_filesystem(), "./path_store/", false, _path_store_segment_size, _path_store_segment_count);
			// If the filesystem is full then clear the path store since it's of no use full anyway
			if (Utilities::OS::get_filesystem().storageAvailable() > 0 && Utilities::OS::get_filesystem().storageAvailable() < 1024) {
				WARNING("FileSystem is full, clearing existing path store");
				_path_store.clear();
			}
		}
#endif // RNS_USE_FS && RNS_PERSIST_PATHS

#if defined(RNS_USE_FS) && RNS_PERSIST_KNOWN_DESTINATIONS
		if (Utilities::OS::get_filesystem()) {
			// CBA Must pass time offset into microStore for accurate timestamps on devices without a real-time clock
#if defined(ARDUINO)
			microStore::set_time_offset(Utilities::OS::getTimeOffset() / 1000);
#endif
			TRACE("Initializing known destinations store...");
			Identity::_known_store.init(Utilities::OS::get_filesystem(), "./known_store/", false,
				Identity::_known_store_segment_size, Identity::_known_store_segment_count);
			if (Utilities::OS::get_filesystem().storageAvailable() > 0 && Utilities::OS::get_filesystem().storageAvailable() < 1024) {
				WARNING("FileSystem is full, clearing existing known destinations store");
				Identity::_known_store.clear();
			}
		}
#endif // RNS_USE_FS && RNS_PERSIST_KNOWN_DESTINATIONS
		Identity::_known_store.set_max_recs(Identity::_known_destinations_maxsize);

#if defined(RNS_USE_FS) && RNS_PERSIST_HASHLIST
		if (Utilities::OS::get_filesystem()) {
			// CBA Must pass time offset into microStore for accurate timestamps on devices without a real-time clock
#if defined(ARDUINO)
			microStore::set_time_offset(Utilities::OS::getTimeOffset() / 1000);
#endif
			TRACE("Initializing packet hashlist store...");
			_packet_hashlist.init(Utilities::OS::get_filesystem(), "./hashlist_store/", false,
				_hashlist_segment_size, _hashlist_segment_count);
			if (Utilities::OS::get_filesystem().storageAvailable() > 0 && Utilities::OS::get_filesystem().storageAvailable() < 1024) {
				WARNING("FileSystem is full, clearing existing packet hashlist store");
				_packet_hashlist.clear();
			}
		}
#endif // RNS_USE_FS && RNS_PERSIST_HASHLIST

		// CBA The following write and clean is very resource intensive so skip at startup
		// and let a later (optimized) scheduled write and clean take care of it.
		// Write path table back and clean caches in case any entries are invalid
		//DEBUG("Writing path table and cleaning caches to clean-up any orphaned paths/files");
		//write_path_table();
		//clean_caches();

		// Read in tunnel table
		read_tunnel_table();

		// Create transport-specific destination for probe requests
		if (Reticulum::probe_destination_enabled()) {
			_probe_destination = {_identity, Type::Destination::IN, Type::Destination::SINGLE, APP_NAME, "probe"};
			_probe_destination.accepts_links(false);
			_probe_destination.set_proof_strategy(Type::Destination::PROVE_ALL);
			DEBUGF("Created probe responder destination %s", _probe_destination.hash().toHex().c_str());
			//_probe_destination.announce();
			_mgmt_destinations.insert(_probe_destination);
			NOTICEF("Transport instance will respond to probe requests on <%s>", _probe_destination.hash().toHex().c_str());
		}

		VERBOSEF("Transport instance %s started", _identity.toString().c_str());
		_start_time = OS::time();
	}
	else {
		INFO("Transport mode is disabled");
	}

	// Sort interfaces according to bitrate
	prioritize_interfaces();

// TODO
/*p
	// Synthesize tunnels for any interfaces wanting it
	for interface in Transport.interfaces:
		interface.tunnel_id = None
		if hasattr(interface, "wants_tunnel") and interface.wants_tunnel:
			Transport.synthesize_tunnel(interface)
*/

//#ifndef NDEBUG
	// CBA DEBUG
	dump_stats();
//#endif
}

/*static*/ void Transport::loop() {
	if (OS::time() > (_jobs_last_run + _job_interval)) {
		jobs();
		_jobs_last_run = OS::time();
	}
}

/*static*/ void Transport::jobs() {
	//TRACE("Transport::jobs()");

	std::vector<Packet> outgoing;
	std::map<Bytes, Interface> path_requests;	// destination_hash -> blocked_interface ({NONE} = no interface to avoid)
	int count;
	_jobs_running = true;

	try {
		if (!_jobs_locked) {

			// Process active and pending link lists
			if (OS::time() > (_links_last_checked + _links_check_interval)) {
				std::set<Link> pending_links(_pending_links);
				for (auto& link : pending_links) {
					if (link.status() == Type::Link::CLOSED) {
						// If we are not a Transport Instance, finding a pending link
						// that was never activated will trigger an expiry of the path
						// to the destination, and an attempt to rediscover the path.
						if (!Reticulum::transport_enabled()) {
							expire_path(link.destination().hash());

							// If we are connected to a shared instance, it will take
							// care of sending out a new path request. If not, we will
							// send one directly.
							if (!_owner.is_connected_to_shared_instance()) {
								double last_path_request = 0;
								auto iter = _path_requests.find(link.destination().hash());
								if (iter != _path_requests.end()) {
									last_path_request = (*iter).second;
								}

								if ((OS::time() - last_path_request) > Type::Transport::PATH_REQUEST_MI) {
									DEBUGF("Trying to rediscover path for %s since an attempted link was never established", link.destination().hash().toHex().c_str());
									if (path_requests.count(link.destination().hash()) == 0) {
										path_requests.emplace(link.destination().hash(), Interface{Type::NONE});
									}
								}
							}
						}

						_pending_links.erase(link);
					}
				}
				std::set<Link> active_links(_active_links);
				for (auto& link : active_links) {
					if (link.status() == Type::Link::CLOSED) {
						_active_links.erase(link);
					}
				}

				// Pump each active link's resource watchdogs. Resource
				// retransmit/timeout retries depend on this — without it,
				// a dropped resource part stalls the transfer forever
				// (we never re-request the missing part, server gives up).
				// const_cast mirrors the pattern used elsewhere when
				// iterating std::set<Link> — the wrapper is const but
				// shared_ptr-backed mutation is the codebase convention.
				for (auto& link_const : active_links) {
					Link& link = const_cast<Link&>(link_const);
					if (link.status() != Type::Link::CLOSED) {
						link.tick_resources();
					}
				}

				_links_last_checked = OS::time();
			}

			// Process receipts list for timed-out packets
			if (OS::time() > (_receipts_last_checked + _receipts_check_interval)) {
				while (_receipts.size() > Type::Transport::MAX_RECEIPTS) {
					//p culled_receipt = Transport.receipts.pop(0)
					PacketReceipt culled_receipt = _receipts.front();
					_receipts.pop_front();
					culled_receipt.set_timeout(-1);
					culled_receipt.check_timeout();
				}

				std::list<PacketReceipt> cull_receipts;
				for (auto& receipt : _receipts) {
					receipt.check_timeout();
					if (receipt.status() != Type::PacketReceipt::SENT) {
						//p if receipt in Transport.receipts:
						//p 	Transport.receipts.remove(receipt)
						cull_receipts.push_back(receipt);
					}
				}
				// CBA since modifying of collection while iterating is forbidden
				for (auto& receipt : cull_receipts) {
					_receipts.remove(receipt);
				}

				_receipts_last_checked = OS::time();
			}

			// Process announces needing retransmission
			if (OS::time() > (_announces_last_checked + _announces_check_interval)) {
				//p for destination_hash in Transport.announce_table:
				for (auto& [destination_hash, announce_entry] : _announce_table) {
				//for (auto& pair : _announce_table) {
				//	const auto& destination_hash = pair.first;
				//	auto& announce_entry = pair.second;
//TRACEF("[0] announce entry data size: %u", announce_entry._packet.data().size());
					//p announce_entry = Transport.announce_table[destination_hash]
					if (announce_entry._retries > 0 && announce_entry._retries >= Type::Transport::LOCAL_REBROADCASTS_MAX) {
						TRACEF("Completed announce processing for %s, local rebroadcast limit reached", destination_hash.toHex().c_str());
						// CBA OK to modify collection here since we're immediately exiting iteration
						_announce_table.erase(destination_hash);
						break;
					}
					else if (announce_entry._retries > Type::Transport::PATHFINDER_R) {
						TRACEF("Completed announce processing for %s, retry limit reached", destination_hash.toHex().c_str());
						// CBA OK to modify collection here since we're immediately exiting iteration
						_announce_table.erase(destination_hash);
						break;
					}
					else {
						if (OS::time() > announce_entry._retransmit_timeout) {
							TRACEF("Performing announce processing for %s...", destination_hash.toHex().c_str());
							announce_entry._retransmit_timeout = OS::time() + Type::Transport::PATHFINDER_G + Type::Transport::PATHFINDER_RW;
							announce_entry._retries += 1;
							//p packet = announce_entry[5]
							//p block_rebroadcasts = announce_entry[7]
							//p attached_interface = announce_entry[8]
							Type::Packet::context_types announce_context = Type::Packet::CONTEXT_NONE;
							if (announce_entry._block_rebroadcasts) {
								announce_context = Type::Packet::PATH_RESPONSE;
							}
							//p announce_data = packet.data
							Identity announce_identity(Identity::recall(announce_entry._packet.destination_hash()));
							//Destination announce_destination(announce_identity, Type::Destination::OUT, Type::Destination::SINGLE, "unknown", "unknown");
							//announce_destination.hash(announce_entry._packet.destination_hash());
							Destination announce_destination(announce_identity, Type::Destination::OUT, Type::Destination::SINGLE, announce_entry._packet.destination_hash());
							//P announce_destination.hexhash = announce_destination.hash.hex()

//if (announce_entry._attached_interface) {
//TRACE("[1] interface is valid");
//TRACEF("[1] interface: %s", announce_entry._attached_interface.debugString().c_str());
//TRACEF("[1] interface: %s", announce_entry._attached_interface.toString().c_str());
//}
							Packet new_packet = Packet(announce_destination, announce_entry._packet.data())
								.attached_interface(announce_entry._attached_interface)
								.packet_type(Type::Packet::ANNOUNCE)
								.context(announce_context)
								.transport_type(Type::Transport::TRANSPORT)
								.header_type(Type::Packet::HEADER_2)
								.transport_id(Transport::_identity.hash())
								.context_flag(announce_entry._packet.context_flag());

							new_packet.hops(announce_entry._hops);
							if (announce_entry._block_rebroadcasts) {
								DEBUGF("Rebroadcasting announce as path response for %s with hop count %d", announce_destination.hash().toHex().c_str(), new_packet.hops());
							}
							else {
								DEBUGF("Rebroadcasting announce for %s with hop count %d", announce_destination.hash().toHex().c_str(), new_packet.hops());
							}
							
							outgoing.push_back(new_packet);

							// This handles an edge case where a peer sends a past
							// request for a destination just after an announce for
							// said destination has arrived, but before it has been
							// rebroadcast locally. In such a case the actual announce
							// is temporarily held, and then reinserted when the path
							// request has been served to the peer.
							//p if destination_hash in Transport.held_announces:
							auto iter =_held_announces.find(destination_hash);
							if (iter != _held_announces.end()) {
								//p held_entry = Transport.held_announces.pop(destination_hash)
								auto held_entry = (*iter).second;
								_held_announces.erase(iter);
								//p Transport.announce_table[destination_hash] = held_entry
								//_announce_table[destination_hash] = held_entry;
								//_announce_table.insert_or_assign({destination_hash, held_entry});
								_announce_table.erase(destination_hash);
								// CBA ACCUMULATES
								_announce_table.insert({destination_hash, held_entry});
								DEBUG("Reinserting held announce into table");
								// CBA IMMEDIATE CULL
								cull_announce_table();
							}
						}
					}
				}

				_announces_last_checked = OS::time();
			}

			// Cull the packet hashlist if it has reached its max size
			// CBA microStore
			// CBA Culling no longer necessary since switch to microStore

			// Cull invalidated path requests
			if (OS::time() > (_pending_prs_last_checked + _pending_prs_check_interval)) {
				std::vector<Bytes> stale_local_prs;
				for (auto& [destination_hash, interface] : _pending_local_path_requests) {
					if (!find_interface_from_hash(destination_hash)) {
						stale_local_prs.push_back(destination_hash);
					}
				}
				for (auto& destination_hash : stale_local_prs) {
					_pending_local_path_requests.erase(destination_hash);
				}

				_pending_prs_last_checked = OS::time();
			}

			// Cull the path request tags list if it has reached its max size
			// CBA Culling no longer necessary since switch to GenerationalSet<>

			if (OS::time() > (_tables_last_culled + _tables_cull_interval)) {

                // Remove unneeded path state entries
				try {
					std::vector<Bytes> stale_path_states;
					stale_path_states.reserve(_path_states.size());
					for (const auto& [destination_hash, state] : _path_states) {
						DestinationEntry destination_entry;
						if (!_new_path_table.get(destination_hash, destination_entry) || !destination_entry) {
							stale_path_states.push_back(destination_hash);
						}
					}
					for (const Bytes& destination_hash : stale_path_states) {
						_path_states.erase(destination_hash);
					}
				}
				catch (const std::bad_alloc&) {
					ERROR("jobs: bad_alloc - out of memory culling path states");
				}
				catch (const std::exception& e) {
					ERRORF("jobs: failed to cull path states: %s", e.what());
				}

				// Cull the reverse table according to timeout
				try {
					std::vector<Bytes> stale_reverse_entries;
					stale_reverse_entries.reserve(_reverse_table.size());
					for (const auto& [packet_hash, reverse_entry] : _reverse_table) {
						if (OS::time() > (reverse_entry._timestamp + REVERSE_TIMEOUT)) {
							stale_reverse_entries.push_back(packet_hash);
						}
					}
					remove_reverse_entries(stale_reverse_entries);
				}
				catch (const std::bad_alloc&) {
					ERROR("jobs: bad_alloc - out of memory culling reverse table");
				}
				catch (const std::exception& e) {
					ERRORF("jobs: failed to cull reverse table: %s", e.what());
				}

				// Cull the link table according to timeout
				try {
					std::vector<Bytes> stale_links;
					stale_links.reserve(_link_table.size());
					for (const auto& [link_id, link_entry] : _link_table) {
						if (link_entry._validated) {
							if (OS::time() > (link_entry._timestamp + LINK_TIMEOUT)) {
								stale_links.push_back(link_id);
							}
						}
						else {
							if (OS::time() > link_entry._proof_timeout) {
								stale_links.push_back(link_id);

								double last_path_request = 0.0;
								const auto& iter = _path_requests.find(link_entry._destination_hash);
								if (iter != _path_requests.end()) {
									last_path_request = (*iter).second;
								}

								uint8_t lr_taken_hops = link_entry._hops;

								bool path_request_throttle = (OS::time() - last_path_request) < PATH_REQUEST_MI;
								bool path_request_conditions = false;
								Interface blocked_if{Type::NONE};

								// If the path has been invalidated between the time of
								// making the link request and now, try to rediscover it
								if (!has_path(link_entry._destination_hash)) {
									DEBUGF("Trying to rediscover path for %s since an attempted link was never established, and path is now missing", link_entry._destination_hash.toHex().c_str());
									path_request_conditions = true;
								}

								// If this link request was originated from a local client
								// attempt to rediscover a path to the destination, if this
								// has not already happened recently.
								else if (!path_request_throttle && lr_taken_hops == 0) {
									DEBUGF("Trying to rediscover path for %s since an attempted local client link was never established", link_entry._destination_hash.toHex().c_str());
									path_request_conditions = true;
								}

								// If the link destination was previously only 1 hop
								// away, this likely means that it was local to one
								// of our interfaces, and that it roamed somewhere else.
								// In that case, try to discover a new path, and mark
								// the old one as unresponsive.
								else if (!path_request_throttle && hops_to(link_entry._destination_hash) == 1) {
									DEBUGF("Trying to rediscover path for %s since an attempted link was never established, and destination was previously local to an interface on this instance", link_entry._destination_hash.toHex().c_str());
									path_request_conditions = true;
									blocked_if = link_entry._receiving_interface;

									if (Reticulum::transport_enabled()) {
										if (link_entry._receiving_interface && link_entry._receiving_interface.mode() != Type::Interface::MODE_BOUNDARY) {
											mark_path_unresponsive(link_entry._destination_hash);
										}
									}
								}

								// If the link initiator is only 1 hop away,
								// this likely means that network topology has
								// changed. In that case, we try to discover a new path,
								// and mark the old one as potentially unresponsive.
								else if ( !path_request_throttle and lr_taken_hops == 1) {
									DEBUGF("Trying to rediscover path for %s since an attempted link was never established, and link initiator is local to an interface on this instance", link_entry._destination_hash.toHex().c_str());
									path_request_conditions = true;
									blocked_if = link_entry._receiving_interface;

									if (Reticulum::transport_enabled()) {
										if (link_entry._receiving_interface && link_entry._receiving_interface.mode() != Type::Interface::MODE_BOUNDARY) {
											mark_path_unresponsive(link_entry._destination_hash);
										}
									}
								}

								if (path_request_conditions) {
									if (path_requests.count(link_entry._destination_hash) == 0) {
										path_requests.emplace(link_entry._destination_hash, blocked_if);
									}

									if (!Reticulum::transport_enabled()) {
										// Drop current path if we are not a transport instance, to
										// allow using higher-hop count paths or reused announces
										// from newly adjacent transport instances.
										expire_path(link_entry._destination_hash);
									}
								}
							}
						}
					}
					remove_links(stale_links);
				}
				catch (const std::bad_alloc&) {
					ERROR("jobs: bad_alloc - out of memory culling link table");
				}
				catch (const std::exception& e) {
					ERRORF("jobs: failed to cull link table: %s", e.what());
				}

				// Cull the path table
				// CBA microStore
				// CBA Culling of path table no longer necessary since switch to microStore

                // Cull the pending path requests table
				try {
					std::vector<Bytes> stale_path_requests;
					for (const auto& [destination_hash, timestamp] : _path_requests) {
						if (OS::time() > (timestamp + PATH_REQUEST_GATE_TIMEOUT)) {
							stale_path_requests.push_back(destination_hash);
						}
					}
					for (const Bytes& destination_hash : stale_path_requests) {
						_path_requests.erase(destination_hash);
					}
				}
				catch (const std::exception& e) {
					ERRORF("jobs: failed to cull path requests: %s", e.what());
				}

				// Cull the pending discovery path requests table
				try {
					std::vector<Bytes> stale_discovery_path_requests;
					stale_discovery_path_requests.reserve(_discovery_path_requests.size());
					for (const auto& [destination_hash, path_entry] : _discovery_path_requests) {
						if (OS::time() > path_entry._timeout) {
							stale_discovery_path_requests.push_back(destination_hash);
							DEBUGF("Waiting path request for %s timed out and was removed", destination_hash.toHex().c_str());
						}
					}
					remove_discovery_path_requests(stale_discovery_path_requests);
				}
				catch (const std::bad_alloc&) {
					ERROR("jobs: bad_alloc - out of memory culling discovery path requests");
				}
				catch (const std::exception& e) {
					ERRORF("jobs: failed to cull discovery path requests: %s", e.what());
				}

				// Cull the tunnel table
				try {
					count = 0;
					std::vector<Bytes> stale_tunnels;
					stale_tunnels.reserve(_tunnels.size());
					for (const auto& [tunnel_id, tunnel_entry] : _tunnels) {
						if (OS::time() > tunnel_entry._expires) {
							stale_tunnels.push_back(tunnel_id);
							TRACEF("Tunnel %s timed out and was removed", tunnel_id.toHex().c_str());
						}
						else {
							std::vector<Bytes> stale_tunnel_paths;
							for (const auto& [destination_hash, destination_entry] : tunnel_entry._serialised_paths) {
								if (OS::time() > (destination_entry._timestamp + DESTINATION_TIMEOUT)) {
									stale_tunnel_paths.push_back(destination_hash);
									TRACEF("Tunnel path to %s timed out and was removed", destination_hash.toHex().c_str());
								}
							}

							//for (const auto& destination_hash : stale_tunnel_paths) {
							for (const Bytes& destination_hash : stale_tunnel_paths) {
								const_cast<TunnelEntry&>(tunnel_entry)._serialised_paths.erase(destination_hash);
								++count;
							}
						}
					}
					if (count > 0) {
						TRACEF("Removed %d tunnel paths", count);
					}
					remove_tunnels(stale_tunnels);
				}
				catch (const std::bad_alloc&) {
					ERROR("jobs: bad_alloc - out of memory culling tunnel table");
				}
				catch (const std::exception& e) {
					ERRORF("jobs: failed to cull tunnel table: %s", e.what());
				}

//#ifndef NDEBUG
				dump_stats();
//#endif

				_tables_last_culled = OS::time();
			}

            // Check expired blackhole entries
			if (OS::time() > (_blackhole_last_checked + _blackhole_check_interval)) {
				try {
					std::vector<Bytes> stale_blackholes;
					double now = OS::time();
					for (const auto& [identity_hash, entry] : _blackholed_identities) {
						if (entry._until > 0.0 && now > entry._until) {
							stale_blackholes.push_back(identity_hash);
						}
					}
					for (const Bytes& identity_hash : stale_blackholes) {
						_blackholed_identities.erase(identity_hash);
					}
					if (!stale_blackholes.empty()) {
						VERBOSEF("Removed %zu expired blackhole entries", stale_blackholes.size());
					}
				}
				catch (const std::exception& e) {
					ERRORF("jobs: failed to expire blackhole entries: %s", e.what());
				}
				_blackhole_last_checked = OS::time();
			}

			// Refresh per-interface and class-level traffic counters and speeds
			if (OS::time() > (_traffic_last_checked + _traffic_check_interval)) {
				try {
					count_traffic();
				}
				catch (const std::exception& e) {
					ERRORF("jobs: failed to count traffic: %s", e.what());
				}
				_traffic_last_checked = OS::time();
			}

            // Run interface-related jobs
			if (OS::time() > (_interface_last_jobs + _interface_jobs_interval)) {
				prioritize_interfaces();
				// TODO
/*
				try {
					for (auto& interface : _interfaces) {
						interface.should_ingress_limit();
						interface.should_ingress_limit_pr();
						interface.process_held_announces();
						if (interface.phy_keepalive()) interface.send_keepalive();
					}
				}
				catch (const std::exception& e) {
					ERRORF("Error while processing held per-interface announces: %s", e.what());
				}
*/
				_interface_last_jobs = OS::time();
			}

			// CBA Periodically persist data
			//if (OS::time() > (_last_saved + _save_interval)) {
			//	persist_data();
			//	_last_saved = OS::time();
			//}
		}
		else {
			// Transport jobs were locked, do nothing
			//p pass
		}
	}
	catch (const std::exception& e) {
		ERROR("An exception occurred while running Transport jobs.");
		ERRORF("The contained exception was: %s", e.what());
	}

	_jobs_running = false;

	// CBA send announce retransmission packets
	for (auto& packet : outgoing) {
		packet.send();
	}

	// Queue link-related path requests into the bounded discovery PR queue
	// for throttled transmission via handle_disovery_path_requests().
	if (!path_requests.empty()) {
		for (const auto& [destination_hash, blocked_if] : path_requests) {
			// Skip if this destination is already queued
			bool already_queued = false;
			for (const auto& entry : _pending_discovery_prs) {
				if (entry._destination_hash == destination_hash) {
					already_queued = true;
					break;
				}
			}
			if (already_queued) continue;
			// Skip if queue is at capacity
			if (_pending_discovery_prs.size() >= MAX_QUEUED_DISCOVERY_PRS) break;
			_pending_discovery_prs.emplace_back(destination_hash, blocked_if);
		}
	}

	// Drain one queued discovery path request if the throttle has elapsed
	if (!_pending_discovery_prs.empty()) {
		handle_disovery_path_requests();
	}

	// Send announces for management destinations
	if (OS::time() > (_last_mgmt_announce + _mgmt_announce_interval)) {
		_last_mgmt_announce = OS::time();
		TRACE("Sending management announces...");
		try {
			//p def job():
			//p 	for destination in Transport.mgmt_destinations: destination.announce()
			//p threading.Thread(target=job, daemon=True).start()
			for (auto destination : _mgmt_destinations) {
				destination.announce();
			}
		}
		catch (const std::exception& e) {
			ERRORF("Error while sending management announces: %s", e.what());
		}
	}
}

/*static*/ bool Transport::transmit(Interface& interface, const Bytes& raw) {
	TRACE("Transport::transmit()");
	bool sent = false;
	// CBA
	if (_callbacks._transmit_packet) {
		try {
			_callbacks._transmit_packet(raw, interface);
		}
		catch (const std::exception& e) {
			DEBUGF("Error while executing transmit packet callback. The contained exception was: %s", e.what());
		}
	}
	try {
		//if hasattr(interface, "ifac_identity") and interface.ifac_identity != None:
		if (interface.ifac_identity()) {
// TODO
/*p
			// Calculate packet access code
			ifac = interface.ifac_identity.sign(raw)[-interface.ifac_size:]

			// Generate mask
			mask = RNS.Cryptography.hkdf(
				length=len(raw)+interface.ifac_size,
				derive_from=ifac,
				salt=interface.ifac_key,
				context=None,
			)

			// Set IFAC flag
			new_header = bytes([raw[0] | 0x80, raw[1]])

			// Assemble new payload with IFAC
			new_raw    = new_header+ifac+raw[2:]
			
			// Mask payload
			i = 0; masked_raw = b""
			for byte in new_raw:
				if i == 0:
					// Mask first header byte, but make sure the
					// IFAC flag is still set
					masked_raw += bytes([byte ^ mask[i] | 0x80])
				elif i == 1 or i > interface.ifac_size+1:
					// Mask second header byte and payload
					masked_raw += bytes([byte ^ mask[i]])
				else:
					// Don't mask the IFAC itself
					masked_raw += bytes([byte])
				i += 1

			// Send it
			sent = interface.on_outgoing(masked_raw)
*/
		}
		else {
			sent = interface.send_outgoing(raw);
		}
	}
	catch (const std::exception& e) {
		ERRORF("Error while transmitting on %s. The contained exception was: %s", interface.toString().c_str(), e.what());
	}
	return sent;
}

/*static*/ bool Transport::outbound(Packet& packet) {
	TRACE("Transport::outbound()");
	++_packets_sent;

	if (!packet.destination()) {
		//throw std::invalid_argument("Can not send packet with no destination.");
		ERROR("Can not send packet with no destination");
		return false;
	}

	TRACEF("Transport::outbound: destination=%s hops=%d", packet.destination_hash().toHex().c_str(), packet.hops());

	while (_jobs_running) {
		TRACE("Transport::outbound: sleeping...");
		OS::sleep(0.0005);
	}
	_jobs_locked = true;

	bool sent = false;
	double outbound_time = OS::time();

	// Check if we have a known path for the destination in the path table
    //if packet.packet_type != RNS.Packet.ANNOUNCE and packet.destination.type != RNS.Destination.PLAIN and packet.destination.type != RNS.Destination.GROUP and packet.destination_hash in Transport.destination_table:
	// CBA microStore
	//auto& destination_entry = get_path(packet.destination_hash());
	DestinationEntry destination_entry;
	_new_path_table.get(packet.destination_hash(), destination_entry);
	if (packet.packet_type() != Type::Packet::ANNOUNCE && packet.destination().type() != Type::Destination::PLAIN && packet.destination().type() != Type::Destination::GROUP && destination_entry) {
		TRACE("Transport::outbound: Path to destination is known");
        //outbound_interface = Transport.destination_table[packet.destination_hash][5]
		Interface outbound_interface = destination_entry.receiving_interface();

		// If there's more than one hop to the destination, and we know
		// a path, we insert the packet into transport by adding the next
		// transport nodes address to the header, and modifying the flags.
		// This rule applies both for "normal" transport, and when connected
		// to a local shared Reticulum instance.
        //if Transport.destination_table[packet.destination_hash][2] > 1:
		if (destination_entry._hops > 1) {
			TRACE("Forwarding packet to next closest interface...");
			if (packet.header_type() == Type::Packet::HEADER_1) {
				// Insert packet into transport
                //new_flags = (RNS.Packet.HEADER_2) << 6 | (Transport.TRANSPORT) << 4 | (packet.flags & 0b00001111)
				uint8_t new_flags = (Type::Packet::HEADER_2) << 6 | (Type::Transport::TRANSPORT) << 4 | (packet.flags() & 0b00001111);
				// CBA RESERVE
				//Bytes new_raw;
				Bytes new_raw(512);
				//new_raw = struct.pack("!B", new_flags)
				new_raw << new_flags;
				//new_raw += packet.raw[1:2]
				new_raw << packet.raw().mid(1,1);
				//new_raw += Transport.destination_table[packet.destination_hash][1]
				new_raw << destination_entry._received_from;
				//new_raw += packet.raw[2:]
				new_raw << packet.raw().mid(2);
				sent = transmit(outbound_interface, new_raw);
				//_path_table[packet.destination_hash][0] = time.time()
				destination_entry._timestamp = OS::time();
			}
		}

		// In the special case where we are connected to a local shared
		// Reticulum instance, and the destination is one hop away, we
		// also add transport headers to inject the packet into transport
		// via the shared instance. Normally a packet for a destination
		// one hop away would just be broadcast directly, but since we
		// are "behind" a shared instance, we need to get that instance
		// to transport it onto the network.
        //elif Transport.destination_table[packet.destination_hash][2] == 1 and Transport.owner.is_connected_to_shared_instance:
		else if (destination_entry._hops == 1 && _owner.is_connected_to_shared_instance()) {
			TRACE("Transport::outbound: Sending packet for directly connected interface to shared instance...");
			if (packet.header_type() == Type::Packet::HEADER_1) {
				// Insert packet into transport
				//new_flags = (RNS.Packet.HEADER_2) << 6 | (Transport.TRANSPORT) << 4 | (packet.flags & 0b00001111)
				uint8_t new_flags = (Type::Packet::HEADER_2) << 6 | (Type::Transport::TRANSPORT) << 4 | (packet.flags() & 0b00001111);
				// CBA RESERVE
				//Bytes new_raw;
				Bytes new_raw(512);
				//new_raw = struct.pack("!B", new_flags)
				new_raw << new_flags;
				//new_raw += packet.raw[1:2]
				new_raw << packet.raw().mid(1, 1);
				//new_raw += Transport.destination_table[packet.destination_hash][1]
				new_raw << destination_entry._received_from;
				//new_raw += packet.raw[2:]
				new_raw << packet.raw().mid(2);
				sent = transmit(outbound_interface, new_raw);
				//Transport.destination_table[packet.destination_hash][0] = time.time()
				destination_entry._timestamp = OS::time();
			}
		}

		// If none of the above applies, we know the destination is
		// directly reachable, and also on which interface, so we
		// simply transmit the packet directly on that one.
		else {
			TRACE("Transport::outbound: Sending packet over directly connected interface...");
			sent = transmit(outbound_interface, packet.raw());
		}
	}
	// If we don't have a known path for the destination, we'll
	// broadcast the packet on all outgoing interfaces, or the
	// just the relevant interface if the packet has an attached
	// interface, or belongs to a link.
	else {
		TRACE("Transport::outbound: Path to destination is unknown");
		bool stored_hash = false;
		for (auto& interface : _interfaces) {
			TRACEF("Transport::outbound: Checking interface %s", interface.toString().c_str());
			if (interface.OUT()) {
				bool should_transmit = true;

				if (packet.destination().type() == Type::Destination::LINK) {
					if (!packet.destination_link()) throw std::invalid_argument("Packet is not associated with a Link");
					if (packet.destination_link().status() == Type::Link::CLOSED) {
						TRACE("Transport::outbound: Pscket destination is link-closed, not transmitting");
						should_transmit = false;
					}
					// CBA TODO Check the following logic which is meant to send Link packets only on the same interface the Link was established on
					else if (packet.destination_link().attached_interface() && interface != packet.destination_link().attached_interface()) {
						TRACE("Transport::outbound: Packet destination link has different attached interface, not transmitting");
						should_transmit = false;
					}
				}

				if (packet.attached_interface() && interface != packet.attached_interface()) {
					TRACE("Transport::outbound: Packet has wrong attached interface, not transmitting");
					should_transmit = false;
				}

				if (packet.packet_type() == Type::Packet::ANNOUNCE) {
					//++_announces_sent;
					if (!packet.attached_interface()) {
						TRACE("Transport::outbound: Packet has no attached interface");
						if (interface.mode() == Type::Interface::MODE_ACCESS_POINT) {
							TRACEF("Blocking announce broadcast on %s due to AP mode", interface.toString().c_str());
							should_transmit = false;
						}
						else if (interface.mode() == Type::Interface::MODE_ROAMING) {
							//local_destination = next((d for d in Transport.destinations if d.hash == packet.destination_hash), None)
							//Destination local_destination({Type::NONE});
							auto iter = _destinations.find(packet.destination_hash());
							//if (iter != _destinations.end()) {
							//	local_destination = (*iter).second;
							//}
							if (iter != _destinations.end()) {
								TRACE("Allowing announce broadcast on roaming-mode interface from instance-local destination");
							}
							else {
								const Interface& from_interface = next_hop_interface(packet.destination_hash());
								//if from_interface == None or not hasattr(from_interface, "mode"):
								if (!from_interface || from_interface.mode() == Type::Interface::MODE_NONE) {
									should_transmit = false;
									if (!from_interface) {
										TRACEF("Blocking announce broadcast on %s since next hop interface doesn't exist", interface.toString().c_str());
									}
									else if (from_interface.mode() == Type::Interface::MODE_NONE) {
										TRACEF("Blocking announce broadcast on %s since next hop interface has no mode configured", interface.toString().c_str());
									}
								}
								else {
									if (from_interface.mode() == Type::Interface::MODE_ROAMING) {
										TRACEF("Blocking announce broadcast on %s due to roaming-mode next-hop interface", interface.toString().c_str());
										should_transmit = false;
									}
									else if (from_interface.mode() == Type::Interface::MODE_BOUNDARY) {
										TRACEF("Blocking announce broadcast on %s due to boundary-mode next-hop interface", interface.toString().c_str());
										should_transmit = false;
									}
								}
							}
						}
						else if (interface.mode() == Type::Interface::MODE_BOUNDARY) {
							//local_destination = next((d for d in Transport.destinations if d.hash == packet.destination_hash), None)
							// next and filter pattern?
							// next(iterable, default)
							// list comprehension: [x for x in xyz if x in a]
							// CBA TODO confirm that above pattern just selects the first matching destination
							auto iter = _destinations.find(packet.destination_hash());
							if (iter != _destinations.end()) {
								TRACE("Allowing announce broadcast on boundary-mode interface from instance-local destination");
							}
							else {
								const Interface& from_interface = next_hop_interface(packet.destination_hash());
								if (!from_interface || from_interface.mode() == Type::Interface::MODE_NONE) {
									should_transmit = false;
									if (!from_interface) {
										TRACEF("Blocking announce broadcast on %s since next hop interface doesn't exist", interface.toString().c_str());
									}
									else if (from_interface.mode() == Type::Interface::MODE_NONE) {
										TRACEF("Blocking announce broadcast on %s since next hop interface has no mode configured", interface.toString().c_str());
									}
								}
								else {
									if (from_interface.mode() == Type::Interface::MODE_ROAMING) {
										TRACEF("Blocking announce broadcast on %s due to roaming-mode next-hop interface", interface.toString().c_str());
										should_transmit = false;
									}
								}
							}
						}
						else {
							// Currently, annouces originating locally are always
							// allowed, and do not conform to bandwidth caps.
							// TODO: Rethink whether this is actually optimal.
							if (packet.hops() > 0) {

// TODO
/*p
								if not hasattr(interface, "announce_cap"):
									interface.announce_cap = RNS.Reticulum.ANNOUNCE_CAP

								if not hasattr(interface, "announce_allowed_at"):
									interface.announce_allowed_at = 0

								if not hasattr(interface, "announce_queue"):
										interface.announce_queue = []
*/

								bool queued_announces = (interface.announce_queue().size() > 0);
								if (!queued_announces && outbound_time > interface.announce_allowed_at()) {
									uint16_t wait_time = 0;
									if (interface.bitrate() > 0 && interface.announce_cap() > 0) {
										uint16_t tx_time = (packet.raw().size() * 8) / interface.bitrate();
										wait_time = (tx_time / interface.announce_cap());
									}
									interface.announce_allowed_at(outbound_time + wait_time);
								}
								else {
									should_transmit = false;
									if (interface.announce_queue().size() < Type::Reticulum::MAX_QUEUED_ANNOUNCES) {
										bool should_queue = true;
										for (auto& entry : interface.announce_queue()) {
											if (entry._destination == packet.destination_hash()) {
												uint64_t emission_timestamp = announce_emitted(packet);
												should_queue = false;
												if (emission_timestamp > entry._emitted) {
													entry._time = outbound_time;
													entry._hops = packet.hops();
													entry._emitted = emission_timestamp;
													entry._raw = packet.raw();
												}
												break;
											}
										}
										if (should_queue) {
											RNS::AnnounceEntry entry(
												packet.destination_hash(),
												outbound_time,
												packet.hops(),
												announce_emitted(packet),
												packet.raw()
											);

											queued_announces = (interface.announce_queue().size() > 0);
											// CBA ACCUMULATES
											interface.add_announce(entry);

											if (!queued_announces) {
												double wait_time = std::max(interface.announce_allowed_at() - OS::time(), (double)0);

												// CBA TODO THREAD?
												//z timer = threading.Timer(wait_time, interface.process_announce_queue)
												//z timer.start()

												if (wait_time < 1000) {
													TRACEF("Added announce to queue (height %lu) on %s for processing in %d ms", interface.announce_queue().size(), interface.toString().c_str(), (int)wait_time);
												}
												else {
													TRACEF("Added announce to queue (height %lu) on %s for processing in %.1f s", interface.announce_queue().size(), interface.toString().c_str(), OS::round(wait_time/1000,1));
												}
											}
											else {
												double wait_time = std::max(interface.announce_allowed_at() - OS::time(), (double)0);
												if (wait_time < 1000) {
													TRACEF("Added announce to queue (height %lu) on %s for processing in %d ms", interface.announce_queue().size(), interface.toString().c_str(), (int)wait_time);
												}
												else {
													TRACEF("Added announce to queue (height %lu) on %s for processing in %.1f s", interface.announce_queue().size(), interface.toString().c_str(), OS::round(wait_time/1000,1));
												}
											}
										}
									}
									else {
										//p pass
									}
								}
							}
							else {
								//p pass
							}
						}
					}
				}
						
				if (should_transmit) {
					TRACE("Transport::outbound: Packet transmission allowed");
					if (!stored_hash) {
						// CBA ACCUMULATES
						_packet_hashlist.put(packet.packet_hash().data(), (uint8_t)packet.packet_hash().size(), nullptr, 0);
						stored_hash = true;
					}

					// TODO: Re-evaluate potential for blocking
					// def send_packet():
					//     Transport.transmit(interface, packet.raw)
					// thread = threading.Thread(target=send_packet)
					// thread.daemon = True
					// thread.start()

					sent = transmit(interface, packet.raw());

					// Per-interface stat hooks. Matches Python Transport.py:1323-1324.
					if (sent) {
						if (packet.packet_type() == Type::Packet::ANNOUNCE) {
							interface.sent_announce();
						}
						if (packet.destination().type() == Type::Destination::PLAIN && packet.is_outbound_pr()) {
							interface.sent_path_request();
						}
					}
				}
				else {
					TRACE("Transport::outbound: Packet transmission refused");
				}
			}
		}
	}

	if (sent) {
		packet.sent(true);
		packet.sent_at(OS::time());

		// Don't generate receipt if it has been explicitly disabled
		if (packet.create_receipt() &&
			// Only generate receipts for DATA packets
			packet.packet_type() == Type::Packet::DATA &&
			// Don't generate receipts for PLAIN destinations
			packet.destination().type() != Type::Destination::PLAIN &&
			// Don't generate receipts for link-related packets
			!(packet.context() >= Type::Packet::KEEPALIVE && packet.context() <= Type::Packet::LRPROOF) &&
			// Don't generate receipts for resource packets
			!(packet.context() >= Type::Packet::RESOURCE && packet.context() <= Type::Packet::RESOURCE_RCL)) {

			PacketReceipt receipt(packet);
			packet.receipt(receipt);
			// CBA ACCUMULATES
			_receipts.push_back(receipt);
		}

		cache_packet(packet);
	}

	_jobs_locked = false;
	return sent;
}

#if 0
/*static*/ bool Transport::packet_filter(const Packet& packet) {
	// TODO: Think long and hard about this.
	// Is it even strictly necessary with the current
	// transport rules?
	if (packet.context() == Type::Packet::KEEPALIVE) {
		return true;
	}
	if (packet.context() == Type::Packet::RESOURCE_REQ) {
		return true;
	}
	if (packet.context() == Type::Packet::RESOURCE_PRF) {
		return true;
	}
	if (packet.context() == Type::Packet::RESOURCE) {
		return true;
	}
	if (packet.context() == Type::Packet::CACHE_REQUEST) {
		return true;
	}
	if (packet.context() == Type::Packet::CHANNEL) {
		return true;
	}

	if (packet.destination_type() == Type::Destination::PLAIN) {
		if (packet.packet_type() != Type::Packet::ANNOUNCE) {
			if (packet.hops() > 1) {
				DEBUGF("Dropped PLAIN packet %s with %d hops", packet.packet_hash().toHex().c_str(), packet.hops());
				return false;
			}
			else {
				return true;
			}
		}
		else {
			DEBUG("Dropped invalid PLAIN announce packet");
			return false;
		}
	}

	if (packet.destination_type() == Type::Destination::GROUP) {
		if (packet.packet_type() != Type::Packet::ANNOUNCE) {
			if (packet.hops() > 1) {
				DEBUGF("Dropped GROUP packet %s with %d hops", packet.packet_hash().toHex().c_str(), packet.hops());
				return false;
			}
			else {
				return true;
			}
		}
		else {
			DEBUG("Dropped invalid GROUP announce packet");
			return false;
		}
	}

	if (!_packet_hashlist.exists(packet.packet_hash().data(), (uint8_t)packet.packet_hash().size())) {
		TRACE("Transport::packet_filter: packet not previously seen");
		return true;
	}
	else {
		if (packet.packet_type() == Type::Packet::ANNOUNCE) {
			if (packet.destination_type() == Type::Destination::SINGLE) {
				TRACE("Transport::packet_filter: packet previously seen but is SINGLE ANNOUNCE");
				return true;
			}
			else {
				DEBUG("Dropped invalid announce packet");
				return false;
			}
		}
	}

	DEBUGF("Filtered packet with hash %s", packet.packet_hash().toHex().c_str());
	return false;
}
#endif

/*static*/ bool Transport::packet_filter(const Packet& packet) {

	// If connected to a shared instance, it will handle
	// packet filtering
	if (_owner && _owner.is_connected_to_shared_instance()) return true;

	// Filter packets intended for other transport instances
	if (packet.transport_id() && packet.packet_type() != Type::Packet::ANNOUNCE) {
		if (packet.transport_id() != Transport::identity().hash()) {
			TRACEF("Ignored packet %s in transport for other transport instance", packet.packet_hash().toHex().c_str());
			return false;
		}
	}

	switch (packet.context()) {
		case Type::Packet::KEEPALIVE: return true;
		case Type::Packet::RESOURCE_REQ: return true;
		case Type::Packet::RESOURCE_PRF: return true;
		case Type::Packet::RESOURCE: return true;
		case Type::Packet::CACHE_REQUEST: return true;
		case Type::Packet::CHANNEL: return true;
	}

	if (packet.destination_type() == Type::Destination::PLAIN) {
		if (packet.packet_type() != Type::Packet::ANNOUNCE) {
			if (packet.hops() > 1) {
				DEBUGF("Dropped PLAIN packet %s with %u hops", packet.packet_hash().toHex().c_str(), packet.hops());
				return false;
			}
			else {
				return true;
			}
		}
		else {
			DEBUG("Dropped invalid PLAIN announce packet");
			return false;
		}
	}

	if (packet.destination_type() == Type::Destination::GROUP) {
		if (packet.packet_type() != Type::Packet::ANNOUNCE) {
			if (packet.hops() > 1) {
				DEBUGF("Dropped GRPUP packet %s with %u hops", packet.packet_hash().toHex().c_str(), packet.hops());
				return false;
			}
			else {
				return true;
			}
		}
		else {
			DEBUG("Dropped invalid GROUP announce packet");
			return false;
		}
	}

	if (!_packet_hashlist.exists(packet.packet_hash().data(), (uint8_t)packet.packet_hash().size())) {
		TRACE("Transport::packet_filter: packet not previously seen");
		return true;
	}
	else {
		if (packet.packet_type() == Type::Packet::ANNOUNCE) {
			if (packet.destination_type() == Type::Destination::SINGLE) {
				TRACE("Transport::packet_filter: packet previously seen but is SINGLE ANNOUNCE");
				return true;
			}
			else {
				DEBUG("Dropped invalid announce packet");
				return false;
			}
		}
	}

	DEBUGF("Filtered packet with hash %s", packet.packet_hash().toHex().c_str());
	return false;
}

/*static*/ void Transport::inbound(const Bytes& raw, const Interface& interface /*= {Type::NONE}*/) {
	TRACEF("Transport::inbound: received %d bytes", raw.size());
	++_packets_received;
	// CBA
	if (_callbacks._receive_packet) {
		try {
			_callbacks._receive_packet(raw, interface);
		}
		catch (const std::exception& e) {
			DEBUGF("Error while executing receive packet callback. The contained exception was: %s", e.what());
		}
	}
// TODO
/*p
	// If interface access codes are enabled,
	// we must authenticate each packet.
	//if len(raw) > 2:
	if (raw.size() > 2) {
		if interface != None and hasattr(interface, "ifac_identity") and interface.ifac_identity != None:
			// Check that IFAC flag is set
			if raw[0] & 0x80 == 0x80:
				if len(raw) > 2+interface.ifac_size:
					// Extract IFAC
					ifac = raw[2:2+interface.ifac_size]

					// Generate mask
					mask = RNS.Cryptography.hkdf(
						length=len(raw),
						derive_from=ifac,
						salt=interface.ifac_key,
						context=None,
					)

					// Unmask payload
					i = 0; unmasked_raw = b""
					for byte in raw:
						if i <= 1 or i > interface.ifac_size+1:
							// Unmask header bytes and payload
							unmasked_raw += bytes([byte ^ mask[i]])
						else:
							// Don't unmask IFAC itself
							unmasked_raw += bytes([byte])
						i += 1
					raw = unmasked_raw

					// Unset IFAC flag
					new_header = bytes([raw[0] & 0x7f, raw[1]])

					// Re-assemble packet
					new_raw = new_header+raw[2+interface.ifac_size:]

					// Calculate expected IFAC
					expected_ifac = interface.ifac_identity.sign(new_raw)[-interface.ifac_size:]

					// Check it
					if ifac == expected_ifac:
						raw = new_raw
					else:
						return

				else:
					return

			else:
				// If the IFAC flag is not set, but should be,
				// drop the packet.
				return

		else:
			// If the interface does not have IFAC enabled,
			// check the received packet IFAC flag.
			if raw[0] & 0x80 == 0x80:
				// If the flag is set, drop the packet
				return
	}
	else {
		return;
	}
*/

	while (_jobs_running) {
		TRACE("Transport::inbound: sleeping...");
		OS::sleep(0.0005);
	}

	if (!_identity) {
		WARNING("Transport::inbound: No identity!");
		return;
	}

	_jobs_locked = true;

	Packet packet(Destination(Type::NONE), raw);
	if (!packet.unpack()) {
		WARNING("Transport::inbound: Packet unpack failed!");
		return;
	}
#ifndef NDEBUG
	TRACEF("Transport::inbound: packet: %s", packet.debugString().c_str());
#endif

	TRACEF("Transport::inbound: destination=%s hops=%d", packet.destination_hash().toHex().c_str(), packet.hops());

	packet.receiving_interface(interface);
	packet.hops(packet.hops() + 1);

	// Stamp signal-quality stats from the receiving interface onto the packet.
	// The interface subclass is expected to populate r_stat_rssi/snr/q
	// synchronously with handle_incoming() so the values describe THIS packet.
	// A NaN value means the interface didn't report that metric. Python keeps
	// a class-level cache keyed by packet_hash so shared-instance clients can
	// look up signal stats via RPC; microReticulum doesn't support being a
	// shared-instance client, so we stamp the packet directly and skip the
	// cache.
	if (interface) {
		if (!std::isnan(interface.r_stat_rssi())) packet.rssi(interface.r_stat_rssi());
		if (!std::isnan(interface.r_stat_snr()))  packet.snr(interface.r_stat_snr());
		if (!std::isnan(interface.r_stat_q()))    packet.q(interface.r_stat_q());
	}

	if (_local_client_interfaces.size() > 0) {
		if (is_local_client_interface(interface)) {
			packet.hops(packet.hops() - 1);
		}
	}
	else if (interface_to_shared_instance(interface)) {
		packet.hops(packet.hops() - 1);
	}

	//if (packet_filter(packet)) {
	// CBA
	bool accept = true;
	if (_callbacks._filter_packet) {
		try {
			accept = _callbacks._filter_packet(packet);
		}
		catch (const std::exception& e) {
			DEBUGF("Error while executing filter packet callback. The contained exception was: %s", e.what());
		}
	}
	if (accept) {
		accept = packet_filter(packet);
	}
	if (accept) {
		TRACE("Transport::inbound: Packet accepted by filter");

		// Defer hashlist insertion for packets belonging to links in
		// our link table, and for LRPROOF packets. On shared-medium
		// interfaces (e.g. LoRa), a packet may arrive on the "wrong"
		// interface first. Premature hash insertion would cause the
		// correct arrival to be filtered as a duplicate.
		// Reference: Python Transport.py lines 1362-1373
		bool remember_packet_hash = true;
		if (_link_table.find(packet.destination_hash()) != _link_table.end()) {
			remember_packet_hash = false;
		}
		if (packet.packet_type() == Type::Packet::PROOF && packet.context() == Type::Packet::LRPROOF) {
			remember_packet_hash = false;
		}
		if (remember_packet_hash) {
			// CBA ACCUMULATES
			_packet_hashlist.put(packet.packet_hash().data(), (uint8_t)packet.packet_hash().size(), nullptr, 0);
		}

		// CBA Currently this packet cache is a noop since it's not forced
		cache_packet(packet);
		
		// Check special conditions for local clients connected
		// through a shared Reticulum instance
		//p from_local_client         = (packet.receiving_interface in Transport.local_client_interfaces)
		bool from_local_client         = (_local_client_interfaces.find(packet.receiving_interface()) != _local_client_interfaces.end());
		//p for_local_client          = (packet.packet_type != RNS.Packet.ANNOUNCE) and (packet.destination_hash in Transport.destination_table and Transport.destination_table[packet.destination_hash][2] == 0)
		//p for_local_client_link     = (packet.packet_type != RNS.Packet.ANNOUNCE) and (packet.destination_hash in Transport.link_table and Transport.link_table[packet.destination_hash][4] in Transport.local_client_interfaces)
		//p for_local_client_link    |= (packet.packet_type != RNS.Packet.ANNOUNCE) and (packet.destination_hash in Transport.link_table and Transport.link_table[packet.destination_hash][2] in Transport.local_client_interfaces)

		// If packet is anything besides ANNOUNCE then determine if it's destinated for a local destination or link
		bool for_local_client = false;
		bool for_local_client_link = false;
		if (packet.packet_type() != Type::Packet::ANNOUNCE) {
			// CBA microStore
			//auto& destination_entry = get_path(packet.destination_hash());
			DestinationEntry destination_entry;
			_new_path_table.get(packet.destination_hash(), destination_entry);
			if (destination_entry) {
			 	if (destination_entry._hops == 0) {
					// Destined for a local destination
					for_local_client = true;
				}
			}
			auto link_iter = _link_table.find(packet.destination_hash());
			if (link_iter != _link_table.end()) {
				LinkEntry link_entry = (*link_iter).second;
			 	if (_local_client_interfaces.find(link_entry._receiving_interface) != _local_client_interfaces.end()) {
					// Destined for a local link
					for_local_client_link = true;
				}
			 	if (_local_client_interfaces.find(link_entry._outbound_interface) != _local_client_interfaces.end()) {
					// Destined for a local link
					for_local_client_link = true;
				}
			}
		}

		// Determine if packet is proof for local destination???
		//p proof_for_local_client    = (packet.destination_hash in Transport.reverse_table) and (Transport.reverse_table[packet.destination_hash][0] in Transport.local_client_interfaces)
		bool proof_for_local_client = false;
		auto reverse_iter = _reverse_table.find(packet.destination_hash());
		if (reverse_iter != _reverse_table.end()) {
			ReverseEntry reverse_entry = (*reverse_iter).second;
			if (_local_client_interfaces.find(reverse_entry._receiving_interface) != _local_client_interfaces.end()) {
				// Proof for local destination???
				proof_for_local_client = true;
			}
		}

		// Plain broadcast packets from local clients are sent
		// directly on all attached interfaces, since they are
		// never injected into transport.

		// If packet is not destined for a local transport-specific destination
		if (_control_hashes.find(packet.destination_hash()) == _control_hashes.end()) {
			// If packet is destination type PLAIN and transport type BROADCAST
			if (packet.destination_type() == Type::Destination::PLAIN && packet.transport_type() == Type::Transport::BROADCAST) {
				// Send to all interfaces except the one the packet was recieved on
				if (from_local_client) {
					for (auto& interface : _interfaces) {
						if (interface != packet.receiving_interface()) {
							TRACEF("Transport::inbound: Broadcasting packet on %s", interface.toString().c_str());
							transmit(interface, packet.raw());
						}
					}
				}
				// If the packet was not from a local client, send
				// it directly to all local clients
				else {
					for (const Interface& interface : _local_client_interfaces) {
						TRACEF("Transport::inbound: Broadcasting packet on %s", interface.toString().c_str());
						transmit(const_cast<Interface&>(interface), packet.raw());
					}
				}
			}
		}

		////////////////////////////////
		// TRANSPORT HANDLING
		////////////////////////////////

		// General transport handling. Takes care of directing
		// packets according to transport tables and recording
		// entries in reverse and link tables.
		if (Reticulum::transport_enabled() || from_local_client || for_local_client || for_local_client_link) {
			TRACE("Transport::inbound: Performing general transport handling");

			// If there is no transport id, but the packet is
			// for a local client, we generate the transport
			// id (it was stripped on the previous hop, since
			// we "spoof" the hop count for clients behind a
			// shared instance, so they look directly reach-
			// able), and reinsert, so the normal transport
			// implementation can handle the packet.
			if (!packet.transport_id() && for_local_client) {
				TRACE("Transport::inbound: Regenerating transport id");
				packet.transport_id(_identity.hash());
			}

			// If this is a cache request, and we can fullfill
			// it, do so and stop processing. Otherwise resume
			// normal processing.
			if (packet.context() == Type::Packet::CACHE_REQUEST) {
				if (cache_request_packet(packet)) {
					TRACE("Transport::inbound: Cached packet");
					return;
				}
			}

			// If the packet is in transport, check whether we
			// are the designated next hop, and process it
			// accordingly if we are.
			if (packet.transport_id() && packet.packet_type() != Type::Packet::ANNOUNCE) {
				TRACE("Transport::inbound: Packet is in transport...");
				if (packet.transport_id() == _identity.hash()) {
					TRACE("Transport::inbound: We are designated next-hop");
					// CBA microStore
					//auto& destination_entry = get_path(packet.destination_hash());
					DestinationEntry destination_entry;
					_new_path_table.get(packet.destination_hash(), destination_entry);
					if (destination_entry) {
						TRACE("Transport::inbound: Found next-hop path to destination");
						Bytes next_hop = destination_entry._received_from;
						uint8_t remaining_hops = destination_entry._hops;
						
						// CBA RESERVE
						//Bytes new_raw;
						Bytes new_raw(512);
						if (remaining_hops > 1) {
							// Just increase hop count and transmit
							//new_raw  = packet.raw[0:1]
							new_raw << packet.raw().left(1);
							//new_raw += struct.pack("!B", packet.hops)
							new_raw << packet.hops();
							//new_raw += next_hop
							new_raw << next_hop;
							//new_raw += packet.raw[(RNS.Identity.TRUNCATED_HASHLENGTH//8)+2:]
							new_raw << packet.raw().mid((Type::Identity::TRUNCATED_HASHLENGTH/8)+2);
						}
						else if (remaining_hops == 1) {
							// Strip transport headers and transmit
							//new_flags = (RNS.Packet.HEADER_1) << 6 | (Transport.BROADCAST) << 4 | (packet.flags & 0b00001111)
							uint8_t new_flags = (Type::Packet::HEADER_1) << 6 | (Type::Transport::BROADCAST) << 4 | (packet.flags() & 0b00001111);
							//new_raw = struct.pack("!B", new_flags)
							new_raw << new_flags;
							//new_raw += struct.pack("!B", packet.hops)
							new_raw << packet.hops();
							//new_raw += packet.raw[(RNS.Identity.TRUNCATED_HASHLENGTH//8)+2:]
							new_raw << packet.raw().mid((Type::Identity::TRUNCATED_HASHLENGTH/8)+2);
						}
						else if (remaining_hops == 0) {
							// Just increase hop count and transmit
							//new_raw  = packet.raw[0:1]
							new_raw << packet.raw().left(1);
							//new_raw += struct.pack("!B", packet.hops)
							new_raw << packet.hops();
							//new_raw += packet.raw[2:]
							new_raw << packet.raw().mid(2);
						}

						Interface outbound_interface = destination_entry.receiving_interface();

						if (packet.packet_type() == Type::Packet::LINKREQUEST) {
							TRACE("Transport::inbound: Packet is next-hop LINKREQUEST");
							double now = OS::time();
							double proof_timeout  = extra_link_proof_timeout(packet.receiving_interface());
							proof_timeout += now + Type::Link::ESTABLISHMENT_TIMEOUT_PER_HOP * std::max((uint8_t)1, remaining_hops);
							uint16_t path_mtu = Link::mtu_from_lr_packet(packet);
							Type::Link::link_mode mode = Link::mode_from_lr_packet(packet);
							uint16_t ph_mtu = 0;
							if (interface) {
								ph_mtu = interface.HW_MTU();
							}
							uint16_t nh_mtu = outbound_interface.HW_MTU();
							if (path_mtu != 0) {
								if (outbound_interface.HW_MTU() == 0) {
									DEBUG("No next-hop HW MTU, disabling link MTU upgrade");
									path_mtu = 0;
									new_raw  = new_raw.left(new_raw.size()-Type::Link::LINK_MTU_SIZE);
								}
								else if (outbound_interface.AUTOCONFIGURE_MTU() == 0 && outbound_interface.FIXED_MTU() == 0) {
									DEBUG("Outbound interface doesn't support MTU autoconfiguration, disabling link MTU upgrade");
									path_mtu = 0;
									new_raw  = new_raw.left(new_raw.size()-Type::Link::LINK_MTU_SIZE);
								}
								else {
									if (nh_mtu < path_mtu || (ph_mtu != 0 && ph_mtu < path_mtu)) {
										try {
											path_mtu = std::min(nh_mtu, (ph_mtu > 0) ? ph_mtu : nh_mtu);
											Bytes clamped_mtu = Link::signalling_bytes(path_mtu, mode);
											DEBUGF("Clamping link MTU to %u", path_mtu);
											new_raw  = new_raw.left(new_raw.size()-Type::Link::LINK_MTU_SIZE)+clamped_mtu;
										}
										catch (const std::exception& e) {
											WARNINGF("Dropping link request packet. The contained exception was: %s", e.what());
											return;
										}
									}
								}
							}
							LinkEntry link_entry(
								now,
								next_hop,
								outbound_interface,
								remaining_hops,
								packet.receiving_interface(),
								packet.hops(),
								packet.destination_hash(),
								false,
								proof_timeout
							);
							// CBA ACCUMULATES
							_link_table.insert({Link::link_id_from_lr_packet(packet), link_entry});
						}
						else {
							TRACE("Transport::inbound: Packet is next-hop other type");
							ReverseEntry reverse_entry(
								packet.receiving_interface(),
								outbound_interface,
								OS::time()
#if RNS_NEIGHBOR_PROBING
								//DIVERGENCE: record next_hop so a returning
								// proof can be attributed to this neighbor for
								// passive liveness inference.
								, next_hop
#endif
							);
							// CBA ACCUMULATES
							_reverse_table.insert({packet.getTruncatedHash(), reverse_entry});
						}
						TRACE("Transport::outbound: Sending packet to next hop...");
						transmit(outbound_interface, new_raw);
						destination_entry._timestamp = OS::time();
					}
					else {
						// TODO: There should probably be some kind of REJECT
						// mechanism here, to signal to the source that their
						// expected path failed.
						TRACEF("Got packet in transport, but no known path to final destination %s. Dropping packet.", packet.destination_hash().toHex().c_str());
					}
				}
				else {
					TRACE("Transport::inbound: We are not designated next-hop so not transporting");
				}
			}
			else {
				TRACE("Transport::inbound: Either packet is announce or packet has no next-hop (possibly for a local destination)");
			}

			// Link transport handling. Directs packets according
			// to entries in the link tables
			if (packet.packet_type() != Type::Packet::ANNOUNCE && packet.packet_type() != Type::Packet::LINKREQUEST && packet.context() != Type::Packet::LRPROOF) {
				TRACE("Transport::inbound: Checking if packet is meant for link transport...");
				auto link_iter = _link_table.find(packet.destination_hash());
				if (link_iter != _link_table.end()) {
					TRACE("Transport::inbound: Found link entry, handling link transport");
					LinkEntry& link_entry = (*link_iter).second;
					// If receiving and outbound interface is
					// the same for this link, direction doesn't
					// matter, and we simply send the packet on.
					Interface outbound_interface({Type::NONE});
					if (link_entry._outbound_interface == link_entry._receiving_interface) {
						// But check that taken hops matches one
						// of the expectede values.
						if (packet.hops() == link_entry._remaining_hops || packet.hops() == link_entry._hops) {
							TRACE("Transport::inbound: Link inbound/outbound interfaes are same, transporting on same interface");
							outbound_interface = link_entry._outbound_interface;
						}
					}
					else {
						// If interfaces differ, we transmit on
						// the opposite interface of what the
						// packet was received on.
						if (packet.receiving_interface() == link_entry._outbound_interface) {
							// Also check that expected hop count matches
							if (packet.hops() == link_entry._remaining_hops) {
								TRACE("Transport::inbound: Link transporting on inbound interface");
								outbound_interface = link_entry._receiving_interface;
							}
						}
						else if (packet.receiving_interface() == link_entry._receiving_interface) {
							// Also check that expected hop count matches
							if (packet.hops() == link_entry._hops) {
								TRACE("Transport::inbound: Link transporting on outbound interface");
								outbound_interface = link_entry._outbound_interface;
							}
						}
					}

					if (outbound_interface) {
						TRACE("Transport::inbound: Transmitting link transport packet");
						// CBA RESERVE
						//Bytes new_raw;
						Bytes new_raw(512);
						//new_raw = packet.raw[0:1]
						new_raw << packet.raw().left(1);
						//new_raw += struct.pack("!B", packet.hops)
						new_raw << packet.hops();
						//new_raw += packet.raw[2:]
						new_raw << packet.raw().mid(2);
						transmit(outbound_interface, new_raw);
						link_entry._timestamp = OS::time();
						// Deferred hashlist insertion for link transport packets
						_packet_hashlist.put(packet.packet_hash().data(), (uint8_t)packet.packet_hash().size(), nullptr, 0);
					}
					else {
						//p pass
					}
				}
			}
		}

		////////////////////////////////
		// LOCAL HANDLING
		////////////////////////////////

		// Announce handling. Handles logic related to incoming
		// announces, queueing rebroadcasts of these, and removal
		// of queued announce rebroadcasts once handed to the next node.
		if (packet.packet_type() == Type::Packet::ANNOUNCE) {
			++_announces_received;
			TRACE("Transport::inbound: Packet is ANNOUNCE");
			Bytes received_from;
			//p local_destination = next((d for d in Transport.destinations if d.hash == packet.destination_hash), None)
			auto iter = _destinations.find(packet.destination_hash());
			if (iter == _destinations.end() && Identity::validate_announce(packet)) {
				TRACE("Transport::inbound: Packet is announce for non-local destination, processing...");
				if (packet.transport_id()) {
					received_from = packet.transport_id();
					
					// Check if this is a next retransmission from
					// another node. If it is, we're removing the
					// announce in question from our pending table
					if (Reticulum::transport_enabled() && _announce_table.count(packet.destination_hash()) > 0) {
						//AnnounceEntry& announce_entry = _announce_table[packet.destination_hash()];
						AnnounceEntry& announce_entry = (*_announce_table.find(packet.destination_hash())).second;

						bool announce_erased = false;
						if ((packet.hops() - 1) == announce_entry._hops) {
							DEBUGF("Heard a local rebroadcast of announce for %s", packet.destination_hash().toHex().c_str());
							announce_entry._local_rebroadcasts += 1;
							if (announce_entry._local_rebroadcasts >= LOCAL_REBROADCASTS_MAX) {
								DEBUGF("Max local rebroadcasts of announce for %s reached, dropping announce from our table", packet.destination_hash().toHex().c_str());
								_announce_table.erase(packet.destination_hash());
								announce_erased = true;
							}
						}

						// CBA Checking announce_erased so we don't access announce_entry if it's already been freed!
						if (!announce_erased && (packet.hops() - 1) == (announce_entry._hops + 1) && announce_entry._retries > 0) {
							double now = OS::time();
							if (now < announce_entry._timestamp) {
								DEBUGF("Rebroadcasted announce for %s has been passed on to another node, no further tries needed", packet.destination_hash().toHex().c_str());
								_announce_table.erase(packet.destination_hash());
								announce_erased = true;
							}
						}
					}
				}
				else {
					received_from = packet.destination_hash();
				}

				// Check if this announce should be inserted into
				// announce and destination tables
				bool should_add = false;

				// First, check that the announce is not for a destination
				// local to this system, and that hops are less than the max
				// CBA TODO determine why packet destination hash is being searched in destinations again since we entered this logic becuase it did not exist above
				//if (not any(packet.destination_hash == d.hash for d in Transport.destinations) and packet.hops < Transport.PATHFINDER_M+1):
				auto iter = _destinations.find(packet.destination_hash());
				if (iter == _destinations.end() && packet.hops() < (PATHFINDER_M+1)) {
					uint64_t announce_emitted = Transport::announce_emitted(packet);

					//p random_blob = packet.data[RNS.Identity.KEYSIZE//8+RNS.Identity.NAME_HASH_LENGTH//8:RNS.Identity.KEYSIZE//8+RNS.Identity.NAME_HASH_LENGTH//8+10]
					Bytes random_blob = packet.data().mid(Type::Identity::KEYSIZE/8 + Type::Identity::NAME_HASH_LENGTH/8, Type::Identity::RANDOM_HASH_LENGTH/8);
					//p random_blobs = []
					std::vector<Bytes> empty_random_blobs;
					std::vector<Bytes>& random_blobs = empty_random_blobs;
					TRACEF("Checking for existing path to %s", packet.destination_hash().toHex().c_str());
					// CBA microStore
					//auto& destination_entry = get_path(packet.destination_hash());
					bool path_found = false;
					DestinationEntry destination_entry;
					_new_path_table.get(packet.destination_hash(), destination_entry);
					if (destination_entry) {
						path_found = true;
						TRACEF("Found existing path to %s", packet.destination_hash().toHex().c_str());
						//p random_blobs = Transport.destination_table[packet.destination_hash][4]
						random_blobs = destination_entry._random_blobs;

						// If we already have a path to the announced
						// destination, but the hop count is equal or
						// less, we'll update our tables.
						if (packet.hops() <= destination_entry._hops) {
							// Make sure we haven't heard the random
							// blob before, and that the announce is
							// newer than any we've already seen for
							// this path. Together this prevents both
							// replay forgery and acceptance of an
							// out-of-order older announce.
							uint64_t path_timebase = timebase_from_random_blobs(random_blobs);
							if (std::find(random_blobs.begin(), random_blobs.end(), random_blob) == random_blobs.end()
									&& announce_emitted > path_timebase) {
								mark_path_unknown_state(packet.destination_hash());
								should_add = true;
							}
							else {
								should_add = false;
							}
						}
						else {
							// If an announce arrives with a larger hop
							// count than we already have in the table,
							// ignore it, unless the path is expired, or
							// the emission timestamp is more recent.
							double now = OS::time();
							double path_expires = destination_entry._expires;
							
							uint64_t path_announce_emitted = 0;
							for (const Bytes& path_random_blob : random_blobs) {
								//p path_announce_emitted = max(path_announce_emitted, int.from_bytes(path_random_blob[5:10], "big"))
								path_announce_emitted = std::max(path_announce_emitted, OS::from_bytes_big_endian(path_random_blob.data() + 5, 5));
								if (path_announce_emitted >= announce_emitted) {
									break;
								}
							}

							if (now >= path_expires) {
								// We also check that the announce is
								// different from ones we've already heard,
								// to avoid loops in the network
								if (std::find(random_blobs.begin(), random_blobs.end(), random_blob) == random_blobs.end()) {
									// TODO: Check that this ^ approach actually
									// works under all circumstances
									DEBUGF("Replacing destination table entry for %s with new announce due to expired path", packet.destination_hash().toHex().c_str());
									mark_path_unknown_state(packet.destination_hash());
									should_add = true;
								}
								else {
									should_add = false;
								}
							}
							else {
								if (announce_emitted > path_announce_emitted) {
									if (std::find(random_blobs.begin(), random_blobs.end(), random_blob) == random_blobs.end()) {
										DEBUGF("Replacing destination table entry for %s with new announce, since it was more recently emitted", packet.destination_hash().toHex().c_str());
										mark_path_unknown_state(packet.destination_hash());
										should_add = true;
									}
									else {
										should_add = false;
									}
								}

								// If we have already heard this announce before,
								// but the path has been marked as unresponsive
								// by a failed communications attempt or similar,
								// allow updating the path table to this one.
								else if (announce_emitted == path_announce_emitted) {
									if (path_is_unresponsive(packet.destination_hash())) {
										DEBUGF("Replacing destination table entry for %s with new announce, since previously tried path was unresponsive", packet.destination_hash().toHex().c_str());
										should_add = true;
									}
									else {
										should_add = false;
									}
								}
							}
						}
					}
					else {
						// If this destination is unknown in our table
						// we should add it
						should_add = true;
					}

#if RNS_REJECT_BLACKHOLED_ANNOUNCE
					// DIVERGENCE: reject announces from blackholed identities
					// Python (Transport.py:313-337) only purges blackholed paths
					// at three triggers: explicit blackhole_identity() call,
					// reload_blackhole() at startup, and the 60s expiry sweep.
					// New announces from a blackholed identity still get accepted
					// into the path table and stay there until the next manual
					// purge. To make blackholing actually effective on a leaf
					// node we filter inline here: if the announce's associated
					// identity is blackholed, reject path acceptance now.
					if (should_add && !_blackholed_identities.empty()) {
						Identity announced_identity = Identity::recall(packet.destination_hash());
						if (announced_identity && is_blackholed(announced_identity.hash())) {
							DEBUGF("Dropping announce from blackholed identity %s", announced_identity.hash().toHex().c_str());
							should_add = false;
						}
					}
#endif

					if (should_add) {
						double now = OS::time();

						bool rate_blocked = false;

						// Announce rate-limit enforcement. The receiving interface opts in
						// by setting announce_rate_target > 0 on itself. Repeated announces
						// from the same destination faster than the target accumulate
						// rate_violations; once over the grace count, the destination is
						// blocked for target+penalty seconds.
						if (packet.context() != Type::Packet::PATH_RESPONSE
								&& packet.receiving_interface()
								&& packet.receiving_interface().announce_rate_target() > 0) {
							auto iter = _announce_rate_table.find(packet.destination_hash());
							if (iter == _announce_rate_table.end()) {
								_announce_rate_table.emplace(packet.destination_hash(), RateEntry(now));
							}
							else {
								RateEntry& rate_entry = iter->second;
								rate_entry._timestamps.push_back(now);
								while (rate_entry._timestamps.size() > Type::Transport::MAX_RATE_TIMESTAMPS) {
									rate_entry._timestamps.erase(rate_entry._timestamps.begin());
								}

								const double current_rate = now - rate_entry._last;

								if (now > rate_entry._blocked_until) {
									const float rate_target = packet.receiving_interface().announce_rate_target();
									if (current_rate < rate_target) {
										rate_entry._rate_violations += 1.0;
									}
									else {
										rate_entry._rate_violations = std::max(0.0, rate_entry._rate_violations - 1.0);
									}

									const uint8_t rate_grace = packet.receiving_interface().announce_rate_grace();
									if (rate_entry._rate_violations > rate_grace) {
										const float rate_penalty = packet.receiving_interface().announce_rate_penalty();
										rate_entry._blocked_until = rate_entry._last + rate_target + rate_penalty;
										rate_blocked = true;
									}
									else {
										rate_entry._last = now;
									}
								}
								else {
									rate_blocked = true;
								}
							}
						}

						uint8_t retries = 0;
						uint8_t announce_hops = packet.hops();
						uint8_t local_rebroadcasts = 0;
						bool block_rebroadcasts = false;
						Interface attached_interface = {Type::NONE};
						
						double retransmit_timeout = now + (Cryptography::random() * PATHFINDER_RW);

						double expires;
						if (packet.receiving_interface().mode() == Type::Interface::MODE_ACCESS_POINT) {
							expires = now + AP_PATH_TIME;
						}
						else if (packet.receiving_interface().mode() == Type::Interface::MODE_ROAMING) {
							expires = now + ROAMING_PATH_TIME;
						}
						else {
							expires = now + PATHFINDER_E;
						}

						// Append the new blob and cap the list at MAX_RANDOM_BLOBS,
						// dropping oldest entries from the front. Matches Python's
						// `random_blobs = random_blobs[-MAX_RANDOM_BLOBS:]` semantics.
						if (std::find(random_blobs.begin(), random_blobs.end(), random_blob) == random_blobs.end()) {
							random_blobs.push_back(random_blob);
							if (random_blobs.size() > MAX_RANDOM_BLOBS) {
								random_blobs.erase(random_blobs.begin(),
									random_blobs.begin() + (random_blobs.size() - MAX_RANDOM_BLOBS));
							}
						}

						if ((Reticulum::transport_enabled() || Transport::from_local_client(packet)) && packet.context() != Type::Packet::PATH_RESPONSE) {
							// Insert announce into announce table for retransmission

							if (rate_blocked) {
								DEBUGF("Blocking rebroadcast of announce from %s due to excessive announce rate", packet.destination_hash().toHex().c_str());
							}
							else {
								if (Transport::from_local_client(packet)) {
									// If the announce is from a local client,
									// it is announced immediately, but only one time.
									retransmit_timeout = now;
									retries = PATHFINDER_R;
								}
								AnnounceEntry announce_entry(
									now,
									retransmit_timeout,
									retries,
									received_from,
									announce_hops,
									packet,
									local_rebroadcasts,
									block_rebroadcasts,
									attached_interface
								);
								// CBA ACCUMULATES
								_announce_table.insert({packet.destination_hash(), announce_entry});
								// CBA IMMEDIATE CULL
								cull_announce_table();
							}
						}
						// TODO: Check from_local_client once and store result
						else if (Transport::from_local_client(packet) && packet.context() == Type::Packet::PATH_RESPONSE) {
							// If this is a path response from a local client,
							// check if any external interfaces have pending
							// path requests.
							//p if packet.destination_hash in Transport.pending_local_path_requests:
							auto iter = _pending_local_path_requests.find(packet.destination_hash());
							if (iter != _pending_local_path_requests.end()) {
								//p desiring_interface = Transport.pending_local_path_requests.pop(packet.destination_hash)
								//const Interface& desiring_interface = (*iter).second;
								_pending_local_path_requests.erase(iter);
								retransmit_timeout = now;
								retries = PATHFINDER_R;

								AnnounceEntry announce_entry(
									now,
									retransmit_timeout,
									retries,
									received_from,
									announce_hops,
									packet,
									local_rebroadcasts,
									block_rebroadcasts,
									attached_interface
								);
								// CBA ACCUMULATES
								_announce_table.insert({packet.destination_hash(), announce_entry});
								// CBA IMMEDIATE CULL
								cull_announce_table();
							}
						}

						// If we have any local clients connected, we re-
						// transmit the announce to them immediately
						if (_local_client_interfaces.size() > 0) {
							Identity announce_identity(Identity::recall(packet.destination_hash()));
							//Destination announce_destination(announce_identity, Type::Destination::OUT, Type::Destination::SINGLE, "unknown", "unknown");
							//announce_destination.hash(packet.destination_hash());
							Destination announce_destination(announce_identity, Type::Destination::OUT, Type::Destination::SINGLE, packet.destination_hash());
							//announce_destination.hexhash(announce_destination.hash().toHex());
							Type::Packet::context_types announce_context = Type::Packet::CONTEXT_NONE;
							Bytes announce_data = packet.data();

							// TODO: Shouldn't the context be PATH_RESPONSE in the first case here?
							if (Transport::from_local_client(packet) && packet.context() == Type::Packet::PATH_RESPONSE) {
								for (const Interface& local_interface : _local_client_interfaces) {
									if (packet.receiving_interface() != local_interface) {
										Packet new_announce = Packet(announce_destination, announce_data)
											.attached_interface(local_interface)
											.packet_type(Type::Packet::ANNOUNCE)
											.context(announce_context)
											.transport_type(Type::Transport::TRANSPORT)
											.header_type(Type::Packet::HEADER_2)
											.transport_id(_identity.hash())
											.context_flag(packet.context_flag());

										new_announce.hops(packet.hops());
										new_announce.send();
									}
								}
							}
							else {
								for (const Interface& local_interface : _local_client_interfaces) {
									if (packet.receiving_interface() != local_interface) {
										Packet new_announce = Packet(announce_destination, announce_data)
											.attached_interface(local_interface)
											.packet_type(Type::Packet::ANNOUNCE)
											.context(announce_context)
											.transport_type(Type::Transport::TRANSPORT)
											.header_type(Type::Packet::HEADER_2)
											.transport_id(_identity.hash())
											.context_flag(packet.context_flag());

										new_announce.hops(packet.hops());
										new_announce.send();
									}
								}
							}
						}

						// If we have any waiting discovery path requests
						// for this destination, we retransmit to that
						// interface immediately
						auto iter = _discovery_path_requests.find(packet.destination_hash());
						if (iter != _discovery_path_requests.end()) {
							PathRequestEntry& pr_entry = (*iter).second;
							attached_interface = pr_entry._requesting_interface;

							DEBUGF("Got matching announce, answering waiting discovery path request for %s on %s", packet.destination_hash().toHex().c_str(), attached_interface.toString().c_str());
							Identity announce_identity(Identity::recall(packet.destination_hash()));
							//Destination announce_destination(announce_identity, Type::Destination::OUT, Type::Destination::SINGLE, "unknown", "unknown");
							//announce_destination.hash(packet.destination_hash());
							Destination announce_destination(announce_identity, Type::Destination::OUT, Type::Destination::SINGLE, packet.destination_hash());
							//announce_destination.hexhash(announce_destination.hash().toHex());
							Type::Packet::context_types announce_context = Type::Packet::CONTEXT_NONE;
							Bytes announce_data = packet.data();

							Packet new_announce = Packet(announce_destination, announce_data)
								.attached_interface(attached_interface)
								.packet_type(Type::Packet::ANNOUNCE)
								.context(Type::Packet::PATH_RESPONSE)
								.transport_type(Type::Transport::TRANSPORT)
								.header_type(Type::Packet::HEADER_2)
								.transport_id(_identity.hash())
								.context_flag(packet.context_flag());

							new_announce.hops(packet.hops());
							new_announce.send();
						}

// CBA microStore
/*
						// CBA ACCUMULATES
						// CBA Culling before adding to esnure table does not exceed maxsize
						TRACEF("Caching packet %s", packet.get_hash().toHex().c_str());
						// CBA Currently this is the ONLY place that packets get cached
						if (Transport::cache_packet(packet, true)) {
							packet.cached(true);
						}
						//TRACEF("Adding packet %s to packet table", packet.get_hash().toHex().c_str());
						//PacketEntry packet_entry(packet);
*/

						// CBA ACCUMULATES
						//_packet_table.insert({packet.get_hash(), packet_entry});
						if (path_found) DEBUGF("Replacing destination %s in path table", packet.destination_hash().toHex().c_str());
						else DEBUGF("Adding destination %s to path table", packet.destination_hash().toHex().c_str());
						DestinationEntry destination_table_entry(
							now,
							received_from,
							announce_hops,
							expires,
							random_blobs,
							//packet.receiving_interface(),
							const_cast<Interface&>(packet.receiving_interface()),
							//packet.receiving_interface().get_hash(),
							packet
							//packet.get_hash()
						);
						// CBA ACCUMULATES
						try {

// CBA microStore
/*
							// CBA First remove any existing path before inserting new one
							remove_path(packet.destination_hash());
							if (_path_table.insert({packet.destination_hash(), destination_table_entry}).second) {
								TRACEF("Added destination %s to path table!", packet.destination_hash().toHex().c_str());
								++_paths_added;
								// CBA IMMEDIATE CULL
								cull_path_table();
							}
							else {
								ERRORF("Failed to add destination %s to path table!", packet.destination_hash().toHex().c_str());
							}
*/
							// CBA microStore
							uint32_t ttl = 0;
							if (packet.receiving_interface().mode() == Type::Interface::MODE_ACCESS_POINT) {
								ttl = AP_PATH_TIME;
							}
							else if (packet.receiving_interface().mode() == Type::Interface::MODE_ROAMING) {
								ttl = ROAMING_PATH_TIME;
							}
							else {
								ttl = DESTINATION_TIMEOUT;
							}
							if (_new_path_table.put(packet.destination_hash().collection(), destination_table_entry, ttl)) {
								TRACEF("Added destination %s to path table!", packet.destination_hash().toHex().c_str());
								mark_path_unknown_state(packet.destination_hash());
								if (path_found) ++_paths_updated;
								else ++_paths_added;
							}
							else {
								ERRORF("Failed to add destination %s to path table!", packet.destination_hash().toHex().c_str());
								++_paths_failed;
							}
						}
						catch (const std::bad_alloc&) {
							ERROR("inbound: bad_alloc - out of memory storing destination entry");
						}
						catch (const std::exception& e) {
							ERRORF("inbound: exception storing destination entry: %s", e.what());
						}

						DEBUGF("Destination %s is now %d hops away via %s on %s", packet.destination_hash().toHex().c_str(), announce_hops, received_from.toHex().c_str(), packet.receiving_interface().toString().c_str());
						//TRACEF("Transport::inbound: Destination %s has data: %s", packet.destination_hash().toHex().c_str(), packet.data().toHex().c_str());
						//TRACEF("Transport::inbound: Destination %s has text: %s", packet.destination_hash().toHex().c_str(), packet.data().toString().c_str());

// TODO
/*
						// If the receiving interface is a tunnel, we add the
						// announce to the tunnels table
						if (packet.receiving_interface().tunnel_id()) {
							tunnel_entry = Transport.tunnels[packet.receiving_interface.tunnel_id];
							paths = tunnel_entry[2];
							paths[packet.destination_hash] = destination_table_entry;
							expires = OS::time() + Transport::DESTINATION_TIMEOUT;
							tunnel_entry[3] = expires;
							DEBUGF("Path to %s associated with tunnel %s", packet.destination_hash().toHex().c_str(), packet.receiving_interface().tunnel_id().toHex().c_str());
						}
*/

						// Call externally registered callbacks from apps
						// wanting to know when an announce arrives
						if (packet.context() != Type::Packet::PATH_RESPONSE) {
							TRACE("Transport::inbound: Not path response, sending to announce handler...");
							for (auto& handler : _announce_handlers) {
								TRACE("Transport::inbound: Checking filter of announce handler...");
								try {
									// Check that the announced destination matches
									// the handlers aspect filter
									bool execute_callback = false;
									Identity announce_identity(Identity::recall(packet.destination_hash()));
									if (handler->aspect_filter().empty()) {
										// If the handlers aspect filter is set to
										// None, we execute the callback in all cases
										execute_callback = true;
									}
									else {
										Bytes handler_expected_hash = Destination::hash_from_name_and_identity(handler->aspect_filter().c_str(), announce_identity);
										if (packet.destination_hash() == handler_expected_hash) {
											execute_callback = true;
										}
									}
									if (execute_callback) {
										// CBA TODO Why does app data come from recall instead of from this announce packet?
										handler->received_announce(
											packet.destination_hash(),
											announce_identity,
											Identity::recall_app_data(packet.destination_hash())
										);
									}
								}
								catch (const std::exception& e) {
									ERROR("Error while processing external announce callback.");
									ERRORF("The contained exception was: %s", e.what());
								}
							}
						}
					}
				}
				else {
					TRACE("Transport::inbound: Packet is announce for local destination, not processing");
				}
			}
			else {
				TRACE("Transport::inbound: Packet is announce for local destination, not processing");
			}
		}

		// Handling for link requests to local destinations
		else if (packet.packet_type() == Type::Packet::LINKREQUEST) {
			TRACE("Transport::inbound: Packet is LINKREQUEST");
			if (!packet.transport_id() || packet.transport_id() == _identity.hash()) {
				TRACE("Transport::inbound: Checking if LINKREQUEST is for local destination");
				auto iter = _destinations.find(packet.destination_hash());
				if (iter != _destinations.end()) {
					auto& destination = (*iter).second;
					if (destination.type() == packet.destination_type()) {
						TRACE("Transport::inbound: Found local destination for LINKREQUEST");

						// MTU clamping. The link request carries the initiator's
						// proposed path MTU in its trailing LINK_MTU_SIZE bytes.
						// If our receiving interface asks for autoconfigured or
						// fixed MTU, clamp the path MTU down to our hardware MTU
						// before the destination parses the link request.
						// Matches Python Transport.py:2099-2118.
						bool drop_packet = false;
						uint16_t path_mtu = Link::mtu_from_lr_packet(packet);
						Type::Link::link_mode mode = Link::mode_from_lr_packet(packet);
						uint16_t nh_mtu = 0;
						if (packet.receiving_interface().AUTOCONFIGURE_MTU()
								|| packet.receiving_interface().FIXED_MTU()) {
							nh_mtu = packet.receiving_interface().HW_MTU();
						}
						else {
							nh_mtu = Type::Reticulum::MTU;
						}

						if (path_mtu > 0) {
							const Bytes& orig = packet.data();
							if (packet.receiving_interface().HW_MTU() == 0) {
								// No hardware MTU known on the receiving
								// interface; strip the trailing MTU bytes so
								// the destination sees a plain link request.
								if (orig.size() >= Type::Link::LINK_MTU_SIZE) {
									packet.data(orig.left(orig.size() - Type::Link::LINK_MTU_SIZE));
								}
							}
							else if (nh_mtu < path_mtu) {
								try {
									Bytes clamped = Link::signalling_bytes(nh_mtu, mode);
									if (orig.size() >= Type::Link::LINK_MTU_SIZE) {
										packet.data(orig.left(orig.size() - Type::Link::LINK_MTU_SIZE) + clamped);
									}
								}
								catch (const std::exception& e) {
									WARNINGF("Dropping link request packet to local destination: %s", e.what());
									drop_packet = true;
								}
							}
						}

						if (!drop_packet) {
							packet.destination(destination);
							destination.receive(packet);
						}
					}
				}
			}
		}
		
		// Handling for data packets to local destinations
		else if (packet.packet_type() == Type::Packet::DATA) {
			TRACE("Transport::inbound: Packet is DATA");
			if (packet.destination_type() == Type::Destination::LINK) {
				// Data is destined for a link
				TRACE("Transport::inbound: Packet is DATA for a LINK");
				std::set<Link> active_links(_active_links);
				for (auto& link : active_links) {
					if (link.link_id() == packet.destination_hash()) {
						if (link.attached_interface() == packet.receiving_interface()) {
							TRACE("Transport::inbound: Packet is DATA for an active LINK");
							packet.link(link);
							const_cast<Link&>(link).receive(packet);
						}
						else {
							// In the strange and rare case that an interface is
							// partly malfunctioning and a link-associated packet
							// arrives on an interface that has failed sending --
							// and transport has failed over to another path --
							// drop the packet hash from the dedup filter so the
							// link can still receive the packet when it finally
							// arrives over the correct interface.
							_packet_hashlist.remove(packet.packet_hash().data(), (uint8_t)packet.packet_hash().size());
						}
						break;
					}
				}
			}
			else {
				// Data is basic (not destined for a link)
				auto iter = _destinations.find(packet.destination_hash());
				if (iter != _destinations.end()) {
					// Data is for a local destination
					DEBUGF("Packet destination %s found, destination is local", packet.destination_hash().toHex().c_str());
					auto& destination = (*iter).second;
					if (destination.type() == packet.destination_type()) {
						TRACEF("Transport::inbound: Packet destination type %d matched, processing", packet.destination_type());
						packet.destination(destination);
						destination.receive(packet);

						if (destination.proof_strategy() == Type::Destination::PROVE_ALL) {
							packet.prove();
						}
						else if (destination.proof_strategy() == Type::Destination::PROVE_APP) {
							if (destination.callbacks()._proof_requested) {
								try {
									if (destination.callbacks()._proof_requested(packet)) {
										packet.prove();
									}
								}
								catch (const std::exception& e) {
									ERRORF("Error while executing proof request callback. The contained exception was: %s", e.what());
								}
							}
						}
					}
					else {
						DEBUGF("Transport::inbound: Packet destination type %d mismatch, ignoring", packet.destination_type());
					}
				}
				else {
					DEBUGF("Transport::inbound: Local destination %s not found, not handling packet locally", packet.destination_hash().toHex().c_str());
				}
			}
		}

		// Handling for proofs and link-request proofs
		else if (packet.packet_type() == Type::Packet::PROOF) {
			TRACE("Transport::inbound: Packet is PROOF");
			if (packet.context() == Type::Packet::LRPROOF) {
				TRACE("Transport::inbound: Packet is LINK PROOF");
				// This is a link request proof, check if it
				// needs to be transported
				if ((Reticulum::transport_enabled() || for_local_client_link || from_local_client) && _link_table.count(packet.destination_hash()) > 0) {
					TRACE("Handling link request proof...");
					LinkEntry& link_entry = (*_link_table.find(packet.destination_hash())).second;
					if (packet.receiving_interface() == link_entry._outbound_interface) {
						try {
							if (packet.data().size() == (Type::Identity::SIGLENGTH/8 + Type::Link::ECPUBSIZE/2) || packet.data().size() == (Type::Identity::SIGLENGTH/8 + Type::Link::ECPUBSIZE/2 + Type::Link::LINK_MTU_SIZE)) {
								Bytes signalling_bytes;
								if (packet.data().size() == (Type::Identity::SIGLENGTH/8 + Type::Link::ECPUBSIZE/2 + Type::Link::LINK_MTU_SIZE)) {
									signalling_bytes = Link::signalling_bytes(Link::mtu_from_lp_packet(packet), Link::mode_from_lp_packet(packet));
								}
								Bytes peer_pub_bytes = packet.data().mid(Type::Identity::SIGLENGTH/8, Type::Link::ECPUBSIZE/2);
								Identity peer_identity = Identity::recall(link_entry._destination_hash);
								if (peer_identity) {
									Bytes peer_sig_pub_bytes = peer_identity.get_public_key().mid(Type::Link::ECPUBSIZE/2, Type::Link::ECPUBSIZE/2);

									Bytes signed_data = packet.destination_hash() + peer_pub_bytes + peer_sig_pub_bytes + signalling_bytes;
									Bytes signature = packet.data().left(Type::Identity::SIGLENGTH/8);

									if (peer_identity.validate(signature, signed_data)) {
										TRACEF("Link request proof validated for transport via %s", link_entry._receiving_interface.toString().c_str());
										//p new_raw = packet.raw[0:1]
										// CBA RESERVE
										//Bytes new_raw = packet.raw().left(1);
										Bytes new_raw(512);
										new_raw << packet.raw().left(1);
										//p new_raw += struct.pack("!B", packet.hops)
										new_raw << packet.hops();
										//p new_raw += packet.raw[2:]
										new_raw << packet.raw().mid(2);
										link_entry._validated = true;
										transmit(link_entry._receiving_interface, new_raw);
									}
									else {
										DEBUGF("Invalid link request proof in transport for link %s, dropping proof.", packet.destination_hash().toHex().c_str());
									}
								}
								else {
									DEBUGF("Could not recall identity for link request proof destination %s, dropping proof.", link_entry._destination_hash.toHex().c_str());
								}
							}
						}
						catch (const std::exception& e) {
							ERRORF("Error while transporting link request proof. The contained exception was: %s", e.what());
						}
					}
					else {
						DEBUG("Link request proof received on wrong interface, not transporting it.");
					}
				}
				else {
					// Check if we can deliver it to a local
					// pending link
					TRACEF("Handling proof for link request %s", packet.destination_hash().toHex().c_str());
					// CBA Must make a copy of _pending_links before traversing since it gets modified
					//for (auto link : _pending_links) {
					std::set<Link> pending_links(_pending_links);
					for (auto& link : pending_links) {
						TRACEF("Checking for link request handling by pending link %s", link.link_id().toHex().c_str());
						if (link.link_id() == packet.destination_hash()) {
							TRACE("Requesting pending link to validate proof");
							const_cast<Link&>(link).validate_proof(packet);
						}
					}
				}
			}
			else if (packet.context() == Type::Packet::RESOURCE_PRF) {
				TRACE("Transport::inbound: Packet is RESOURCE PROOF");
				std::set<Link> active_links(_active_links);
				for (auto& link : active_links) {
					if (link.link_id() == packet.destination_hash()) {
						const_cast<Link&>(link).receive(packet);
					}
				}
			}
			else {
				TRACE("Transport::inbound: Packet is regular PROOF");
				if (packet.destination_type() == Type::Destination::LINK) {
					std::set<Link> active_links(_active_links);
					for (auto& link : active_links) {
						if (link.link_id() == packet.destination_hash()) {
							packet.link(link);
						}
					}
				}

				Bytes proof_hash;
				if (packet.data().size() == Type::PacketReceipt::EXPL_LENGTH) {
					proof_hash = packet.data().left(Type::Identity::HASHLENGTH/8);
				}

				// Check if this proof needs to be transported
				if ((Reticulum::transport_enabled() || from_local_client || proof_for_local_client) && _reverse_table.find(packet.destination_hash()) != _reverse_table.end()) {
					ReverseEntry reverse_entry = (*_reverse_table.find(packet.destination_hash())).second;
					if (packet.receiving_interface() == reverse_entry._outbound_interface) {
						TRACEF("Proof received on correct interface, transporting it via %s", reverse_entry._receiving_interface.toString().c_str());
						//p new_raw = packet.raw[0:1]
						// CBA RESERVE
						//Bytes new_raw = packet.raw().left(1);
						Bytes new_raw(512);
						new_raw << packet.raw().left(1);
						//p new_raw += struct.pack("!B", packet.hops)
						new_raw << packet.hops();
						//p new_raw += packet.raw[2:]
						new_raw << packet.raw().mid(2);
						transmit(reverse_entry._receiving_interface, new_raw);
					}
					else {
						DEBUG("Proof received on wrong interface, not transporting it.");
					}
				}
				else {
					TRACE("Proof is not candidate for transporting");
				}

				std::list<PacketReceipt> cull_receipts;
				for (auto& receipt : _receipts) {
					bool receipt_validated = false;
					if (proof_hash) {
						// Only test validation if hash matches
						if (receipt.hash() == proof_hash) {
							receipt_validated = receipt.validate_proof_packet(packet);
						}
					}
					else {
						// TODO: This looks like it should actually
						// be rewritten when implicit proofs are added.

						// In case of an implicit proof, we have
						// to check every single outstanding receipt
						receipt_validated = receipt.validate_proof_packet(packet);
					}

					// CBA TODO requires modifying of collection while iterating which is forbidden
					if (receipt_validated) {
						//p if receipt in Transport.receipts:
						//p 	Transport.receipts.remove(receipt)
						cull_receipts.push_back(receipt);
					}
				}
				// CBA since modifying of collection while iterating is forbidden
				for (auto& receipt : cull_receipts) {
					_receipts.remove(receipt);
				}
			}
		}
	}

	_jobs_locked = false;
}

/*static*/ void Transport::synthesize_tunnel(const Interface& interface) {
// TODO
/*p
	Bytes interface_hash = interface.get_hash();
	Bytes public_key     = _identity.get_public_key();
	Bytes random_hash    = Identity::get_random_hash();
	
	tunnel_id_data = public_key+interface_hash
	tunnel_id      = RNS.Identity.full_hash(tunnel_id_data)

	signed_data    = tunnel_id_data+random_hash
	signature      = Transport.identity.sign(signed_data)
	
	data           = signed_data+signature

	tnl_snth_dst   = RNS.Destination(None, RNS.Destination.OUT, RNS.Destination.PLAIN, Transport.APP_NAME, "tunnel", "synthesize")

	packet = RNS.Packet(tnl_snth_dst, data, packet_type = RNS.Packet.DATA, transport_type = RNS.Transport.BROADCAST, header_type = RNS.Packet.HEADER_1, attached_interface = interface)
	packet.send()

	interface.wants_tunnel = False
*/
}

/*static*/ void Transport::tunnel_synthesize_handler(const Bytes& data, const Packet& packet) {
// TODO
/*p
	try:
		expected_length = RNS.Identity.KEYSIZE//8+RNS.Identity.HASHLENGTH//8+RNS.Reticulum.TRUNCATED_HASHLENGTH//8+RNS.Identity.SIGLENGTH//8
		if len(data) == expected_length:
			public_key     = data[:RNS.Identity.KEYSIZE//8]
			interface_hash = data[RNS.Identity.KEYSIZE//8:RNS.Identity.KEYSIZE//8+RNS.Identity.HASHLENGTH//8]
			tunnel_id_data = public_key+interface_hash
			tunnel_id      = RNS.Identity.full_hash(tunnel_id_data)
			random_hash    = data[RNS.Identity.KEYSIZE//8+RNS.Identity.HASHLENGTH//8:RNS.Identity.KEYSIZE//8+RNS.Identity.HASHLENGTH//8+RNS.Reticulum.TRUNCATED_HASHLENGTH//8]
			
			signature      = data[RNS.Identity.KEYSIZE//8+RNS.Identity.HASHLENGTH//8+RNS.Reticulum.TRUNCATED_HASHLENGTH//8:expected_length]
			signed_data    = tunnel_id_data+random_hash

			remote_transport_identity = RNS.Identity(create_keys=False)
			remote_transport_identity.load_public_key(public_key)

			if remote_transport_identity.validate(signature, signed_data):
				Transport.handle_tunnel(tunnel_id, packet.receiving_interface)

	except Exception as e:
		RNS.log("An error occurred while validating tunnel establishment packet.", RNS.LOG_DEBUG)
		RNS.log("The contained exception was: "+str(e), RNS.LOG_DEBUG)
*/
}

/*static*/ void Transport::handle_tunnel(const Bytes& tunnel_id, const Interface& interface) {
// TODO
/*p
	expires = time.time() + Transport.DESTINATION_TIMEOUT
	if not tunnel_id in Transport.tunnels:
		RNS.log("Tunnel endpoint "+RNS.prettyhexrep(tunnel_id)+" established.", RNS.LOG_DEBUG)
		paths = {}
		tunnel_entry = [tunnel_id, interface, paths, expires]
		interface.tunnel_id = tunnel_id
		Transport.tunnels[tunnel_id] = tunnel_entry
	else:
		RNS.log("Tunnel endpoint "+RNS.prettyhexrep(tunnel_id)+" reappeared. Restoring paths...", RNS.LOG_DEBUG)
		tunnel_entry = Transport.tunnels[tunnel_id]
		tunnel_entry[1] = interface
		tunnel_entry[3] = expires
		interface.tunnel_id = tunnel_id
		paths = tunnel_entry[2]

		deprecated_paths = []
		for destination_hash, path_entry in paths.items():
			received_from = path_entry[1]
			announce_hops = path_entry[2]
			expires = path_entry[3]
			random_blobs = path_entry[4]
			receiving_interface = interface
			packet = path_entry[6]
			new_entry = [time.time(), received_from, announce_hops, expires, random_blobs, receiving_interface, packet]

			should_add = False
			if destination_hash in Transport.destination_table:
				old_entry = Transport.destination_table[destination_hash]
				old_hops = old_entry[2]
				old_expires = old_entry[3]
				if announce_hops <= old_hops or time.time() > old_expires:
					should_add = True
				else:
					RNS.log("Did not restore path to "+RNS.prettyhexrep(packet.destination_hash)+" because a newer path with fewer hops exist", RNS.LOG_DEBUG)
			else:
				if time.time() < expires:
					should_add = True
				else:
					RNS.log("Did not restore path to "+RNS.prettyhexrep(packet.destination_hash)+" because it has expired", RNS.LOG_DEBUG)

			if should_add:
				Transport.destination_table[destination_hash] = new_entry
				RNS.log("Restored path to "+RNS.prettyhexrep(packet.destination_hash)+" is now "+str(announce_hops)+" hops away via "+RNS.prettyhexrep(received_from)+" on "+str(receiving_interface), RNS.LOG_DEBUG)
			else:
				deprecated_paths.append(destination_hash)

		for deprecated_path in deprecated_paths:
			RNS.log("Removing path to "+RNS.prettyhexrep(deprecated_path)+" from tunnel "+RNS.prettyhexrep(tunnel_id), RNS.LOG_DEBUG)
			paths.pop(deprecated_path)
*/
}

/*static*/ void Transport::register_interface(Interface& interface) {
	TRACEF("Transport: Registering interface %s %s", interface.get_hash().toHex().c_str(), interface.toString().c_str());
	if (!find_interface_from_hash(interface.get_hash())) {
		_interfaces.push_back(interface);
	}
	// CBA TODO set or add transport as listener on interface to receive incoming packets?
}

/*static*/ void Transport::deregister_interface(const Interface& interface) {
	TRACEF("Transport: Deregistering interface %s", interface.toString().c_str());
	const Bytes hash = interface.get_hash();
	_interfaces.erase(
		std::remove_if(_interfaces.begin(), _interfaces.end(),
			[&](const Interface& i) { return i.get_hash() == hash; }),
		_interfaces.end()
	);
}

/*static*/ void Transport::register_destination(Destination& destination) {
	//TRACE("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
	TRACEF("Transport: Registering destination %s", destination.toString().c_str());
	destination.mtu(Type::Reticulum::MTU);
	if (destination.direction() == Type::Destination::IN) {
		auto iter = _destinations.find(destination.hash());
		if (iter != _destinations.end()) {
			//p raise KeyError("Attempt to register an already registered destination.")
			throw std::runtime_error("Attempt to register an already registered destination.");
		}

		// CBA ACCUMULATES
		_destinations.insert({destination.hash(), destination});

		if (_owner && _owner.is_connected_to_shared_instance()) {
			if (destination.type() == Type::Destination::SINGLE) {
				TRACEF("Transport:register_destination: Announcing destination %s", destination.toString().c_str());
				destination.announce({}, true);
			}
		}
	}
	else {
		TRACEF("Transport:register_destination: Skipping registration (not direction IN) of destination %s", destination.toString().c_str());
	}

/*
	for (auto& [hash, destination] : _destinations) {
		TRACEF("Transport::register_destination: Listed destination %s", destination.toString().c_str());
	}
*/
	//TRACE("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
}

/*static*/ void Transport::deregister_destination(const Destination& destination) {
	TRACEF("Transport: Deregistering destination %s", destination.toString().c_str());
	auto iter = _destinations.find(destination.hash());
	if (iter != _destinations.end()) {
		TRACEF("Transport::deregister_destination: Found and removing destination %s", (*iter).second.toString().c_str());
		_destinations.erase(iter);
	}
}

/*static*/ void Transport::register_link(Link& link) {
	TRACEF("Transport: Registering link %s", link.toString().c_str());
	if (link.initiator()) {
		// CBA ACCUMULATES
		_pending_links.insert(link);
	}
	else {
		// CBA ACCUMULATES
		_active_links.insert(link);
	}
}

/*static*/ void Transport::activate_link(Link& link) {
	TRACEF("Transport: Activating link %s", link.toString().c_str());
	if (_pending_links.find(link) != _pending_links.end()) {
		if (link.status() != Type::Link::ACTIVE) {
			throw std::runtime_error("Invalid link state for link activation: " + std::to_string(link.status()));
		}
		_pending_links.erase(link);
		// CBA ACCUMULATES
		_active_links.insert(link);
		link.status(Type::Link::ACTIVE);
	}
	else {
		ERROR("Attempted to activate a link that was not in the pending table");
	}
}

/*
Registers an announce handler.

:param handler: Must be an object with an *aspect_filter* attribute and a *received_announce(destination_hash, announced_identity, app_data)* callable. See the :ref:`Announce Example<example-announce>` for more info.
*/
/*static*/ void Transport::register_announce_handler(HAnnounceHandler handler) {
	TRACEF("Transport: Registering announce handler %s", handler->aspect_filter().c_str());
	_announce_handlers.insert(handler);
}

/*
Deregisters an announce handler.

:param handler: The announce handler to be deregistered.
*/
/*static*/ void Transport::deregister_announce_handler(HAnnounceHandler handler) {
	TRACEF("Transport: Deregistering announce handler %s", handler->aspect_filter().c_str());
	if (_announce_handlers.find(handler) != _announce_handlers.end()) {
		TRACEF("Transport::deregister_announce_handler: Found and removing handler%s", handler->aspect_filter().c_str());
		_announce_handlers.erase(handler);
	}
}

/*static*/ bool Transport::is_interface_from_hash(const Bytes& interface_hash) {
	return (bool)find_interface_from_hash(interface_hash);
}

/*static*/ Interface Transport::find_interface_from_hash(const Bytes& interface_hash) {
	auto iter = std::find_if(_interfaces.begin(), _interfaces.end(),
		[&](const Interface& i) { return i.get_hash() == interface_hash; });
	if (iter != _interfaces.end()) {
		return *iter;
	}
	return {Type::NONE};
}

/*static*/ bool Transport::should_cache_packet(const Packet& packet) {
	// TODO: Rework the caching system. It's currently
	// not very useful to even cache Resource proofs,
	// disabling it for now, until redesigned.
	// if packet.context == RNS.Packet.RESOURCE_PRF:
	//     return True

	return false;
}

// When caching packets to storage, they are written
// exactly as they arrived over their interface. This
// means that they have not had their hop count
// increased yet! Take note of this when reading from
// the packet cache.
/*static*/ bool Transport::cache_packet(const Packet& packet, bool force_cache /*= false*/) {
	//TRACEF("Checking to see if packet %s should be cached", packet.get_hash().toHex().c_str());
#if defined(RNS_USE_FS) && RNS_PERSIST_PATHS
	if (should_cache_packet(packet) || force_cache) {
		TRACEF("Saving packet %s to storage", packet.get_hash().toHex().c_str());
		try {
			char packet_cache_path[Type::Reticulum::FILEPATH_MAXSIZE];
			snprintf(packet_cache_path, Type::Reticulum::FILEPATH_MAXSIZE, "%s/%s", Reticulum::_cachepath, packet.get_hash().toHex().c_str());
			return (Persistence::serialize(packet, packet_cache_path) > 0);
		}
		catch (const std::exception& e) {
			ERRORF("Error writing packet to cache. The contained exception was: %s", e.what());
		}
	}
#endif
	return false;
}

/*static*/ bool Transport::is_cached_packet(const Bytes& packet_hash) {
#if defined(RNS_USE_FS) && RNS_PERSIST_PATHS
	try {
		char packet_cache_path[Type::Reticulum::FILEPATH_MAXSIZE];
		snprintf(packet_cache_path, Type::Reticulum::FILEPATH_MAXSIZE, "%s/%s", Reticulum::_cachepath, packet_hash.toHex().c_str());
		return OS::file_exists(packet_cache_path);
	}
	catch (const std::exception& e) {
		ERRORF("Exception occurred while getting cached packet: %s", e.what());
	}
#endif
	return false;
}

/*static*/ Packet Transport::get_cached_packet(const Bytes& packet_hash) {
	TRACEF("Loading packet %s from cache storage", packet_hash.toHex().c_str());
#if defined(RNS_USE_FS) && RNS_PERSIST_PATHS
	try {
		char packet_cache_path[Type::Reticulum::FILEPATH_MAXSIZE];
		snprintf(packet_cache_path, Type::Reticulum::FILEPATH_MAXSIZE, "%s/%s", Reticulum::_cachepath, packet_hash.toHex().c_str());
		Packet packet({Type::NONE});
		if (Persistence::deserialize(packet, packet_cache_path) > 0) {
			packet.update_hash();
		}
		return packet;
	}
	catch (const std::exception& e) {
		ERRORF("Exception occurred while getting cached packet: %s", e.what());
	}
#endif
	return {Type::NONE};
}

/*static*/ bool Transport::clear_cached_packet(const Bytes& packet_hash) {
	TRACEF("Clearing packet %s from cache storage", packet_hash.toHex().c_str());
#if defined(RNS_USE_FS) && RNS_PERSIST_PATHS
	try {
		char packet_cache_path[Type::Reticulum::FILEPATH_MAXSIZE];
		snprintf(packet_cache_path, Type::Reticulum::FILEPATH_MAXSIZE, "%s/%s", Reticulum::_cachepath, packet_hash.toHex().c_str());
		double start_time = OS::time();
		bool success = Utilities::OS::remove_file(packet_cache_path);
		double diff_time = OS::time() - start_time;
		if (diff_time < 1.0) {
			DEBUGF("Remove cached packet in %d ms", (int)(diff_time*1000));
		}
		else {
			DEBUGF("Remove cached packet in %f s", diff_time);
		}
	}
	catch (const std::exception& e) {
		ERROR("Exception occurred while clearing cached packet.");
		ERRORF("The contained exception was: %s", e.what());
	}
#endif
	return false;
}

/*static*/ bool Transport::cache_request_packet(const Packet& packet) {
	if (packet.data().size() == Type::Identity::HASHLENGTH/8) {
		const Packet& cached_packet = get_cached_packet(packet.data());

		if (cached_packet) {
			// If the packet was retrieved from the local
			// cache, replay it to the Transport instance,
			// so that it can be directed towards it original
			// destination.
			inbound(cached_packet.raw(), cached_packet.receiving_interface());
			return true;
		}
		else {
			return false;
		}
	}
	else {
		return false;
	}
}

/*static*/ void Transport::cache_request(const Bytes& packet_hash, const Destination& destination) {
	const Packet& cached_packet = get_cached_packet(packet_hash);
	if (cached_packet) {
		// The packet was found in the local cache,
		// replay it to the Transport instance.
		inbound(cached_packet.raw(), cached_packet.receiving_interface());
	}
	else {
		// The packet is not in the local cache,
		// query the network.
		Packet(destination, packet_hash).context(Type::Packet::CACHE_REQUEST).send();
	}
}

/*static*/ DestinationEntry& Transport::get_path(const Bytes& destination_hash) {
	auto iter = _path_table.find(destination_hash);
	if (iter == _path_table.end()) return empty_destination_entry;
	DestinationEntry& destination_entry = (*iter).second;
	if (!destination_entry.announce_packet()) {
		DEBUGF("Entry for destination %s found but is missing announce packet, discarding", destination_hash.toHex().c_str());
		remove_path(destination_hash);
		return empty_destination_entry;
	}
	if (!destination_entry.receiving_interface()) {
		DEBUGF("Entry for destination %s found but is missing receiving interface, discarding", destination_hash.toHex().c_str());
		remove_path(destination_hash);
		return empty_destination_entry;
	}
	return destination_entry;
}

/*static*/ bool Transport::remove_path(const Bytes& destination_hash) {
// CBA microStore
/*
	auto iter = _path_table.find(destination_hash);
	if (iter == _path_table.end()) return false;
	// Remove cached packet file associated with this destination
	char packet_cache_path[Type::Reticulum::FILEPATH_MAXSIZE];
	snprintf(packet_cache_path, Type::Reticulum::FILEPATH_MAXSIZE, "%s/%s", Reticulum::_cachepath, iter->second.announce_packet_hash().toHex().c_str());
	if (OS::file_exists(packet_cache_path)) {
		OS::remove_file(packet_cache_path);
	}
	if (_path_table.erase(destination_hash) < 1) {
		WARNINGF("Failed to remove destination %s from path table", destination_hash.toHex().c_str());
		return false;
	}
	return true;
*/
	// CBA microStore
	return _new_path_table.remove(destination_hash.collection());
}

/*
:param destination_hash: A destination hash as *bytes*.
:returns: *True* if a path to the destination is known, otherwise *False*.
*/
/*static*/ bool Transport::has_path(const Bytes& destination_hash) {
// CBA microStore
/*
	return (_path_table.find(destination_hash) != _path_table.end());
*/
	// CBA microStore
	return _new_path_table.exists(destination_hash.collection());
}

/*
:param destination_hash: A destination hash as *bytes*.
:returns: The number of hops to the specified destination, or ``RNS.Transport.PATHFINDER_M`` if the number of hops is unknown.
*/
/*static*/ uint8_t Transport::hops_to(const Bytes& destination_hash) {
	// CBA microStore
	//auto& destination_entry = get_path(destination_hash);
	DestinationEntry destination_entry;
	_new_path_table.get(destination_hash, destination_entry);
	if (destination_entry) {
		return destination_entry._hops;
	}
	else {
		return PATHFINDER_M;
	}
}

/*
:param destination_hash: A destination hash as *bytes*.
:returns: The destination hash as *bytes* for the next hop to the specified destination, or *None* if the next hop is unknown.
*/
/*static*/ Bytes Transport::next_hop(const Bytes& destination_hash) {
	// CBA microStore
	//auto& destination_entry = get_path(destination_hash);
	DestinationEntry destination_entry;
	_new_path_table.get(destination_hash, destination_entry);
	if (destination_entry) {
		return destination_entry._received_from;
	}
	else {
		return {};
	}
}

/*
:param destination_hash: A destination hash as *bytes*.
:returns: The interface for the next hop to the specified destination, or *None* if the interface is unknown.
*/
/*static*/ Interface Transport::next_hop_interface(const Bytes& destination_hash) {
	// CBA microStore
	//auto& destination_entry = get_path(destination_hash);
	DestinationEntry destination_entry;
	_new_path_table.get(destination_hash, destination_entry);
	if (destination_entry) {
		return destination_entry.receiving_interface();
	}
	else {
		return {Type::NONE};
	}
}

/*static*/ uint32_t Transport::next_hop_interface_bitrate(const Bytes& destination_hash) {
	const Interface& interface = next_hop_interface(destination_hash);
	if (interface) {
		return interface.bitrate();
	}
	else {
		return 0;
	}
}

/*static*/ uint16_t Transport::next_hop_interface_hw_mtu(const Bytes& destination_hash) {
	const Interface& interface = next_hop_interface(destination_hash);
	if (interface) {
		if (interface.AUTOCONFIGURE_MTU() || interface.FIXED_MTU()) return interface.HW_MTU();
		else return 0;
	}
	else {
		return 0;
	}
}

/*static*/ double Transport::next_hop_per_bit_latency(const Bytes& destination_hash) {
	uint32_t bitrate = next_hop_interface_bitrate(destination_hash);
	if (bitrate > 0) {
		return (1.0/(double)bitrate);
	}
	else {
		return 0.0;
	}
}

/*static*/ double Transport::next_hop_per_byte_latency(const Bytes& destination_hash) {
	double per_bit_latency = next_hop_per_bit_latency(destination_hash);
	if (per_bit_latency > 0.0) {
		return per_bit_latency*8.0;
	}
	else {
		return 0.0;
	}
}

/*static*/ double Transport::first_hop_timeout(const Bytes& destination_hash) {
	double latency = next_hop_per_byte_latency(destination_hash);
	if (latency > 0.0) {
		return Type::Reticulum::MTU * latency + Type::Reticulum::DEFAULT_PER_HOP_TIMEOUT;
	}
	else {
		return Type::Reticulum::DEFAULT_PER_HOP_TIMEOUT;
	}
}

/*static*/ double Transport::extra_link_proof_timeout(const Interface& interface) {
	if (interface) {
		return ((1.0/(double)interface.bitrate())*8.0)*Type::Reticulum::MTU;
	}
	else {
		return 0.0;
	}
}

/*static*/ bool Transport::expire_path(const Bytes& destination_hash) {
	// CBA microStore
	//auto& destination_entry = get_path(destination_hash);
/*
	DestinationEntry destination_entry;
	_new_path_table.get(destination_hash, destination_entry);
	if (destination_entry) {
		destination_entry._timestamp = 0;
		_new_path_table.put(destination_hash, destination_entry);
		_tables_last_culled = 0;
		return true;
	}
	else {
		return false;
	}
*/
	// Just removing the path table entry directly
	return _new_path_table.remove(destination_hash);
}

/*p

    @staticmethod
    def mark_path_unresponsive(destination_hash):
        if destination_hash in Transport.destination_table:
            Transport.path_states[destination_hash] = Transport.STATE_UNRESPONSIVE
            return True
        else:
            return False

    @staticmethod
    def mark_path_responsive(destination_hash):
        if destination_hash in Transport.destination_table:
            Transport.path_states[destination_hash] = Transport.STATE_RESPONSIVE
            return True
        else:
            return False

    @staticmethod
    def mark_path_unknown_state(destination_hash):
        if destination_hash in Transport.destination_table:
            Transport.path_states[destination_hash] = Transport.STATE_UNKNOWN
            return True
        else:
            return False

    @staticmethod
    def path_is_unresponsive(destination_hash):
        if destination_hash in Transport.path_states:
            if Transport.path_states[destination_hash] == Transport.STATE_UNRESPONSIVE:
                return True

        return False

*/

/*static*/ bool Transport::mark_path_unresponsive(const Bytes& destination_hash) {
	DestinationEntry destination_entry;
	if (_new_path_table.get(destination_hash, destination_entry) && destination_entry) {
		_path_states[destination_hash] = STATE_UNRESPONSIVE;
		return true;
	}
	return false;
}

/*static*/ bool Transport::mark_path_responsive(const Bytes& destination_hash) {
	DestinationEntry destination_entry;
	if (_new_path_table.get(destination_hash, destination_entry) && destination_entry) {
		_path_states[destination_hash] = STATE_RESPONSIVE;
		return true;
	}
	return false;
}

/*static*/ bool Transport::mark_path_unknown_state(const Bytes& destination_hash) {
	DestinationEntry destination_entry;
	if (_new_path_table.get(destination_hash, destination_entry) && destination_entry) {
		_path_states[destination_hash] = STATE_UNKNOWN;
		return true;
	}
	return false;
}

/*static*/ bool Transport::path_is_unresponsive(const Bytes& destination_hash) {
	auto iter = _path_states.find(destination_hash);
	if (iter != _path_states.end()) {
		return iter->second == STATE_UNRESPONSIVE;
	}
	return false;
}

// Sorts _interfaces in place by bitrate descending so subsequent iteration
// in outbound paths, announce broadcast and discovery PR fanout prefers
// higher-bitrate interfaces. Called from start() and once per jobs() tick;
// std::sort over a small vector (typical n <= a few) is effectively free.
/*static*/ void Transport::prioritize_interfaces() {
	try {
		std::sort(_interfaces.begin(), _interfaces.end(),
			[](const Interface& a, const Interface& b) {
				return a.bitrate() > b.bitrate();
			});
	}
	catch (const std::exception& e) {
		ERRORF("Could not prioritize interfaces according to bitrate: %s", e.what());
	}
}

// Refreshes interface byte-counter snapshots, per-interface current rx/tx
// speeds, and class-level cumulative byte totals and aggregate speeds. Child
// interfaces (those with a parent_interface) are skipped to avoid double-
// counting traffic already attributed to the parent. In Python this runs as
// a sleeping background thread (count_traffic_loop); here it is called once
// per tick of jobs() and gated by _traffic_check_interval.
/*static*/ void Transport::count_traffic() {
	uint64_t rxb = 0;
	uint64_t txb = 0;
	double rxs = 0.0;
	double txs = 0.0;

	for (const auto& interface : _interfaces) {
		if (interface.parent_interface()) continue;

		double now = OS::time();
		size_t irxb = interface.rxbytes();
		size_t itxb = interface.txbytes();

		if (interface.traffic_counter_ts() != 0.0) {
			size_t rx_diff = irxb - interface.traffic_counter_rxb();
			size_t tx_diff = itxb - interface.traffic_counter_txb();
			double ts_diff = now  - interface.traffic_counter_ts();
			rxb += rx_diff;
			txb += tx_diff;
			double crxs = 0.0;
			double ctxs = 0.0;
			if (ts_diff > 0) {
				crxs = (static_cast<double>(rx_diff) * 8.0) / ts_diff;
				ctxs = (static_cast<double>(tx_diff) * 8.0) / ts_diff;
			}
			interface.current_rx_speed(crxs); rxs += crxs;
			interface.current_tx_speed(ctxs); txs += ctxs;
		}

		interface.update_traffic_counter(now, irxb, itxb);
	}

	_traffic_rxb += rxb;
	_traffic_txb += txb;
	_speed_rx     = rxs;
	_speed_tx     = txs;
}

// Drains one entry from the pending discovery path requests queue if the
// per-transmission throttle (DISCOVERY_PR_TX_THROTTLE) has elapsed. In Python
// this runs as a sleeping background thread; here we are called once per
// jobs() tick and drain at most one entry to enforce the same average rate.
/*static*/ void Transport::handle_disovery_path_requests() {
	if (_pending_discovery_prs.empty()) return;
	if (OS::time() < (_pending_discovery_prs_last_tx + DISCOVERY_PR_TX_THROTTLE)) return;

	PendingDiscoveryPREntry entry = _pending_discovery_prs.front();
	_pending_discovery_prs.pop_front();
	_pending_discovery_prs_last_tx = OS::time();

	if (!entry._blocked_interface) {
		request_path(entry._destination_hash);
	}
	else {
		for (const auto& interface : _interfaces) {
			if (interface.get_hash() != entry._blocked_interface.get_hash()) {
				request_path(entry._destination_hash, interface);
			}
		}
	}
}


/*p
    @staticmethod
    def await_path(destination_hash, timeout=None, on_interface=None):
        """
        Requests a path to the destination from the network and
        blocks until the path is available, or the timeout is reached.

        :param destination_hash: A destination hash as *bytes*.
        :param timeout: An optional timeout in seconds.
        :param on_interface: If specified, the path request will only be sent on this interface. In normal use, Reticulum handles this automatically, and this parameter should not be used.
        :returns: *True* if a path to the destination is found, otherwise *False*.
        """
        timeout = time.time()+(timeout if timeout else Transport.PATH_REQUEST_TIMEOUT)
        if Transport.has_path(destination_hash): return True
        else: Transport.request_path(destination_hash, on_interface=on_interface)
        while not Transport.has_path(destination_hash) and time.time() < timeout: time.sleep(0.05)
        return Transport.has_path(destination_hash)
*/

/*
Requests a path to the destination from the network. If
another reachable peer on the network knows a path, it
will announce it.

:param destination_hash: A destination hash as *bytes*.
:param on_interface: If specified, the path request will only be sent on this interface. In normal use, Reticulum handles this automatically, and this parameter should not be used.
*/
///*static*/ void Transport::request_path(const Bytes& destination_hash, const Interface& on_interface /*= {Type::NONE}*/, const Bytes& tag /*= {}*/, bool recursive /*= false*/) {
/*static*/ void Transport::request_path(const Bytes& destination_hash, const Interface& on_interface, const Bytes& tag /*= {}*/, bool recursive /*= false*/) {
	Bytes request_tag;
	if (!tag) {
		request_tag = Identity::get_random_hash();
	}
	else {
		request_tag = tag;
	}

	Bytes path_request_data;
	if (Reticulum::transport_enabled()) {
		path_request_data = destination_hash + _identity.hash() + request_tag;
	}
	else {
		path_request_data = destination_hash + request_tag;
	}

	Destination path_request_dst({Type::NONE}, Type::Destination::OUT, Type::Destination::PLAIN, Type::Transport::APP_NAME, "path.request");
	Packet packet = Packet(path_request_dst, path_request_data).attached_interface(on_interface);
	// packet_type=DATA, context=CONTEXT_NONE, transport_type=BROADCAST, header_type=HEADER_1 are all defaults.

	if (on_interface && recursive) {
// TODO
/*p
		if not hasattr(on_interface, "announce_cap"):
			on_interface.announce_cap = RNS.Reticulum.ANNOUNCE_CAP

		if not hasattr(on_interface, "announce_allowed_at"):
			on_interface.announce_allowed_at = 0

		if not hasattr(on_interface, "announce_queue"):
			on_interface.announce_queue = []
*/

		bool queued_announces = (on_interface.announce_queue().size() > 0);
		if (queued_announces) {
			TRACEF("Blocking recursive path request on %s due to queued announces", on_interface.toString().c_str());
			return;
		}
		else {
			double now = OS::time();
			if (now < on_interface.announce_allowed_at()) {
				TRACEF("Blocking recursive path request on %s due to active announce cap", on_interface.toString().c_str());
				return;
			}
			else {
				//p tx_time   = ((len(path_request_data)+RNS.Reticulum.HEADER_MINSIZE)*8) / on_interface.bitrate
				uint32_t wait_time = 0;
				if ( on_interface.bitrate() > 0 && on_interface.announce_cap() > 0) {
					uint32_t tx_time = ((path_request_data.size() + Type::Reticulum::HEADER_MINSIZE)*8) / on_interface.bitrate();
					wait_time = (tx_time / on_interface.announce_cap());
				}
				const_cast<Interface&>(on_interface).announce_allowed_at(now + wait_time);
			}
		}
	}

	packet.is_outbound_pr(true);
	packet.send();
	_path_requests[destination_hash] = OS::time();
}

/*static*/ void Transport::request_path(const Bytes& destination_hash) {
	return request_path(destination_hash, {Type::NONE});
}

// Parse the request payload from the client. Python sends a list
// [bool include_link_count] (and treats missing as False). To stay tolerant
// of older/newer clients we accept either bare false-y or [true]/[false].
//
// Note: `data` is raw msgpack bytes (the spliced 3rd element of the request
// envelope -- see Link::request and the pack_request_envelope helper in
// Link.cpp). It is NOT bin-wrapped.
static bool remote_status_parse_include_link_count(const Bytes& data) {
	if (!data || data.size() == 0) return false;
	const uint8_t b0 = data.data()[0];
	if ((b0 & 0xf0) != 0x90) return false;  // not a fixarray
	const size_t n = b0 & 0x0f;
	if (n == 0 || data.size() < 2) return false;
	const uint8_t b1 = data.data()[1];
	return b1 == 0xc3;  // msgpack true
}

// Strip "TypeName[detail]" -> "TypeName" for the "type" field, matching
// Python's class-name emission in Reticulum.py get_interface_stats().
static std::string remote_status_interface_type_name(const Interface& iface) {
	std::string s = iface.toString();
	auto pos = s.find('[');
	if (pos == std::string::npos) return s;
	return s.substr(0, pos);
}

// Builds the per-interface map. Only fields the C++ Interface actually
// tracks are emitted; rnstatus.py is robust to missing optional fields.
// Insertion order matches Reticulum.py:1294-1482 so the wire bytes stay
// aligned with Python's umsgpack output.
//
// Strings are packed via Packer::pack(const char*) / pack(c_str, size).
// MsgPack's std::string serialize path fails on some embedded targets
// (e.g. heltec_wifi_lora_32_V4); the const-char-pointer overload produces
// the same msgpack str wire format on all platforms.
static void remote_status_pack_interface(MsgPack::Packer& p, const Interface& iface) {
	// 18 entries: name, short_name, hash, type, status, mode, clients,
	//             bitrate, rx, rxb, tx, txb, rxs, txs, rssi, snr, q,
	//             announce_queue.
	//
	// `clients` is accessed unguarded at rnstatus.py:429 (ifstat["clients"]),
	// so the key MUST be present even when no meaningful value -- emit nil
	// to signal "not a shared instance / no connected clients".
	//
	// rssi / snr / q are a microReticulum-specific extension Python's
	// get_interface_stats doesn't emit. Forward-compatible -- Python clients
	// ignore unknown keys; clients that look for them get the receiving
	// interface's last reported signal-quality stats, with nil for "not
	// reported by this interface" (e.g. non-radio interfaces such as UDP).
	p.packMapSize(18);

	// Cache std::string values so .c_str() / .size() reference stable storage.
	const std::string name_str = iface.toString();
	const std::string short_name_str = iface.name();
	const std::string type_str = remote_status_interface_type_name(iface);

	p.pack("name");
	p.pack(name_str.c_str(), name_str.size());

	p.pack("short_name");
	p.pack(short_name_str.c_str(), short_name_str.size());

	p.pack("hash");
	Bytes h = iface.get_hash();
	p.packBinary(h.data(), h.size());

	p.pack("type");
	p.pack(type_str.c_str(), type_str.size());

	p.pack("status");
	p.packBool(iface.online());

	p.pack("mode");
	p.serialize(static_cast<uint32_t>(iface.mode()));

	// microReticulum is a leaf node, not a shared-instance host. Order
	// matches Python's get_interface_stats() (clients after mode).
	p.pack("clients");
	p.packNil();

	p.pack("bitrate");
	if (iface.bitrate() > 0) {
		p.serialize(static_cast<uint32_t>(iface.bitrate()));
	} else {
		p.packNil();
	}

	p.pack("rx");
	p.serialize(static_cast<uint64_t>(iface.rx()));
	p.pack("rxb");
	p.serialize(static_cast<uint64_t>(iface.rxbytes()));

	p.pack("tx");
	p.serialize(static_cast<uint64_t>(iface.tx()));
	p.pack("txb");
	p.serialize(static_cast<uint64_t>(iface.txbytes()));

	p.pack("rxs");
	p.packFloat64(iface.current_rx_speed());

	p.pack("txs");
	p.packFloat64(iface.current_tx_speed());

	p.pack("rssi");
	if (std::isnan(iface.r_stat_rssi())) {
		p.packNil();
	} else {
		p.packFloat64(static_cast<double>(iface.r_stat_rssi()));
	}

	p.pack("snr");
	if (std::isnan(iface.r_stat_snr())) {
		p.packNil();
	} else {
		p.packFloat64(static_cast<double>(iface.r_stat_snr()));
	}

	p.pack("q");
	if (std::isnan(iface.r_stat_q())) {
		p.packNil();
	} else {
		p.packFloat64(static_cast<double>(iface.r_stat_q()));
	}

	p.pack("announce_queue");
	p.serialize(static_cast<uint32_t>(iface.announce_queue().size()));
}

// Builds the top-level stats map. Order matches Python's get_interface_stats().
// Emits 7 keys: interfaces, rx, rxb, tx, txb, rxs, txs. transport_id /
// transport_uptime can be added once Transport exposes them.
static Bytes remote_status_build_stats_payload() {
	MsgPack::Packer p;
	p.packMapSize(7);

	auto& interfaces = Transport::get_interfaces();

	p.pack("interfaces");
	p.packArraySize(static_cast<size_t>(interfaces.size()));
	for (auto& iface : interfaces) {
		remote_status_pack_interface(p, iface);
	}

	uint64_t total_rx = 0;
	uint64_t total_tx = 0;
	for (auto& iface : interfaces) {
		total_rx += iface.rx();
		total_tx += iface.tx();
	}

	p.pack("rx");
	p.serialize(total_rx);
	p.pack("rxb");
	p.serialize(Transport::traffic_rxb());

	p.pack("tx");
	p.serialize(total_tx);
	p.pack("txb");
	p.serialize(Transport::traffic_txb());

	p.pack("rxs");
	p.packFloat64(Transport::speed_rx());

	p.pack("txs");
	p.packFloat64(Transport::speed_tx());

	return Bytes(p.data(), p.size());
}

/*static*/ Bytes Transport::remote_status_handler(const Bytes& path, const Bytes& data, const Bytes& request_id, const Bytes& link_id, const Identity& remote_identity, double requested_at) {

	const bool include_link_count = remote_status_parse_include_link_count(data);

	Bytes stats = remote_status_build_stats_payload();

	// Outer: [stats_map] or [stats_map, link_count]. The Link layer wraps
	// this return value as [bin(request_id), <these bytes>] via
	// pack_response_envelope -- splice-compatible with Python's
	// umsgpack.packb([request_id, response]).
	MsgPack::Packer outer;
	outer.packArraySize(include_link_count ? 2 : 1);

	Bytes result(outer.data(), outer.size());
	result.append(stats);

	if (include_link_count) {
		MsgPack::Packer lc;
		const size_t link_count = _link_table.size();
		lc.serialize(static_cast<uint32_t>(link_count));
		Bytes lc_bytes(lc.data(), lc.size());
		result.append(lc_bytes);
	}

	return result;
}

// Parsed request payload from rnpath. Mirrors Python's [command, dest_hash?, max_hops?].
struct RemotePathRequest {
	MsgPack::str_t command;          // empty if absent
	Bytes dest_hash;                 // empty if absent / nil
	uint32_t max_hops = 0;
	bool max_hops_present = false;
};

// Parse the msgpack request payload sent by rnpath:
//   ["table"|"rates", destination_hash_or_nil, max_hops_or_nil]
// rnpath always sends a 2- or 3-element array; the trailing slots may be nil
// (Python None) when the user did not pass a filter. Returns false on any
// malformed input; the handler then sends no response.
static bool remote_path_parse_request(const Bytes& data, RemotePathRequest& out) {
	if (!data || data.size() == 0) return false;
	MsgPack::Unpacker u;
	u.feed(data.data(), data.size());

	if (!u.isArray()) {
		TRACE("remote_path_parse_request: Data is not array of elements");
		return false;
	}
	size_t n = u.unpackArraySize();
	if (n < 1) {
		TRACE("remote_path_parse_request: No elements in data array");
		return false;
	}

	// Element 0: command (required)
	if (!u.isStr()) {
		TRACE("remote_path_parse_request: Command element is not a string");
		return false;
	}
	if (!u.deserialize(out.command)) {
		TRACE("remote_path_parse_request: Failed to deseriaize command element");
		return false;
	}

	// Element 1: destination_hash (optional, may be nil)
	if (n >= 2) {
		if (u.isNil()) {
			u.unpackNil();
		} else if (u.isBin()) {
			MsgPack::bin_t<uint8_t> ph;
			if (!u.deserialize(ph)) return false;
			out.dest_hash = Bytes(ph.data(), ph.size());
		} else {
			TRACE("remote_path_parse_request: Failed to deseriaize destination_hash element");
			return false;
		}
	}

	// Element 2: max_hops (optional, may be nil)
	if (n >= 3) {
		if (u.isNil()) {
			u.unpackNil();
		} else if (u.isUInt() || u.isInt()) {
			uint32_t hops = 0;
			if (!u.deserialize(hops)) return false;
			out.max_hops = hops;
			out.max_hops_present = true;
		} else {
			TRACE("remote_path_parse_request: Failed to deseriaize max_hops element");
			return false;
		}
	}

	return true;
}

// Packs one path-table entry as a 6-key map. Insertion order matches
// Reticulum.py:get_path_table (lines 1495-1502): hash, timestamp, via, hops,
// expires, interface. rnpath.py:283-289 accesses every field unguarded.
static void remote_path_pack_path_entry(MsgPack::Packer& p,
                                        const Bytes& dest_hash,
                                        const DestinationEntry& entry) {
	p.packMapSize(6);

	p.pack("hash");
	p.packBinary(dest_hash.data(), dest_hash.size());

	p.pack("timestamp");
	p.packFloat64(entry._timestamp);

	p.pack("via");
	p.packBinary(entry._received_from.data(), entry._received_from.size());

	p.pack("hops");
	p.serialize(static_cast<uint32_t>(entry._hops));

	p.pack("expires");
	p.packFloat64(entry._expires);

	p.pack("interface");
	// Cache the std::string so its c_str() points at stable storage for the
	// duration of the pack call.
	const std::string iface_str = entry._receiving_interface.toString();
	p.pack(iface_str.c_str(), iface_str.size());
}

// Packs one announce-rate-table entry as a 5-key map. Insertion order matches
// Reticulum.py:get_rate_table (lines 1517-1524): hash, last, rate_violations,
// blocked_until, timestamps. rnpath.py:339-369 accesses every field unguarded.
static void remote_path_pack_rate_entry(MsgPack::Packer& p,
                                        const Bytes& dest_hash,
                                        const Transport::RateEntry& entry) {
	p.packMapSize(5);

	p.pack("hash");
	p.packBinary(dest_hash.data(), dest_hash.size());

	p.pack("last");
	p.packFloat64(entry._last);

	// Python stores this as int (counter incremented by 1); C++ field is
	// double for legacy reasons. Emit as uint to match Python's wire bytes --
	// rnpath.py:351 only checks `> 0` and stringifies, both work either way.
	p.pack("rate_violations");
	p.serialize(static_cast<uint32_t>(entry._rate_violations));

	p.pack("blocked_until");
	p.packFloat64(entry._blocked_until);

	// timestamps must always be a (possibly empty) list -- rnpath.py:344
	// does entry["timestamps"][0] unguarded. RateEntry::RateEntry(double now)
	// at Transport.h:259 seeds the vector with `now`, so >=1 element in practice.
	p.pack("timestamps");
	p.packArraySize(entry._timestamps.size());
	for (double ts : entry._timestamps) {
		p.packFloat64(ts);
	}
}

/*static*/ Bytes Transport::remote_path_handler(const Bytes& path, const Bytes& data, const Bytes& request_id, const Bytes& link_id, const Identity& remote_identity, double requested_at) {

	TRACEF("remote_path_handler: Parsing remote path request of %u bytes", data.size());
	RemotePathRequest req;
	if (!remote_path_parse_request(data, req)) {
		WARNING("remote_path_handler: Failed to parse remote path request");
		return {Bytes::NONE};
	}
	TRACEF("remote_path_handler: Remote path request command: %s", req.command.c_str());

	const bool cmd_is_table = (req.command == "table");
	const bool cmd_is_rates = (req.command == "rates");
	if (!cmd_is_table && !cmd_is_rates) {
		WARNING("remote_path_handler: Unknown command in remote path request");
		return {Bytes::NONE};
	}

	MsgPack::Packer p;

	if (cmd_is_table) {
		// For single destination
		if (req.dest_hash.size() > 0) {
			DestinationEntry destination_entry;
			_new_path_table.get(req.dest_hash, destination_entry);
#if RNS_BLOCK_UNRESPONSIVE_ANNOUNCE
			// DIVERGENCE: not advertising unresponsive paths
			// Do not advertise paths we have personally marked
			// UNRESPONSIVE. See path_request() for the rationale; the same
			// reasoning applies to remote management table queries since
			// downstream consumers may treat the returned set as usable
			// paths. State self-heals when a fresh announce resets it via
			// mark_path_unknown_state.
			if (destination_entry && !path_is_unresponsive(req.dest_hash)) {
#else
			if (destination_entry) {
#endif
				p.packArraySize(1);
				remote_path_pack_path_entry(p, req.dest_hash, destination_entry);
			}
			else {
				p.packArraySize(0);
			}
		}
		// For filtering all paths
		else {
			// CBA NOTE Transport::get_path_table() is currently empty since migrating to microStore path store,
			// but leaving this as-is for now since we don't currently ahve the ability to stream the entire path
			// list (nor do we likely want to on resource-constrained devices) so leaving this as a no-op for now.

			size_t match_count = 0;
			for (const auto& path : _new_path_table) {
				if (req.dest_hash.size() > 0 && path.key != req.dest_hash) continue;
				if (req.max_hops_present && path.value._hops > req.max_hops) continue;
#if RNS_BLOCK_UNRESPONSIVE_ANNOUNCE
				// DIVERGENCE: not advertising unresponsive paths
				if (path_is_unresponsive(path.key)) continue;
#endif
				match_count++;
				if (match_count >= 100) break;
			}
			p.packArraySize(match_count);
/*
			// Second pass: emit.
			for (auto& kv : table) {
				if (req.dest_hash.size() > 0 && kv.first != req.dest_hash) continue;
				if (req.max_hops_present && kv.second._hops > req.max_hops) continue;
				remote_path_pack_path_entry(p, kv.first, kv.second);
			}
*/
			match_count = 0;
			for (const auto& path : _new_path_table) {
				if (req.dest_hash.size() > 0 && path.key != req.dest_hash) continue;
				if (req.max_hops_present && path.value._hops > req.max_hops) continue;
#if RNS_BLOCK_UNRESPONSIVE_ANNOUNCE
				// DIVERGENCE: not advertising unresponsive paths
				if (path_is_unresponsive(path.key)) continue;
#endif
				remote_path_pack_path_entry(p, path.key, path.value);
				match_count++;
				if (match_count >= 100) break;
			}
		}
	}
	else {
		// cmd_is_rates
		size_t match_count = 0;
		for (auto& kv : _announce_rate_table) {
			if (req.dest_hash.size() > 0 && kv.first != req.dest_hash) continue;
			match_count++;
		}
		p.packArraySize(match_count);
		for (auto& kv : _announce_rate_table) {
			if (req.dest_hash.size() > 0 && kv.first != req.dest_hash) continue;
			remote_path_pack_rate_entry(p, kv.first, kv.second);
		}
	}

	return Bytes(p.data(), p.size());
}

#if defined(RNS_ENABLE_REMOTE_PROVISIONING) && defined(RNS_USE_PROVISIONING)
/*static*/ Bytes Transport::remote_provision_handler(const Bytes& path, const Bytes& data, const Bytes& request_id, const Bytes& link_id, const Identity& remote_identity, double requested_at) {
	TRACEF("remote_provision_handler: forwarding %u bytes to Provisioning::Provisioner", (unsigned)data.size());
	return Provisioning::Provisioner::instance().handle_message(data);
}
#endif

/*static*/ void Transport::path_request_handler(const Bytes& data, const Packet& packet) {
	++_path_requests_received;
	TRACE("Transport::path_request_handler");
	try {
		// If there is at least bytes enough for a destination
		// hash in the packet, we assume those bytes are the
		// destination being requested.
		if (data.size() >= Type::Identity::TRUNCATED_HASHLENGTH/8) {
			Bytes destination_hash = data.left(Type::Identity::TRUNCATED_HASHLENGTH/8);
			//TRACEF("Transport::path_request_handler: destination_hash: %s", destination_hash.toHex().c_str());
			// If there is also enough bytes for a transport
			// instance ID and at least one tag byte, we
			// assume the next bytes to be the trasport ID
			// of the requesting transport instance.
			Bytes requesting_transport_instance;
			if (data.size() > (Type::Identity::TRUNCATED_HASHLENGTH/8)*2) {
				requesting_transport_instance = data.mid(Type::Identity::TRUNCATED_HASHLENGTH/8, Type::Identity::TRUNCATED_HASHLENGTH/8);
				//TRACEF("Transport::path_request_handler: requesting_transport_instance: %s", requesting_transport_instance.toHex().c_str());
			}

			Bytes tag_bytes;
			if (data.size() > Type::Identity::TRUNCATED_HASHLENGTH/8*2) {
				tag_bytes = data.mid(Type::Identity::TRUNCATED_HASHLENGTH/8*2);
			}
			else if (data.size() > Type::Identity::TRUNCATED_HASHLENGTH/8) {
				tag_bytes = data.mid(Type::Identity::TRUNCATED_HASHLENGTH/8);
			}

			if (tag_bytes) {
				//TRACEF("Transport::path_request_handler: tag_bytes: %s", tag_bytes.toHex().c_str());
				if (tag_bytes.size() > Type::Identity::TRUNCATED_HASHLENGTH/8) {
					tag_bytes = tag_bytes.left(Type::Identity::TRUNCATED_HASHLENGTH/8);
				}

				Bytes unique_tag = destination_hash + tag_bytes;
				//TRACEF("Transport::path_request_handler: unique_tag: %s", unique_tag.toHex().c_str());

				if (!_discovery_pr_tags.contains(unique_tag)) {
					// CBA ACCUMULATES
					_discovery_pr_tags.insert(unique_tag);

					path_request(
						destination_hash,
						from_local_client(packet),
						packet.receiving_interface(),
						requesting_transport_instance,
						tag_bytes
					);
				}
				else {
					DEBUGF("Ignoring duplicate path request for %s with tag %s", destination_hash.toHex().c_str(), unique_tag.toHex().c_str());
				}
			}
			else {
				DEBUGF("Ignoring tagless path request for %s", destination_hash.toHex().c_str());
			}
		}
	}
	catch (const std::exception& e) {
		ERRORF("Error while handling path request. The contained exception was: %s", e.what());
	}
}

/*static*/ void Transport::path_request(const Bytes& destination_hash, bool is_from_local_client, const Interface& attached_interface, const Bytes& requestor_transport_id /*= {}*/, const Bytes& tag /*= {}*/) {
	TRACE("Transport::path_request");
	bool should_search_for_unknown = false;
	std::string interface_str;

	if (attached_interface) {
		if (Reticulum::transport_enabled() && (attached_interface.mode() & Interface::DISCOVER_PATHS_FOR) > 0) {
			TRACE("Transport::path_request_handler: interface allows searching for unknown paths");
			should_search_for_unknown = true;
		}

		interface_str = " on " + attached_interface.toString();
	}

	DEBUGF("Path request for destination %s%s", destination_hash.toHex().c_str(), interface_str.c_str());

	bool destination_exists_on_local_client = false;
	if (_local_client_interfaces.size() > 0) {
		// CBA microStore
		//auto& destination_entry = get_path(destination_hash);
		DestinationEntry destination_entry;
		_new_path_table.get(destination_hash, destination_entry);
		if (destination_entry) {
			TRACEF("Transport::path_request_handler: path entry found for destination %s", destination_hash.toHex().c_str());
			if (is_local_client_interface(destination_entry.receiving_interface())) {
				destination_exists_on_local_client = true;
				// CBA ACCUMULATES
				_pending_local_path_requests.insert({destination_hash, attached_interface});
			}
		}
		else {
			TRACEF("Transport::path_request_handler: entry not found for destination %s", destination_hash.toHex().c_str());
		}
	}

	// CBA microStore
	//DestinationEntry& destination_entry = get_path(destination_hash);
	DestinationEntry destination_entry;
	_new_path_table.get(destination_hash, destination_entry);
	//local_destination = next((d for d in Transport.destinations if d.hash == destination_hash), None)
	auto destinations_iter = _destinations.find(destination_hash);
	if (destinations_iter != _destinations.end()) {
		auto& local_destination = (*destinations_iter).second;
		local_destination.announce({Bytes::NONE}, true, attached_interface, tag);
		DEBUGF("Answering path request for destination %s%s, destination is local to this system", destination_hash.toHex().c_str(), interface_str.c_str());
	}
    //p elif (RNS.Reticulum.transport_enabled() or is_from_local_client) and (destination_hash in Transport.destination_table):
	else if ((Reticulum::transport_enabled() || is_from_local_client) && destination_entry) {
		TRACEF("Transport::path_request_handler: entry found for destination %s", destination_hash.toHex().c_str());

#if RNS_BLOCK_UNRESPONSIVE_ANNOUNCE
		// DIVERGENCE: not advertising unresponsive paths
		// Python (Transport.py path_request handling) does not consult
		// path_states when answering path requests, so a node will keep
		// advertising a path it has personally observed to be broken.
		// We filter here: if we have marked this path UNRESPONSIVE via a
		// failed link attempt (see jobs() link-table teardown), do not
		// poison the requester with a path we already know is bad. A
		// fresh same-generation announce resets the state to UNKNOWN
		// (see mark_path_unknown_state call after _new_path_table.put),
		// so the filter self-heals.
		if (path_is_unresponsive(destination_hash)) {
			DEBUGF("Not answering path request for destination %s%s, path is marked unresponsive", destination_hash.toHex().c_str(), interface_str.c_str());
			return;
		}
#endif

		const Packet announce_packet = destination_entry.announce_packet();
TRACEF("announce_packet destination_hash: %s", announce_packet.destination_hash().toHex().c_str());
TRACEF("announce_packet hops: %u", announce_packet.hops());
TRACEF("announce_packet str: %s", announce_packet.toString().c_str());
		const Bytes& next_hop = destination_entry._received_from;
		const Interface& receiving_interface = destination_entry.receiving_interface();

		if (attached_interface.mode() == Type::Interface::MODE_ROAMING && attached_interface == receiving_interface) {
			DEBUG("Not answering path request on roaming-mode interface, since next hop is on same roaming-mode interface");
		}
		else {
			if (requestor_transport_id && destination_entry._received_from == requestor_transport_id) {
				// TODO: Find a bandwidth efficient way to invalidate our
				// known path on this signal. The obvious way of signing
				// path requests with transport instance keys is quite
				// inefficient. There is probably a better way. Doing
				// path invalidation here would decrease the network
				// convergence time. Maybe just drop it?
				DEBUGF("Not answering path request for destination %s%s, since next hop is the requestor", destination_hash.toHex().c_str(), interface_str.c_str());
			}
			else {
				DEBUGF("Answering path request for destination %s%s, path is known", destination_hash.toHex().c_str(), interface_str.c_str());

				double now = OS::time();
				uint8_t retries = Type::Transport::PATHFINDER_R;
				uint8_t local_rebroadcasts = 0;
				bool block_rebroadcasts = true;
				// CBA TODO Determine if okay to take hops directly from DestinationEntry
				uint8_t announce_hops = announce_packet.hops();

				double retransmit_timeout = 0;
				if (is_from_local_client) {
					retransmit_timeout = now;
				}
				else {
					// TODO: Look at this timing
					retransmit_timeout = now + Type::Transport::PATH_REQUEST_GRACE /*+ (RNS.rand() * Transport.PATHFINDER_RW)*/;

					// If we are answering on a roaming-mode interface, wait a
					// little longer, to allow potential more well-connected
					// peers to answer first.
					if (attached_interface && attached_interface.mode() == Type::Interface::MODE_ROAMING) {
						retransmit_timeout += Type::Transport::PATH_REQUEST_RG;
					}
				}

				// This handles an edge case where a peer sends a past
				// request for a destination just after an announce for
				// said destination has arrived, but before it has been
				// rebroadcast locally. In such a case the actual announce
				// is temporarily held, and then reinserted when the path
				// request has been served to the peer.
				auto announce_iter = _announce_table.find(announce_packet.destination_hash());
				if (announce_iter != _announce_table.end()) {
					AnnounceEntry& held_entry = (*announce_iter).second;
					// CBA ACCUMULATES
					_held_announces.insert({announce_packet.destination_hash(), held_entry});
				}

/*
				// CBA ACCUMULATES
				_announce_table.insert({announce_packet.destination_hash(), {
					now,
					retransmit_timeout,
					retries,
					// BUG?
					//destination_entry.receiving_interface,
					destination_entry._received_from,
					announce_hops,
					announce_packet,
					local_rebroadcasts,
					block_rebroadcasts,
					attached_interface
				}});
*/
				AnnounceEntry announce_entry(
					now,
					retransmit_timeout,
					retries,
					// BUG?
					//destination_entry.receiving_interface,
					destination_entry._received_from,
					announce_hops,
					announce_packet,
					local_rebroadcasts,
					block_rebroadcasts,
					attached_interface
				);
				// CBA ACCUMULATES
				_announce_table.insert({announce_packet.destination_hash(), announce_entry});
				// CBA IMMEDIATE CULL
				cull_announce_table();

				// Send PATH_RESPONSE immediately for local client requests
				// rather than waiting for the jobs() loop. On resource-
				// constrained platforms (e.g. ESP32), continuous TCP backbone
				// data can starve the cooperative jobs() loop for many
				// seconds, causing path discovery timeouts for local clients.
				if (is_from_local_client) {
					Identity imm_identity(Identity::recall(announce_packet.destination_hash()));
					if (imm_identity) {
						Destination imm_destination(imm_identity, Type::Destination::OUT, Type::Destination::SINGLE, announce_packet.destination_hash());
						Packet imm_packet = Packet(imm_destination, announce_packet.data())
							.attached_interface(attached_interface)
							.packet_type(Type::Packet::ANNOUNCE)
							.context(Type::Packet::PATH_RESPONSE)
							.transport_type(Type::Transport::TRANSPORT)
							.header_type(Type::Packet::HEADER_2)
							.transport_id(_identity.hash())
							.context_flag(announce_packet.context_flag());
						imm_packet.hops(announce_hops);
						imm_packet.send();
						_announce_table.erase(announce_packet.destination_hash());
					}
				}
			}
		}
	}
	else if (is_from_local_client) {
		// Forward path request on all interfaces
		// except the local client
		DEBUGF("Forwarding path request from local client for destination %s%s to all other interfaces", destination_hash.toHex().c_str(), interface_str.c_str());
		Bytes request_tag = Identity::get_random_hash();
		for (auto& interface : _interfaces) {
			if (interface != attached_interface) {
				request_path(destination_hash, interface, request_tag);
			}
		}
	}
	else if (should_search_for_unknown) {
		TRACEF("Transport::path_request_handler: searching for unknown path to %s", destination_hash.toHex().c_str());
		if (_discovery_path_requests.find(destination_hash) != _discovery_path_requests.end()) {
			DEBUGF("There is already a waiting path request for destination %s on behalf of path request%s", destination_hash.toHex().c_str(), interface_str.c_str());
		}
		else {
			// Forward path request on all interfaces
			// except the requestor interface
			DEBUGF("Attempting to discover unknown path to destination %s on behalf of path request%s", destination_hash.toHex().c_str(), interface_str.c_str());
			//p pr_entry = { "destination_hash": destination_hash, "timeout": time.time()+Transport.PATH_REQUEST_TIMEOUT, "requesting_interface": attached_interface }
			//p _discovery_path_requests[destination_hash] = pr_entry;
			// CBA ACCUMULATES
			_discovery_path_requests.insert({destination_hash, {
				destination_hash,
				OS::time() + Type::Transport::PATH_REQUEST_TIMEOUT,
				attached_interface
			}});

			for (auto& interface : _interfaces) {
#if RNS_SAME_INTERFACE_PATH_REQUESTS
				// DIVERGENCE: forward path requests on same interface
				// CBA EXPERIMENTAL forwarding path requests even on requestor interface in order to support
				//  path-finding over LoRa mesh
				if (true) {
#else
				//if (interface != attached_interface) {
#endif
					TRACEF("Transport::path_request: requesting path on interface %s", interface.toString().c_str());
					// Use the previously extracted tag from this path request
					// on the new path requests as well, to avoid potential loops
					request_path(destination_hash, interface, tag, true);
				}
				else {
					TRACEF("Transport::path_request: not requesting path on same interface %s", interface.toString().c_str());
				}
			}
		}
	}
	else if (!is_from_local_client && _local_client_interfaces.size() > 0) {
		// Forward the path request on all local
		// client interfaces
		DEBUGF("Forwarding path request for destination %s%s to local clients", destination_hash.toHex().c_str(), interface_str.c_str());
		for (const Interface& interface : _local_client_interfaces) {
			request_path(destination_hash, interface);
		}
	}
	else {
		DEBUGF("Ignoring path request for destination %s%s, no path known", destination_hash.toHex().c_str(), interface_str.c_str());
	}
}

/*static*/ bool Transport::from_local_client(const Packet& packet) {
	if (packet.receiving_interface().parent_interface()) {
		return is_local_client_interface(packet.receiving_interface());
	}
	else {
		return false;
	}
}

/*static*/ bool Transport::is_local_client_interface(const Interface& interface) {
	if (interface.parent_interface()) {
		if (interface.parent_interface()->is_local_shared_instance()) {
			return true;
		}
		else {
			return false;
		}
	}
	else {
		return false;
	}
}

/*static*/ bool Transport::interface_to_shared_instance(const Interface& interface) {
	if (interface.is_connected_to_shared_instance()) {
		return true;
	}
	else {
		return false;
	}
}

// Clean-shutdown sequence for the interface layer: tear down all links so peers
// see the connection drop, drain for 150ms to let teardown packets leave, then
// call detach() on every interface so they can release resources before
// destruction. Python additionally orders LocalServerInterface and
// LocalClientInterface teardown separately for its shared-instance protocol;
// microReticulum does not support shared-instance clients/servers, so those
// branches collapse to the unified loop here.
/*static*/ void Transport::detach_interfaces() {
	TRACE("Transport::detach_interfaces()");

	size_t closed_links = 0;
	// Iterate by value -- Link is a value-type wrapping a shared impl, so a
	// copy still routes teardown() to the same underlying object. std::set
	// yields const references which can't call the non-const teardown().
	for (Link link : _active_links) {
		try { link.teardown(); closed_links++; }
		catch (const std::exception& e) {
			WARNINGF("Could not tear down active link before interface detach: %s", e.what());
		}
	}
	for (Link link : _pending_links) {
		try { link.teardown(); closed_links++; }
		catch (const std::exception& e) {
			WARNINGF("Could not tear down pending link before interface detach: %s", e.what());
		}
	}

	// Provide a 150ms window to allow link teardown packets to leave local transport
	if (closed_links > 0) {
		OS::sleep(0.15f);
	}

	DEBUG("Detaching interfaces");
	for (Interface& iface : _interfaces) {
		try {
			iface.detach();
		}
		catch (const std::exception& e) {
			ERRORF("Error while detaching %s: %s", iface.toString().c_str(), e.what());
		}
	}
	DEBUG("All interfaces detached");
}

/*static*/ void Transport::shared_connection_disappeared() {
// TODO
/*p
	for link in Transport.active_links:
		link.teardown()

	for link in Transport.pending_links:
		link.teardown()

	Transport.announce_table    = {}
	Transport.destination_table = {}
	Transport.reverse_table     = {}
	Transport.link_table        = {}
	Transport.held_announces    = {}
	Transport.announce_handlers = []
	Transport.tunnels           = {}
*/
}

/*static*/ void Transport::shared_connection_reappeared() {
// TODO
/*p
	if Transport.owner.is_connected_to_shared_instance:
		for registered_destination in Transport.destinations:
			if registered_destination.type == RNS.Destination.SINGLE:
				registered_destination.announce(path_response=True)
*/
}

// Empties every interface's pending announce queue. Useful when shutting down
// cleanly without flushing buffered outbound announces, or when an interface
// is being deregistered.
/*static*/ void Transport::drop_announce_queues() {
	for (Interface& iface : _interfaces) {
		auto& queue = iface.announce_queue();
		size_t na = queue.size();
		if (na > 0) {
			queue.clear();
			VERBOSEF("Dropped %zu announce%s on %s", na, na == 1 ? "" : "s", iface.toString().c_str());
		}
	}
}

/*p
    @staticmethod
    def timebase_from_random_blob(random_blob):
        return int.from_bytes(random_blob[5:10], "big")

    @staticmethod
    def timebase_from_random_blobs(random_blobs):
        timebase = 0
        for random_blob in random_blobs:
            emitted = Transport.timebase_from_random_blob(random_blob)
            if emitted > timebase: timebase = emitted

        return timebase
*/

/*static*/ uint64_t Transport::announce_emitted(const Packet& packet) {
	//p random_blob = packet.data[RNS.Identity.KEYSIZE//8+RNS.Identity.NAME_HASH_LENGTH//8:RNS.Identity.KEYSIZE//8+RNS.Identity.NAME_HASH_LENGTH//8+10]
	//p announce_emitted = int.from_bytes(random_blob[5:10], "big")
	Bytes random_blob = packet.data().mid(Type::Identity::KEYSIZE/8+Type::Identity::NAME_HASH_LENGTH/8, 10);
	if (random_blob) {
		return OS::from_bytes_big_endian(random_blob.data() + 5, 5);
	}
	return 0;
}

// Extracts the 5-byte big-endian emission timebase from offset 5 of a single
// random_blob. Returns 0 if the blob is shorter than 10 bytes.
/*static*/ uint64_t Transport::timebase_from_random_blob(const Bytes& random_blob) {
	if (random_blob.size() < 10) {
		return 0;
	}
	return OS::from_bytes_big_endian(random_blob.data() + 5, 5);
}

// Returns the maximum emission timebase across all stored random blobs for
// a path. Used during announce ingestion to reject announces that are older
// than what we've already seen for the same destination.
/*static*/ uint64_t Transport::timebase_from_random_blobs(const std::vector<Bytes>& random_blobs) {
	uint64_t timebase = 0;
	for (const Bytes& blob : random_blobs) {
		uint64_t emitted = timebase_from_random_blob(blob);
		if (emitted > timebase) {
			timebase = emitted;
		}
	}
	return timebase;
}

// Adds an identity to the blackhole list. Source is set to this transport's
// own identity hash. If until > 0 it is treated as a unix-timestamp expiry;
// 0 means permanent. After insertion, any paths in the path table associated
// with the blackholed identity are removed and the local blackhole file is
// rewritten. Returns true if a new entry was added, false on error or if the
// identity was already blackholed.
/*static*/ bool Transport::blackhole_identity(const Bytes& identity_hash, double until, const std::string& reason) {
	try {
		if (_blackholed_identities.find(identity_hash) != _blackholed_identities.end()) {
			return false;
		}
		_blackholed_identities.emplace(identity_hash, BlackholeEntry(_identity.hash(), until, reason));
		persist_blackhole();
		remove_blackholed_paths();
		INFOF("Blackholed identity %s", identity_hash.toHex().c_str());
		return true;
	}
	catch (const std::exception& e) {
		ERRORF("Error while blackholing identity: %s", e.what());
		return false;
	}
}

// Removes a previously-blackholed identity from the list and rewrites the
// local blackhole file. Returns true if an entry was removed.
/*static*/ bool Transport::unblackhole_identity(const Bytes& identity_hash) {
	try {
		auto iter = _blackholed_identities.find(identity_hash);
		if (iter == _blackholed_identities.end()) {
			return false;
		}
		_blackholed_identities.erase(iter);
		persist_blackhole();
		INFOF("Lifted blackhole for identity %s", identity_hash.toHex().c_str());
		return true;
	}
	catch (const std::exception& e) {
		ERRORF("Error while unblackholing identity: %s", e.what());
		return false;
	}
}

// Quick membership check used by the inline announce filter.
/*static*/ bool Transport::is_blackholed(const Bytes& identity_hash) {
	return _blackholed_identities.find(identity_hash) != _blackholed_identities.end();
}

// Loads the persisted blackhole list from {storagepath}/blackhole_local and
// then purges any path-table entries associated with the loaded identities.
// Entries with an expired 'until' timestamp are silently skipped during load.
// Called once during Transport::start().
/*static*/ void Transport::reload_blackhole() {
	std::string path = std::string(Reticulum::storagepath()) + "/blackhole_local";
	if (!OS::file_exists(path.c_str())) {
		return;
	}

	Bytes data;
	size_t n = OS::read_file(path.c_str(), data);
	if (n == 0 || data.size() == 0) {
		return;
	}

	try {
		MsgPack::Unpacker u;
		u.feed(data.data(), data.size());
		if (!u.isMap()) {
			WARNING("Blackhole file is not a msgpack map; ignoring");
			return;
		}
		const size_t map_size = u.unpackMapSize();
		double now = OS::time();
		size_t loaded = 0;
		for (size_t i = 0; i < map_size; i++) {
			MsgPack::bin_t<uint8_t> key_bin;
			if (!u.deserialize(key_bin)) return;
			Bytes identity_hash(key_bin.data(), key_bin.size());

			// Each value is a 3-element array: [source, until, reason]
			if (!u.isArray()) return;
			const size_t arr_size = u.unpackArraySize();
			if (arr_size < 3) return;
			MsgPack::bin_t<uint8_t> src_bin;
			if (!u.deserialize(src_bin)) return;
			Bytes source(src_bin.data(), src_bin.size());
			double until = 0.0;
			if (!u.deserialize(until)) return;
			// Decode through MsgPack::str_t (aliased to std::string on native and
			// Arduino's String on embedded) then normalise to std::string for storage.
			MsgPack::str_t reason_str;
			if (!u.deserialize(reason_str)) return;
			std::string reason(reason_str.c_str(), reason_str.length());

			if (until > 0.0 && now > until) {
				continue;   // expired, skip
			}
			_blackholed_identities.emplace(identity_hash, BlackholeEntry(source, until, reason));
			loaded++;
		}
		if (loaded > 0) {
			NOTICEF("Loaded %zu blackholed identities from storage", loaded);
		}
	}
	catch (const std::exception& e) {
		ERRORF("Could not load blackholed identities: %s", e.what());
	}

	remove_blackholed_paths();
}

// Scans the path table and removes any destination whose associated identity
// (resolved via Identity::recall) is currently blackholed. Called after every
// blackhole_identity() and at startup after reload_blackhole().
/*static*/ void Transport::remove_blackholed_paths() {
	if (_blackholed_identities.empty()) return;

	std::vector<Bytes> drop_destinations;
	try {
		for (const auto& path : _new_path_table) {
			Bytes destination_hash = path.key;
			Identity associated = Identity::recall(destination_hash);
			if (associated && is_blackholed(associated.hash())) {
				drop_destinations.push_back(destination_hash);
			}
		}
	}
	catch (const std::exception& e) {
		ERRORF("Error while enumerating blackhole-associated destinations: %s", e.what());
	}

	for (const Bytes& destination_hash : drop_destinations) {
		try {
			_new_path_table.remove(destination_hash);
		}
		catch (const std::exception& e) {
			ERRORF("Error while dropping blackhole-associated destination from path table: %s", e.what());
		}
	}

	if (!drop_destinations.empty()) {
		INFOF("Removed %zu destinations associated with blackholed identities from path table", drop_destinations.size());
	}
}

// Writes the local blackhole list (entries whose source is this transport's
// own identity) to {storagepath}/blackhole_local as a msgpack map of
// identity_hash -> [source, until, reason]. Entries originating from other
// sources are not re-persisted by us (they belong to their own source files
// in the multi-source design that is currently out of scope).
/*static*/ void Transport::persist_blackhole() {
	try {
		MsgPack::Packer p;
		size_t local_count = 0;
		for (const auto& [hash, entry] : _blackholed_identities) {
			if (entry._source == _identity.hash()) local_count++;
		}
		p.packMapSize(local_count);
		for (const auto& [hash, entry] : _blackholed_identities) {
			if (entry._source != _identity.hash()) continue;
			p.packBinary(hash.data(), hash.size());
			p.packArraySize(3);
			p.packBinary(entry._source.data(), entry._source.size());
			p.packFloat64(entry._until);
			p.pack(entry._reason.c_str(), entry._reason.size());
		}

		Bytes data(p.data(), p.size());
		std::string path = std::string(Reticulum::storagepath()) + "/blackhole_local";
		size_t written = OS::write_file(path.c_str(), data);
		if (written != data.size()) {
			WARNINGF("Short write while persisting blackhole list (%zu of %zu bytes)", written, data.size());
		}
	}
	catch (const std::exception& e) {
		ERRORF("Error while persisting blackhole list: %s", e.what());
	}
}

// Request handler for the /list endpoint on the blackhole publishing
// destination. Returns the current blackhole list as a msgpack map matching
// Python's serialization so cross-stack clients can ingest it.
/*static*/ Bytes Transport::blackhole_list_handler(const Bytes& path, const Bytes& data, const Bytes& request_id, const Bytes& link_id, const Identity& remote_identity, double requested_at) {
	try {
		// Wire format must match Python's Transport.blackholed_identities:
		// {identity_hash: {"source": <bytes>, "until": <float>, "reason": <str>}}.
		// The local persistence file uses a compact positional 3-array
		// instead -- this handler is the only Python-interop surface.
		MsgPack::Packer p;
		p.packMapSize(_blackholed_identities.size());
		for (const auto& [hash, entry] : _blackholed_identities) {
			p.packBinary(hash.data(), hash.size());
			p.packMapSize(3);
			p.pack("source");
			p.packBinary(entry._source.data(), entry._source.size());
			p.pack("until");
			p.packFloat64(entry._until);
			p.pack("reason");
			p.pack(entry._reason.c_str(), entry._reason.size());
		}
		return Bytes(p.data(), p.size());
	}
	catch (const std::exception& e) {
		ERRORF("Error while processing blackhole list request: %s", e.what());
		return {};
	}
}


//#define CUSTOM 1

/*static*/ bool Transport::read_path_table() {
	DEBUG("Transport::read_path_table");
#if defined(RNS_USE_FS) && RNS_PERSIST_PATHS
	char destination_table_path[Type::Reticulum::FILEPATH_MAXSIZE];
	snprintf(destination_table_path, Type::Reticulum::FILEPATH_MAXSIZE, "%s/destination_table", Reticulum::_storagepath);
	if (!_owner.is_connected_to_shared_instance() && OS::file_exists(destination_table_path)) {
		try {
#if CUSTOM
TRACEF("Transport::read_path_table: buffer capacity %d bytes", Persistence::_buffer.capacity());
			if (Utilities::OS::read_file(destination_table_path, Persistence::_buffer) > 0) {
				TRACEF("Transport::read_path_table: read: %d bytes", Persistence::_buffer.size());
#ifndef NDEBUG
				// CBA DEBUG Dump path table
TRACEF("Transport::read_path_table: buffer addr: 0x%X", Persistence::_buffer.data());
TRACEF("Transport::read_path_table: buffer size %d bytes", Persistence::_buffer.size());
				//TRACE("SERIALIZED: destination_table");
				//TRACE(Persistence::_buffer.toString().c_str());
#endif
#ifdef USE_MSGPACK
				DeserializationError error = deserializeMsgPack(Persistence::_document, Persistence::_buffer.data());
#else
				DeserializationError error = deserializeJson(Persistence::_document, Persistence::_buffer.data());
#endif
				TRACEF("Transport::read_path_table: doc size: %d bytes", Persistence::_buffer.size());
				if (!error) {
					// Calculate crc for dirty-checking before write
					_path_table_crc = Crc::crc32(0, Persistence::_buffer.data(), Persistence::_buffer.size());
					_path_table = Persistence::_document.as<PathTable>();
#else	// CUSTOM
				// Calculate crc for dirty-checking before later write
				if (Persistence::deserialize(_path_table, destination_table_path, _path_table_crc) > 0) {
#endif	// CUSTOM

					TRACEF("Transport::read_path_table: successfully deserialized path table with %d entries", _path_table.size());
					std::vector<Bytes> invalid_paths;
					for (auto& [destination_hash, destination_entry] : _path_table) {
#ifndef NDEBUG
						TRACEF("Transport::read_path_table: hash: %s entry: {%s}", destination_hash.toHex().c_str(), destination_entry.debugString().c_str());
#endif
						// CBA Avoid accessing announce_packet() and receiving_interface() until actually needed in order to
						// take advantage of lazy loading and avoid incurring memory hit to store if not actually needed.

						// CBA Optimized to not check for a valid cached announce packet,
						// and instead checking if/when the path is actually used.
						/*
						// CBA If announce packet is not cached then remove destination entry (it's useless without announce packet)
						if (!is_cached_packet(destination_entry.announce_packet_hash())) {
							// remove destination
							WARNINGF("Transport::read_path_table: removing invalid path to %s due to missing announce packet", destination_hash.toHex().c_str());
							invalid_paths.push_back(destination_hash);
							continue;
						}
						*/
						// CBA If receiving interface is not present then remove destination entry (it's useless without receiving interface)
						if (!is_interface_from_hash(destination_entry.receiving_interface_hash())) {
							// remove destination
							WARNINGF("Transport::read_path_table: removing invalid path to %s due to missing receiving interface", destination_hash.toHex().c_str());
							invalid_paths.push_back(destination_hash);
							continue;
						}
						DEBUGF("Loaded path table entry for %s from storage", destination_hash.toHex().c_str());
						OS::reset_watchdog();
					}
					for (const auto& destination_hash : invalid_paths) {
						_path_table.erase(destination_hash);
					}
                    VERBOSEF("Loaded %lu path table entries from storage", _path_table.size());
					return true;
				}
				else {
					TRACE("Transport::read_path_table: failed to deserialize");
				}
#if CUSTOM
			}
			else {
				TRACE("Transport::read_path_table: destination table read failed");
			}
#else	// CUSTOM
#endif	// CUSTOM
		}
		catch (const std::exception& e) {
			ERRORF("Could not load destination table from storage, the contained exception was: %s", e.what());
		}
	}
#endif
	return false;
}

/*static*/ bool Transport::write_path_table() {
	DEBUG("Transport::write_path_table");

	if (Transport::_owner.is_connected_to_shared_instance()) {
		return true;
	}

	bool success = false;
#if defined(RNS_USE_FS) && RNS_PERSIST_PATHS
	if (_saving_path_table) {
		double wait_interval = 0.2;
		double wait_timeout = 5;
		double wait_start = OS::time();
		while (_saving_path_table) {
			OS::sleep(wait_interval);
			if (OS::time() > (wait_start + wait_timeout)) {
				ERROR("Could not save path table to storage, waiting for previous save operation timed out.");
				return false;
			}
		}
	}

	try {
		_saving_path_table = true;
		double save_start = OS::time();

/*p
		serialised_destinations = []
		for destination_hash in Transport.destination_table:
			// Get the destination entry from the destination table
			de = Transport.destination_table[destination_hash]
			interface_hash = de[5].get_hash()

			// Only store destination table entry if the associated
			// interface is still active
			interface = Transport.find_interface_from_hash(interface_hash)
			if interface != None:
				// Get the destination entry from the destination table
				de = Transport.destination_table[destination_hash]
				timestamp = de[0]
				received_from = de[1]
				hops = de[2]
				expires = de[3]
				random_blobs = de[4]
				packet_hash = de[6].get_hash()

				serialised_entry = [
					destination_hash,
					timestamp,
					received_from,
					hops,
					expires,
					random_blobs,
					interface_hash,
					packet_hash
				]

				serialised_destinations.append(serialised_entry)

				Transport.cache(de[6], force_cache=True)

		destination_table_path = RNS.Reticulum.storagepath+"/destination_table"
		file = open(destination_table_path, "wb")
		file.write(umsgpack.packb(serialised_destinations))
		file.close()
*/

#if CUSTOM
		{
			Persistence::_document.set(_path_table);
			TRACEF("Transport::write_path_table: doc size %d bytes", Persistence::_document.memoryUsage());

			//size_t size = 8192;
			size_t size = Persistence::_buffer.capacity();
TRACEF("Transport::write_path_table: obtaining buffer size %lu bytes", size);
			uint8_t* buffer = Persistence::_buffer.writable(size);
TRACEF("Transport::write_path_table: buffer addr: %ld", (long)buffer);
#ifdef USE_MSGPACK
			size_t length = serializeMsgPack(Persistence::_document, buffer, size);
#else
			size_t length = serializeJson(Persistence::_document, buffer, size);
#endif
			TRACEF("Transport::write_path_table: serialized %d bytes", length);
			if (length < size) {
				Persistence::_buffer.resize(length);
			}
		}
		if (Persistence::_buffer.size() > 0) {
#ifndef NDEBUG
			// CBA DEBUG Dump path table
TRACEF("Transport::write_path_table: buffer addr: %ld", (long)Persistence::_buffer.data());
TRACEF("Transport::write_path_table: buffer size %lu bytes", Persistence::_buffer.size());
			//TRACE("SERIALIZED: destination_table");
			//TRACE(Persistence::_buffer.toString().c_str());
#endif
			// Check crc to see if data has changed before writing
			uint32_t crc = Crc::crc32(0, Persistence::_buffer.data(), Persistence::_buffer.size());
			if (_path_table_crc > 0 && crc == _path_table_crc) {
				TRACE("Transport::write_path_table: no change detected, skipping write");
			}
			else {
				TRACE("Transport::write_path_table: change detected, writing...");
				DEBUGF("Saving %d path table entries to storage...", _path_table.size());
				char destination_table_path[Type::Reticulum::FILEPATH_MAXSIZE];
				snprintf(destination_table_path, Type::Reticulum::FILEPATH_MAXSIZE, "%s/destination_table", Reticulum::_storagepath);
				if (Utilities::OS::write_file(destination_table_path, Persistence::_buffer) == Persistence::_buffer.size()) {
					TRACEF("Transport::write_path_table: wrote %d entries, %d bytes", _path_table.size(), Persistence::_buffer.size());
					_path_table_crc = crc;
					success = true;

#ifndef NDEBUG
					// CBA DEBUG Dump path table
					//TRACE("FILE: destination_table");
					//if (OS::read_file("./destination_table", Persistence::_buffer) > 0) {
					//	TRACE(Persistence::_buffer.toString().c_str());
					//}
#endif
				}
				else {
					ERROR("Transport::write_path_table: write failed");
				}
			}
		}
		else {
			ERROR("Transport::write_path_table: failed to serialize");
		}
#else	// CUSTOM
		uint32_t crc = Persistence::crc(_path_table);
		if (_path_table_crc > 0 && crc == _path_table_crc) {
			TRACE("Transport::write_path_table: no change detected, skipping write");
		}
		else {
			TRACE("Transport::write_path_table: change detected, writing...");
			DEBUGF("Saving %d path table entries to storage...", _path_table.size());
			char destination_table_path[Type::Reticulum::FILEPATH_MAXSIZE];
			snprintf(destination_table_path, Type::Reticulum::FILEPATH_MAXSIZE, "%s/destination_table", Reticulum::_storagepath);
			size_t len = Persistence::serialize(_path_table, destination_table_path, _path_table_crc);
			if (len > 0) {
				TRACEF("Transport::write_path_table: wrote %d entries, %d bytes", _path_table.size(), len);
				success = true;
			}
			else {
				ERROR("Transport::write_path_table: serialize failed");
			}
		}
#endif	// CUSTOM

		if (success) {
			DEBUGF("Saved %lu path table entries in %.3f seconds", _path_table.size(), OS::round(OS::time() - save_start, 3));
		}
	}
	catch (const std::exception& e) {
		ERRORF("Could not save path table to storage, the contained exception was: %s", e.what());
	}
#endif

	_saving_path_table = false;

	return success;
}

/*static*/ void Transport::read_tunnel_table() {
	DEBUG("Transport::read_tunnel_table");
#if defined(RNS_USE_FS) && RNS_PERSIST_PATHS
// TODO
/*p
		tunnel_table_path = RNS.Reticulum.storagepath+"/tunnels"
		if os.path.isfile(tunnel_table_path) and not Transport.owner.is_connected_to_shared_instance:
			serialised_tunnels = []
			try:
				file = open(tunnel_table_path, "rb")
				serialised_tunnels = umsgpack.unpackb(file.read())
				file.close()

				for serialised_tunnel in serialised_tunnels:
					tunnel_id = serialised_tunnel[0]
					interface_hash = serialised_tunnel[1]
					serialised_paths = serialised_tunnel[2]
					expires = serialised_tunnel[3]

					tunnel_paths = {}
					for serialised_entry in serialised_paths:
						destination_hash = serialised_entry[0]
						timestamp = serialised_entry[1]
						received_from = serialised_entry[2]
						hops = serialised_entry[3]
						expires = serialised_entry[4]
						random_blobs = serialised_entry[5]
						receiving_interface = Transport.find_interface_from_hash(serialised_entry[6])
						announce_packet = Transport.get_cached_packet(serialised_entry[7])

						if announce_packet != None:
							announce_packet.unpack()
							// We increase the hops, since reading a packet
							// from cache is equivalent to receiving it again
							// over an interface. It is cached with it's non-
							// increased hop-count.
							announce_packet.hops += 1

							tunnel_path = [timestamp, received_from, hops, expires, random_blobs, receiving_interface, announce_packet]
							tunnel_paths[destination_hash] = tunnel_path

					tunnel = [tunnel_id, None, tunnel_paths, expires]
					Transport.tunnels[tunnel_id] = tunnel

				if len(Transport.destination_table) == 1:
					specifier = "entry"
				else:
					specifier = "entries"

				RNS.log("Loaded "+str(len(Transport.tunnels))+" tunnel table "+specifier+" from storage", RNS.LOG_VERBOSE)

			except Exception as e:
				RNS.log("Could not load tunnel table from storage, the contained exception was: "+str(e), RNS.LOG_ERROR)
*/
#endif
}

/*static*/ void Transport::write_tunnel_table() {
#if defined(RNS_USE_FS) && RNS_PERSIST_PATHS
// TODO
/*p
	if not Transport.owner.is_connected_to_shared_instance:
		if hasattr(Transport, "saving_tunnel_table"):
			wait_interval = 0.2
			wait_timeout = 5
			wait_start = time.time()
			while Transport.saving_tunnel_table:
				time.sleep(wait_interval)
				if time.time() > wait_start+wait_timeout:
					RNS.log("Could not save tunnel table to storage, waiting for previous save operation timed out.", RNS.LOG_ERROR)
					return False

		try:
			Transport.saving_tunnel_table = True
			save_start = time.time()
			RNS.log("Saving tunnel table to storage...", RNS.LOG_DEBUG)

			serialised_tunnels = []
			for tunnel_id in Transport.tunnels:
				te = Transport.tunnels[tunnel_id]
				interface = te[1]
				tunnel_paths = te[2]
				expires = te[3]

				if interface != None:
					interface_hash = interface.get_hash()
				else:
					interface_hash = None

				serialised_paths = []
				for destination_hash in tunnel_paths:
					de = tunnel_paths[destination_hash]

					timestamp = de[0]
					received_from = de[1]
					hops = de[2]
					expires = de[3]
					random_blobs = de[4]
					packet_hash = de[6].get_hash()

					serialised_entry = [
						destination_hash,
						timestamp,
						received_from,
						hops,
						expires,
						random_blobs,
						interface_hash,
						packet_hash
					]

					serialised_paths.append(serialised_entry)

					Transport.cache(de[6], force_cache=True)


				serialised_tunnel = [tunnel_id, interface_hash, serialised_paths, expires]
				serialised_tunnels.append(serialised_tunnel)

			tunnels_path = RNS.Reticulum.storagepath+"/tunnels"
			file = open(tunnels_path, "wb")
			file.write(umsgpack.packb(serialised_tunnels))
			file.close()

			DEBUGF("Saved %lu tunnel table entries in %.3f seconds", serialised_tunnels.size(), OS::round(OS::time() - save_start))
		except Exception as e:
			ERRORF("Could not save tunnel table to storage, the contained exception was: %s", e.what())

		Transport.saving_tunnel_table = False
*/
#endif
}

/*static*/ void Transport::persist_data() {
	TRACE("Transport::persist_data()");
	write_path_table();
	write_tunnel_table();
}

/*static*/ void Transport::clean_caches() {

	// If currently cleaning caches then disregard
	if (cleaning_caches) {
		WARNING("Transport::clean_caches: already cleaning!");
		return;
	}
	cleaning_caches = true;

	TRACE("Transport::clean_caches()");
#if defined(RNS_USE_FS) && RNS_PERSIST_PATHS
	// CBA Remove cached packets no longer in path list
	std::list<std::string> remove_list;
	OS::list_directory(Reticulum::_cachepath, [&remove_list](const char* file_name) {
		TRACEF("Transport::clean_caches: Checking for use of cached packet %s", file_name);
		bool found = false;
		for (auto& [destination_hash, destination_entry] : _path_table) {
			if (strcasecmp(file_name, destination_entry.announce_packet_hash().toHex().c_str()) == 0) {
				found = true;
				break;
			}
		}
		if (!found) {
			remove_list.push_back(file_name);
		}
		//OS::reset_watchdog();
		OS::run_loop();
	});
    for (auto& file_name : remove_list) {
		TRACEF("Transport::clean_caches: No matching path found, removing cached packet %s", file_name.c_str());
		char packet_cache_path[Type::Reticulum::FILEPATH_MAXSIZE];
		snprintf(packet_cache_path, Type::Reticulum::FILEPATH_MAXSIZE, "%s/%s", Reticulum::_cachepath, file_name.c_str());
		OS::remove_file(packet_cache_path);
		//OS::reset_watchdog();
		OS::run_loop();
	}
#endif

	cleaning_caches = false;
}

/*static*/ void Transport::clear_storage() {
	TRACE("Transport::clear_storage()");
#if defined(RNS_USE_FS) && RNS_PERSIST_PATHS
	try {
		char file_path[Type::Reticulum::FILEPATH_MAXSIZE];

		snprintf(file_path, Type::Reticulum::FILEPATH_MAXSIZE, "%s/destination_table", Reticulum::_storagepath);
		if (OS::file_exists(file_path)) {
			OS::remove_file(file_path);
		}

		snprintf(file_path, Type::Reticulum::FILEPATH_MAXSIZE, "%s/tunnels", Reticulum::_storagepath);
		if (OS::file_exists(file_path)) {
			OS::remove_file(file_path);
		}

		// Clear the microStore-backed stores (removes their segment files on disk)
		_path_store.clear();
		_packet_hashlist.clear();

		// Remove cached announce packets
		if (OS::directory_exists(Reticulum::_cachepath)) {
			for (auto& file_name : OS::list_directory(Reticulum::_cachepath)) {
				char packet_cache_path[Type::Reticulum::FILEPATH_MAXSIZE];
				snprintf(packet_cache_path, Type::Reticulum::FILEPATH_MAXSIZE, "%s/%s", Reticulum::_cachepath, file_name.c_str());
				OS::remove_file(packet_cache_path);
				OS::run_loop();
			}
		}
	}
	catch (const std::exception& e) {
		ERRORF("Transport::clear_storage: failed to clear storage, the contained exception was: %s", e.what());
	}
#endif
}

/*static*/ void Transport::dump_stats() {

#ifdef RNS_DEBUG_HEAP
	Memory::dump_heap_stats();
#endif // RNS_DEBUG_HEAP

#ifdef RNS_DEBUG_MEMORY
	size_t memory = Memory::heap_available();
	uint8_t memory_pct = 0;
	size_t memory_size = Memory::heap_size();
	if (memory_size >0 ) memory_pct = (uint8_t)((double)memory / (double)memory_size * 100.0);
	if (_last_memory == 0) {
		_last_memory = memory;
	}

#if defined(ESP32)
	// CBA NOTE It appears that ESP.getFreePsram() may not accurately reflect available availabe PSRAM
	// because it appears to always decrease (even to zero) as if PSRAM is being leaked. Maybe it doesn't
	// accurately reflect PSRAM freed with calls to free()???
	size_t psram = ESP.getFreePsram();
	uint8_t psram_pct = 0;
	size_t psram_size = ESP.getPsramSize();
	if (psram_size > 0) psram_pct = (uint8_t)((double)psram / (double)psram_size * 100.0);
	if (_last_psram == 0) {
		_last_psram = psram;
	}
#else
	size_t psram = 0;
	uint8_t psram_pct = 0;
#endif

	size_t flash = OS::storage_available();
	if (_last_flash == 0) {
		_last_flash = flash;
	}
	uint8_t flash_pct = 0;
	size_t flash_size = OS::storage_size();
	if (flash_size > 0) flash_pct = (uint8_t)((double)flash / (double)flash_size * 100.0);

	// memory
	// storage
	HEADF(LOG_VERBOSE, "sram: %u (%u%%) [%d] psram: %u (%u%%) [%d] flash: %u (%u%%) [%d]", memory, memory_pct, memory - _last_memory, psram, psram_pct, psram - _last_psram, flash, flash_pct, flash - _last_flash);
#endif // RNS_DEBUG_MEMORY

#ifdef RNS_DEBUG_METRICS
	// _destinations
	// _path_table
	// _reverse_table
	// _announce_table
	// _held_announces
	HEADF(LOG_VERBOSE, "paths: %u dsts: %u revr: %u annc: %u held: %u", _new_path_table.size(), _destinations.size(), _reverse_table.size(), _announce_table.size(), _held_announces.size());

	// _path_requests
	// _discovery_path_requests
	// _pending_local_path_requests
	// _discovery_pr_tags
	// _control_destinations
	// _control_hashes
	VERBOSEF("preqs: %u dpreqs: %u ppreqs: %u dprt: %u cdsts: %u chshs: %u", _path_requests.size(), _discovery_path_requests.size(), _pending_local_path_requests.size(), _discovery_pr_tags.size(), _control_destinations.size(), _control_hashes.size());

	// _packet_hashlist
	// _receipts
	// _link_table
	// _pending_links
	// _active_links
	// _tunnels
	uint32_t destination_path_responses = 0;
	for (auto& [destination_hash, destination] : _destinations) {
		destination_path_responses += destination.path_responses().size();
	}
	uint32_t interface_announces = 0;
	for (auto& interface : _interfaces) {
		interface_announces += interface.announce_queue().size();
	}
	VERBOSEF("phl: %u rcp: %u lt: %u pl: %u al: %u tun: %u", _packet_hashlist.size(), _receipts.size(), _link_table.size(), _pending_links.size(), _active_links.size(), _tunnels.size());
	VERBOSEF("pin: %u pout: %u padd: %u pupd: %u pfail: %u dpr: %u ikd: %u ia: %u\r\n", _packets_received, _packets_sent, _paths_added, _paths_updated, _paths_failed, destination_path_responses, Identity::known_destinations().size(), interface_announces);
#endif // RNS_DEBUG_METRICS

#ifdef RNS_DEBUG_MEMORY
	_last_memory = memory;
	_last_psram = psram;
	_last_flash = flash;
#endif // RNS_DEBUG_MEMORY

#ifdef RNS_DEBUG_PATHSTORE
	if (_path_store) {
		HEAD("Path Store Stats", LOG_TRACE);
		_path_store.dumpInfo();
	}
#endif // RNS_DEBUG_PATHSTORE

}

/*p
    @staticmethod
    def void_queues():
        Transport.held_announces = {}
        Transport.receipts = []
        Transport.reverse_table = {}
*/

/*static*/ void Transport::exit_handler() {
	TRACE("Transport::exit_handler()");
	if (!_owner.is_connected_to_shared_instance()) {
		persist_data();
	}
	detach_interfaces();
}

/*p

    @staticmethod
    def blackhole_identity(identity_hash, until=None, reason=None):
        try:
            if not identity_hash in Transport.blackholed_identities:
                entry = {"source": Transport.identity.hash, "until": until, "reason": reason}
                Transport.blackholed_identities[identity_hash] = entry
                Transport.persist_blackhole()
                Transport.remove_blackholed_paths()
                RNS.log(f"Blackholed identity {RNS.prettyhexrep(identity_hash)}", RNS.LOG_INFO)
                return True

            else: return None

        except Exception as e:
            RNS.log(f"Error while blackholing identity: {e}", RNS.LOG_ERROR)
            return False

    @staticmethod
    def unblackhole_identity(identity_hash):
        try:
            if identity_hash in Transport.blackholed_identities:
                Transport.blackholed_identities.pop(identity_hash)
                Transport.persist_blackhole()
                RNS.log(f"Lifted blackhole for identity {RNS.prettyhexrep(identity_hash)}", RNS.LOG_INFO)
                return True

            else: return None

        except Exception as e:
            RNS.log(f"Error while unblackholing identity: {e}", RNS.LOG_ERROR)
            return False

    @staticmethod
    def reload_blackhole():
        now = time.time()
        source_count = 0
        dest_len = (RNS.Reticulum.TRUNCATED_HASHLENGTH//8)*2
        for filename in os.listdir(RNS.Reticulum.blackholepath):
            try:
                if filename == "local": source_identity_hash = Transport.identity.hash
                else:
                    if len(filename) != dest_len: raise ValueError(f"Identity hash length for blackhole source {filename} is invalid")
                    source_identity_hash = bytes.fromhex(filename)
                    if not source_identity_hash in RNS.Reticulum.blackhole_sources():
                        RNS.log(f"Skipping disabled blackhole source {RNS.prettyhexrep(source_identity_hash)}", RNS.LOG_VERBOSE) if RNS.sl(RNS.LOG_VERBOSE) else None
                        continue

                sourcepath = os.path.join(RNS.Reticulum.blackholepath, filename)
                with open(sourcepath, "rb") as f:
                    packed = f.read()
                    source_list = umsgpack.unpackb(packed)
                    for identity_hash in source_list:
                        if len(identity_hash) == RNS.Reticulum.TRUNCATED_HASHLENGTH//8:
                            if identity_hash in Transport.blackholed_identities:
                                if Transport.blackholed_identities[identity_hash]["source"] == Transport.identity.hash:
                                    continue

                            se        = source_list[identity_hash]
                            until     = se["until"] if "until" in se else None
                            reason    = se["reason"] if "reason" in se else None
                            entry     = {"source": source_identity_hash, "until": until, "reason": reason}

                            if until == None or now < until: Transport.blackholed_identities[identity_hash] = entry

                source_count += 1

            except Exception as e:
                RNS.log(f"Could not load blackholed identities from source file {filename}: {e}", RNS.LOG_ERROR)
                RNS.trace_exception(e)

        Transport.remove_blackholed_paths()

    def remove_blackholed_paths():
        drop_destinations = []
        for destination_hash in Transport.path_table.copy():
            try:
                associated_identity = RNS.Identity.recall(destination_hash, _no_use=True)
                if associated_identity and associated_identity.hash in Transport.blackholed_identities:
                    if not destination_hash in drop_destinations: drop_destinations.append(destination_hash)
            except Exception as e:
                RNS.log(f"Error while enumerating blackhole-associated destinations: {e}", RNS.LOG_ERROR)

        for destination_hash in drop_destinations:
            try:
                with Transport.path_table_lock:
                    if destination_hash in Transport.path_table: Transport.path_table.pop(destination_hash)
            except Exception as e:
                RNS.log(f"Error while dropping blackhole-associated destination from path table: {e}", RNS.LOG_ERROR)

        if len(drop_destinations) > 0:
            ms = "" if len(drop_destinations) == 1 else "s"
            RNS.log(f"Removed {len(drop_destinations)} destination{ms} associated with blackholed identities from path table", RNS.LOG_INFO)

    @staticmethod
    def blackhole_list_handler(path, data, request_id, link_id, remote_identity, requested_at):
        try: return Transport.blackholed_identities
        except Exception as e:
            RNS.log("An error occurred while processing blackhole list request from "+str(remote_identity), RNS.LOG_ERROR)
            RNS.log("The contained exception was: "+str(e), RNS.LOG_ERROR)

        return None

    @staticmethod
    def persist_blackhole():
        try:
            local_blackhole = {}
            for identity_hash in Transport.blackholed_identities:
                if Transport.blackholed_identities[identity_hash]["source"] == Transport.identity.hash:
                    local_blackhole[identity_hash] = Transport.blackholed_identities[identity_hash]

            packed = umsgpack.packb(local_blackhole)
            localpath = os.path.join(RNS.Reticulum.blackholepath, "local")
            tmppath = f"{localpath}.tmp"
            with open(tmppath, "wb") as f: f.write(packed)
            if os.path.isfile(localpath): os.unlink(localpath)
            os.rename(tmppath, localpath)

        except Exception as e:
            RNS.log(f"Error while persisting blackhole list: {e}", RNS.LOG_ERROR)
            RNS.trace_exception(e)
*/

/*static*/ Destination Transport::find_destination_from_hash(const Bytes& destination_hash) {
	TRACEF("Transport::find_destination_from_hash: Searching for destination %s", destination_hash.toHex().c_str());
	auto iter = _destinations.find(destination_hash);
	if (iter != _destinations.end()) {
		TRACEF("Transport::find_destination_from_hash: Found destination %s", (*iter).second.toString().c_str());
		return (*iter).second;
	}

	return {Type::NONE};
}

/*static*/ Packet Transport::find_announce_packet_from_hash(const Bytes& destination_hash) {
	TRACEF("Transport::find_announce_packet_from_hash: Searching for announce packet for destination %s", destination_hash.toHex().c_str());
	DestinationEntry destination_entry;
	if (_new_path_table.get(destination_hash, destination_entry)) {
		return destination_entry.announce_packet();
	}

	return {Type::NONE};
}

/*static*/ void Transport::cull_path_table() {
	TRACE("Transport::cull_path_table()");
	if (_path_table.size() > _path_table_maxsize) {
		try {
			// Build lightweight (timestamp, key) index to avoid copying full DestinationEntry
			// objects (which contain nested std::set<Bytes>) — prevents OOM on heap-constrained
			// devices when the table hits max capacity.
			std::vector<std::pair<double, Bytes>> sorted_keys;
			sorted_keys.reserve(_path_table.size());
			for (const auto& [key, entry] : _path_table) {
				sorted_keys.emplace_back(entry._timestamp, key);
			}
			// Sort ascending by timestamp so oldest entries are removed first
			std::sort(sorted_keys.begin(), sorted_keys.end());

			uint16_t count = 0;
			for (const auto& [timestamp, destination_hash] : sorted_keys) {
				TRACEF("Transport::cull_path_table: Removing destination %s from path table", destination_hash.toHex().c_str());
#if defined(RNS_USE_FS) && RNS_PERSIST_PATHS
				// CBA microStore
				//auto& destination_entry = get_path(destination_hash);
				DestinationEntry destination_entry;
				_new_path_table.get(destination_hash, destination_entry);
				if (destination_entry) {
					// Remove cached packet file associated with this destination
					char packet_cache_path[Type::Reticulum::FILEPATH_MAXSIZE];
					snprintf(packet_cache_path, Type::Reticulum::FILEPATH_MAXSIZE, "%s/%s", Reticulum::_cachepath, destination_entry.announce_packet_hash().toHex().c_str());
					if (OS::file_exists(packet_cache_path)) {
						OS::remove_file(packet_cache_path);
					}
				}
#endif
				if (_path_table.erase(destination_hash) < 1) {
					WARNINGF("Failed to remove destination %s from path table", destination_hash.toHex().c_str());
				}
				++count;
				if (_path_table.size() <= _path_table_maxsize) {
					break;
				}
			}
			DEBUGF("Removed %d path(s) from path table", count);
		}
		catch (const std::bad_alloc& e) {
			ERROR("cull_path_table: bad_alloc - out of memory building sort index, falling back to single erase");
			// Fallback: std::min_element does no heap allocation — erase one oldest entry
			auto oldest = std::min_element(
				_path_table.begin(), _path_table.end(),
				[](const std::pair<const Bytes, DestinationEntry>& a,
			   const std::pair<const Bytes, DestinationEntry>& b) {
				return a.second._timestamp < b.second._timestamp;
			}
			);
			if (oldest != _path_table.end()) {
				_path_table.erase(oldest);
			}
		}
		catch (const std::exception& e) {
			ERRORF("cull_path_table: exception: %s", e.what());
		}
	}
}

/*static*/ void Transport::cull_announce_table() {
	TRACE("Transport::cull_announce_table()");
	if (_announce_table.size() > _announce_table_maxsize) {
		try {
			// Build lightweight (timestamp, key) index to avoid copying full AnnounceEntry
			// objects (which contain nested std::set<Bytes>) — prevents OOM on heap-constrained
			// devices when the table hits max capacity.
			std::vector<std::pair<double, Bytes>> sorted_keys;
			sorted_keys.reserve(_announce_table.size());
			for (const auto& [key, entry] : _announce_table) {
				sorted_keys.emplace_back(entry._timestamp, key);
			}
			// Sort ascending by timestamp so oldest entries are removed first
			std::sort(sorted_keys.begin(), sorted_keys.end());

			uint16_t count = 0;
			for (const auto& [timestamp, destination_hash] : sorted_keys) {
				TRACEF("Transport::cull_announce_table: Removing destination %s from path table", destination_hash.toHex().c_str());
				if (_announce_table.erase(destination_hash) < 1) {
					WARNINGF("Failed to remove destination %s from path table", destination_hash.toHex().c_str());
				}
				++count;
				if (_announce_table.size() <= _path_table_maxsize) {
					break;
				}
			}
			DEBUGF("Removed %d path(s) from path table", count);
		}
		catch (const std::bad_alloc& e) {
			ERROR("cull_announce_table: bad_alloc - out of memory building sort index, falling back to single erase");
			// Fallback: std::min_element does no heap allocation — erase one oldest entry
			auto oldest = std::min_element(
				_announce_table.begin(), _announce_table.end(),
				[](const std::pair<const Bytes, AnnounceEntry>& a,
			   const std::pair<const Bytes, AnnounceEntry>& b) {
				return a.second._timestamp < b.second._timestamp;
			}
			);
			if (oldest != _announce_table.end()) {
				_announce_table.erase(oldest);
			}
		}
		catch (const std::exception& e) {
			ERRORF("cull_announce_table: exception: %s", e.what());
		}
	}
}

/*static*/ uint16_t Transport::remove_reverse_entries(const std::vector<Bytes>& hashes) {
	uint16_t count = 0;
	for (const auto& truncated_packet_hash : hashes) {
		_reverse_table.erase(truncated_packet_hash);
		++count;
	}
	if (count > 0) {
		TRACEF("Released %u reverse table entries", count);
	}
	return count;
}

/*static*/ uint16_t Transport::remove_links(const std::vector<Bytes>& hashes) {
	uint16_t count = 0;
	for (const auto& link_id : hashes) {
		_link_table.erase(link_id);
		++count;
	}
	if (count > 0) {
		TRACEF("Released %u links", count);
	}
	return count;
}

/*static*/ uint16_t Transport::remove_paths(const std::vector<Bytes>& hashes) {
	uint16_t count = 0;
	for (const auto& destination_hash : hashes) {
		//_path_table.erase(destination_hash);
		remove_path(destination_hash);
		++count;
	}
	if (count > 0) {
		TRACEF("Released %u paths", count);
	}
	return count;
}

/*static*/ uint16_t Transport::remove_discovery_path_requests(const std::vector<Bytes>& hashes) {
	uint16_t count = 0;
	for (const auto& destination_hash : hashes) {
		_discovery_path_requests.erase(destination_hash);
		++count;
	}
	if (count > 0) {
		TRACEF("Released %u waiting path requests", count);
	}
	return count;
}

/*static*/ uint16_t Transport::remove_tunnels(const std::vector<Bytes>& hashes) {
	uint16_t count = 0;
	for (const auto& tunnel_id : hashes) {
		_tunnels.erase(tunnel_id);
		++count;
	}
	if (count > 0) {
		TRACEF("Released %u tunnels", count);
	}
	return count;
}

/*static*/ void Transport::set_identity_prv(const Bytes& prv_bytes) {
	_identity = RNS::Identity(false);
	_identity.load_private_key(prv_bytes);
}

/*static*/ Bytes Transport::get_identity_prv() {
	RNS::Bytes prv_bytes;
	return _identity.get_private_key();
}