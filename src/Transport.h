#pragma once

#include "Bytes.h"
#include "Type.h"

#include <memory>
#include <vector>
#include <list>
#include <set>
#include <map>
#include <functional>
#include <stdint.h>

namespace RNS {

	class Reticulum;
	class Identity;
	class Destination;
	class Interface;
	class Link;
	class Packet;
	class PacketReceipt;

	class AnnounceHandler {
	public:
		AnnounceHandler(const std::string &aspect_filter) {
			_aspect_filter = aspect_filter;
		}
		virtual void received_announce(const Bytes &destination_hash, const Identity &announced_identity, const Bytes &app_data) = 0;
	private:
		std::string _aspect_filter;
	};
	using HAnnounceHandler = std::shared_ptr<AnnounceHandler>;

    /*
    Through static methods of this class you can interact with the
    Transport system of Reticulum.
    */
	class Transport {

	private:
		class DestinationEntry {
		public:
			DestinationEntry(uint64_t time, Bytes received_from, uint8_t announce_hops, uint64_t expires, std::set<Bytes> random_blobs, Interface &receiving_interface, const Packet &packet) :
				_timestamp(time),
				_received_from(received_from),
				_hops(announce_hops),
				_expires(expires),
				_random_blobs(random_blobs),
				_receiving_interface(receiving_interface),
				_packet(packet)
			{
			}
		public:
			uint64_t _timestamp = 0;
			Bytes _received_from;
			uint8_t _hops = 0;
			uint64_t _expires = 0;
			std::set<Bytes> _random_blobs;
			Interface &_receiving_interface;
			const Packet &_packet;
		};

		class AnnounceEntry {
		public:
			AnnounceEntry(uint64_t timestamp, uint16_t retransmit_timeout, uint8_t retries, Bytes received_from, uint8_t hops, const Packet &packet, uint8_t local_rebroadcasts, bool block_rebroadcasts, const Interface &attached_interface) : 
				_timestamp(timestamp),
				_retransmit_timeout(retransmit_timeout),
				_retries(retries),
				_received_from(received_from),
				_hops(hops),
				_packet(packet),
				_local_rebroadcasts(local_rebroadcasts),
				_block_rebroadcasts(block_rebroadcasts),
				_attached_interface(attached_interface)
			{
			}
		public:
			uint64_t _timestamp = 0;
			uint16_t _retransmit_timeout = 0;
			uint8_t _retries = 0;
			Bytes _received_from;
			uint8_t _hops = 0;
			const Packet &_packet;
			uint8_t _local_rebroadcasts = 0;
			bool _block_rebroadcasts = false;
			const Interface &_attached_interface;
		};

		class LinkEntry {
		public:
			LinkEntry(uint64_t timestamp, const Bytes &next_hop, const Interface &outbound_interface, uint8_t remaining_hops, const Interface &receiving_interface, uint8_t hops, const Bytes &destination_hash, bool validated, uint64_t proof_timeout) : 
				_timestamp(timestamp),
				_next_hop(next_hop),
				_outbound_interface(outbound_interface),
				_remaining_hops(remaining_hops),
				_receiving_interface(receiving_interface),
				_hops(hops),
				_destination_hash(destination_hash),
				_validated(validated),
				_proof_timeout(proof_timeout)
			{
			}
		public:
			uint64_t _timestamp = 0;
			Bytes _next_hop;
			const Interface &_outbound_interface;
			uint8_t _remaining_hops = 0;
			const Interface &_receiving_interface;
			uint8_t _hops = 0;
			Bytes _destination_hash;
			bool _validated = false;
			uint64_t _proof_timeout = 0;
		};

		class ReverseEntry {
		public:
			ReverseEntry(const Interface &receiving_interface, const Interface &outbound_interface, uint64_t timestamp) : 
				_receiving_interface(receiving_interface),
				_outbound_interface(outbound_interface),
				_timestamp(timestamp)
			{
			}
		public:
			const Interface &_receiving_interface;
			const Interface &_outbound_interface;
			uint64_t _timestamp = 0;
		};

	public:
		// Constants
/*
		enum types {
			BROADCAST    = 0x00,
			TRANSPORT    = 0x01,
			RELAY        = 0x02,
			TUNNEL       = 0x03,
			NONE         = 0xFF,
		};
*/

		enum reachabilities {
			REACHABILITY_UNREACHABLE = 0x00,
			REACHABILITY_DIRECT      = 0x01,
			REACHABILITY_TRANSPORT   = 0x02,
		};

		static constexpr const char* APP_NAME = "rnstransport";

		static const uint8_t PATHFINDER_M    = 128;       // Max hops
		// Maximum amount of hops that Reticulum will transport a packet.

		static const uint8_t PATHFINDER_R      = 1;          // Retransmit retries
		static const uint8_t PATHFINDER_G      = 5;          // Retry grace period
		static constexpr const float PATHFINDER_RW       = 0.5;        // Random window for announce rebroadcast
		static const uint32_t PATHFINDER_E      = 60*60*24*7; // Path expiration of one week
		static const uint32_t AP_PATH_TIME      = 60*60*24;   // Path expiration of one day for Access Point paths
		static const uint32_t ROAMING_PATH_TIME = 60*60*6;    // Path expiration of 6 hours for Roaming paths

		// TODO: Calculate an optimal number for this in
		// various situations
		static const uint8_t LOCAL_REBROADCASTS_MAX = 2;          // How many local rebroadcasts of an announce is allowed

		static const uint8_t PATH_REQUEST_TIMEOUT = 15;           // Default timuout for client path requests in seconds
		static constexpr const float PATH_REQUEST_GRACE     = 0.35;         // Grace time before a path announcement is made, allows directly reachable peers to respond first
		static const uint8_t PATH_REQUEST_RW      = 2;            // Path request random window
		static const uint8_t PATH_REQUEST_MI      = 5;            // Minimum interval in seconds for automated path requests

		static constexpr const float LINK_TIMEOUT            = Type::Link::STALE_TIME * 1.25;
		static const uint16_t REVERSE_TIMEOUT      = 30*60;        // Reverse table entries are removed after 30 minutes
		static const uint32_t DESTINATION_TIMEOUT  = 60*60*24*7;   // Destination table entries are removed if unused for one week
		static const uint16_t MAX_RECEIPTS         = 1024;         // Maximum number of receipts to keep track of
		static const uint8_t MAX_RATE_TIMESTAMPS   = 16;           // Maximum number of announce timestamps to keep per destination

	public:
		static void start(const Reticulum &reticulum_instance);
		static void jobloop();
		static void jobs();
		static void transmit(Interface &interface, const Bytes &raw);
		static bool outbound(Packet &packet);
		static bool packet_filter(const Packet &packet);
		//static void inbound(const Bytes &raw, const Interface &interface = {Type::NONE});
		static void inbound(const Bytes &raw, const Interface &interface);
		static void inbound(const Bytes &raw);
		static void synthesize_tunnel(const Interface &interface);
		static void tunnel_synthesize_handler(const Bytes &data, const Packet &packet);
		static void handle_tunnel(const Bytes &tunnel_id, const Interface &interface);
		static void register_interface(Interface &interface);
		static void deregister_interface(const Interface &interface);
		static void register_destination(Destination &destination);
		static void deregister_destination(const Destination &destination);
		static void register_link(const Link &link);
		static void activate_link(Link &link);
		static void register_announce_handler(HAnnounceHandler handler);
		static void deregister_announce_handler(HAnnounceHandler handler);
		static Interface find_interface_from_hash(const Bytes &interface_hash);
		static bool should_cache(const Packet &packet);
		static void cache(const Packet &packet, bool force_cache = false);
		static Packet get_cached_packet(const Bytes &packet_hash);
		static bool cache_request_packet(const Packet &packet);
		static void cache_request(const Bytes &packet_hash, const Destination &destination);
		static bool has_path(const Bytes &destination_hash);
		static uint8_t hops_to(const Bytes &destination_hash);
		static Bytes next_hop(const Bytes &destination_hash);
		static Interface next_hop_interface(const Bytes &destination_hash);
		static bool expire_path(const Bytes &destination_hash);
		//static void request_path(const Bytes &destination_hash, const Interface &on_interface = {Type::NONE}, const Bytes &tag = {}, bool recursive = false);
		static void request_path(const Bytes &destination_hash, const Interface &on_interface, const Bytes &tag = {}, bool recursive = false);
		static void request_path(const Bytes &destination_hash);
		static void path_request_handler(const Bytes &data, const Packet &packet);
		static void path_request(const Bytes &destination_hash, bool is_from_local_client, const Interface &attached_interface, const Bytes &requestor_transport_id = {}, const Bytes &tag = {});
		static bool from_local_client(const Packet &packet);
		static bool is_local_client_interface(const Interface &interface);
		static bool interface_to_shared_instance(const Interface &interface);
		static void detach_interfaces();
		static void shared_connection_disappeared();
		static void shared_connection_reappeared();
		static void drop_announce_queues();
		static bool announce_emitted(const Packet &packet);
		static void save_packet_hashlist();
		static void save_path_table();
		static void save_tunnel_table();
		static void persist_data();
		static void exit_handler();

	private:
		// CBA MUST use references to interfaces here in order for virtul overrides for send/receive to work
		//static std::set<std::reference_wrapper<const Interface>, std::less<const Interface>> _interfaces;           // All active interfaces
		static std::list<std::reference_wrapper<Interface>> _interfaces;           // All active interfaces
		static std::set<Destination> _destinations;           // All active destinations
		//static std::map<Bytes, Destination> _destinations;           // All active destinations
		static std::set<Link> _pending_links;           // Links that are being established
		static std::set<Link> _active_links;           // Links that are active
		static std::set<Bytes> _packet_hashlist;           // A list of packet hashes for duplicate detection
		static std::set<PacketReceipt> _receipts;           // Receipts of all outgoing packets for proof processing

		// TODO: "destination_table" should really be renamed to "path_table"
		// Notes on memory usage: 1 megabyte of memory can store approximately
		// 55.100 path table entries or approximately 22.300 link table entries.

		static std::map<Bytes, AnnounceEntry> _announce_table;           // A table for storing announces currently waiting to be retransmitted
		static std::map<Bytes, DestinationEntry> _destination_table;           // A lookup table containing the next hop to a given destination
		static std::map<Bytes, ReverseEntry> _reverse_table;           // A lookup table for storing packet hashes used to return proofs and replies
		static std::map<Bytes, LinkEntry> _link_table;           // A lookup table containing hops for links
		static std::map<Bytes, AnnounceEntry> _held_announces;           // A table containing temporarily held announce-table entries
		static std::set<HAnnounceHandler> _announce_handlers;           // A table storing externally registered announce handlers
		//z_tunnels              = {}           // A table storing tunnels to other transport instances
		//z_announce_rate_table  = {}           // A table for keeping track of announce rates
		static std::set<Bytes> _path_requests;           // A table for storing path request timestamps

		//z_discovery_path_requests  = {}       // A table for keeping track of path requests on behalf of other nodes
		//z_discovery_pr_tags        = []       // A table for keeping track of tagged path requests
		static uint16_t _max_pr_taXgxs;    // Maximum amount of unique path request tags to remember

		// Transport control destinations are used
		// for control purposes like path requests
		static std::set<Destination> _control_destinations;
		static std::set<Bytes> _control_hashes;

		// Interfaces for communicating with
		// local clients connected to a shared
		// Reticulum instance
		static std::set<Interface> _local_client_interfaces;

		//z_local_client_rssi_cache    = []
		//z_local_client_snr_cache     = []
		static uint16_t _LOCAL_CLIENT_CACHE_MAXSIZE;

		std::map<Bytes, Interface> _pending_local_path_requests;

		static uint64_t _start_time;
		static bool _jobs_locked;
		static bool _jobs_running;
		static uint32_t _job_interval;
		static uint64_t _links_last_checked;
		static uint32_t _links_check_interval;
		static uint64_t _receipts_last_checked;
		static uint32_t _receipts_check_interval;
		static uint64_t _announces_last_checked;
		static uint32_t _announces_check_interval;
		static uint32_t _hashlist_maxsize;
		static uint64_t _tables_last_culled;
		static uint32_t _tables_cull_interval;

		static Reticulum _owner;
		static Identity _identity;
	};

}