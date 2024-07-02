#include "Transport.h"

#include "Reticulum.h"
#include "Destination.h"
#include "Identity.h"
#include "Packet.h"
#include "Interface.h"
#include "Log.h"
#include "Cryptography/Random.h"
#include "Utilities/OS.h"
#include "Utilities/Persistence.h"

#include <algorithm>
#include <unistd.h>
#include <time.h>

using namespace RNS;
using namespace RNS::Type::Transport;
using namespace RNS::Utilities;

// Imported from Crypto.cpp
extern uint8_t crypto_crc8(uint8_t tag, const void *data, unsigned size);

#if defined(INTERFACES_SET)
///*static*/ std::set<std::reference_wrapper<const Interface>, std::less<const Interface>> Transport::_interfaces;
/*static*/ std::set<std::reference_wrapper<Interface>, std::less<Interface>> Transport::_interfaces;
#elif defined(INTERFACES_LIST)
/*static*/ std::list<std::reference_wrapper<Interface>> Transport::_interfaces;
#elif defined(INTERFACES_MAP)
/*static*/ std::map<Bytes, Interface&> Transport::_interfaces;
#endif
#if defined(DESTINATIONS_SET)
/*static*/ std::set<Destination> Transport::_destinations;
#elif defined(DESTINATIONS_MAP)
/*static*/ std::map<Bytes, Destination> Transport::_destinations;
#endif
/*static*/ std::set<Link> Transport::_pending_links;
/*static*/ std::set<Link> Transport::_active_links;
/*static*/ std::set<Bytes> Transport::_packet_hashlist;
/*static*/ std::list<PacketReceipt> Transport::_receipts;

/*static*/ std::map<Bytes, Transport::AnnounceEntry> Transport::_announce_table;
/*static*/ std::map<Bytes, Transport::DestinationEntry> Transport::_destination_table;
/*static*/ std::map<Bytes, Transport::ReverseEntry> Transport::_reverse_table;
/*static*/ std::map<Bytes, Transport::LinkEntry> Transport::_link_table;
/*static*/ std::map<Bytes, Transport::AnnounceEntry> Transport::_held_announces;
/*static*/ std::set<HAnnounceHandler> Transport::_announce_handlers;
/*static*/ std::map<Bytes, Transport::TunnelEntry> Transport::_tunnels;
/*static*/ std::map<Bytes, double> Transport::_path_requests;

/*static*/ std::map<Bytes, Transport::PathRequestEntry> Transport::_discovery_path_requests;
/*static*/ std::set<Bytes> Transport::_discovery_pr_tags;

/*static*/ std::set<Destination> Transport::_control_destinations;
/*static*/ std::set<Bytes> Transport::_control_hashes;

///*static*/ std::set<Interface> Transport::_local_client_interfaces;
/*static*/ std::set<std::reference_wrapper<const Interface>, std::less<const Interface>> Transport::_local_client_interfaces;

/*static*/ std::map<Bytes, const Interface&> Transport::_pending_local_path_requests;

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
/*static*/ double Transport::_tables_last_culled		= 0.0;
// CBA MCU
/*static*/ //float Transport::_tables_cull_interval		= 5.0;
/*static*/ float Transport::_tables_cull_interval		= 60.0;
/*static*/ bool Transport::_saving_path_table			= false;
// CBA MCU
/*static*/ //uint16_t Transport::_hashlist_maxsize		= 1000000;
/*static*/ uint16_t Transport::_hashlist_maxsize		= 10000;
// CBA MCU
/*static*/ //uint16_t Transport::_max_pr_tags			= 32000;
/*static*/ uint16_t Transport::_max_pr_tags				= 320;

// CBA
/*static*/ uint16_t Transport::_path_table_maxsize		= 10;
/*static*/ uint16_t Transport::_path_table_maxpersist	= 10;
/*static*/ double Transport::_last_saved				= 0.0;
/*static*/ float Transport::_save_interval				= 3600.0;
/*static*/ uint8_t Transport::_destination_table_crc	= 0;

/*static*/ Reticulum Transport::_owner({Type::NONE});
/*static*/ Identity Transport::_identity({Type::NONE});

// CBA
/*static*/ Transport::Callbacks Transport::_callbacks;

// CBA Stats
/*static*/ uint32_t Transport::_packets_sent = 0;
/*static*/ uint32_t Transport::_packets_received = 0;
/*static*/ uint32_t Transport::_destinations_added = 0;

/*static*/ void Transport::start(const Reticulum& reticulum_instance) {
	info("Transport starting...");
	_jobs_running = true;
	_owner = reticulum_instance;

	// Initialize time-based variables *after* time offset update
	_jobs_last_run = OS::time();
	_links_last_checked = OS::time();
	_receipts_last_checked = OS::time();
	_announces_last_checked = OS::time();
	_tables_last_culled = OS::time();
	_last_saved = OS::time();

	// ensure required directories exist
	if (!OS::directory_exists(Reticulum::_cachepath.c_str())) {
		verbose("No cache directory, creating...");
		OS::create_directory(Reticulum::_cachepath.c_str());
	}

	if (!_identity) {
		std::string transport_identity_path = Reticulum::_storagepath + "/transport_identity";
		DEBUG("Checking for transport identity...");
		try {
// CBA TEST
//OS::remove_file(transport_identity_path.c_str());
			if (OS::file_exists(transport_identity_path.c_str())) {
				_identity = Identity::from_file(transport_identity_path.c_str());
			}

			if (!_identity) {
				verbose("No valid Transport Identity in storage, creating...");
				_identity = Identity();
				_identity.to_file(transport_identity_path.c_str());
			}
			else {
				verbose("Loaded Transport Identity from storage");
			}
		}
		catch (std::exception& e) {
			error("Failed to check for transport identity, the contained exception was: " + std::string(e.what()));
		}
	}

// TODO
/*
	packet_hashlist_path = Reticulum::storagepath + "/packet_hashlist";
	if (!owner.is_connected_to_shared_instance()) {
		if (os.path.isfile(packet_hashlist_path)) {
			try {
				file = open(packet_hashlist_path, "rb");
				packet_hashlist = umsgpack.unpackb(file.read());
				file.close();
			}
			catch (std::exception& e) {
				error("Could not load packet hashlist from storage, the contained exception was: " + std::string(e.what()));
			}
		}
	}
*/

	// Create transport-specific destination for path request
	Destination path_request_destination({Type::NONE}, Type::Destination::IN, Type::Destination::PLAIN, APP_NAME, "path.request");
	path_request_destination.set_packet_callback(path_request_handler);
	_control_destinations.insert(path_request_destination);
	_control_hashes.insert(path_request_destination.hash());
	DEBUG("Created transport-specific path request destination " + path_request_destination.hash().toHex());

	// Create transport-specific destination for tunnel synthesize
	Destination tunnel_synthesize_destination({Type::NONE}, Type::Destination::IN, Type::Destination::PLAIN, APP_NAME, "tunnel.synthesize");
	tunnel_synthesize_destination.set_packet_callback(tunnel_synthesize_handler);
	// CBA BUG?
    //p Transport.control_destinations.append(Transport.tunnel_synthesize_handler)
	_control_destinations.insert(tunnel_synthesize_destination);
	_control_hashes.insert(tunnel_synthesize_destination.hash());
	DEBUG("Created transport-specific tunnel synthesize destination " + tunnel_synthesize_destination.hash().toHex());

	_jobs_running = false;

	// CBA Threading
	//p thread = threading.Thread(target=Transport.jobloop, daemon=True)
	//p thread.start()

	if (Reticulum::transport_enabled()) {
		info("Transport mode is enabled");

		// Read in path table and then write and clean in case any entries are invalid
		read_path_table();
		write_path_table();
		clean_caches();

		read_tunnel_table();

		// Create transport-specific destination for probe requests
		if (Reticulum::probe_destination_enabled()) {
			Destination probe_destination(_identity, Type::Destination::IN, Type::Destination::SINGLE, APP_NAME, "probe");
			probe_destination.accepts_links(false);
			probe_destination.set_proof_strategy(Type::Destination::PROVE_ALL);
			DEBUG("Created probe responder destination " + probe_destination.hash().toHex());
			probe_destination.announce();
			notice("Transport Instance will respond to probe requests on " + probe_destination.toString());
		}

		verbose("Transport instance " + _identity.toString() + " started");
		_start_time = OS::time();
	}

// TODO
/*
	// Synthesize tunnels for any interfaces wanting it
	for interface in Transport.interfaces:
		interface.tunnel_id = None
		if hasattr(interface, "wants_tunnel") and interface.wants_tunnel:
			Transport.synthesize_tunnel(interface)
*/

#ifndef NDEBUG
	// CBA DEBUG
	dump_stats();
#endif
}

/*static*/ void Transport::loop() {
	if (OS::time() > (_jobs_last_run + _job_interval)) {
		jobs();
		_jobs_last_run = OS::time();
	}
}

/*static*/ void Transport::jobs() {

	std::vector<Packet> outgoing;
	std::set<Bytes> path_requests;
	int count;
	_jobs_running = true;

	try {
		if (!_jobs_locked) {

			// Process active and pending link lists
			if (OS::time() > (_links_last_checked + _links_check_interval)) {
				for (auto& link : _pending_links) {
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
									DEBUG("Trying to rediscover path for " + link.destination().hash().toHex() + " since an attempted link was never established");
									//if (path_requests.find(link.destination().hash()) == path_requests.end()) {
									if (path_requests.count(link.destination().hash()) == 0) {
										path_requests.insert(link.destination().hash());
									}
								}
							}
						}

						_pending_links.erase(link);
					}
				}
				for (auto& link : _active_links) {
					if (link.status() == Type::Link::CLOSED) {
						_active_links.erase(link);
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
				for (auto& receipt : _receipts) {
					cull_receipts.remove(receipt);
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
//TRACE("[0] announce entry data size: " + std::to_string(announce_entry._packet.data().size()));
					//p announce_entry = Transport.announce_table[destination_hash]
					if (announce_entry._retries > Type::Transport::PATHFINDER_R) {
						TRACE("Completed announce processing for " + destination_hash.toHex() + ", retry limit reached");
						// CBA OK to modify collection here since we're immediately exiting iteration
						_announce_table.erase(destination_hash);
						break;
					}
					else {
						if (OS::time() > announce_entry._retransmit_timeout) {
							TRACE("Performing announce processing for " + destination_hash.toHex() + "...");
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
//TRACE("[1] interface: " + announce_entry._attached_interface.debugString());
//TRACE("[1] interface: " + announce_entry._attached_interface.toString());
//}
							Packet new_packet(
								announce_destination,
								//{Type::NONE},
								announce_entry._attached_interface,
								//{Type::NONE},
								announce_entry._packet.data(),
								Type::Packet::ANNOUNCE,
								announce_context,
								Type::Transport::TRANSPORT,
								Type::Packet::HEADER_2,
								Transport::_identity.hash()
							);

							new_packet.hops(announce_entry._hops);
							if (announce_entry._block_rebroadcasts) {
								DEBUG("Rebroadcasting announce as path response for " + announce_destination.hash().toHex() + " with hop count " + std::to_string(new_packet.hops()));
							}
							else {
								DEBUG("Rebroadcasting announce for " + announce_destination.hash().toHex() + " with hop count " + std::to_string(new_packet.hops()));
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
								_announce_table.insert({destination_hash, held_entry});
								DEBUG("Reinserting held announce into table");
							}
						}
					}
				}

				_announces_last_checked = OS::time();
			}

			// Cull the packet hashlist if it has reached its max size
			if (_packet_hashlist.size() > _hashlist_maxsize) {
				std::set<Bytes>::iterator iter = _packet_hashlist.begin();
				std::advance(iter, _packet_hashlist.size() - _hashlist_maxsize);
				_packet_hashlist.erase(_packet_hashlist.begin(), iter);
			}

			// Cull the path request tags list if it has reached its max size
			if (_discovery_pr_tags.size() > _max_pr_tags) {
				std::set<Bytes>::iterator iter = _discovery_pr_tags.begin();
				std::advance(iter, _discovery_pr_tags.size() - _max_pr_tags);
				_discovery_pr_tags.erase(_discovery_pr_tags.begin(), iter);
			}

			// Cull the path table if it has reached its max size
			cull_path_table();

			if (OS::time() > (_tables_last_culled + _tables_cull_interval)) {

				// Cull the reverse table according to timeout
				std::vector<Bytes> stale_reverse_entries;
				for (const auto& [packet_hash, reverse_entry] : _reverse_table) {
					if (OS::time() > (reverse_entry._timestamp + REVERSE_TIMEOUT)) {
						stale_reverse_entries.push_back(packet_hash);
					}
				}

				// Cull the link table according to timeout
				std::vector<Bytes> stale_links;
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
							
							// If the path has been invalidated between the time of
							// making the link request and now, try to rediscover it
							if (!has_path(link_entry._destination_hash)) {
								DEBUG("Trying to rediscover path for " + link_entry._destination_hash.toHex() + " since an attempted link was never established, and path is now missing");
								path_request_conditions = true;
							}

							// If this link request was originated from a local client
							// attempt to rediscover a path to the destination, if this
							// has not already happened recently.
							else if (!path_request_throttle && lr_taken_hops == 0) {
								DEBUG("Trying to rediscover path for " + link_entry._destination_hash.toHex() + " since an attempted local client link was never established");
								path_request_conditions = true;
							}

							// If the link destination was previously only 1 hop
							// away, this likely means that it was local to one
							// of our interfaces, and that it roamed somewhere else.
							// In that case, try to discover a new path.
							else if (!path_request_throttle && hops_to(link_entry._destination_hash) == 1) {
								DEBUG("Trying to rediscover path for " + link_entry._destination_hash.toHex() + " since an attempted link was never established, and destination was previously local to an interface on this instance");
								path_request_conditions = true;
							}

							// If the link destination was previously only 1 hop
							// away, this likely means that it was local to one
							// of our interfaces, and that it roamed somewhere else.
							// In that case, try to discover a new path.
							else if ( !path_request_throttle and lr_taken_hops == 1) {
								DEBUG("Trying to rediscover path for " + link_entry._destination_hash.toHex() + " since an attempted link was never established, and link initiator is local to an interface on this instance");
								path_request_conditions = true;
							}

							if (path_request_conditions) {
								if (path_requests.count(link_entry._destination_hash) == 0) {
									path_requests.insert(link_entry._destination_hash);
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

				// Cull the path table
				std::vector<Bytes> stale_paths;
				for (const auto& [destination_hash, destination_entry] : _destination_table) {
					const Interface& attached_interface = destination_entry.receiving_interface();
					double destination_expiry;
					if (attached_interface && attached_interface.mode() == Type::Interface::MODE_ACCESS_POINT) {
						destination_expiry = destination_entry._timestamp + AP_PATH_TIME;
					}
					else if (attached_interface && attached_interface.mode() == Type::Interface::MODE_ROAMING) {
						destination_expiry = destination_entry._timestamp + ROAMING_PATH_TIME;
					}
					else {
						destination_expiry = destination_entry._timestamp + DESTINATION_TIMEOUT;
					}

					if (OS::time() > destination_expiry) {
						stale_paths.push_back(destination_hash);
						DEBUG("Path to " + destination_hash.toHex() + " timed out and was removed");
					}
					else if (_interfaces.count(attached_interface.get_hash()) == 0) {
						stale_paths.push_back(destination_hash);
						DEBUG("Path to " + destination_hash.toHex() + " was removed since the attached interface no longer exists");
					}
				}

				// Cull the pending discovery path requests table
				std::vector<Bytes> stale_discovery_path_requests;
				for (const auto& [destination_hash, path_entry] : _discovery_path_requests) {
					if (OS::time() > path_entry._timeout) {
						stale_discovery_path_requests.push_back(destination_hash);
						DEBUG("Waiting path request for " + destination_hash.toString() + " timed out and was removed");
					}
				}

				// Cull the tunnel table
				count = 0;
				std::vector<Bytes> stale_tunnels;
				for (const auto& [tunnel_id, tunnel_entry] : _tunnels) {
					if (OS::time() > tunnel_entry._expires) {
						stale_tunnels.push_back(tunnel_id);
						TRACE("Tunnel " + tunnel_id.toHex() + " timed out and was removed");
					}
					else {
						std::vector<Bytes> stale_tunnel_paths;
						for (const auto& [destination_hash, destination_entry] : tunnel_entry._serialised_paths) {
							if (OS::time() > (destination_entry._timestamp + DESTINATION_TIMEOUT)) {
								stale_tunnel_paths.push_back(destination_hash);
								TRACE("Tunnel path to " + destination_hash.toHex() + " timed out and was removed");
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
					TRACE("Removed " + std::to_string(count) + " tunnel paths");
				}
				
				count = 0;
				for (const auto& truncated_packet_hash : stale_reverse_entries) {
					_reverse_table.erase(truncated_packet_hash);
					++count;
				}
				if (count > 0) {
					TRACE("Released " + std::to_string(count) + " reverse table entries");
				}

				count = 0;
				for (const auto& link_id : stale_links) {
					_link_table.erase(link_id);
					++count;
				}
				if (count > 0) {
					TRACE("Released " + std::to_string(count) + " links");
				}

				count = 0;
				for (const auto& destination_hash : stale_paths) {
					//_destination_table.erase(destination_hash);
					remove_path(destination_hash);
					++count;
				}
				if (count > 0) {
					TRACE("Removed " + std::to_string(count) + " paths");
				}

				count = 0;
				for (const auto& destination_hash : stale_discovery_path_requests) {
					_discovery_path_requests.erase(destination_hash);
					++count;
				}
				if (count > 0) {
					TRACE("Removed " + std::to_string(count) + " waiting path requests");
				}

				count = 0;
				for (const auto& tunnel_id : stale_tunnels) {
					_tunnels.erase(tunnel_id);
					++count;
				}
				if (count > 0) {
					TRACE("Removed " + std::to_string(count) + " tunnels");
				}

#ifndef NDEBUG
				dump_stats();
#endif

				_tables_last_culled = OS::time();
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
	catch (std::exception& e) {
		error("An exception occurred while running Transport jobs.");
		error("The contained exception was: " + std::string(e.what()));
	}

	_jobs_running = false;

	// CBA send announce retransmission packets
	for (auto& packet : outgoing) {
		packet.send();
	}

	// CBA send link-related path requests
	for (auto& destination_hash : path_requests) {
		request_path(destination_hash);
	}
}

/*static*/ void Transport::transmit(Interface& interface, const Bytes& raw) {
	TRACE("Transport::transmit()");
	// CBA
	if (_callbacks._transmit_packet) {
		try {
			_callbacks._transmit_packet(raw, interface);
		}
		catch (std::exception& e) {
			DEBUG("Error while executing transmit packet callback. The contained exception was: " + std::string(e.what()));
		}
	}
	try {
		//if hasattr(interface, "ifac_identity") and interface.ifac_identity != None:
		if (interface.ifac_identity()) {
// TODO
/*
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
			interface.on_outgoing(masked_raw)
*/
		}
		else {
			//interface.on_outgoing(raw);
			interface.process_outgoing(raw);
		}
	}
	catch (std::exception& e) {
		error("Error while transmitting on " + interface.toString() + ". The contained exception was: " + e.what());
	}
}

/*static*/ bool Transport::outbound(Packet& packet) {
	TRACE("Transport::outbound()");
	++_packets_sent;

	if (!packet.destination()) {
		//throw std::invalid_argument("Can not send packet with no destination.");
		error("Can not send packet with no destination");
		return false;
	}

	TRACE("Transport::outbound: destination=" + packet.destination_hash().toHex() + " hops=" + std::to_string(packet.hops()));

	while (_jobs_running) {
		TRACE("Transport::outbound: sleeping...");
		OS::sleep(0.0005);
	}

	_jobs_locked = true;

	bool sent = false;
	double outbound_time = OS::time();

	// Check if we have a known path for the destination in the path table
    //if packet.packet_type != RNS.Packet.ANNOUNCE and packet.destination.type != RNS.Destination.PLAIN and packet.destination.type != RNS.Destination.GROUP and packet.destination_hash in Transport.destination_table:
	if (packet.packet_type() != Type::Packet::ANNOUNCE && packet.destination().type() != Type::Destination::PLAIN && packet.destination().type() != Type::Destination::GROUP && _destination_table.find(packet.destination_hash()) != _destination_table.end()) {
		TRACE("Transport::outbound: Path to destination is known");
        //outbound_interface = Transport.destination_table[packet.destination_hash][5]
		DestinationEntry destination_entry = (*_destination_table.find(packet.destination_hash())).second;
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
				Bytes new_raw;
				//new_raw = struct.pack("!B", new_flags)
				new_raw << new_flags;
				//new_raw += packet.raw[1:2]
				new_raw << packet.raw().mid(1,1);
				//new_raw += Transport.destination_table[packet.destination_hash][1]
				new_raw << destination_entry._received_from;
				//new_raw += packet.raw[2:]
				new_raw << packet.raw().mid(2);
				transmit(outbound_interface, new_raw);
				//_destination_table[packet.destination_hash][0] = time.time()
				destination_entry._timestamp = OS::time();
				sent = true;
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
				Bytes new_raw;
				//new_raw = struct.pack("!B", new_flags)
				new_raw << new_flags;
				//new_raw += packet.raw[1:2]
				new_raw << packet.raw().mid(1, 1);
				//new_raw += Transport.destination_table[packet.destination_hash][1]
				new_raw << destination_entry._received_from;
				//new_raw += packet.raw[2:]
				new_raw << packet.raw().mid(2);
				transmit(outbound_interface, new_raw);
				//Transport.destination_table[packet.destination_hash][0] = time.time()
				destination_entry._timestamp = OS::time();
				sent = true;
			}
		}

		// If none of the above applies, we know the destination is
		// directly reachable, and also on which interface, so we
		// simply transmit the packet directly on that one.
		else {
			TRACE("Transport::outbound: Sending packet over directly connected interface...");
			transmit(outbound_interface, packet.raw());
			sent = true;
		}
	}
	// If we don't have a known path for the destination, we'll
	// broadcast the packet on all outgoing interfaces, or the
	// just the relevant interface if the packet has an attached
	// interface, or belongs to a link.
	else {
		TRACE("Transport::outbound: Path to destination is unknown");
		bool stored_hash = false;
#if defined(INTERFACES_SET)
		for (const Interface& interface : _interfaces) {
#elif defined(INTERFACES_LIST)
		for (Interface& interface : _interfaces) {
#elif defined(INTERFACES_MAP)
		for (auto& [hash, interface] : _interfaces) {
#endif
			TRACE("Transport::outbound: Checking interface " + interface.toString());
			if (interface.OUT()) {
				bool should_transmit = true;

				if (packet.destination().type() == Type::Destination::LINK) {
					if (packet.destination().status() == Type::Link::CLOSED) {
						TRACE("Transport::outbound: Pscket destination is link-closed, not transmitting");
						should_transmit = false;
					}
					// CBA Bug? Destination has no member attached_interface
					//z if (interface != packet.destination().attached_interface()) {
					//z 	should_transmit = false;
					//z }
				}
				
				if (packet.attached_interface() && interface != packet.attached_interface()) {
					TRACE("Transport::outbound: Packet has wrong attached interface, not transmitting");
					should_transmit = false;
				}

				if (packet.packet_type() == Type::Packet::ANNOUNCE) {
					if (!packet.attached_interface()) {
						TRACE("Transport::outbound: Packet has no attached interface");
						if (interface.mode() == Type::Interface::MODE_ACCESS_POINT) {
							TRACE("Blocking announce broadcast on " + interface.toString() + " due to AP mode");
							should_transmit = false;
						}
						else if (interface.mode() == Type::Interface::MODE_ROAMING) {
							//local_destination = next((d for d in Transport.destinations if d.hash == packet.destination_hash), None)
							//Destination local_destination({Type::NONE});
#if defined(DESTINATIONS_SET)
							bool found_local = false;
							for (auto& destination : _destinations) {
								if (destination.hash() == packet.destination_hash()) {
									//local_destination = destination;
									found_local = true;
									break;
								}
							}
                            //if local_destination != None:
							//if (local_destination) {
							if (found_local) {
#elif defined(DESTINATIONS_MAP)
							auto iter = _destinations.find(packet.destination_hash());
							//if (iter != _destinations.end()) {
							//	local_destination = (*iter).second;
							//}
							if (iter != _destinations.end()) {
#endif
								TRACE("Allowing announce broadcast on roaming-mode interface from instance-local destination");
							}
							else {
								const Interface& from_interface = next_hop_interface(packet.destination_hash());
								//if from_interface == None or not hasattr(from_interface, "mode"):
								if (!from_interface || from_interface.mode() == Type::Interface::MODE_NONE) {
									should_transmit = false;
									if (!from_interface) {
										TRACE("Blocking announce broadcast on " + interface.toString() + " since next hop interface doesn't exist");
									}
									else if (from_interface.mode() == Type::Interface::MODE_NONE) {
										TRACE("Blocking announce broadcast on " + interface.toString() + " since next hop interface has no mode configured");
									}
								}
								else {
									if (from_interface.mode() == Type::Interface::MODE_ROAMING) {
										TRACE("Blocking announce broadcast on " + interface.toString() + " due to roaming-mode next-hop interface");
										should_transmit = false;
									}
									else if (from_interface.mode() == Type::Interface::MODE_BOUNDARY) {
										TRACE("Blocking announce broadcast on " + interface.toString() + " due to boundary-mode next-hop interface");
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
#if defined(DESTINATIONS_SET)
							//Destination local_destination({Type::Destination::NONE});
							bool found_local = false;
							for (auto& destination : _destinations) {
								if (destination.hash() == packet.destination_hash()) {
									//local_destination = destination;
									found_local = true;
									break;
								}
							}
                            //if local_destination != None:
							//if (local_destination) {
							if (found_local) {
#elif defined(DESTINATIONS_MAP)
							auto iter = _destinations.find(packet.destination_hash());
							if (iter != _destinations.end()) {
#endif
								TRACE("Allowing announce broadcast on boundary-mode interface from instance-local destination");
							}
							else {
								const Interface& from_interface = next_hop_interface(packet.destination_hash());
								if (!from_interface || from_interface.mode() == Type::Interface::MODE_NONE) {
									should_transmit = false;
									if (!from_interface) {
										TRACE("Blocking announce broadcast on " + interface.toString() + " since next hop interface doesn't exist");
									}
									else if (from_interface.mode() == Type::Interface::MODE_NONE) {
										TRACE("Blocking announce broadcast on "  + interface.toString() + " since next hop interface has no mode configured");
									}
								}
								else {
									if (from_interface.mode() == Type::Interface::MODE_ROAMING) {
										TRACE("Blocking announce broadcast on " + interface.toString() + " due to roaming-mode next-hop interface");
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
/*
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
#if defined(INTERFACES_SET)
									const_cast<Interface&>(interface).announce_allowed_at(outbound_time + wait_time);
#else
									interface.announce_allowed_at(outbound_time + wait_time);
#endif
								}
								else {
									should_transmit = false;
									if (interface.announce_queue().size() < Type::Reticulum::MAX_QUEUED_ANNOUNCES) {
										bool should_queue = true;
										for (auto& entry : interface.announce_queue()) {
											if (entry._destination == packet.destination_hash()) {
												double emission_timestamp = announce_emitted(packet);
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
											Interface::AnnounceEntry entry(
												packet.destination_hash(),
												outbound_time,
												packet.hops(),
												announce_emitted(packet),
												packet.raw()
											);

											queued_announces = (interface.announce_queue().size() > 0);
#if defined(INTERFACES_SET)
											const_cast<Interface&>(interface).add_announce(entry);
#else
											interface.add_announce(entry);
#endif

											if (!queued_announces) {
												double wait_time = std::max(interface.announce_allowed_at() - OS::time(), (double)0);

												// CBA TODO THREAD?
												//z timer = threading.Timer(wait_time, interface.process_announce_queue)
												//z timer.start()

												if (wait_time < 1000) {
													TRACE("Added announce to queue (height " + std::to_string(interface.announce_queue().size()) + ") on " + interface.toString() + " for processing in " + std::to_string((int)wait_time) + " ms");
												}
												else {
													TRACE("Added announce to queue (height " + std::to_string(interface.announce_queue().size()) + ") on " + interface.toString() + " for processing in " + std::to_string(OS::round(wait_time/1000,1)) + " s");
												}
											}
											else {
												double wait_time = std::max(interface.announce_allowed_at() - OS::time(), (double)0);
												if (wait_time < 1000) {
													TRACE("Added announce to queue (height " + std::to_string(interface.announce_queue().size()) + ") on " + interface.toString() + " for processing in " + std::to_string((int)wait_time) + " ms");
												}
												else {
													TRACE("Added announce to queue (height " + std::to_string(interface.announce_queue().size()) + ") on " + interface.toString() + " for processing in " + std::to_string(OS::round(wait_time/1000,1)) + " s");
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
						_packet_hashlist.insert(packet.packet_hash());
						stored_hash = true;
					}

					// TODO: Re-evaluate potential for blocking
					// def send_packet():
					//     Transport.transmit(interface, packet.raw)
					// thread = threading.Thread(target=send_packet)
					// thread.daemon = True
					// thread.start()

#if defined(INTERFACES_SET)
					transmit(const_cast<Interface&>(interface), packet.raw());
#else
					transmit(interface, packet.raw());
#endif
					sent = true;
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
			_receipts.push_back(receipt);
		}
		
		cache_packet(packet);
	}

	_jobs_locked = false;
	return sent;
}

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
				DEBUG("Dropped PLAIN packet " + packet.packet_hash().toHex() + " with " + std::to_string(packet.hops()) + " hops");
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
				DEBUG("Dropped GROUP packet " + packet.packet_hash().toHex() + " with " + std::to_string(packet.hops()) + " hops");
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

	if (_packet_hashlist.find(packet.packet_hash()) == _packet_hashlist.end()) {
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

	DEBUG("Filtered packet with hash " + packet.packet_hash().toHex());
	return false;
}

/*static*/ void Transport::inbound(const Bytes& raw, const Interface& interface /*= {Type::NONE}*/) {
	TRACE("Transport::inbound()");
	++_packets_received;
	// CBA
	if (_callbacks._receive_packet) {
		try {
			_callbacks._receive_packet(raw, interface);
		}
		catch (std::exception& e) {
			DEBUG("Error while executing receive packet callback. The contained exception was: " + std::string(e.what()));
		}
	}
// TODO
/*
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
		warning("Transport::inbound: No identity!");
		return;
	}

	_jobs_locked = true;

	Packet packet({Type::NONE}, raw);
	if (!packet.unpack()) {
		warning("Transport::inbound: Packet unpack failed!");
		return;
	}
#ifndef NDEBUG
	TRACE("Transport::inbound: packet: " + packet.debugString());
#endif

	TRACE("Transport::inbound: destination=" + packet.destination_hash().toHex() + " hops=" + std::to_string(packet.hops()));

	packet.receiving_interface(interface);
	packet.hops(packet.hops() + 1);

// TODO
/*
	if (interface) {
		if hasattr(interface, "r_stat_rssi"):
			if interface.r_stat_rssi != None:
				packet.rssi = interface.r_stat_rssi
				if len(Transport.local_client_interfaces) > 0:
					Transport.local_client_rssi_cache.append([packet.packet_hash, packet.rssi])

					while len(Transport.local_client_rssi_cache) > Transport.LOCAL_CLIENT_CACHE_MAXSIZE:
						Transport.local_client_rssi_cache.pop()

		if hasattr(interface, "r_stat_snr"):
			if interface.r_stat_rssi != None:
				packet.snr = interface.r_stat_snr
				if len(Transport.local_client_interfaces) > 0:
					Transport.local_client_snr_cache.append([packet.packet_hash, packet.snr])

					while len(Transport.local_client_snr_cache) > Transport.LOCAL_CLIENT_CACHE_MAXSIZE:
						Transport.local_client_snr_cache.pop()
	}
*/

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
		catch (std::exception& e) {
			DEBUG("Error while executing filter packet callback. The contained exception was: " + std::string(e.what()));
		}
	}
	if (accept) {
		accept = packet_filter(packet);
	}
	if (accept) {
		TRACE("Transport::inbound: Packet accepted by filter");
		_packet_hashlist.insert(packet.packet_hash());
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
			auto destination_iter = _destination_table.find(packet.destination_hash());
			if (destination_iter != _destination_table.end()) {
				DestinationEntry destination_entry = (*destination_iter).second;
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
#if defined(INTERFACES_SET)
					for (const Interface& interface : _interfaces) {
#elif defined(INTERFACES_LIST)
					for (Interface& interface : _interfaces) {
#elif defined(INTERFACES_MAP)
					for (auto& [hash, interface] : _interfaces) {
#endif
						if (interface != packet.receiving_interface()) {
							TRACE("Transport::inbound: Broadcasting packet on " + interface.toString());
#if defined(INTERFACES_SET)
							transmit(const_cast<Interface&>(interface), packet.raw());
#else
							transmit(interface, packet.raw());
#endif
						}
					}
				}
				// If the packet was not from a local client, send
				// it directly to all local clients
				else {
					for (const Interface& interface : _local_client_interfaces) {
						TRACE("Transport::inbound: Broadcasting packet on " + interface.toString());
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
					auto destination_iter = _destination_table.find(packet.destination_hash());
					if (destination_iter != _destination_table.end()) {
						TRACE("Transport::inbound: Found next-hop path to destination");
						DestinationEntry destination_entry = (*destination_iter).second;
						Bytes next_hop = destination_entry._received_from;
						uint8_t remaining_hops = destination_entry._hops;
						
						Bytes new_raw;
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
							double proof_timeout = now + Type::Link::ESTABLISHMENT_TIMEOUT_PER_HOP * std::max((uint8_t)1, remaining_hops);
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
							_link_table.insert({packet.getTruncatedHash(), link_entry});
						}
						else {
							TRACE("Transport::inbound: Packet is next-hop other type");
							ReverseEntry reverse_entry(
								packet.receiving_interface(),
								outbound_interface,
								OS::time()
							);
							_reverse_table.insert({packet.getTruncatedHash(), reverse_entry});
						}
						TRACE("Transport::outbound: Sending packet to next hop...");
#if defined(INTERFACES_SET)
						transmit(const_cast<Interface&>(outbound_interface), new_raw);
#else
						transmit(outbound_interface, new_raw);
#endif
						destination_entry._timestamp = OS::time();
					}
					else {
						// TODO: There should probably be some kind of REJECT
						// mechanism here, to signal to the source that their
						// expected path failed.
						TRACE("Got packet in transport, but no known path to final destination " + packet.destination_hash().toHex() + ". Dropping packet.");
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
					LinkEntry link_entry = (*link_iter).second;
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
						Bytes new_raw;
						//new_raw = packet.raw[0:1]
						new_raw << packet.raw().left(1);
						//new_raw += struct.pack("!B", packet.hops)
						new_raw << packet.hops();
						//new_raw += packet.raw[2:]
						new_raw << packet.raw().mid(2);
						transmit(outbound_interface, new_raw);
						link_entry._timestamp = OS::time();
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
			TRACE("Transport::inbound: Packet is ANNOUNCE");
			Bytes received_from;
			//p local_destination = next((d for d in Transport.destinations if d.hash == packet.destination_hash), None)
#if defined(DESTINATIONS_SET)
			//Destination local_destination({Type::NONE});
			bool found_local = false;
			for (auto& destination : _destinations) {
				if (destination.hash() == packet.destination_hash()) {
					//local_destination = destination;
					found_local = true;
					break;
				}
			}
            //if local_destination == None and RNS.Identity.validate_announce(packet): 
			//if (!local_destination && Identity::validate_announce(packet)) {
			if (!found_local && Identity::validate_announce(packet)) {
#elif defined(DESTINATIONS_MAP)
			auto iter = _destinations.find(packet.destination_hash());
			if (iter == _destinations.end() && Identity::validate_announce(packet)) {
#endif
				TRACE("Transport::inbound: Packet is announce for non-local destination, processing...");
				if (packet.transport_id()) {
					received_from = packet.transport_id();
					
					// Check if this is a next retransmission from
					// another node. If it is, we're removing the
					// announce in question from our pending table
					if (Reticulum::transport_enabled() && _announce_table.count(packet.destination_hash()) > 0) {
						//AnnounceEntry& announce_entry = _announce_table[packet.destination_hash()];
						AnnounceEntry& announce_entry = (*_announce_table.find(packet.destination_hash())).second;
						
						if ((packet.hops() - 1) == announce_entry._hops) {
							DEBUG("Heard a local rebroadcast of announce for " + packet.destination_hash().toHex());
							announce_entry._local_rebroadcasts += 1;
							if (announce_entry._local_rebroadcasts >= LOCAL_REBROADCASTS_MAX) {
								DEBUG("Max local rebroadcasts of announce for " + packet.destination_hash().toHex() + " reached, dropping announce from our table");
								_announce_table.erase(packet.destination_hash());
							}
						}

						if ((packet.hops() - 1) == (announce_entry._hops + 1) && announce_entry._retries > 0) {
							double now = OS::time();
							if (now < announce_entry._timestamp) {
								DEBUG("Rebroadcasted announce for " + packet.destination_hash().toHex() + " has been passed on to another node, no further tries needed");
								_announce_table.erase(packet.destination_hash());
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
				// CBA TODO determine why packet desitnation hash is being searched in destinations again since we entered this logic becuase it did not exist above
				//if (not any(packet.destination_hash == d.hash for d in Transport.destinations) and packet.hops < Transport.PATHFINDER_M+1):
#if defined(DESTINATIONS_SET)
				bool found_local = false;
				for (auto& destination : _destinations) {
					if (destination.hash() == packet.destination_hash()) {
						found_local = true;
						break;
					}
				}
				if (!found_local && packet.hops() < (PATHFINDER_M+1)) {
#elif defined(DESTINATIONS_MAP)
				auto iter = _destinations.find(packet.destination_hash());
				if (iter == _destinations.end() && packet.hops() < (PATHFINDER_M+1)) {
#endif
					double announce_emitted = Transport::announce_emitted(packet);

					//p random_blob = packet.data[RNS.Identity.KEYSIZE//8+RNS.Identity.NAME_HASH_LENGTH//8:RNS.Identity.KEYSIZE//8+RNS.Identity.NAME_HASH_LENGTH//8+10]
					Bytes random_blob = packet.data().mid(Type::Identity::KEYSIZE/8 + Type::Identity::NAME_HASH_LENGTH/8, Type::Identity::RANDOM_HASH_LENGTH/8);
					//p random_blobs = []
					std::set<Bytes> empty_random_blobs;
					std::set<Bytes>& random_blobs = empty_random_blobs;
					auto iter = _destination_table.find(packet.destination_hash());
					if (iter != _destination_table.end()) {
						DestinationEntry destination_entry = (*iter).second;
						//p random_blobs = Transport.destination_table[packet.destination_hash][4]
						random_blobs = destination_entry._random_blobs;

						// If we already have a path to the announced
						// destination, but the hop count is equal or
						// less, we'll update our tables.
						if (packet.hops() <= destination_entry._hops) {
							// Make sure we haven't heard the random
							// blob before, so announces can't be
							// replayed to forge paths.
							// TODO: Check whether this approach works
							// under all circumstances
							//p if not random_blob in random_blobs:
							if (random_blobs.find(random_blob) == random_blobs.end()) {
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
							
							double path_announce_emitted = 0;
							for (const Bytes& path_random_blob : random_blobs) {
								//p path_announce_emitted = max(path_announce_emitted, int.from_bytes(path_random_blob[5:10], "big"))
								// CBA TODO
								//z path_announce_emitted = std::max(path_announce_emitted, int.from_bytes(path_random_blob[5:10], "big"));
								if (path_announce_emitted >= announce_emitted) {
									break;
								}
							}

							if (now >= path_expires) {
								// We also check that the announce is
								// different from ones we've already heard,
								// to avoid loops in the network
								if (random_blobs.find(random_blob) == random_blobs.end()) {
									// TODO: Check that this ^ approach actually
									// works under all circumstances
									DEBUG("Replacing destination table entry for " + packet.destination_hash().toHex() + " with new announce due to expired path");
									should_add = true;
								}
								else {
									should_add = false;
								}
							}
							else {
								if (announce_emitted > path_announce_emitted) {
									if (random_blobs.find(random_blob) == random_blobs.end()) {
										DEBUG("Replacing destination table entry for " + packet.destination_hash().toHex() + " with new announce, since it was more recently emitted");
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

					if (should_add) {
						double now = OS::time();

						bool rate_blocked = false;

// TODO
/*
						if packet.context != RNS.Packet.PATH_RESPONSE and packet.receiving_interface.announce_rate_target != None:
							if not packet.destination_hash in Transport.announce_rate_table:
								rate_entry = { "last": now, "rate_violations": 0, "blocked_until": 0, "timestamps": [now]}
								Transport.announce_rate_table[packet.destination_hash] = rate_entry

							else:
								rate_entry = Transport.announce_rate_table[packet.destination_hash]
								rate_entry["timestamps"].append(now)

								while len(rate_entry["timestamps"]) > Transport.MAX_RATE_TIMESTAMPS:
									rate_entry["timestamps"].pop(0)

								current_rate = now - rate_entry["last"]

								if now > rate_entry["blocked_until"]:

									if current_rate < packet.receiving_interface.announce_rate_target:
										rate_entry["rate_violations"] += 1

									else:
										rate_entry["rate_violations"] = std::max(0, rate_entry["rate_violations"]-1)

									if rate_entry["rate_violations"] > packet.receiving_interface.announce_rate_grace:
										rate_target = packet.receiving_interface.announce_rate_target
										rate_penalty = packet.receiving_interface.announce_rate_penalty
										rate_entry["blocked_until"] = rate_entry["last"] + rate_target + rate_penalty
										rate_blocked = True
									else:
										rate_entry["last"] = now

								else:
									rate_blocked = True
*/

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

						random_blobs.insert(random_blob);

						if ((Reticulum::transport_enabled() || Transport::from_local_client(packet)) && packet.context() != Type::Packet::PATH_RESPONSE) {
							// Insert announce into announce table for retransmission

							if (rate_blocked) {
								DEBUG("Blocking rebroadcast of announce from " + packet.destination_hash().toHex() + " due to excessive announce rate");
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
								_announce_table.insert({packet.destination_hash(), announce_entry});
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
								_announce_table.insert({packet.destination_hash(), announce_entry});
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
										Packet new_announce(
											announce_destination,
											local_interface,
											announce_data,
											Type::Packet::ANNOUNCE,
											announce_context,
											Type::Transport::TRANSPORT,
											Type::Packet::HEADER_2,
											_identity.hash()
										);

										new_announce.hops(packet.hops());
										new_announce.send();
									}
								}
							}
							else {
								for (const Interface& local_interface : _local_client_interfaces) {
									if (packet.receiving_interface() != local_interface) {
										Packet new_announce(
											announce_destination,
											local_interface,
											announce_data,
											Type::Packet::ANNOUNCE,
											announce_context,
											Type::Transport::TRANSPORT,
											Type::Packet::HEADER_2,
											_identity.hash()
										);

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

							DEBUG("Got matching announce, answering waiting discovery path request for " + packet.destination_hash().toHex() + " on " + attached_interface.toString());
							Identity announce_identity(Identity::recall(packet.destination_hash()));
							//Destination announce_destination(announce_identity, Type::Destination::OUT, Type::Destination::SINGLE, "unknown", "unknown");
							//announce_destination.hash(packet.destination_hash());
							Destination announce_destination(announce_identity, Type::Destination::OUT, Type::Destination::SINGLE, packet.destination_hash());
							//announce_destination.hexhash(announce_destination.hash().toHex());
							Type::Packet::context_types announce_context = Type::Packet::CONTEXT_NONE;
							Bytes announce_data = packet.data();

							Packet new_announce(
								announce_destination,
								attached_interface,
								announce_data,
								Type::Packet::ANNOUNCE,
								Type::Packet::PATH_RESPONSE,
								Type::Transport::TRANSPORT,
								Type::Packet::HEADER_2,
								_identity.hash()
							);

							new_announce.hops(packet.hops());
							new_announce.send();
						}

						// CBA Culling before adding to esnure table does not exceed maxsize
						TRACE("Caching packet " + packet.get_hash().toHex());
						if (RNS::Transport::cache_packet(packet, true)) {
							packet.cached(true);
						}
						//TRACE("Adding packet " + packet.get_hash().toHex() + " to packet table");
						//PacketEntry packet_entry(packet);
						//_packet_table.insert({packet.get_hash(), packet_entry});
						TRACE("Adding destination " + packet.destination_hash().toHex() + " to path table");
						DestinationEntry destination_table_entry(
							now,
							received_from,
							announce_hops,
							expires,
							random_blobs,
							//packet.receiving_interface(),
							//const_cast<Interface&>(packet.receiving_interface()),
							packet.receiving_interface().get_hash(),
							//packet
							packet.get_hash()
						);
						if (_destination_table.insert({packet.destination_hash(), destination_table_entry}).second) {
							++_destinations_added;
							cull_path_table();
						}

						DEBUG("Destination " + packet.destination_hash().toHex() + " is now " + std::to_string(announce_hops) + " hops away via " + received_from.toHex() + " on " + packet.receiving_interface().toString());
						//TRACE("Transport::inbound: Destination " + packet.destination_hash().toHex() + " has data: " + packet.data().toHex());
						//TRACE("Transport::inbound: Destination " + packet.destination_hash().toHex() + " has text: " + packet.data().toString());

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
							DEBUG("Path to " + packet.destination_hash().toHex() + " associated with tunnel " + packet.receiving_interface().tunnel_id().toHex());
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
										// CBA TODO Why does app data come from recall instead of from this annuonce packet?
										handler->received_announce(
											packet.destination_hash(),
											announce_identity,
											Identity::recall_app_data(packet.destination_hash())
										);
									}
								}
								catch (std::exception& e) {
									error("Error while processing external announce callback.");
									error("The contained exception was: " + std::string(e.what()));
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
#if defined(DESTINATIONS_SET)
				for (auto& destination : _destinations) {
					if (destination.hash() == packet.destination_hash() && destination.type() == packet.destination_type()) {
#elif defined(DESTINATIONS_MAP)
				auto iter = _destinations.find(packet.destination_hash());
				if (iter != _destinations.end()) {
					auto& destination = (*iter).second;
					if (destination.type() == packet.destination_type()) {
#endif
						packet.destination(destination);
						// CBA iterator over std::set is always const so need to make temporarily mutable
						//destination.receive(packet);
#if defined(DESTINATIONS_SET)
						const_cast<Destination&>(destination).receive(packet);
#else
						destination.receive(packet);
#endif
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
				for (auto& link : _active_links) {
					if (link.link_id() == packet.destination_hash()) {
						TRACE("Transport::inbound: Packet is DATA for an active LINK");
						packet.link(link);
						const_cast<Link&>(link).receive(packet);
					}
				}
			}
			else {
				// Data is basic (not destined for a link)
#if defined(DESTINATIONS_SET)
				for (auto& destination : _destinations) {
					if (destination.hash() == packet.destination_hash() && destination.type() == packet.destination_type()) {
#elif defined(DESTINATIONS_MAP)
				auto iter = _destinations.find(packet.destination_hash());
				if (iter != _destinations.end()) {
					// Data is for a local destination
					DEBUG("Packet destination " + packet.destination_hash().toHex() + " found, destination is local");
					auto& destination = (*iter).second;
					if (destination.type() == packet.destination_type()) {
						TRACE("Transport::inbound: Packet destination type " + std::to_string(packet.destination_type()) + " matched, processing");
#endif
						packet.destination(destination);
#if defined(DESTINATIONS_SET)
						const_cast<Destination&>(destination).receive(packet);
#else
						destination.receive(packet);
#endif

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
								catch (std::exception& e) {
									error(std::string("Error while executing proof request callback. The contained exception was: ") + e.what());
								}
							}
						}
					}
					else {
						DEBUG("Transport::inbound: Packet destination type " + std::to_string(packet.destination_type()) + " mismatch, ignoring");
					}
				}
				else {
					DEBUG("Transport::inbound: Local destination " + packet.destination_hash().toHex() + " not found, not handling packet locally");
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
				if ((Reticulum::transport_enabled() || for_local_client_link || from_local_client) && _link_table.find(packet.destination_hash()) != _link_table.end()) {
					LinkEntry link_entry = (*_link_table.find(packet.destination_hash())).second;
					if (packet.receiving_interface() == link_entry._outbound_interface) {
						try {
							if (packet.data().size() == (Type::Identity::SIGLENGTH/8 + Type::Link::ECPUBSIZE/2)) {
								Bytes peer_pub_bytes = packet.data().mid(Type::Identity::SIGLENGTH/8, Type::Link::ECPUBSIZE/2);
								Identity peer_identity = Identity::recall(link_entry._destination_hash);
								Bytes peer_sig_pub_bytes = peer_identity.get_public_key().mid(Type::Link::ECPUBSIZE/2, Type::Link::ECPUBSIZE/2);

								Bytes signed_data = packet.destination_hash() + peer_pub_bytes + peer_sig_pub_bytes;
								Bytes signature = packet.data().left(Type::Identity::SIGLENGTH/8);

								if (peer_identity.validate(signature, signed_data)) {
									TRACE("Link request proof validated for transport via " + link_entry._receiving_interface.toString());
									//p new_raw = packet.raw[0:1]
									Bytes new_raw = packet.raw().left(1);
									//p new_raw += struct.pack("!B", packet.hops)
									new_raw << packet.hops();
									//p new_raw += packet.raw[2:]
									new_raw << packet.raw().mid(2);
									link_entry._validated = true;
									transmit(link_entry._receiving_interface, new_raw);
								}
								else {
									DEBUG("Invalid link request proof in transport for link " + packet.destination_hash().toHex() + ", dropping proof.");
								}
							}
						}
						catch (std::exception& e) {
							error("Error while transporting link request proof. The contained exception was: " + std::string(e.what()));
						}
					}
					else {
						DEBUG("Link request proof received on wrong interface, not transporting it.");
					}
				}
				else {
					// Check if we can deliver it to a local
					// pending link
					for (auto& link : _pending_links) {
						if (link.link_id() == packet.destination_hash()) {
							// TODO
							//z link.validate_proof(packet);
						}
					}
				}
			}
			else if (packet.context() == Type::Packet::RESOURCE_PRF) {
				TRACE("Transport::inbound: Packet is RESOURCE PROOF");
				for (auto& link : _active_links) {
					if (link.link_id() == packet.destination_hash()) {
						// TODO
						//z link.receive(packet);
					}
				}
			}
			else {
				TRACE("Transport::inbound: Packet is regular PROOF");
				if (packet.destination_type() == Type::Destination::LINK) {
					for (auto& link : _active_links) {
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
						TRACE("Proof received on correct interface, transporting it via " + reverse_entry._receiving_interface.toString());
						//p new_raw = packet.raw[0:1]
						Bytes new_raw = packet.raw().left(1);
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
				for (auto& receipt : _receipts) {
					cull_receipts.remove(receipt);
				}
			}
		}
	}

	_jobs_locked = false;
}

/*static*/ void Transport::synthesize_tunnel(const Interface& interface) {
// TODO
/*
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
/*
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
/*
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
	TRACE("Transport: Registering interface " + interface.get_hash().toHex() + " " + interface.toString());
#if defined(INTERFACES_SET)
	_interfaces.insert(interface);
#elif defined(INTERFACES_LIST)
	_interfaces.push_back(interface);
#elif defined(INTERFACES_MAP)
	_interfaces.insert({interface.get_hash(), interface});
#endif
	// CBA TODO set or add transport as listener on interface to receive incoming packets
}

/*static*/ void Transport::deregister_interface(const Interface& interface) {
	TRACE("Transport: Deregistering interface " + interface.toString());
#if defined(INTERFACES_SET)
	//for (auto iter = _interfaces.begin(); iter != _interfaces.end(); ++iter) {
	//	if ((*iter).get() == interface) {
	//		_interfaces.erase(iter);
	//		TRACE("Transport::deregister_interface: Found and removed interface " + (*iter).get().toString());
	//		break;
	//	}
	//}
	//auto iter = _interfaces.find(interface);
	auto iter = _interfaces.find(const_cast<Interface&>(interface));
	if (iter != _interfaces.end()) {
		_interfaces.erase(iter);
		TRACE("Transport::deregister_interface: Found and removed interface " + (*iter).get().toString());
	}
#elif defined(INTERFACES_LIST)
	for (auto iter = _interfaces.begin(); iter != _interfaces.end(); ++iter) {
		if ((*iter).get() == interface) {
			_interfaces.erase(iter);
			TRACE("Transport::deregister_interface: Found and removed interface " + (*iter).get().toString());
			break;
		}
	}
#elif defined(INTERFACES_MAP)
	auto iter = _interfaces.find(interface.get_hash());
	if (iter != _interfaces.end()) {
		TRACE("Transport::deregister_interface: Found and removed interface " + (*iter).second.toString());
		_interfaces.erase(iter);
	}
#endif
}

/*static*/ void Transport::register_destination(Destination& destination) {
	//TRACE("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
	TRACE("Transport: Registering destination " + destination.toString());
	destination.mtu(Type::Reticulum::MTU);
	if (destination.direction() == Type::Destination::IN) {
#if defined(DESTINATIONS_SET)
		for (auto& registered_destination : _destinations) {
			if (destination.hash() == registered_destination.hash()) {
				//p raise KeyError("Attempt to register an already registered destination.")
				throw std::runtime_error("Attempt to register an already registered destination.");
			}
		}

		_destinations.insert(destination);
#elif defined(DESTINATIONS_MAP)
		auto iter = _destinations.find(destination.hash());
		if (iter != _destinations.end()) {
			//p raise KeyError("Attempt to register an already registered destination.")
			throw std::runtime_error("Attempt to register an already registered destination.");
		}

		_destinations.insert({destination.hash(), destination});
#endif

		if (_owner && _owner.is_connected_to_shared_instance()) {
			if (destination.type() == Type::Destination::SINGLE) {
				TRACE("Transport:register_destination: Announcing destination " + destination.toString());
				destination.announce({}, true);
			}
		}
	}
	else {
		TRACE("Transport:register_destination: Skipping registration (not direction IN) of destination " + destination.toString());
	}

/*
#if defined(DESTINATIONS_SET)
	for (const Destination& destination : _destinations) {
#elif defined(DESTINATIONS_MAP)
	for (auto& [hash, destination] : _destinations) {
#endif
		TRACE("Transport::register_destination: Listed destination " + destination.toString());
	}
*/
	//TRACE("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
}

/*static*/ void Transport::deregister_destination(const Destination& destination) {
	TRACE("Transport: Deregistering destination " + destination.toString());
#if defined(DESTINATIONS_SET)
	if (_destinations.find(destination) != _destinations.end()) {
		_destinations.erase(destination);
		TRACE("Transport::deregister_destination: Found and removed destination " + destination.toString());
	}
#elif defined(DESTINATIONS_MAP)
	auto iter = _destinations.find(destination.hash());
	if (iter != _destinations.end()) {
		_destinations.erase(iter);
		TRACE("Transport::deregister_destination: Found and removed destination " + (*iter).second.toString());
	}
#endif
}

/*static*/ void Transport::register_link(const Link& link) {
// TODO
/*
	TRACE("Transport: Registering link " + link.toString());
	if (link.initiator()) {
		_pending_links.insert(link);
	}
	else {
		_active_links.insert(link);
	}
*/
}

/*static*/ void Transport::activate_link(Link& link) {
// TODO
/*
	TRACE("Transport: Activating link " + link.toString());
	if (_pending_links.find(link) != _pending_links.end()) {
		if (link.status() != Type::Link::ACTIVE) {
			throw std::runtime_error("Invalid link state for link activation: " + link.status_string());
		}
		_pending_links.erase(link);
		_active_links.insert(link);
		link.status(Type::Link::ACTIVE);
	}
	else {
		error("Attempted to activate a link that was not in the pending table");
	}
*/
}

/*
Registers an announce handler.

:param handler: Must be an object with an *aspect_filter* attribute and a *received_announce(destination_hash, announced_identity, app_data)* callable. See the :ref:`Announce Example<example-announce>` for more info.
*/
/*static*/ void Transport::register_announce_handler(HAnnounceHandler handler) {
	TRACE("Transport: Registering announce handler " + handler->aspect_filter());
	_announce_handlers.insert(handler);
}

/*
Deregisters an announce handler.

:param handler: The announce handler to be deregistered.
*/
/*static*/ void Transport::deregister_announce_handler(HAnnounceHandler handler) {
	TRACE("Transport: Deregistering announce handler " + handler->aspect_filter());
	if (_announce_handlers.find(handler) != _announce_handlers.end()) {
		_announce_handlers.erase(handler);
		TRACE("Transport::deregister_announce_handler: Found and removed handler" + handler->aspect_filter());
	}
}

/*static*/ Interface Transport::find_interface_from_hash(const Bytes& interface_hash) {
#if defined(INTERFACES_SET)
	for (const Interface& interface : _interfaces) {
		if (interface.get_hash() == interface_hash) {
			TRACE("Transport::find_interface_from_hash: Found interface " + interface.toString());
			return interface;
		}
	}
#elif defined(INTERFACES_LIST)
	for (Interface& interface : _interfaces) {
		if (interface.get_hash() == interface_hash) {
			TRACE("Transport::find_interface_from_hash: Found interface " + interface.toString());
			return interface;
		}
	}
#elif defined(INTERFACES_MAP)
	auto iter = _interfaces.find(interface_hash);
	if (iter != _interfaces.end()) {
		TRACE("Transport::find_interface_from_hash: Found interface " + (*iter).second.toString());
		return (*iter).second;
	}
#endif

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
	TRACE("Checking to see if packet " + packet.get_hash().toHex() + " should be cached");
	if (should_cache_packet(packet) || force_cache) {
		TRACE("Saving packet " + packet.get_hash().toHex() + " to storage");
		try {
			std::string packet_cache_path = Reticulum::_cachepath + "/" + packet.get_hash().toHex();
			return Persistence::serialize(packet, packet_cache_path.c_str());
		}
		catch (std::exception& e) {
			error("Error writing packet to cache. The contained exception was: " + std::string(e.what()));
		}
	}
	return false;
}

/*static*/ Packet Transport::get_cached_packet(const Bytes& packet_hash) {
	TRACE("Loading packet " + packet_hash.toHex() + " from cache storage");
	try {
/*p
		packet_hash = RNS.hexrep(packet_hash, delimit=False)
		path = RNS.Reticulum.cachepath+"/"+packet_hash

		if os.path.isfile(path):
			file = open(path, "rb")
			cached_data = umsgpack.unpackb(file.read())
			file.close()

			packet = RNS.Packet(None, cached_data[0])
			interface_reference = cached_data[1]

			for interface in Transport.interfaces:
				if str(interface) == interface_reference:
					packet.receiving_interface = interface

			return packet
		else:
			return None
*/
		std::string packet_cache_path = Reticulum::_cachepath + "/" + packet_hash.toHex();
		//return Persistence::deserialize(packet_cache_path.c_str());
		Packet packet({Type::NONE});
		Persistence::deserialize(packet, packet_cache_path.c_str());
		packet.update_hash();
		return packet;
	}
	catch (std::exception& e) {
		error("Exception occurred while getting cached packet.");
		error("The contained exception was: " + std::string(e.what()));
	}
	return {Type::NONE};
}

/*static*/ bool Transport::clear_cached_packet(const Bytes& packet_hash) {
	TRACE("Clearing packet " + packet_hash.toHex() + " from cache storage");
	try {
		std::string packet_cache_path = Reticulum::_cachepath + "/" + packet_hash.toHex();
		double start_time = OS::time();
		bool success = RNS::Utilities::OS::remove_file(packet_cache_path.c_str());
		double diff_time = OS::time() - start_time;
		if (diff_time < 1.0) {
			DEBUG("Remove cached packet in " + std::to_string((int)(diff_time*1000)) + " ms");
		}
		else {
			DEBUG("Remove cached packet in " + std::to_string(diff_time) + " s");
		}
	}
	catch (std::exception& e) {
		error("Exception occurred while clearing cached packet.");
		error("The contained exception was: " + std::string(e.what()));
	}
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
		Packet request(destination, packet_hash, Type::Packet::DATA, Type::Packet::CACHE_REQUEST);
		request.send();
	}
}

/*static*/ bool Transport::remove_path(const Bytes& destination_hash) {
	if (_destination_table.erase(destination_hash) > 0) {
		// CBA also remove cached announce packet if exists
	}
	return false;
}

/*
:param destination_hash: A destination hash as *bytes*.
:returns: *True* if a path to the destination is known, otherwise *False*.
*/
/*static*/ bool Transport::has_path(const Bytes& destination_hash) {
	if (_destination_table.find(destination_hash) != _destination_table.end()) {
		return true;
	}
	else {
		return false;
	}
}

/*
:param destination_hash: A destination hash as *bytes*.
:returns: The number of hops to the specified destination, or ``RNS.Transport.PATHFINDER_M`` if the number of hops is unknown.
*/
/*static*/ uint8_t Transport::hops_to(const Bytes& destination_hash) {
	auto iter = _destination_table.find(destination_hash);
	if (iter != _destination_table.end()) {
		DestinationEntry destination_entry = (*iter).second;
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
	auto iter = _destination_table.find(destination_hash);
	if (iter != _destination_table.end()) {
		DestinationEntry destination_entry = (*iter).second;
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
	auto iter = _destination_table.find(destination_hash);
	if (iter != _destination_table.end()) {
		DestinationEntry destination_entry = (*iter).second;
		return destination_entry.receiving_interface();
	}
	else {
		return {Type::NONE};
	}
}

/*static*/ bool Transport::expire_path(const Bytes& destination_hash) {
	auto iter = _destination_table.find(destination_hash);
	if (iter != _destination_table.end()) {
		DestinationEntry destination_entry = (*iter).second;
		destination_entry._timestamp = 0;
		_tables_last_culled = 0;
		return true;
	}
	else {
		return false;
	}
}

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
	if (tag) {
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
	Packet packet(path_request_dst, on_interface, path_request_data, Type::Packet::DATA, Type::Packet::CONTEXT_NONE, Type::Transport::BROADCAST, Type::Packet::HEADER_1);

	if (on_interface && recursive) {
// TODO
/*
		if not hasattr(on_interface, "announce_cap"):
			on_interface.announce_cap = RNS.Reticulum.ANNOUNCE_CAP

		if not hasattr(on_interface, "announce_allowed_at"):
			on_interface.announce_allowed_at = 0

		if not hasattr(on_interface, "announce_queue"):
			on_interface.announce_queue = []
*/

		bool queued_announces = (on_interface.announce_queue().size() > 0);
		if (queued_announces) {
			TRACE("Blocking recursive path request on " + on_interface.toString() + " due to queued announces");
			return;
		}
		else {
			double now = OS::time();
			if (now < on_interface.announce_allowed_at()) {
				TRACE("Blocking recursive path request on " + on_interface.toString() + " due to active announce cap");
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

	packet.send();
	_path_requests[destination_hash] = OS::time();
}

/*static*/ void Transport::request_path(const Bytes& destination_hash) {
	return request_path(destination_hash, {Type::NONE});
}

/*static*/ void Transport::path_request_handler(const Bytes& data, const Packet& packet) {
	TRACE("Transport::path_request_handler");
	try {
		// If there is at least bytes enough for a destination
		// hash in the packet, we assume those bytes are the
		// destination being requested.
		if (data.size() >= Type::Identity::TRUNCATED_HASHLENGTH/8) {
			Bytes destination_hash = data.left(Type::Identity::TRUNCATED_HASHLENGTH/8);
			//TRACE("Transport::path_request_handler: destination_hash: " + destination_hash.toHex());
			// If there is also enough bytes for a transport
			// instance ID and at least one tag byte, we
			// assume the next bytes to be the trasport ID
			// of the requesting transport instance.
			Bytes requesting_transport_instance;
			if (data.size() > (Type::Identity::TRUNCATED_HASHLENGTH/8)*2) {
				requesting_transport_instance = data.mid(Type::Identity::TRUNCATED_HASHLENGTH/8, Type::Identity::TRUNCATED_HASHLENGTH/8);
				//TRACE("Transport::path_request_handler: requesting_transport_instance: " + requesting_transport_instance.toHex());
			}

			Bytes tag_bytes;
			if (data.size() > Type::Identity::TRUNCATED_HASHLENGTH/8*2) {
				tag_bytes = data.mid(Type::Identity::TRUNCATED_HASHLENGTH/8*2);
			}
			else if (data.size() > Type::Identity::TRUNCATED_HASHLENGTH/8) {
				tag_bytes = data.mid(Type::Identity::TRUNCATED_HASHLENGTH/8);
			}

			if (tag_bytes) {
				//TRACE("Transport::path_request_handler: tag_bytes: " + tag_bytes.toHex());
				if (tag_bytes.size() > Type::Identity::TRUNCATED_HASHLENGTH/8) {
					tag_bytes = tag_bytes.left(Type::Identity::TRUNCATED_HASHLENGTH/8);
				}

				Bytes unique_tag = destination_hash + tag_bytes;
				//TRACE("Transport::path_request_handler: unique_tag: " + unique_tag.toHex());

				if (_discovery_pr_tags.find(unique_tag) == _discovery_pr_tags.end()) {
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
					DEBUG("Ignoring duplicate path request for " + destination_hash.toHex() + " with tag " + unique_tag.toHex());
				}
			}
			else {
				DEBUG("Ignoring tagless path request for " + destination_hash.toHex());
			}
		}
	}
	catch (std::exception& e) {
		error("Error while handling path request. The contained exception was: " + std::string(e.what()));
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

	DEBUG("Path request for destination " + destination_hash.toHex() + interface_str);

	bool destination_exists_on_local_client = false;
	if (_local_client_interfaces.size() > 0) {
		auto iter = _destination_table.find(destination_hash);
		if (iter != _destination_table.end()) {
			TRACE("Transport::path_request_handler: entry found for destination " + destination_hash.toHex());
			DestinationEntry& destination_entry = (*iter).second;
			if (is_local_client_interface(destination_entry.receiving_interface())) {
				destination_exists_on_local_client = true;
				_pending_local_path_requests.insert({destination_hash, attached_interface});
			}
		}
		else {
			TRACE("Transport::path_request_handler: entry not found for destination " + destination_hash.toHex());
		}
	}

	auto destination_iter = _destination_table.find(destination_hash);
	//local_destination = next((d for d in Transport.destinations if d.hash == destination_hash), None)
#if defined(DESTINATIONS_SET)
	Destination local_destination({Type::NONE});
	for (auto& destination : _destinations) {
		if (destination.hash() == destination_hash) {
			local_destination = destination;
			break;
		}
	}
    //if local_destination != None:
	if (local_destination) {
#elif defined(DESTINATIONS_MAP)
	auto iter = _destinations.find(destination_hash);
	if (iter != _destinations.end()) {
		auto& local_destination = (*iter).second;
#endif
		local_destination.announce({Bytes::NONE}, true, attached_interface, tag);
		DEBUG("Answering path request for destination " + destination_hash.toHex() + interface_str + ", destination is local to this system");
	}
    //p elif (RNS.Reticulum.transport_enabled() or is_from_local_client) and (destination_hash in Transport.destination_table):
	else if ((Reticulum::transport_enabled() || is_from_local_client) && destination_iter != _destination_table.end()) {
		TRACE("Transport::path_request_handler: entry found for destination " + destination_hash.toHex());
		DestinationEntry& destination_entry = (*destination_iter).second;
		const Packet& announce_packet = destination_entry.announce_packet();
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
				DEBUG("Not answering path request for destination " + destination_hash.toHex() + interface_str + ", since next hop is the requestor");
			}
			else {
				DEBUG("Answering path request for destination " + destination_hash.toHex() + interface_str + ", path is known");

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
				}

				// This handles an edge case where a peer sends a past
				// request for a destination just after an announce for
				// said destination has arrived, but before it has been
				// rebroadcast locally. In such a case the actual announce
				// is temporarily held, and then reinserted when the path
				// request has been served to the peer.
				const Packet& announce_packet = announce_packet;
				auto announce_iter = _announce_table.find(announce_packet.destination_hash());
				if (announce_iter != _announce_table.end()) {
					AnnounceEntry& held_entry = (*announce_iter).second;
					_held_announces.insert({announce_packet.destination_hash(), held_entry});
				}

/*
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
				_announce_table.insert({announce_packet.destination_hash(), announce_entry});
			}
		}
	}
	else if (is_from_local_client) {
		// Forward path request on all interfaces
		// except the local client
		DEBUG("Forwarding path request from local client for destination " + destination_hash.toHex() + interface_str + " to all other interfaces");
		Bytes request_tag = Identity::get_random_hash();
#if defined(INTERFACES_SET)
		for (const Interface& interface : _interfaces) {
#elif defined(INTERFACES_LIST)
		for (Interface& interface : _interfaces) {
#elif defined(INTERFACES_MAP)
		for (auto& [hash, interface] : _interfaces) {
#endif
			if (interface != attached_interface) {
				request_path(destination_hash, interface, request_tag);
			}
		}
	}
	else if (should_search_for_unknown) {
		TRACE("Transport::path_request_handler: searching for unknown path to " + destination_hash.toHex());
		if (_discovery_path_requests.find(destination_hash) != _discovery_path_requests.end()) {
			DEBUG("There is already a waiting path request for destination " + destination_hash.toHex() + " on behalf of path request" + interface_str);
		}
		else {
			// Forward path request on all interfaces
			// except the requestor interface
			DEBUG("Attempting to discover unknown path to destination " + destination_hash.toHex() + " on behalf of path request" + interface_str);
			//p pr_entry = { "destination_hash": destination_hash, "timeout": time.time()+Transport.PATH_REQUEST_TIMEOUT, "requesting_interface": attached_interface }
			//p _discovery_path_requests[destination_hash] = pr_entry;
			_discovery_path_requests.insert({destination_hash, {
				destination_hash,
				OS::time() + Type::Transport::PATH_REQUEST_TIMEOUT,
				attached_interface
			}});

#if defined(INTERFACES_SET)
			for (const Interface& interface : _interfaces) {
#elif defined(INTERFACES_LIST)
			for (Interface& interface : _interfaces) {
#elif defined(INTERFACES_MAP)
			for (auto& [hash, interface] : _interfaces) {
#endif
				// CBA EXPERIMENTAL forwarding path requests even on requestor interface in order to support
				//  path-finding over LoRa mesh
				//if (interface != attached_interface) {
				if (true) {
					TRACE("Transport::path_request: requesting path on interface " + interface.toString());
					// Use the previously extracted tag from this path request
					// on the new path requests as well, to avoid potential loops
					request_path(destination_hash, interface, tag, true);
				}
				else {
					TRACE("Transport::path_request: not requesting path on same interface " + interface.toString());
				}
			}
		}
	}
	else if (!is_from_local_client && _local_client_interfaces.size() > 0) {
		// Forward the path request on all local
		// client interfaces
		DEBUG("Forwarding path request for destination " + destination_hash.toHex() + interface_str + " to local clients");
		for (const Interface& interface : _local_client_interfaces) {
			request_path(destination_hash, interface);
		}
	}
	else {
		DEBUG("Ignoring path request for destination " + destination_hash.toHex() + interface_str + ", no path known");
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

/*static*/ void Transport::detach_interfaces() {
// TODO
/*
	detachable_interfaces = []

	for interface in Transport.interfaces:
		// Currently no rules are being applied
		// here, and all interfaces will be sent
		// the detach call on RNS teardown.
		if True:
			detachable_interfaces.append(interface)
		else:
			pass
	
	for interface in Transport.local_client_interfaces:
		// Currently no rules are being applied
		// here, and all interfaces will be sent
		// the detach call on RNS teardown.
		if True:
			detachable_interfaces.append(interface)
		else:
			pass

	for interface in detachable_interfaces:
		interface.detach()
*/
}

/*static*/ void Transport::shared_connection_disappeared() {
// TODO
/*
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
/*
	if Transport.owner.is_connected_to_shared_instance:
		for registered_destination in Transport.destinations:
			if registered_destination.type == RNS.Destination.SINGLE:
				registered_destination.announce(path_response=True)
*/
}

/*static*/ void Transport::drop_announce_queues() {
// TODO
/*
	for interface in Transport.interfaces:
		if hasattr(interface, "announce_queue") and interface.announce_queue != None:
			na = len(interface.announce_queue)
			if na > 0:
				if na == 1:
					na_str = "1 announce"
				else:
					na_str = str(na)+" announces"

				interface.announce_queue = []
				RNS.log("Dropped "+na_str+" on "+str(interface), RNS.LOG_VERBOSE)
*/
}

/*static*/ bool Transport::announce_emitted(const Packet& packet) {
// TODO
/*
	random_blob = packet.data[RNS.Identity.KEYSIZE//8+RNS.Identity.NAME_HASH_LENGTH//8:RNS.Identity.KEYSIZE//8+RNS.Identity.NAME_HASH_LENGTH//8+10]
	announce_emitted = int.from_bytes(random_blob[5:10], "big")

	return announce_emitted
*/
	// MOCK
	return false;
}

/*static*/ void Transport::write_packet_hashlist() {
// TODO
/*
	if not Transport.owner.is_connected_to_shared_instance:
		if hasattr(Transport, "saving_packet_hashlist"):
			wait_interval = 0.2
			wait_timeout = 5
			wait_start = time.time()
			while Transport.saving_packet_hashlist:
				time.sleep(wait_interval)
				if time.time() > wait_start+wait_timeout:
					RNS.log("Could not save packet hashlist to storage, waiting for previous save operation timed out.", RNS.LOG_ERROR)
					return False

		try:
			Transport.saving_packet_hashlist = True
			save_start = time.time()

			if not RNS.Reticulum.transport_enabled():
				Transport.packet_hashlist = []
			else:
				RNS.log("Saving packet hashlist to storage...", RNS.LOG_DEBUG)

			packet_hashlist_path = RNS.Reticulum.storagepath+"/packet_hashlist"
			file = open(packet_hashlist_path, "wb")
			file.write(umsgpack.packb(Transport.packet_hashlist))
			file.close()

			save_time = time.time() - save_start
			if save_time < 1:
				time_str = str(round(save_time*1000,2))+"ms"
			else:
				time_str = str(round(save_time,2))+"s"
			RNS.log("Saved packet hashlist in "+time_str, RNS.LOG_DEBUG)

		except Exception as e:
			RNS.log("Could not save packet hashlist to storage, the contained exception was: "+str(e), RNS.LOG_ERROR)

		Transport.saving_packet_hashlist = False
*/
}

/*static*/ bool Transport::read_path_table() {
	DEBUG("Transport::read_path_table");
	std::string destination_table_path = Reticulum::_storagepath + "/destination_table";
	if (!_owner.is_connected_to_shared_instance() && OS::file_exists(destination_table_path.c_str())) {
/*p
		serialised_destinations = []
		try:
			file = open(destination_table_path, "rb")
			serialised_destinations = umsgpack.unpackb(file.read())
			file.close()

			for serialised_entry in serialised_destinations:
				destination_hash = serialised_entry[0]

				if len(destination_hash) == RNS.Reticulum.TRUNCATED_HASHLENGTH//8:
					timestamp = serialised_entry[1]
					received_from = serialised_entry[2]
					hops = serialised_entry[3]
					expires = serialised_entry[4]
					random_blobs = serialised_entry[5]
					receiving_interface = Transport.find_interface_from_hash(serialised_entry[6])
					announce_packet = Transport.get_cached_packet(serialised_entry[7])

					if announce_packet != None and receiving_interface != None:
						announce_packet.unpack()
						// We increase the hops, since reading a packet
						// from cache is equivalent to receiving it again
						// over an interface. It is cached with it's non-
						// increased hop-count.
						announce_packet.hops += 1
						Transport.destination_table[destination_hash] = [timestamp, received_from, hops, expires, random_blobs, receiving_interface, announce_packet]
						RNS.log("Loaded path table entry for "+RNS.prettyhexrep(destination_hash)+" from storage", RNS.LOG_DEBUG)
					else:
						RNS.log("Could not reconstruct path table entry from storage for "+RNS.prettyhexrep(destination_hash), RNS.LOG_DEBUG)
						if announce_packet == None:
							RNS.log("The announce packet could not be loaded from cache", RNS.LOG_DEBUG)
						if receiving_interface == None:
							RNS.log("The interface is no longer available", RNS.LOG_DEBUG)
*/

		try {
TRACE("Transport::start: buffer capacity " + std::to_string(Persistence::_buffer.capacity()) + " bytes");
			if (RNS::Utilities::OS::read_file(destination_table_path.c_str(), Persistence::_buffer) > 0) {
				TRACE("Transport::start: read: " + std::to_string(Persistence::_buffer.size()) + " bytes");
#ifndef NDEBUG
				// CBA DEBUG Dump path table
TRACE("Transport::start: buffer addr: " + std::to_string((long)Persistence::_buffer.data()));
TRACE("Transport::start: buffer size " + std::to_string(Persistence::_buffer.size()) + " bytes");
				//TRACE("SERIALIZED: destination_table");
				//TRACE(Persistence::_buffer.toString());
#endif
#ifdef USE_MSGPACK
				DeserializationError error = deserializeMsgPack(Persistence::_document, Persistence::_buffer.data());
#else
				DeserializationError error = deserializeJson(Persistence::_document, Persistence::_buffer.data());
#endif
				TRACE("Transport::start: doc size: " + std::to_string(Persistence::_buffer.size()) + " bytes");
				if (!error) {
					_destination_table = Persistence::_document.as<std::map<Bytes, DestinationEntry>>();
					TRACE("Transport::start: successfully deserialized path table with " + std::to_string(_destination_table.size()) + " entries");
					// Calculate crc for dirty-checking before write
					_destination_table_crc = crypto_crc8(0, Persistence::_buffer.data(), Persistence::_buffer.size());
					std::vector<Bytes> invalid_paths;
					for (auto& [destination_hash, destination_entry] : _destination_table) {
#ifndef NDEBUG
						TRACE("Transport::start: entry: " + destination_hash.toHex() + " = " + destination_entry.debugString());
#endif
						// CBA TODO If announce packet load fails then remove destination entry (it's useless without announce packet)
						if (!destination_entry.announce_packet()) {
							// remove destination
							RNS::warning("Transport::start: removing invalid path to " + destination_hash.toHex());
							invalid_paths.push_back(destination_hash);
						}
					}
					for (const auto& destination_hash : invalid_paths) {
						_destination_table.erase(destination_hash);
					}
					return true;
				}
				else {
					TRACE("Transport::start: failed to deserialize");
				}
			}
			else {
				TRACE("Transport::start: destination table read failed");
			}
			verbose("Loaded " + std::to_string(_destination_table.size()) + " path table entries from storage");

		}
		catch (std::exception& e) {
			error("Could not load destination table from storage, the contained exception was: " + std::string(e.what()));
		}
	}
	return false;
}

/*static*/ bool Transport::write_path_table() {
	DEBUG("Transport::write_path_table");
//return false;

	if (Transport::_owner.is_connected_to_shared_instance()) {
		return true;
	}

	bool success = false;
	if (_saving_path_table) {
		double wait_interval = 0.2;
		double wait_timeout = 5;
		double wait_start = OS::time();
		while (_saving_path_table) {
			OS::sleep(wait_interval);
			if (OS::time() > (wait_start + wait_timeout)) {
				error("Could not save path table to storage, waiting for previous save operation timed out.");
				return false;
			}
		}
	}

	try {
		_saving_path_table = true;
		double save_start = OS::time();
		DEBUG("Saving " + std::to_string(_destination_table.size()) + " path table entries to storage...");

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

		std::string destination_table_path = Reticulum::_storagepath + "/destination_table";

		{
			Persistence::_document.set(_destination_table);
			TRACE("Transport::write_path_table: doc size " + std::to_string(Persistence::_document.memoryUsage()) + " bytes");

			//size_t size = 8192;
			size_t size = Persistence::_buffer.capacity();
TRACE("Transport::write_path_table: obtaining buffer size " + std::to_string(size) + " bytes");
			uint8_t* buffer = Persistence::_buffer.writable(size);
TRACE("Transport::write_path_table: buffer addr: " + std::to_string((long)buffer));
#ifdef USE_MSGPACK
			size_t length = serializeMsgPack(Persistence::_document, buffer, size);
#else
			size_t length = serializeJson(Persistence::_document, buffer, size);
#endif
			TRACE("Transport::write_path_table: serialized " + std::to_string(length) + " bytes");
			if (length < size) {
				Persistence::_buffer.resize(length);
			}
		}
		if (Persistence::_buffer.size() > 0) {
#ifndef NDEBUG
			// CBA DEBUG Dump path table
TRACE("Transport::write_path_table: buffer addr: " + std::to_string((long)Persistence::_buffer.data()));
TRACE("Transport::write_path_table: buffer size " + std::to_string(Persistence::_buffer.size()) + " bytes");
			//TRACE("SERIALIZED: destination_table");
			//TRACE(Persistence::_buffer.toString());
#endif
			// Check crc to see if data has changed before writing
			uint8_t crc = crypto_crc8(0, Persistence::_buffer.data(), Persistence::_buffer.size());
			if (_destination_table_crc > 0 && crc == _destination_table_crc) {
				TRACE("Transport::write_path_table: no change detected, skipping write");
			}
			else if (RNS::Utilities::OS::write_file(destination_table_path.c_str(), Persistence::_buffer) == Persistence::_buffer.size()) {
				TRACE("Transport::write_path_table: wrote " + std::to_string(_destination_table.size()) + " entries, " + std::to_string(Persistence::_buffer.size()) + " bytes");
				_destination_table_crc = crc;
				success = true;

#ifndef NDEBUG
				// CBA DEBUG Dump path table
				//TRACE("FILE: destination_table");
				//if (OS::read_file("/destination_table", Persistence::_buffer) > 0) {
				//	TRACE(Persistence::_buffer.toString());
				//}
#endif
			}
			else {
				TRACE("Transport::write_path_table: write failed");
			}
		}
		else {
			TRACE("Transport::write_path_table: failed to serialize");
		}

		if (success) {
			double save_time = OS::time() - save_start;
			if (save_time < 1.0) {
				//DEBUG("Saved " + std::to_string(_destination_table.size()) + " path table entries in " + std::to_string(OS::round(save_time * 1000, 1)) + " ms");
				DEBUG("Saved " + std::to_string(_destination_table.size()) + " path table entries in " + std::to_string((int)(save_time*1000)) + " ms");
			}
			else {
				//DEBUG("Saved " + std::to_string(_destination_table.size()) + " path table entries in " + std::to_string(OS::round(save_time, 1)) + " s");
				DEBUG("Saved " + std::to_string(_destination_table.size()) + " path table entries in " + std::to_string(save_time) + " s");
			}
		}
	}
	catch (std::exception& e) {
		error("Could not save path table to storage, the contained exception was: " + std::string(e.what()));
	}

	_saving_path_table = false;

	return success;
}

/*static*/ void Transport::read_tunnel_table() {
	DEBUG("Transport::read_tunnel_table");
// TODO
/*
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
}

/*static*/ void Transport::write_tunnel_table() {
// TODO
/*
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

			save_time = time.time() - save_start
			if save_time < 1:
				time_str = str(round(save_time*1000,2))+" ms"
			else:
				time_str = str(round(save_time,2))+" s"
			RNS.log("Saved "+str(len(serialised_tunnels))+" tunnel table entries in "+time_str, RNS.LOG_DEBUG)
		except Exception as e:
			RNS.log("Could not save tunnel table to storage, the contained exception was: "+str(e), RNS.LOG_ERROR)

		Transport.saving_tunnel_table = False
*/
}

/*static*/ void Transport::persist_data() {
	TRACE("Transport::persist_data()");
	write_packet_hashlist();
	write_path_table();
	write_tunnel_table();
}

/*static*/ void Transport::clean_caches() {
	TRACE("Transport::clean_caches()");
	// CBA Remove cached packets no longer in path list
    std::list<std::string> files = OS::list_directory(Reticulum::_cachepath.c_str());
    for (auto& file : files) {
		TRACE("Transport::clean_caches: Checking for use of cached packet " + file);
		bool found = false;
		for (auto& [destination_hash, destination_entry] : _destination_table) {
			if (file.compare(destination_entry._announce_packet.toHex()) == 0) {
				found = true;
				break;
			}
		}
		if (!found) {
			TRACE("Transport::clean_caches: Removing cached packet " + file);
			std::string packet_cache_path = Reticulum::_cachepath + "/" + file;
			OS::remove_file(packet_cache_path.c_str());
		}
	}
}

/*static*/ void Transport::dump_stats() {

#ifndef NDEBUG
	// memory
	// storage
	// _destinations
	// _destination_table
	// _reverse_table
	// _announce_table
	// _held_announces
	head("mem: " + std::to_string(OS::memory_available()) + " flash: " + std::to_string(OS::storage_available()) + " paths: " + std::to_string(_destination_table.size()) + " dsts: " + std::to_string(_destinations.size()) + " revr: " + std::to_string(_reverse_table.size()) + " annc: " + std::to_string(_announce_table.size()) + " held: " + std::to_string(_held_announces.size()), LOG_EXTREME);
	// _path_requests
	// _discovery_path_requests
	// _pending_local_path_requests
	// _discovery_pr_tags
	// _control_destinations
	// _control_hashes
	TRACE("preqs: " + std::to_string(_path_requests.size()) + " dpreqs: " + std::to_string(_discovery_path_requests.size()) + " ppreqs: " + std::to_string(_pending_local_path_requests.size()) + " dprt: " + std::to_string(_discovery_pr_tags.size()) + " cdsts: " + std::to_string(_control_destinations.size()) + " chshs: " + std::to_string(_control_hashes.size()));
	// _packet_hashlist
	// _receipts
	// _link_table
	// _pending_links
	// _active_links
	// _tunnels
	TRACE("phl: " + std::to_string(_packet_hashlist.size()) + " rcp: " + std::to_string(_receipts.size()) + " lt: " + std::to_string(_link_table.size()) + " pl: " + std::to_string(_pending_links.size()) + " al: " + std::to_string(_active_links.size()) + " tun: " + std::to_string(_tunnels.size()));
	TRACE("pin: " + std::to_string(_packets_received) + " pout: " + std::to_string(_packets_sent) + " dadd: " + std::to_string(_destinations_added) + "\r\n");
#endif

}

/*static*/ void Transport::exit_handler() {
	TRACE("Transport::exit_handler()");
	if (!_owner.is_connected_to_shared_instance()) {
		persist_data();
	}
}

/*static*/ Destination Transport::find_destination_from_hash(const Bytes& destination_hash) {
	TRACE("Transport::find_destination_from_hash: Searching for destination " + destination_hash.toHex());
#if defined(DESTINATIONS_SET)
	for (const Destination& destination : _destinations) {
		if (destination.get_hash() == destination_hash) {
			TRACE("Transport::find_destination_from_hash: Found destination " + destination.toString());
			return destination;
		}
	}
#elif defined(DESTINATIONS_MAP)
	auto iter = _destinations.find(destination_hash);
	if (iter != _destinations.end()) {
		TRACE("Transport::find_destination_from_hash: Found destination " + (*iter).second.toString());
		return (*iter).second;
	}
#endif

	return {Type::NONE};
}

/*static*/ void Transport::cull_path_table() {
	if (_destination_table.size() > _path_table_maxsize) {
		// TODO prune by age, or better yet by last use
/*
		std::map<Bytes, DestinationEntry>::iterator iter = _destination_table.begin();
		// naively erase from front of table
		std::advance(iter, _destination_table.size() - _path_table_maxsize + 1);
		_destination_table.erase(_destination_table.begin(), iter);
*/
/*
		uint16_t count = 0;
		std::set<DestinationEntry> sorted_values;
		MapToValues(_destination_table, sorted_values);
		for (auto& destination_entry : sorted_values) {
			Packet announce_packet = destination_entry.announce_packet();
			TRACE("Transport::cull_path_table: Removing destination " + announce_packet.destination_hash().toHex() + " from path table");
			// Remove destination from path table
			if (_destination_table.erase(announce_packet.destination_hash()) < 1) {
				warning("Failed to remove destination " + announce_packet.destination_hash().toHex() + " from path table");
			}
			// Remove announce packet from packet table
			if (_packet_table.erase(destination_entry._announce_packet) < 1) {
				warning("Failed to remove packet " + destination_entry._announce_packet.toHex() + " from packet table");
			}
			// Remove cached packet file
			std::string packet_cache_path = Reticulum::_cachepath + "/" + destination_entry._announce_packet.toHex();
			if (OS::file_exists(packet_cache_path.c_str())) {
				OS::remove_file(packet_cache_path.c_str());
			}
			++count;
			if (_destination_table.size() <= _path_table_maxsize) {
				break;
			}
		}
		DEBUG("Removed " + std::to_string(count) + " path(s) from path table");
*/
		uint16_t count = 0;
		std::vector<std::pair<Bytes,DestinationEntry>> sorted_pairs;
		// Copy key/value pairs from map into vector
		std::for_each(_destination_table.begin(), _destination_table.end(), [&](const std::pair<const Bytes, DestinationEntry>& ref) {
			sorted_pairs.push_back(ref);
		});
		// Sort vector using specified comparator
		std::sort(sorted_pairs.begin(), sorted_pairs.end(), [](const std::pair<Bytes,DestinationEntry> &left, const std::pair<Bytes,DestinationEntry> &right) {
			return left.second._timestamp < right.second._timestamp;
		});
		// Iterate vector of sorted values
		for (auto& [destination_hash, destination_entry] : sorted_pairs) {
			TRACE("Transport::cull_path_table: Removing destination " + destination_hash.toHex() + " from path table");
			// Remove destination from path table
			if (_destination_table.erase(destination_hash) < 1) {
				warning("Failed to remove destination " + destination_hash.toHex() + " from path table");
			}
			// Remove announce packet from packet table
			if (_packet_table.erase(destination_entry._announce_packet) < 1) {
				warning("Failed to remove packet " + destination_entry._announce_packet.toHex() + " from packet table");
			}
			// Remove cached packet file
			std::string packet_cache_path = Reticulum::_cachepath + "/" + destination_entry._announce_packet.toHex();
			if (OS::file_exists(packet_cache_path.c_str())) {
				OS::remove_file(packet_cache_path.c_str());
			}
			++count;
			if (_destination_table.size() <= _path_table_maxsize) {
				break;
			}
		}
		DEBUG("Removed " + std::to_string(count) + " path(s) from path table");
	}
}