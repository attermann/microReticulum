#include "Transport.h"

#include "Reticulum.h"
#include "Destination.h"
#include "Identity.h"
#include "Packet.h"
#include "Interface.h"
#include "Log.h"
#include "Cryptography/Random.h"
#include "Utilities/OS.h"

#include <algorithm>
#include <unistd.h>
#include <time.h>

using namespace RNS;
using namespace RNS::Utilities;

///*static*/ std::set<std::reference_wrapper<const Interface>, std::less<const Interface>> Transport::_interfaces;
/*static*/ std::list<std::reference_wrapper<Interface>> Transport::_interfaces;
/*static*/ std::set<Destination> Transport::_destinations;
/*static*/ std::set<Link> Transport::_pending_links;
/*static*/ std::set<Link> Transport::_active_links;
/*static*/ std::set<Bytes> Transport::_packet_hashlist;
/*static*/ std::set<PacketReceipt> Transport::_receipts;

/*static*/ std::map<Bytes, Transport::AnnounceEntry> Transport::_announce_table;
/*static*/ std::map<Bytes, Transport::DestinationEntry> Transport::_destination_table;
/*static*/ std::map<Bytes, Transport::ReverseEntry> Transport::_reverse_table;
/*static*/ std::map<Bytes, Transport::LinkEntry> Transport::_link_table;
/*static*/ std::set<HAnnounceHandler> Transport::_announce_handlers;
/*static*/ std::set<Bytes> Transport::_path_requests;

/*static*/ uint16_t Transport::_max_pr_taXgxs			= 32000;

/*static*/ std::set<Destination> Transport::_control_destinations;
/*static*/ std::set<Bytes> Transport::_control_hashes;

/*static*/ std::set<Interface> Transport::_local_client_interfaces;

/*static*/ uint16_t Transport::_LOCAL_CLIENT_CACHE_MAXSIZE = 512;

/*static*/ uint64_t Transport::_start_time				= 0;
/*static*/ bool Transport::_jobs_locked					= false;
/*static*/ bool Transport::_jobs_running				= false;
/*static*/ uint32_t Transport::_job_interval			= 250;
/*static*/ uint64_t Transport::_links_last_checked		= 0;
/*static*/ uint32_t Transport::_links_check_interval	= 1000;
/*static*/ uint64_t Transport::_receipts_last_checked	= 0;
/*static*/ uint32_t Transport::_receipts_check_interval	= 1000;
/*static*/ uint64_t Transport::_announces_last_checked	= 0;
/*static*/ uint32_t Transport::_announces_check_interval= 1000;
/*static*/ uint32_t Transport::_hashlist_maxsize		= 1000000;
/*static*/ uint64_t Transport::_tables_last_culled		= 0;
/*static*/ uint32_t Transport::_tables_cull_interval	= 5000;

/*static*/ Reticulum Transport::_owner({Type::NONE});
/*static*/ Identity Transport::_identity({Type::NONE});

/*static*/ void Transport::start(const Reticulum& reticulum_instance) {
	_jobs_running = true;
	_owner = reticulum_instance;

	if (!_identity) {
		//ztransport_identity_path = Reticulum::storagepath+"/transport_identity"
		//zif (os.path.isfile(transport_identity_path)) {
		//z	identity = Identity.from_file(transport_identity_path);
		//z}

		if (!_identity) {
			verbose("No valid Transport Identity in storage, creating...");
			_identity = new Identity();
			//z_identity.to_file(transport_identity_path);
		}
		else {
			verbose("Loaded Transport Identity from storage");
		}
	}

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
				error("Could not load packet hashlist from storage, the contained exception was: " + e.what());
			}
		}
	}

	// Create transport-specific destinations
	Transport.path_request_destination = RNS.Destination(None, RNS.Destination.IN, RNS.Destination.PLAIN, Transport.APP_NAME, "path", "request")
	Transport.path_request_destination.set_packet_callback(Transport.path_request_handler)
	Transport.control_destinations.append(Transport.path_request_destination)
	Transport.control_hashes.append(Transport.path_request_destination.hash)

	Transport.tunnel_synthesize_destination = RNS.Destination(None, RNS.Destination.IN, RNS.Destination.PLAIN, Transport.APP_NAME, "tunnel", "synthesize")
	Transport.tunnel_synthesize_destination.set_packet_callback(Transport.tunnel_synthesize_handler)
	Transport.control_destinations.append(Transport.tunnel_synthesize_handler)
	Transport.control_hashes.append(Transport.tunnel_synthesize_destination.hash)
*/

	_jobs_running = false;

/*
	thread = threading.Thread(target=Transport.jobloop, daemon=True)
	thread.start()

	if RNS.Reticulum.transport_enabled():
		destination_table_path = RNS.Reticulum.storagepath+"/destination_table"
		tunnel_table_path = RNS.Reticulum.storagepath+"/tunnels"

		if os.path.isfile(destination_table_path) and not Transport.owner.is_connected_to_shared_instance:
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

				if len(Transport.destination_table) == 1:
					specifier = "entry"
				else:
					specifier = "entries"

				RNS.log("Loaded "+str(len(Transport.destination_table))+" path table "+specifier+" from storage", RNS.LOG_VERBOSE)

			except Exception as e:
				RNS.log("Could not load destination table from storage, the contained exception was: "+str(e), RNS.LOG_ERROR)

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

		if RNS.Reticulum.probe_destination_enabled():
			Transport.probe_destination = RNS.Destination(Transport.identity, RNS.Destination.IN, RNS.Destination.SINGLE, Transport.APP_NAME, "probe")
			Transport.probe_destination.accepts_links(False)
			Transport.probe_destination.set_proof_strategy(RNS.Destination.PROVE_ALL)
			Transport.probe_destination.announce()
			RNS.log("Transport Instance will respond to probe requests on "+str(Transport.probe_destination), RNS.LOG_NOTICE)
		else:
			Transport.probe_destination = None

		RNS.log("Transport instance "+str(Transport.identity)+" started", RNS.LOG_VERBOSE)
		Transport.start_time = time.time()

	// Synthesize tunnels for any interfaces wanting it
	for interface in Transport.interfaces:
		interface.tunnel_id = None
		if hasattr(interface, "wants_tunnel") and interface.wants_tunnel:
			Transport.synthesize_tunnel(interface)
*/
}

/*static*/ void Transport::jobloop() {
	while (true) {
		jobs();
		OS::sleep(_job_interval);
	}
}

/*static*/ void Transport::jobs() {

	std::vector<Packet> outgoing;
	std::vector<Bytes> path_requests;
	_jobs_running = true;

	try {
/*
		if (!Transport.jobs_locked) {

			// Process active and pending link lists
			if (time(nullptr) > _links_last_checked + Transport.links_check_interval) {

				for (auto& link : _pending_links) {
					if link.status() == RNS.Link.CLOSED:
						// If we are not a Transport Instance, finding a pending link
						// that was never activated will trigger an expiry of the path
						// to the destination, and an attempt to rediscover the path.
						if not RNS.Reticulum.transport_enabled():
							Transport.expire_path(link.destination.hash)

							// If we are connected to a shared instance, it will take
							// care of sending out a new path request. If not, we will
							// send one directly.
							if not Transport.owner.is_connected_to_shared_instance:
								last_path_request = 0
								if link.destination.hash in Transport.path_requests:
									last_path_request = Transport.path_requests[link.destination.hash]

								if time.time() - last_path_request > Transport.PATH_REQUEST_MI:
									RNS.log("Trying to rediscover path for "+RNS.prettyhexrep(link.destination.hash)+" since an attempted link was never established", RNS.LOG_DEBUG)
									if not link.destination.hash in path_requests:
										path_requests.append(link.destination.hash)

						Transport.pending_links.remove(link)
				}
				for (auto& link : _active_links) {
					if link.status == RNS.Link.CLOSED:
						Transport.active_links.remove(link)
				}

				_links_last_checked = time(NULL)

			// Process receipts list for timed-out packets
			if time.time() > Transport.receipts_last_checked+Transport.receipts_check_interval:
				while len(Transport.receipts) > Transport.MAX_RECEIPTS:
					culled_receipt = Transport.receipts.pop(0)
					culled_receipt.timeout = -1
					culled_receipt.check_timeout()

				for receipt in Transport.receipts:
					receipt.check_timeout()
					if receipt.status != RNS.PacketReceipt.SENT:
						if receipt in Transport.receipts:
							Transport.receipts.remove(receipt)

				Transport.receipts_last_checked = time.time()

			// Process announces needing retransmission
			if time.time() > Transport.announces_last_checked+Transport.announces_check_interval:
				for destination_hash in Transport.announce_table:
					announce_entry = Transport.announce_table[destination_hash]
					if announce_entry[2] > Transport.PATHFINDER_R:
						RNS.log("Completed announce processing for "+RNS.prettyhexrep(destination_hash)+", retry limit reached", RNS.LOG_EXTREME)
						Transport.announce_table.pop(destination_hash)
						break
					else:
						if time.time() > announce_entry[1]:
							announce_entry[1] = time.time() + Transport.PATHFINDER_G + Transport.PATHFINDER_RW
							announce_entry[2] += 1
							packet = announce_entry[5]
							block_rebroadcasts = announce_entry[7]
							attached_interface = announce_entry[8]
							announce_context = RNS.Packet.NONE
							if block_rebroadcasts:
								announce_context = RNS.Packet.PATH_RESPONSE
							announce_data = packet.data
							announce_identity = RNS.Identity.recall(packet.destination_hash)
							announce_destination = RNS.Destination(announce_identity, RNS.Destination.OUT, RNS.Destination.SINGLE, "unknown", "unknown");
							announce_destination.hash = packet.destination_hash
							announce_destination.hexhash = announce_destination.hash.hex()
							
							new_packet = RNS.Packet(
								announce_destination,
								announce_data,
								RNS.Packet.ANNOUNCE,
								context = announce_context,
								header_type = RNS.Packet.HEADER_2,
								transport_type = Transport.TRANSPORT,
								transport_id = Transport.identity.hash,
								attached_interface = attached_interface
							)

							new_packet.hops = announce_entry[4]
							if block_rebroadcasts:
								RNS.log("Rebroadcasting announce as path response for "+RNS.prettyhexrep(announce_destination.hash)+" with hop count "+str(new_packet.hops), RNS.LOG_DEBUG)
							else:
								RNS.log("Rebroadcasting announce for "+RNS.prettyhexrep(announce_destination.hash)+" with hop count "+str(new_packet.hops), RNS.LOG_DEBUG)
							
							outgoing.append(new_packet)

							// This handles an edge case where a peer sends a past
							// request for a destination just after an announce for
							// said destination has arrived, but before it has been
							// rebroadcast locally. In such a case the actual announce
							// is temporarily held, and then reinserted when the path
							// request has been served to the peer.
							if destination_hash in Transport.held_announces:
								held_entry = Transport.held_announces.pop(destination_hash)
								Transport.announce_table[destination_hash] = held_entry
								RNS.log("Reinserting held announce into table", RNS.LOG_DEBUG)

				Transport.announces_last_checked = time.time()


			// Cull the packet hashlist if it has reached its max size
			if len(Transport.packet_hashlist) > Transport.hashlist_maxsize:
				Transport.packet_hashlist = Transport.packet_hashlist[len(Transport.packet_hashlist)-Transport.hashlist_maxsize:len(Transport.packet_hashlist)-1]

			// Cull the path request tags list if it has reached its max size
			if len(Transport.discovery_pr_tags) > Transport.max_pr_tags:
				Transport.discovery_pr_tags = Transport.discovery_pr_tags[len(Transport.discovery_pr_tags)-Transport.max_pr_tags:len(Transport.discovery_pr_tags)-1]

			if time.time() > Transport.tables_last_culled + Transport.tables_cull_interval:
				// Cull the reverse table according to timeout
				stale_reverse_entries = []
				for truncated_packet_hash in Transport.reverse_table:
					reverse_entry = Transport.reverse_table[truncated_packet_hash]
					if time.time() > reverse_entry[2] + Transport.REVERSE_TIMEOUT:
						stale_reverse_entries.append(truncated_packet_hash)

				// Cull the link table according to timeout
				stale_links = []
				for link_id in Transport.link_table:
					link_entry = Transport.link_table[link_id]

					if link_entry[7] == True:
						if time.time() > link_entry[0] + Transport.LINK_TIMEOUT:
							stale_links.append(link_id)
					else:
						if time.time() > link_entry[8]:
							stale_links.append(link_id)

							last_path_request = 0
							if link_entry[6] in Transport.path_requests:
								last_path_request = Transport.path_requests[link_entry[6]]

							lr_taken_hops = link_entry[5]

							path_request_throttle = time.time() - last_path_request < Transport.PATH_REQUEST_MI
							path_request_conditions = False
							
							// If the path has been invalidated between the time of
							// making the link request and now, try to rediscover it
							if not Transport.has_path(link_entry[6]):
								RNS.log("Trying to rediscover path for "+RNS.prettyhexrep(link_entry[6])+" since an attempted link was never established, and path is now missing", RNS.LOG_DEBUG)
								path_request_conditions =True

							// If this link request was originated from a local client
							// attempt to rediscover a path to the destination, if this
							// has not already happened recently.
							elif not path_request_throttle and lr_taken_hops == 0:
								RNS.log("Trying to rediscover path for "+RNS.prettyhexrep(link_entry[6])+" since an attempted local client link was never established", RNS.LOG_DEBUG)
								path_request_conditions = True

							// If the link destination was previously only 1 hop
							// away, this likely means that it was local to one
							// of our interfaces, and that it roamed somewhere else.
							// In that case, try to discover a new path.
							elif not path_request_throttle and Transport.hops_to(link_entry[6]) == 1:
								RNS.log("Trying to rediscover path for "+RNS.prettyhexrep(link_entry[6])+" since an attempted link was never established, and destination was previously local to an interface on this instance", RNS.LOG_DEBUG)
								path_request_conditions = True

							// If the link destination was previously only 1 hop
							// away, this likely means that it was local to one
							// of our interfaces, and that it roamed somewhere else.
							// In that case, try to discover a new path.
							elif not path_request_throttle and lr_taken_hops == 1:
								RNS.log("Trying to rediscover path for "+RNS.prettyhexrep(link_entry[6])+" since an attempted link was never established, and link initiator is local to an interface on this instance", RNS.LOG_DEBUG)
								path_request_conditions = True

							if path_request_conditions:
								if not link_entry[6] in path_requests:
									path_requests.append(link_entry[6])

								if not RNS.Reticulum.transport_enabled():
									// Drop current path if we are not a transport instance, to
									// allow using higher-hop count paths or reused announces
									// from newly adjacent transport instances.
									Transport.expire_path(link_entry[6])

				// Cull the path table
				stale_paths = []
				for destination_hash in Transport.destination_table:
					destination_entry = Transport.destination_table[destination_hash]
					attached_interface = destination_entry[5]

					if attached_interface != None and hasattr(attached_interface, "mode") and attached_interface.mode == RNS.Interfaces.Interface.Interface.MODE_ACCESS_POINT:
						destination_expiry = destination_entry[0] + Transport.AP_PATH_TIME
					elif attached_interface != None and hasattr(attached_interface, "mode") and attached_interface.mode == RNS.Interfaces.Interface.Interface.MODE_ROAMING:
						destination_expiry = destination_entry[0] + Transport.ROAMING_PATH_TIME
					else:
						destination_expiry = destination_entry[0] + Transport.DESTINATION_TIMEOUT

					if time.time() > destination_expiry:
						stale_paths.append(destination_hash)
						RNS.log("Path to "+RNS.prettyhexrep(destination_hash)+" timed out and was removed", RNS.LOG_DEBUG)
					elif not attached_interface in Transport.interfaces:
						stale_paths.append(destination_hash)
						RNS.log("Path to "+RNS.prettyhexrep(destination_hash)+" was removed since the attached interface no longer exists", RNS.LOG_DEBUG)

				// Cull the pending discovery path requests table
				stale_discovery_path_requests = []
				for destination_hash in Transport.discovery_path_requests:
					entry = Transport.discovery_path_requests[destination_hash]

					if time.time() > entry["timeout"]:
						stale_discovery_path_requests.append(destination_hash)
						RNS.log("Waiting path request for "+RNS.prettyhexrep(destination_hash)+" timed out and was removed", RNS.LOG_DEBUG)

				// Cull the tunnel table
				stale_tunnels = []
				ti = 0
				for tunnel_id in Transport.tunnels:
					tunnel_entry = Transport.tunnels[tunnel_id]

					expires = tunnel_entry[3]
					if time.time() > expires:
						stale_tunnels.append(tunnel_id)
						RNS.log("Tunnel "+RNS.prettyhexrep(tunnel_id)+" timed out and was removed", RNS.LOG_EXTREME)
					else:
						stale_tunnel_paths = []
						tunnel_paths = tunnel_entry[2]
						for tunnel_path in tunnel_paths:
							tunnel_path_entry = tunnel_paths[tunnel_path]

							if time.time() > tunnel_path_entry[0] + Transport.DESTINATION_TIMEOUT:
								stale_tunnel_paths.append(tunnel_path)
								RNS.log("Tunnel path to "+RNS.prettyhexrep(tunnel_path)+" timed out and was removed", RNS.LOG_EXTREME)

						for tunnel_path in stale_tunnel_paths:
							tunnel_paths.pop(tunnel_path)
							ti += 1


				if ti > 0:
					if ti == 1:
						RNS.log("Removed "+str(ti)+" tunnel path", RNS.LOG_EXTREME)
					else:
						RNS.log("Removed "+str(ti)+" tunnel paths", RNS.LOG_EXTREME)


				
				i = 0
				for truncated_packet_hash in stale_reverse_entries:
					Transport.reverse_table.pop(truncated_packet_hash)
					i += 1

				if i > 0:
					if i == 1:
						RNS.log("Released "+str(i)+" reverse table entry", RNS.LOG_EXTREME)
					else:
						RNS.log("Released "+str(i)+" reverse table entries", RNS.LOG_EXTREME)

				

				i = 0
				for link_id in stale_links:
					Transport.link_table.pop(link_id)
					i += 1

				if i > 0:
					if i == 1:
						RNS.log("Released "+str(i)+" link", RNS.LOG_EXTREME)
					else:
						RNS.log("Released "+str(i)+" links", RNS.LOG_EXTREME)

				i = 0
				for destination_hash in stale_paths:
					Transport.destination_table.pop(destination_hash)
					i += 1

				if i > 0:
					if i == 1:
						RNS.log("Removed "+str(i)+" path", RNS.LOG_EXTREME)
					else:
						RNS.log("Removed "+str(i)+" paths", RNS.LOG_EXTREME)

				i = 0
				for destination_hash in stale_discovery_path_requests:
					Transport.discovery_path_requests.pop(destination_hash)
					i += 1

				if i > 0:
					if i == 1:
						RNS.log("Removed "+str(i)+" waiting path request", RNS.LOG_EXTREME)
					else:
						RNS.log("Removed "+str(i)+" waiting path requests", RNS.LOG_EXTREME)

				i = 0
				for tunnel_id in stale_tunnels:
					Transport.tunnels.pop(tunnel_id)
					i += 1

				if i > 0:
					if i == 1:
						RNS.log("Removed "+str(i)+" tunnel", RNS.LOG_EXTREME)
					else:
						RNS.log("Removed "+str(i)+" tunnels", RNS.LOG_EXTREME)

				Transport.tables_last_culled = time.time()

		else:
			// Transport jobs were locked, do nothing
			pass
*/
	}
	catch (std::exception& e) {
		error("An exception occurred while running Transport jobs.");
		error("The contained exception was: " + std::string(e.what()));
	}

	_jobs_running = false;

	for (auto& packet : outgoing) {
		packet.send();
	}

	for (auto& destination_hash : path_requests) {
		request_path(destination_hash);
	}
}

/*static*/ void Transport::transmit(Interface& interface, const Bytes& raw) {
	try {
		//if hasattr(interface, "ifac_identity") and interface.ifac_identity != None:
		if (interface.ifac_identity()) {
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
			interface.processOutgoing(masked_raw)
*/
		}
		else {
			interface.processOutgoing(raw);
		}
	}
	catch (std::exception& e) {
		error("Error while transmitting on " + interface.toString() + ". The contained exception was: " + e.what());
	}
}

/*static*/ bool Transport::outbound(Packet& packet) {
	extreme("Transport::outbound()");

	extreme("Transport::outbound: destination=" + packet.destination_hash().toHex() + " hops=" + std::to_string(packet.hops()));

	while (_jobs_running) {
		extreme("Transport::outbound: sleeping...");
		OS::sleep(5);
	}

	_jobs_locked = true;

	bool sent = false;
	uint64_t outbound_time = OS::time();

	// Check if we have a known path for the destination in the path table
    //if packet.packet_type != RNS.Packet.ANNOUNCE and packet.destination.type != RNS.Destination.PLAIN and packet.destination.type != RNS.Destination.GROUP and packet.destination_hash in Transport.destination_table:
	if (packet.packet_type() != Type::Packet::ANNOUNCE && packet.destination().type() != Type::Destination::PLAIN && packet.destination().type() != Type::Destination::GROUP && _destination_table.find(packet.destination_hash()) != _destination_table.end()) {
		extreme("Transport::outbound: Path to destination is known");
        //outbound_interface = Transport.destination_table[packet.destination_hash][5]
		DestinationEntry destination_entry = (*_destination_table.find(packet.destination_hash())).second;
		Interface& outbound_interface = destination_entry._receiving_interface;

		// If there's more than one hop to the destination, and we know
		// a path, we insert the packet into transport by adding the next
		// transport nodes address to the header, and modifying the flags.
		// This rule applies both for "normal" transport, and when connected
		// to a local shared Reticulum instance.
        //if Transport.destination_table[packet.destination_hash][2] > 1:
		if (destination_entry._hops > 1) {
			extreme("Forwarding packet to next closest interface...");
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
			extreme("Transport::outbound: Sending packet for directly connected interface to shared instance...");
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
			extreme("Transport::outbound: Sending packet over directly connected interface...");
			transmit(outbound_interface, packet.raw());
			sent = true;
		}
	}
	// If we don't have a known path for the destination, we'll
	// broadcast the packet on all outgoing interfaces, or the
	// just the relevant interface if the packet has an attached
	// interface, or belongs to a link.
	else {
		extreme("Transport::outbound: Path to destination is unknown");
		bool stored_hash = false;
		for (const Interface& interface : _interfaces) {
			extreme("Transport::outbound: Checking interface " + interface.toString());
			if (interface.OUT()) {
				bool should_transmit = true;

				if (packet.destination().type() == Type::Destination::LINK) {
					if (packet.destination().status() == Type::Link::CLOSED) {
						should_transmit = false;
					}
					// CBA Destination has no member attached_interface
					//zif (interface != packet.destination().attached_interface()) {
					//z	should_transmit = false;
					//z}
				}
				
				if (packet.attached_interface() && interface != packet.attached_interface()) {
					should_transmit = false;
				}

				if (packet.packet_type() == Type::Packet::ANNOUNCE) {
					if (!packet.attached_interface()) {
						extreme("Transport::outbound: Packet has no attached interface");
						if (interface.mode() == Type::Interface::MODE_ACCESS_POINT) {
							extreme("Blocking announce broadcast on " + interface.toString() + " due to AP mode");
							should_transmit = false;
						}
						else if (interface.mode() == Type::Interface::MODE_ROAMING) {
							//local_destination = next((d for d in Transport.destinations if d.hash == packet.destination_hash), None)
							//Destination local_destination({Type::NONE});
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
								//extreme("Allowing announce broadcast on roaming-mode interface from instance-local destination");
							}
							else {
								const Interface& from_interface = next_hop_interface(packet.destination_hash());
								//if from_interface == None or not hasattr(from_interface, "mode"):
								if (!from_interface || from_interface.mode() == Type::Interface::MODE_NONE) {
									should_transmit = false;
									if (!from_interface) {
										extreme("Blocking announce broadcast on " + interface.toString() + " since next hop interface doesn't exist");
									}
									else if (from_interface.mode() == Type::Interface::MODE_NONE) {
										extreme("Blocking announce broadcast on " + interface.toString() + " since next hop interface has no mode configured");
									}
								}
								else {
									if (from_interface.mode() == Type::Interface::MODE_ROAMING) {
										extreme("Blocking announce broadcast on " + interface.toString() + " due to roaming-mode next-hop interface");
										should_transmit = false;
									}
									else if (from_interface.mode() == Type::Interface::MODE_BOUNDARY) {
										extreme("Blocking announce broadcast on " + interface.toString() + " due to boundary-mode next-hop interface");
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
							//Destination local_destination({Typeestination::NONE});
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
								//extreme("Allowing announce broadcast on boundary-mode interface from instance-local destination");
							}
							else {
								const Interface& from_interface = next_hop_interface(packet.destination_hash());
								if (!from_interface || from_interface.mode() == Type::Interface::MODE_NONE) {
									should_transmit = false;
									if (!from_interface) {
										extreme("Blocking announce broadcast on " + interface.toString() + " since next hop interface doesn't exist");
									}
									else if (from_interface.mode() == Type::Interface::MODE_NONE) {
										extreme("Blocking announce broadcast on "  + interface.toString() + " since next hop interface has no mode configured");
									}
								}
								else {
									if (from_interface.mode() == Type::Interface::MODE_ROAMING) {
										extreme("Blocking announce broadcast on " + interface.toString() + " due to roaming-mode next-hop interface");
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
									uint16_t tx_time = (packet.raw().size() * 8) / interface.bitrate();
									uint16_t wait_time = (tx_time / interface.announce_cap());
									const_cast<Interface&>(interface).announce_allowed_at(outbound_time + wait_time);
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
											Interface::AnnounceEntry entry(
												packet.destination_hash(),
												outbound_time,
												packet.hops(),
												announce_emitted(packet),
												packet.raw()
											);

											queued_announces = (interface.announce_queue().size() > 0);
											const_cast<Interface&>(interface).add_announce(entry);

											if (!queued_announces) {
												uint64_t wait_time = std::max(interface.announce_allowed_at() - OS::time(), (uint64_t)0);
												// CBA TODO THREAD?
												//ztimer = threading.Timer(wait_time, interface.process_announce_queue)
												//ztimer.start()

												std::string wait_time_str;
												if (wait_time < 1000) {
													wait_time_str = std::to_string(wait_time) + "ms";
												}
												else {
													wait_time_str = std::to_string(OS::round(wait_time/1000,1)) + "s";
												}

												std::string ql_str = std::to_string(interface.announce_queue().size());
												extreme("Added announce to queue (height " + ql_str + ") on " + interface.toString() + " for processing in " + wait_time_str);
											}
											else {
												uint64_t wait_time = std::max(interface.announce_allowed_at() - OS::time(), (uint64_t)0);

												std::string wait_time_str;
												if (wait_time < 1000) {
													wait_time_str = std::to_string(wait_time) + "ms";
												}
												else {
													wait_time_str = std::to_string(OS::round(wait_time/1000,2)) + "s";
												}

												std::string ql_str = std::to_string(interface.announce_queue().size());
												extreme("Added announce to queue (height " + ql_str + ") on " + interface.toString() + " for processing in " + wait_time_str);
											}
										}
									}
									else {
										// future
									}
								}
							}
							else {
								// future
							}
						}
					}
				}
						
				if (should_transmit) {
					extreme("Transport::outbound: Packet transmission allowed");
					if (!stored_hash) {
						_packet_hashlist.insert(packet.packet_hash());
						stored_hash = true;
					}

					// TODO: Re-evaluate potential for blocking
					// def send_packet():
					//     Transport.transmit(const_cast<Interface&>(interface), packet.raw)
					// thread = threading.Thread(target=send_packet)
					// thread.daemon = True
					// thread.start()

					transmit(const_cast<Interface&>(interface), packet.raw());
					sent = true;
				}
				else {
					extreme("Transport::outbound: Packet transmission refused");
				}
			}
		}
	}

	if (sent) {
		packet.sent(true);
		packet.sent_at(time(nullptr));

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
			_receipts.insert(receipt);
		}
		
		cache(packet);
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
				debug("Dropped PLAIN packet " + packet.packet_hash().toHex() + " with " + std::to_string(packet.hops()) + " hops");
				return false;
			}
			else {
				return true;
			}
		}
		else {
			debug("Dropped invalid PLAIN announce packet");
			return false;
		}
	}

	if (packet.destination_type() == Type::Destination::GROUP) {
		if (packet.packet_type() != Type::Packet::ANNOUNCE) {
			if (packet.hops() > 1) {
				debug("Dropped GROUP packet " + packet.packet_hash().toHex() + " with " + std::to_string(packet.hops()) + " hops");
				return false;
			}
			else {
				return true;
			}
		}
		else {
			debug("Dropped invalid GROUP announce packet");
			return false;
		}
	}

	if (_packet_hashlist.find(packet.packet_hash()) == _packet_hashlist.end()) {
		return true;
	}
	else {
		if (packet.packet_type() == Type::Packet::ANNOUNCE) {
			if (packet.destination_type() == Type::Destination::SINGLE) {
				return true;
			}
			else {
				debug("Dropped invalid announce packet");
				return false;
			}
		}
	}

	extreme("Filtered packet with hash " + packet.packet_hash().toHex());
	return false;
}

/*static*/ void Transport::inbound(const Bytes& raw, const Interface& interface /*= {Type::NONE}*/) {
	extreme("Transport::inbound()");
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
		extreme("Transport::inbound: sleeping...");
		OS::sleep(5);
	}

	if (!_identity) {
		warning("Transport::inbound: No identity!");
		return;
	}

	_jobs_locked = true;

	Packet packet({Type::NONE}, raw);
	if (!packet.unpack()) {
		warning("Transport::inbound: Pscket unpack failed!");
		return;
	}

	extreme("Transport::inbound: destination=" + packet.destination_hash().toHex() + " hops=" + std::to_string(packet.hops()));

	packet.receiving_interface(interface);
	packet.hops(packet.hops() + 1);

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

	if (packet_filter(packet)) {
		extreme("Transport::inbound: Packet accepted by filter");
		_packet_hashlist.insert(packet.packet_hash());
		cache(packet);
		
		// Check special conditions for local clients connected
		// through a shared Reticulum instance
		//from_local_client         = (packet.receiving_interface in Transport.local_client_interfaces)
		bool from_local_client         = (_local_client_interfaces.find(packet.receiving_interface()) != _local_client_interfaces.end());
		//for_local_client          = (packet.packet_type != RNS.Packet.ANNOUNCE) and (packet.destination_hash in Transport.destination_table and Transport.destination_table[packet.destination_hash][2] == 0)
		//for_local_client_link     = (packet.packet_type != RNS.Packet.ANNOUNCE) and (packet.destination_hash in Transport.link_table and Transport.link_table[packet.destination_hash][4] in Transport.local_client_interfaces)
		//for_local_client_link    |= (packet.packet_type != RNS.Packet.ANNOUNCE) and (packet.destination_hash in Transport.link_table and Transport.link_table[packet.destination_hash][2] in Transport.local_client_interfaces)
		bool for_local_client = false;
		bool for_local_client_link = false;
		if (packet.packet_type() != Type::Packet::ANNOUNCE) {
			auto destination_iter = _destination_table.find(packet.destination_hash());
			if (destination_iter != _destination_table.end()) {
				DestinationEntry destination_entry = (*destination_iter).second;
			 	if (destination_entry._hops == 0) {
					for_local_client = true;
				}
			}
			auto link_iter = _link_table.find(packet.destination_hash());
			if (link_iter != _link_table.end()) {
				LinkEntry link_entry = (*link_iter).second;
			 	if (_local_client_interfaces.find(link_entry._receiving_interface) != _local_client_interfaces.end()) {
					for_local_client_link = true;
				}
			 	if (_local_client_interfaces.find(link_entry._outbound_interface) != _local_client_interfaces.end()) {
					for_local_client_link = true;
				}
			}
		}
		//proof_for_local_client    = (packet.destination_hash in Transport.reverse_table) and (Transport.reverse_table[packet.destination_hash][0] in Transport.local_client_interfaces)
		bool proof_for_local_client = false;
		auto reverse_iter = _reverse_table.find(packet.destination_hash());
		if (reverse_iter != _reverse_table.end()) {
			ReverseEntry reverse_entry = (*reverse_iter).second;
			if (_local_client_interfaces.find(reverse_entry._receiving_interface) != _local_client_interfaces.end()) {
				proof_for_local_client = true;
			}
		}

		// Plain broadcast packets from local clients are sent
		// directly on all attached interfaces, since they are
		// never injected into transport.
		if (_control_hashes.find(packet.destination_hash()) == _control_hashes.end()) {
			if (packet.destination_type() == Type::Destination::PLAIN && packet.transport_type() == Type::Transport::BROADCAST) {
				// Send to all interfaces except the originator
				if (from_local_client) {
					for (const Interface& interface : _interfaces) {
						if (interface != packet.receiving_interface()) {
							extreme("Transport::inbound: Broadcasting packet on " + interface.toString());
							transmit(const_cast<Interface&>(interface), packet.raw());
						}
					}
				}
				// If the packet was not from a local client, send
				// it directly to all local clients
				else {
					for (auto& interface : _local_client_interfaces) {
						extreme("Transport::inbound: Broadcasting packet on " + interface.toString());
						transmit(const_cast<Interface&>(interface), packet.raw());
					}
				}
			}
		}

		// General transport handling. Takes care of directing
		// packets according to transport tables and recording
		// entries in reverse and link tables.
		if (Reticulum::transport_enabled() || from_local_client or for_local_client or for_local_client_link) {
			extreme("Transport::inbound: Performing general transport handling");

			// If there is no transport id, but the packet is
			// for a local client, we generate the transport
			// id (it was stripped on the previous hop, since
			// we "spoof" the hop count for clients behind a
			// shared instance, so they look directly reach-
			// able), and reinsert, so the normal transport
			// implementation can handle the packet.
			if (!packet.transport_id() && for_local_client) {
				packet.transport_id(_identity.hash());
			}

			// If this is a cache request, and we can fullfill
			// it, do so and stop processing. Otherwise resume
			// normal processing.
			if (packet.context() == Type::Packet::CACHE_REQUEST) {
				if (cache_request_packet(packet)) {
					extreme("Transport::inbound: Cached packet");
					return;
				}
			}

			// If the packet is in transport, check whether we
			// are the designated next hop, and process it
			// accordingly if we are.
			if (packet.transport_id() && packet.packet_type() != Type::Packet::ANNOUNCE) {
				if (packet.transport_id() == _identity.hash()) {
					auto destination_iter = _destination_table.find(packet.destination_hash());
					if (destination_iter != _destination_table.end()) {
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

						Interface& outbound_interface = destination_entry._receiving_interface;

						if (packet.packet_type() == Type::Packet::LINKREQUEST) {
							uint64_t now = OS::time();
							uint64_t proof_timeout = now + Type::Link::ESTABLISHMENT_TIMEOUT_PER_HOP*1000 * std::max((uint8_t)1, remaining_hops);
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
							ReverseEntry reverse_entry(
								packet.receiving_interface(),
								outbound_interface,
								OS::time()
							);
							_reverse_table.insert({packet.getTruncatedHash(), reverse_entry});
						}
						transmit(const_cast<Interface&>(outbound_interface), new_raw);
						destination_entry._timestamp = OS::time();
					}
					else {
						// TODO: There should probably be some kind of REJECT
						// mechanism here, to signal to the source that their
						// expected path failed.
						extreme("Got packet in transport, but no known path to final destination " + packet.destination_hash().toHex() + ". Dropping packet.");
					}
				}
			}

			// Link transport handling. Directs packets according
			// to entries in the link tables
			if (packet.packet_type() != Type::Packet::ANNOUNCE && packet.packet_type() != Type::Packet::LINKREQUEST && packet.context() == Type::Packet::LRPROOF) {
				auto link_iter = _link_table.find(packet.destination_hash());
				if (link_iter != _link_table.end()) {
					LinkEntry link_entry = (*link_iter).second;
					// If receiving and outbound interface is
					// the same for this link, direction doesn't
					// matter, and we simply send the packet on.
					Interface outbound_interface({Type::NONE});
					if (link_entry._outbound_interface == link_entry._receiving_interface) {
						// But check that taken hops matches one
						// of the expectede values.
						if (packet.hops() == link_entry._remaining_hops || packet.hops() == link_entry._hops) {
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
								outbound_interface = link_entry._receiving_interface;
							}
						}
						else if (packet.receiving_interface() == link_entry._receiving_interface) {
							// Also check that expected hop count matches
							if (packet.hops() == link_entry._hops) {
								outbound_interface = link_entry._outbound_interface;
							}
						}
					}

					if (outbound_interface) {
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
						// future
					}
				}
			}
		}

		// Announce handling. Handles logic related to incoming
		// announces, queueing rebroadcasts of these, and removal
		// of queued announce rebroadcasts once handed to the next node.
		if (packet.packet_type() == Type::Packet::ANNOUNCE) {
			extreme("Transport::inbound: Packet is ANNOUNCE");
			Bytes received_from;
			//p local_destination = next((d for d in Transport.destinations if d.hash == packet.destination_hash), None)
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
				extreme("Transport::inbound: Packet is announce for non-local destination, processing...");
				if (packet.transport_id()) {
					received_from = packet.transport_id();
					
					// Check if this is a next retransmission from
					// another node. If it is, we're removing the
					// announce in question from our pending table
					if (Reticulum::transport_enabled() && _announce_table.count(packet.destination_hash()) > 0) {
						//AnnounceEntry& announce_entry = _announce_table[packet.destination_hash()];
						AnnounceEntry& announce_entry = (*_announce_table.find(packet.destination_hash())).second;
						
						if ((packet.hops() - 1) == announce_entry._hops) {
							debug("Heard a local rebroadcast of announce for " + packet.destination_hash().toHex());
							announce_entry._local_rebroadcasts += 1;
							if (announce_entry._local_rebroadcasts >= Transport::LOCAL_REBROADCASTS_MAX) {
								debug("Max local rebroadcasts of announce for " + packet.destination_hash().toHex() + " reached, dropping announce from our table");
								_announce_table.erase(packet.destination_hash());
							}
						}

						if ((packet.hops() - 1) == (announce_entry._hops + 1) && announce_entry._retries > 0) {
							uint64_t now = OS::time();
							if (now < announce_entry._timestamp) {
								debug("Rebroadcasted announce for " + packet.destination_hash().toHex() + " has been passed on to another node, no further tries needed");
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
				//if (not any(packet.destination_hash == d.hash for d in Transport.destinations) and packet.hops < Transport.PATHFINDER_M+1):
				bool found_local = false;
				for (auto& destination : _destinations) {
					if (destination.hash() == packet.destination_hash()) {
						found_local = true;
						break;
					}
				}
				if (!found_local && packet.hops() < (Transport::PATHFINDER_M+1)) {
					extreme("Transport::inbound: Packet is announce for non-local destination, processing...");
/*
					uint64_t announce_emitted = Transport::announce_emitted(packet);
					
					//prandom_blob = packet.data[RNS.Identity.KEYSIZE//8+RNS.Identity.NAME_HASH_LENGTH//8:RNS.Identity.KEYSIZE//8+RNS.Identity.NAME_HASH_LENGTH//8+10]
					Bytes random_blob = packet.data().mid(Identity::KEYSIZE/8 + Identity::NAME_HASH_LENGTH/8, 10);
					//prandom_blobs = []
					auto iter = _destination_table.find(packet.destination_hash());
					if (iter != _destination_table.end()) {
						DestinationEntry destination_entry = (*iter).second;
						//prandom_blobs = Transport.destination_table[packet.destination_hash][4]

						// If we already have a path to the announced
						// destination, but the hop count is equal or
						// less, we'll update our tables.
						if (packet.hops() <= destination_entry._hops) {
							// Make sure we haven't heard the random
							// blob before, so announces can't be
							// replayed to forge paths.
							// TODO: Check whether this approach works
							// under all circumstances
							//pif not random_blob in random_blobs:
							if (destination_entry._random_blobs.find(random_blob) == destination_entry._random_blobs.end()) {
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
							uint64_t now = OS::time();
							uint64_t path_expires = destination_entry._expires;
							
							uint64_t path_announce_emitted = 0;
							for (const Bytes& path_random_blob : destination_entry._random_blobs) {
								//path_announce_emitted = std::max(path_announce_emitted, int.from_bytes(path_random_blob[5:10], "big"))
								//zpath_announce_emitted = std::max(path_announce_emitted, int.from_bytes(path_random_blob[5:10], "big"))
								if (path_announce_emitted >= announce_emitted) {
									break;
								}
							}

							if (now >= path_expires) {
								// We also check that the announce is
								// different from ones we've already heard,
								// to avoid loops in the network
								if (destination_entry._random_blobs.find(random_blob) == destination_entry._random_blobs.end()) {
									// TODO: Check that this ^ approach actually
									// works under all circumstances
									debug("Replacing destination table entry for " + packet.destination_hash().toHex() + " with new announce due to expired path");
									should_add = true;
								}
								else {
									should_add = false;
								}
							}
							else {
								if (announce_emitted > path_announce_emitted) {
									if (destination_entry._random_blobs.find(random_blob) == destination_entry._random_blobs.end()) {
										debug("Replacing destination table entry for " + packet.destination_hash().toHex() + " with new announce, since it was more recently emitted");
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
						uint64_t now = OS::time();

						bool rate_blocked = false;


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


						uint8_t retries = 0;
						uint8_t announce_hops = packet.hops();
						uint8_t local_rebroadcasts = 0;
						bool block_rebroadcasts = false;
						Interface attached_interface = {Type::NONE};
						
						uint64_t retransmit_timeout = now + (RNS::Cryptography::random() * Transport::PATHFINDER_RW);

						uint64_t expires;
						if (packet.receiving_interface().mode() == Type::Interface::MODE_ACCESS_POINT) {
							expires = now + Transport::AP_PATH_TIME;
						}
						else if (packet.receiving_interface().mode() == Type::Interface::MODE_ROAMING) {
							expires = now + Transport::ROAMING_PATH_TIME;
						}
						else {
							expires = now + Transport::PATHFINDER_E;
						}

						std::set<Bytes> random_blobs;
						random_blobs.insert(random_blob);

						if (Reticulum::transport_enabled() || from_local_client(packet) && packet.context() != Type::Packet::PATH_RESPONSE) {
							// Insert announce into announce table for retransmission

							if (rate_blocked) {
								debug("Blocking rebroadcast of announce from " + packet.destination_hash().toHex() + " due to excessive announce rate");
							}
							else {
								if (from_local_client(packet)) {
									// If the announce is from a local client,
									// it is announced immediately, but only one time.
									retransmit_timeout = now;
									retries = Transport::PATHFINDER_R;
								}
								_announce_table[packet.destination_hash] = [
									now,
									retransmit_timeout,
									retries,
									received_from,
									announce_hops,
									packet,
									local_rebroadcasts,
									block_rebroadcasts,
									attached_interface
								];
							}
						}
						// TODO: Check from_local_client once and store result
						else if (from_local_client(packet) && packet.context() == Type::Packet::PATH_RESPONSE) {
							// If this is a path response from a local client,
							// check if any external interfaces have pending
							// path requests.
							//pif packet.destination_hash in Transport.pending_local_path_requests:
							auto iter = pending_local_path_requests.find(packet.destination_hash());
							if (iter != _destination_table.end()) {
								DestinationEntry destination_entry = (*iter).second;
								desiring_interface = _pending_local_path_requests.pop(packet.destination_hash());
								retransmit_timeout = now;
								retries = Transport::PATHFINDER_R;

								Transport.announce_table[packet.destination_hash] = [
									now,
									retransmit_timeout,
									retries,
									received_from,
									announce_hops,
									packet,
									local_rebroadcasts,
									block_rebroadcasts,
									attached_interface
								];

						// If we have any local clients connected, we re-
						// transmit the announce to them immediately
						if (_local_client_interfaces.size() > 0) {
							announce_identity = Identity::recall(packet.destination_hash());
							announce_destination = Destination(announce_identity, Destination.OUT, Destination.SINGLE, "unknown", "unknown");
							announce_destination.hash(packet.destination_hash());
							announce_destination.hexhash = announce_destination.hash().toHex();
							announce_context = {Type::NONE};
							announce_data = packet.data();

							// TODO: Shouldn't the context be PATH_RESPONSE in the first case here?
							if (from_local_client(packet) && packet.context() == Packet.PATH_RESPONSE) {
								for (auto& local_interface : _local_client_interfaces) {
									if packet.receiving_interface() != local_interface) {
										Packet new_announce(
											announce_destination,
											announce_data,
											Type::Packet::ANNOUNCE,
											context = announce_context,
											header_type = RNS.Packet.HEADER_2,
											transport_type = Transport.TRANSPORT,
											transport_id = Transport.identity.hash,
											attached_interface = local_interface
										);
										
										new_announce.hops(packet.hops);
										new_announce.send();
									}
								}
							}
							else {
								for (auto& local_interface : _local_client_interfaces) {
									if (packet.receiving_interface() != local_interface) {
										Packet new_announce(
											announce_destination,
											announce_data,
											Type::Packet::ANNOUNCE,
											context = announce_context,
											header_type = RNS.Packet.HEADER_2,
											transport_type = Transport.TRANSPORT,
											transport_id = Transport.identity.hash,
											attached_interface = local_interface
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
							PathRequestEntry pr_entry = (*iter).second;
							attached_interface = pr_entry._requesting_interface;

							interface_str = " on " + attached_interface.toString();

							debug("Got matching announce, answering waiting discovery path request for " + packet.destination_hash().toHex() + interface_str);
							announce_identity = Identity::recall(packet.destination_hash());
							Destination announce_destination(announce_identity, RNS.Destination.OUT, RNS.Destination.SINGLE, "unknown", "unknown");
							announce_destination.hash(packet.destination_hash());
							announce_destination.hexhash = announce_destination.hash().toHex();
							announce_context = {Type::NONE};
							announce_data = packet.data();

							Packet new_announce(
								announce_destination,
								announce_data,
								Type::Packet::ANNOUNCE,
								context = Type::Packet::PATH_RESPONSE,
								header_type = Type::Packet::HEADER_2,
								transport_type = Type::Transport::TRANSPORT,
								transport_id = _identity.hash(),
								attached_interface = attached_interface
							);

							new_announce.hops(packet.hops());
							new_announce.send();
						}

						DestinationEntry destination_table_entry(
							now,
							received_from,
							announce_hops,
							expires,
							random_blobs,
							packet.receiving_interface(),
							packet
						);
						_destination_table.insert({packet.destination_hash(), destination_table_entry});
						debug("Destination " + packet.destination_hash().toHex() + " is now " + std::to_string(announce_hops) + " hops away via " + received_from.toHex() + " on " + packet.receiving_interface().toString());

						// If the receiving interface is a tunnel, we add the
						// announce to the tunnels table
						if (packet.receiving_interface().tunnel_id()) {
							tunnel_entry = Transport.tunnels[packet.receiving_interface.tunnel_id];
							paths = tunnel_entry[2]
							paths[packet.destination_hash] = destination_table_entry
							expires = OS::time() + Transport::DESTINATION_TIMEOUT;
							tunnel_entry[3] = expires
							debug("Path to " + packet.destination_hash().toHex() + " associated with tunnel " + packet.receiving_interface().tunnel_id().toHex());
						}

						// Call externally registered callbacks from apps
						// wanting to know when an announce arrives
						if (packet.context() != Type::Packet::PATH_RESPONSE) {
							for (auto& handler : Transport.announce_handlers) {
								try {
									// Check that the announced destination matches
									// the handlers aspect filter
									execute_callback = false;
									announce_identity = Identity::recall(packet.destination_hash());
									if (handler.aspect_filter == None) {
										// If the handlers aspect filter is set to
										// None, we execute the callback in all cases
										execute_callback = true;
									}
									else {
										handler_expected_hash = RNS.Destination.hash_from_name_and_identity(handler.aspect_filter, announce_identity)
										if (packet.destination_hash() == handler_expected_hash() ) {
											execute_callback = true;
										}
									}
									if (execute_callback) {
										handler.received_announce(
											destination_hash=packet.destination_hash(),
											announced_identity=announce_identity,
											app_data=Identity::recall_app_data(packet.destination_hash())
										);
									}
								}
								catch (std::exception& e) {
									error("Error while processing external announce callback.");
									error("The contained exception was: " + e.what();
								}
							}
						}
					}
*/
				}
				else {
					extreme("Transport::inbound: Packet is announce for local destination, not processing");
				}
			}
			else {
				extreme("Transport::inbound: Packet is announce for local destination, not processing");
			}
		}

		// Handling for link requests to local destinations
		else if (packet.packet_type() == Type::Packet::LINKREQUEST) {
			extreme("Transport::inbound: Packet is LINKREQUEST");
			if (!packet.transport_id() || packet.transport_id() == _identity.hash()) {
				for (auto& destination : _destinations) {
					if (destination.hash() == packet.destination_hash() && destination.type() == packet.destination_type()) {
						packet.destination(destination);
						// CBA iterator over std::set is always const so need to make temporarily mutable
						//destination.receive(packet);
						const_cast<Destination&>(destination).receive(packet);
					}
				}
			}
		}
		
		// Handling for local data packets
		else if (packet.packet_type() == Type::Packet::DATA) {
			extreme("Transport::inbound: Packet is DATA");
			if (packet.destination_type() == Type::Destination::LINK) {
				for (auto& link : _active_links) {
					if (link.link_id() == packet.destination_hash()) {
						packet.link(link);
						const_cast<Link&>(link).receive(packet);
					}
				}
			}
			else {
				for (auto& destination : _destinations) {
					if (destination.hash() == packet.destination_hash() && destination.type() == packet.destination_type()) {
						packet.destination(destination);
						const_cast<Destination&>(destination).receive(packet);

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
				}
			}
		}

		// Handling for proofs and link-request proofs
		else if (packet.packet_type() == Type::Packet::PROOF) {
			extreme("Transport::inbound: Packet is PROOF");
/*
			if packet.context == RNS.Packet.LRPROOF:
				// This is a link request proof, check if it
				// needs to be transported
				if (RNS.Reticulum.transport_enabled() or for_local_client_link or from_local_client) and packet.destination_hash in Transport.link_table:
					link_entry = Transport.link_table[packet.destination_hash]
					if packet.receiving_interface == link_entry[2]:
						try:
							if len(packet.data) == RNS.Identity.SIGLENGTH//8+RNS.Link.ECPUBSIZE//2:
								peer_pub_bytes = packet.data[RNS.Identity.SIGLENGTH//8:RNS.Identity.SIGLENGTH//8+RNS.Link.ECPUBSIZE//2]
								peer_identity = RNS.Identity.recall(link_entry[6])
								peer_sig_pub_bytes = peer_identity.get_public_key()[RNS.Link.ECPUBSIZE//2:RNS.Link.ECPUBSIZE]

								signed_data = packet.destination_hash+peer_pub_bytes+peer_sig_pub_bytes
								signature = packet.data[:RNS.Identity.SIGLENGTH//8]

								if peer_identity.validate(signature, signed_data):
									RNS.log("Link request proof validated for transport via "+str(link_entry[4]), RNS.LOG_EXTREME)
									new_raw = packet.raw[0:1]
									new_raw += struct.pack("!B", packet.hops)
									new_raw += packet.raw[2:]
									Transport.link_table[packet.destination_hash][7] = True
									Transport.transmit(link_entry[4], new_raw)

								else:
									RNS.log("Invalid link request proof in transport for link "+RNS.prettyhexrep(packet.destination_hash)+", dropping proof.", RNS.LOG_DEBUG)

						except Exception as e:
							RNS.log("Error while transporting link request proof. The contained exception was: "+str(e), RNS.LOG_ERROR)

					else:
						RNS.log("Link request proof received on wrong interface, not transporting it.", RNS.LOG_DEBUG)
				else:
					// Check if we can deliver it to a local
					// pending link
					for link in Transport.pending_links:
						if link.link_id == packet.destination_hash:
							link.validate_proof(packet)

			elif packet.context == RNS.Packet.RESOURCE_PRF:
				for link in Transport.active_links:
					if link.link_id == packet.destination_hash:
						link.receive(packet)
			else:
				if packet.destination_type == RNS.Destination.LINK:
					for link in Transport.active_links:
						if link.link_id == packet.destination_hash:
							packet.link = link
							
				if len(packet.data) == RNS.PacketReceipt.EXPL_LENGTH:
					proof_hash = packet.data[:RNS.Identity.HASHLENGTH//8]
				else:
					proof_hash = None

				// Check if this proof neds to be transported
				if (RNS.Reticulum.transport_enabled() or from_local_client or proof_for_local_client) and packet.destination_hash in Transport.reverse_table:
					reverse_entry = Transport.reverse_table.pop(packet.destination_hash)
					if packet.receiving_interface == reverse_entry[1]:
						RNS.log("Proof received on correct interface, transporting it via "+str(reverse_entry[0]), RNS.LOG_EXTREME)
						new_raw = packet.raw[0:1]
						new_raw += struct.pack("!B", packet.hops)
						new_raw += packet.raw[2:]
						Transport.transmit(reverse_entry[0], new_raw)
					else:
						RNS.log("Proof received on wrong interface, not transporting it.", RNS.LOG_DEBUG)

				for receipt in Transport.receipts:
					receipt_validated = False
					if proof_hash != None:
						// Only test validation if hash matches
						if receipt.hash == proof_hash:
							receipt_validated = receipt.validate_proof_packet(packet)
					else:
						// TODO: This looks like it should actually
						// be rewritten when implicit proofs are added.

						// In case of an implicit proof, we have
						// to check every single outstanding receipt
						receipt_validated = receipt.validate_proof_packet(packet)

					if receipt_validated:
						if receipt in Transport.receipts:
							Transport.receipts.remove(receipt)
*/
		}
	}

	_jobs_locked = false;
}

/*static*/ void Transport::synthesize_tunnel(const Interface& interface) {
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
	extreme("Transport: Registering interface " + interface.toString());
	_interfaces.push_back(interface);
	extreme("Transport: Listing all registered interfaces...");
	for (Interface& found_interface : _interfaces) {
		extreme("Transport: Found interface " + found_interface.toString());
	}
	// CBA TODO set or add transport as listener on interface to receive incoming packets
}

/*static*/ void Transport::deregister_interface(const Interface& interface) {
	extreme("Transport: Deregistering interface " + interface.toString());
	//if (_interfaces.find(interface) != _interfaces.end()) {
	//	_interfaces.erase(interface);
	//}
	for (auto iter = _interfaces.begin(); iter != _interfaces.end(); ++iter) {
		if ((*iter).get() == interface) {
			_interfaces.erase(iter);
			break;
		}
	}
}

/*static*/ void Transport::register_destination(Destination& destination) {
	extreme("Transport: Registering destination " + destination.toString());
	destination.mtu(Type::Reticulum::MTU);
	if (destination.direction() == Type::Destination::IN) {
		for (auto& registered_destination : _destinations) {
			if (destination.hash() == registered_destination.hash()) {
				//raise KeyError("Attempt to register an already registered destination.")
				throw std::runtime_error("Attempt to register an already registered destination.");
			}
		}
		
		_destinations.insert(destination);

		if (_owner.is_connected_to_shared_instance()) {
			if (destination.type() == Type::Destination::SINGLE) {
				destination.announce({}, true);
			}
		}
	}
}

/*static*/ void Transport::deregister_destination(const Destination& destination) {
	extreme("Transport: Deregistering destination " + destination.toString());
	if (_destinations.find(destination) != _destinations.end()) {
		_destinations.erase(destination);
	}
}

/*static*/ void Transport::register_link(const Link& link) {
/*
	extreme("Transport: Registering link " + link.toString());
	if (link.initiator()) {
		_pending_links.insert(link);
	}
	else {
		_active_links.insert(link);
	}
*/
}

/*static*/ void Transport::activate_link(Link& link) {
/*
	extreme("Transport: Activating link " + link.toString());
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
	extreme("Transport: Transport::register_announce_handler()");
	//if hasattr(handler, "received_announce") and callable(handler.received_announce):
		//if hasattr(handler, "aspect_filter"):
			_announce_handlers.insert(handler);
}

/*
Deregisters an announce handler.

:param handler: The announce handler to be deregistered.
*/
/*static*/ void Transport::deregister_announce_handler(HAnnounceHandler handler) {
	extreme("Transport: Transport::deregister_announce_handler()");
	if (_announce_handlers.find(handler) != _announce_handlers.end()) {
		_announce_handlers.erase(handler);
	}
}

/*static*/ Interface Transport::find_interface_from_hash(const Bytes& interface_hash) {
	for (const Interface& interface : _interfaces) {
		if (interface.get_hash() == interface_hash) {
			return interface;
		}
	}

	return {Type::NONE};
}

/*static*/ bool Transport::should_cache(const Packet& packet) {
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
/*static*/ void Transport::cache(const Packet& packet, bool force_cache /*= false*/) {
/*
	if (should_cache(packet) || force_cache) {
		try {
			//packet_hash = RNS.hexrep(packet.get_hash(), delimit=False)
			Bytes packet_hash = packet.get_hash().toHex();
			//interface_reference = None
			std::string interface_reference;
			if (packet.receiving_interface()) {
				interface_reference = packet.receiving_interface().toString();
			}

			file = open(RNS.Reticulum.cachepath+"/"+packet_hash, "wb")  
			file.write(umsgpack.packb([packet.raw, interface_reference]))
			file.close()
		}
		catch (std::exception& e) {
			error("Error writing packet to cache. The contained exception was: " + e.what());
		}
	}
*/
}

/*static*/ Packet Transport::get_cached_packet(const Bytes& packet_hash) {
/*
	try {
		//packet_hash = RNS.hexrep(packet_hash, delimit=False)
		Bytes packet_hash = packet_hash.toHex();
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
	}
	catch (std::exception& e) {
		error("Exception occurred while getting cached packet.");
		error("The contained exception was: " + e.what());
	}
*/
	// MOCK
	return {Type::NONE};
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
		return Transport::PATHFINDER_M;
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
		return destination_entry._receiving_interface;
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
/*
	if tag == None:
		request_tag = RNS.Identity.get_random_hash()
	else:
		request_tag = tag

	if RNS.Reticulum.transport_enabled():
		path_request_data = destination_hash+Transport.identity.hash+request_tag
	else:
		path_request_data = destination_hash+request_tag

	path_request_dst = RNS.Destination(None, RNS.Destination.OUT, RNS.Destination.PLAIN, Transport.APP_NAME, "path", "request")
	packet = RNS.Packet(path_request_dst, path_request_data, packet_type = RNS.Packet.DATA, transport_type = RNS.Transport.BROADCAST, header_type = RNS.Packet.HEADER_1, attached_interface = on_interface)

	if on_interface != None and recursive:
		if not hasattr(on_interface, "announce_cap"):
			on_interface.announce_cap = RNS.Reticulum.ANNOUNCE_CAP

		if not hasattr(on_interface, "announce_allowed_at"):
			on_interface.announce_allowed_at = 0

		if not hasattr(on_interface, "announce_queue"):
			on_interface.announce_queue = []

		queued_announces = True if len(on_interface.announce_queue) > 0 else False
		if queued_announces:
			RNS.log("Blocking recursive path request on "+str(on_interface)+" due to queued announces", RNS.LOG_EXTREME)
			return
		else:
			now = time.time()
			if now < on_interface.announce_allowed_at:
				RNS.log("Blocking recursive path request on "+str(on_interface)+" due to active announce cap", RNS.LOG_EXTREME)
				return
			else:
				tx_time   = ((len(path_request_data)+RNS.Reticulum.HEADER_MINSIZE)*8) / on_interface.bitrate
				wait_time = (tx_time / on_interface.announce_cap)
				on_interface.announce_allowed_at = now + wait_time

	packet.send()
	Transport.path_requests[destination_hash] = time.time()
*/
}

/*static*/ void Transport::request_path(const Bytes& destination_hash) {
	return request_path(destination_hash, {Type::NONE});
}

/*static*/ void Transport::path_request_handler(const Bytes& data, const Packet& packet) {
/*
	try:
		// If there is at least bytes enough for a destination
		// hash in the packet, we assume those bytes are the
		// destination being requested.
		if len(data) >= RNS.Identity.TRUNCATED_HASHLENGTH//8:
			destination_hash = data[:RNS.Identity.TRUNCATED_HASHLENGTH//8]
			// If there is also enough bytes for a transport
			// instance ID and at least one tag byte, we
			// assume the next bytes to be the trasport ID
			// of the requesting transport instance.
			if len(data) > (RNS.Identity.TRUNCATED_HASHLENGTH//8)*2:
				requesting_transport_instance = data[RNS.Identity.TRUNCATED_HASHLENGTH//8:(RNS.Identity.TRUNCATED_HASHLENGTH//8)*2]
			else:
				requesting_transport_instance = None

			tag_bytes = None
			if len(data) > (RNS.Identity.TRUNCATED_HASHLENGTH//8)*2:
				tag_bytes = data[RNS.Identity.TRUNCATED_HASHLENGTH//8*2:]

			elif len(data) > (RNS.Identity.TRUNCATED_HASHLENGTH//8):
				tag_bytes = data[RNS.Identity.TRUNCATED_HASHLENGTH//8:]

			if tag_bytes != None:
				if len(tag_bytes) > RNS.Identity.TRUNCATED_HASHLENGTH//8:
					tag_bytes = tag_bytes[:RNS.Identity.TRUNCATED_HASHLENGTH//8]

				unique_tag = destination_hash+tag_bytes

				if not unique_tag in Transport.discovery_pr_tags:
					Transport.discovery_pr_tags.append(unique_tag)

					Transport.path_request(
						destination_hash,
						Transport.from_local_client(packet),
						packet.receiving_interface,
						requestor_transport_id = requesting_transport_instance,
						tag=tag_bytes
					)

				else:
					RNS.log("Ignoring duplicate path request for "+RNS.prettyhexrep(destination_hash)+" with tag "+RNS.prettyhexrep(unique_tag), RNS.LOG_DEBUG)

			else:
				RNS.log("Ignoring tagless path request for "+RNS.prettyhexrep(destination_hash), RNS.LOG_DEBUG)

	except Exception as e:
		RNS.log("Error while handling path request. The contained exception was: "+str(e), RNS.LOG_ERROR)
*/
}

/*static*/ void Transport::path_request(const Bytes& destination_hash, bool is_from_local_client, const Interface& attached_interface, const Bytes& requestor_transport_id /*= {}*/, const Bytes& tag /*= {}*/) {
/*
	should_search_for_unknown = False

	if attached_interface != None:
		if RNS.Reticulum.transport_enabled() and attached_interface.mode in RNS.Interfaces.Interface.Interface.DISCOVER_PATHS_FOR:
			should_search_for_unknown = True

		interface_str = " on "+str(attached_interface)
	else:
		interface_str = ""

	RNS.log("Path request for "+RNS.prettyhexrep(destination_hash)+interface_str, RNS.LOG_DEBUG)

	destination_exists_on_local_client = False
	if len(Transport.local_client_interfaces) > 0:
		if destination_hash in Transport.destination_table:
			destination_interface = Transport.destination_table[destination_hash][5]
			
			if Transport.is_local_client_interface(destination_interface):
				destination_exists_on_local_client = True
				Transport.pending_local_path_requests[destination_hash] = attached_interface
	
	//local_destination = next((d for d in Transport.destinations if d.hash == destination_hash), None)
	Destination local_destination({Type::NONE});
	for (auto& destination : _destinations) {
		if (destination.hash() == destination_hash) {
			local_destination = destination;
			break;
		}
	}
    //if local_destination != None:
	if (local_destination) {
		local_destination.announce(path_response=True, tag=tag, attached_interface=attached_interface);
		debug("Answering path request for " + destination_hash.toHex() + interface_str + ", destination is local to this system");
	}

	elif (RNS.Reticulum.transport_enabled() or is_from_local_client) and (destination_hash in Transport.destination_table):
		packet = Transport.destination_table[destination_hash][6]
		next_hop = Transport.destination_table[destination_hash][1]
		received_from = Transport.destination_table[destination_hash][5]

		if attached_interface.mode == RNS.Interfaces.Interface.Interface.MODE_ROAMING and attached_interface == received_from:
			RNS.log("Not answering path request on roaming-mode interface, since next hop is on same roaming-mode interface", RNS.LOG_DEBUG)
		
		else:
			if requestor_transport_id != None and next_hop == requestor_transport_id:
				// TODO: Find a bandwidth efficient way to invalidate our
				// known path on this signal. The obvious way of signing
				// path requests with transport instance keys is quite
				// inefficient. There is probably a better way. Doing
				// path invalidation here would decrease the network
				// convergence time. Maybe just drop it?
				RNS.log("Not answering path request for "+RNS.prettyhexrep(destination_hash)+interface_str+", since next hop is the requestor", RNS.LOG_DEBUG)
			else:
				RNS.log("Answering path request for "+RNS.prettyhexrep(destination_hash)+interface_str+", path is known", RNS.LOG_DEBUG)

				now = time.time()
				retries = Transport.PATHFINDER_R
				local_rebroadcasts = 0
				block_rebroadcasts = True
				announce_hops      = packet.hops

				if is_from_local_client:
					retransmit_timeout = now
				else:
					// TODO: Look at this timing
					retransmit_timeout = now + Transport.PATH_REQUEST_GRACE // + (RNS.rand() * Transport.PATHFINDER_RW)

				// This handles an edge case where a peer sends a past
				// request for a destination just after an announce for
				// said destination has arrived, but before it has been
				// rebroadcast locally. In such a case the actual announce
				// is temporarily held, and then reinserted when the path
				// request has been served to the peer.
				if packet.destination_hash in Transport.announce_table:
					held_entry = Transport.announce_table[packet.destination_hash]
					Transport.held_announces[packet.destination_hash] = held_entry
				
				Transport.announce_table[packet.destination_hash] = [now, retransmit_timeout, retries, received_from, announce_hops, packet, local_rebroadcasts, block_rebroadcasts, attached_interface]

	elif is_from_local_client:
		// Forward path request on all interfaces
		// except the local client
		RNS.log("Forwarding path request from local client for "+RNS.prettyhexrep(destination_hash)+interface_str+" to all other interfaces", RNS.LOG_DEBUG)
		request_tag = RNS.Identity.get_random_hash()
		for interface in Transport.interfaces:
			if not interface == attached_interface:
				Transport.request_path(destination_hash, interface, tag = request_tag)

	elif should_search_for_unknown:
		if destination_hash in Transport.discovery_path_requests:
			RNS.log("There is already a waiting path request for "+RNS.prettyhexrep(destination_hash)+" on behalf of path request"+interface_str, RNS.LOG_DEBUG)
		else:
			// Forward path request on all interfaces
			// except the requestor interface
			RNS.log("Attempting to discover unknown path to "+RNS.prettyhexrep(destination_hash)+" on behalf of path request"+interface_str, RNS.LOG_DEBUG)
			pr_entry = { "destination_hash": destination_hash, "timeout": time.time()+Transport.PATH_REQUEST_TIMEOUT, "requesting_interface": attached_interface }
			Transport.discovery_path_requests[destination_hash] = pr_entry

			for interface in Transport.interfaces:
				if not interface == attached_interface:
					// Use the previously extracted tag from this path request
					// on the new path requests as well, to avoid potential loops
					Transport.request_path(destination_hash, on_interface=interface, tag=tag, recursive=True)

	elif not is_from_local_client and len(Transport.local_client_interfaces) > 0:
		// Forward the path request on all local
		// client interfaces
		RNS.log("Forwarding path request for "+RNS.prettyhexrep(destination_hash)+interface_str+" to local clients", RNS.LOG_DEBUG)
		for interface in Transport.local_client_interfaces:
			Transport.request_path(destination_hash, on_interface=interface)

	else:
		RNS.log("Ignoring path request for "+RNS.prettyhexrep(destination_hash)+interface_str+", no path known", RNS.LOG_DEBUG)
*/
}

/*static*/ bool Transport::from_local_client(const Packet& packet) {
/*
	if hasattr(packet.receiving_interface, "parent_interface"):
		return Transport.is_local_client_interface(packet.receiving_interface)
	else:
		return False
*/
	// MOCK
	return false;
}

/*static*/ bool Transport::is_local_client_interface(const Interface& interface) {
/*
	if hasattr(interface, "parent_interface"):
		if hasattr(interface.parent_interface, "is_local_shared_instance"):
			return True
		else:
			return False
	else:
		return False
*/
	// MOCK
	return false;
}

/*static*/ bool Transport::interface_to_shared_instance(const Interface& interface) {
/*
	if hasattr(interface, "is_connected_to_shared_instance"):
		return True
	else:
		return False
*/
	// MOCK
	return false;
}

/*static*/ void Transport::detach_interfaces() {
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
/*
	if Transport.owner.is_connected_to_shared_instance:
		for registered_destination in Transport.destinations:
			if registered_destination.type == RNS.Destination.SINGLE:
				registered_destination.announce(path_response=True)
*/
}

/*static*/ void Transport::drop_announce_queues() {
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
/*
	random_blob = packet.data[RNS.Identity.KEYSIZE//8+RNS.Identity.NAME_HASH_LENGTH//8:RNS.Identity.KEYSIZE//8+RNS.Identity.NAME_HASH_LENGTH//8+10]
	announce_emitted = int.from_bytes(random_blob[5:10], "big")

	return announce_emitted
*/
	// MOCK
	return false;
}

/*static*/ void Transport::save_packet_hashlist() {
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

/*static*/ void Transport::save_path_table() {
/*
	if not Transport.owner.is_connected_to_shared_instance:
		if hasattr(Transport, "saving_path_table"):
			wait_interval = 0.2
			wait_timeout = 5
			wait_start = time.time()
			while Transport.saving_path_table:
				time.sleep(wait_interval)
				if time.time() > wait_start+wait_timeout:
					RNS.log("Could not save path table to storage, waiting for previous save operation timed out.", RNS.LOG_ERROR)
					return False

		try:
			Transport.saving_path_table = True
			save_start = time.time()
			RNS.log("Saving path table to storage...", RNS.LOG_DEBUG)

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

			save_time = time.time() - save_start
			if save_time < 1:
				time_str = str(round(save_time*1000,2))+"ms"
			else:
				time_str = str(round(save_time,2))+"s"
			RNS.log("Saved "+str(len(serialised_destinations))+" path table entries in "+time_str, RNS.LOG_DEBUG)

		except Exception as e:
			RNS.log("Could not save path table to storage, the contained exception was: "+str(e), RNS.LOG_ERROR)

		Transport.saving_path_table = False
*/
}

/*static*/ void Transport::save_tunnel_table() {
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
				time_str = str(round(save_time*1000,2))+"ms"
			else:
				time_str = str(round(save_time,2))+"s"
			RNS.log("Saved "+str(len(serialised_tunnels))+" tunnel table entries in "+time_str, RNS.LOG_DEBUG)
		except Exception as e:
			RNS.log("Could not save tunnel table to storage, the contained exception was: "+str(e), RNS.LOG_ERROR)

		Transport.saving_tunnel_table = False
*/
}

/*static*/ void Transport::persist_data() {
	save_packet_hashlist();
	save_path_table();
	save_tunnel_table();
}

/*static*/ void Transport::exit_handler() {
	extreme("Transport::exit_handler()");
	if (!_owner.is_connected_to_shared_instance()) {
		persist_data();
	}
}