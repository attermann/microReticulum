#pragma once

#include "Packet.h"
#include "Bytes.h"
#include "Type.h"

#include <memory>
#include <vector>
#include <list>
#include <set>
#include <map>
#include <functional>
#include <stdint.h>

//#define INTERFACES_SET
//#define INTERFACES_LIST
#define INTERFACES_MAP

//#define DESTINATIONS_SET
#define DESTINATIONS_MAP

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
		// The initialisation method takes the optional
		// aspect_filter argument. If aspect_filter is set to
		// None, all announces will be passed to the instance.
		// If only some announces are wanted, it can be set to
		// an aspect string.
		AnnounceHandler(const char* aspect_filter = nullptr) { if (aspect_filter != nullptr) _aspect_filter = aspect_filter; }
		// This method will be called by Reticulums Transport
		// system when an announce arrives that matches the
		// configured aspect filter. Filters must be specific,
		// and cannot use wildcards.
		virtual void received_announce(const Bytes& destination_hash, const Identity& announced_identity, const Bytes& app_data) = 0;
		std::string& aspect_filter() { return _aspect_filter; }
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
		// CBA TODO Analyze safety of using Inrerface references here
		// CBA TODO Analyze safety of using Packet references here
		class DestinationEntry {
		public:
			DestinationEntry(double time, const Bytes& received_from, uint8_t announce_hops, double expires, const std::set<Bytes>& random_blobs, Interface& receiving_interface, const Packet& packet) :
				_timestamp(time),
				_received_from(received_from),
				_hops(announce_hops),
				_expires(expires),
				_random_blobs(random_blobs),
				_receiving_interface(receiving_interface),
				_announce_packet(packet)
			{
			}
		public:
			double _timestamp = 0;
			Bytes _received_from;
			uint8_t _hops = 0;
			double _expires = 0;
			std::set<Bytes> _random_blobs;
			// CBA TODO does this need to be a reference in order for virtual method callbacks to work?
			Interface& _receiving_interface;
			//const Packet& _announce_packet;
			Packet _announce_packet;
		};

		// CBA TODO Analyze safety of using Inrerface references here
		// CBA TODO Analyze safety of using Packet references here
		class AnnounceEntry {
		public:
			AnnounceEntry(double timestamp, double retransmit_timeout, uint8_t retries, const Bytes& received_from, uint8_t hops, const Packet& packet, uint8_t local_rebroadcasts, bool block_rebroadcasts, const Interface& attached_interface) :
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
			double _timestamp = 0;
			double _retransmit_timeout = 0;
			uint8_t _retries = 0;
			Bytes _received_from;
			uint8_t _hops = 0;
			// CBA Storing packet reference causes memory issues, presumably because orignal packet is being destroyed
			//  MUST use instance instad of reference!!!
			//const Packet& _packet;
			Packet _packet;
			uint8_t _local_rebroadcasts = 0;
			bool _block_rebroadcasts = false;
			// CBA TODO does this need to be a reference in order for virtual method callbacks to work?
			//const Interface& _attached_interface;
			Interface _attached_interface;
		};

		// CBA TODO Analyze safety of using Inrerface references here
		class LinkEntry {
		public:
			LinkEntry(double timestamp, const Bytes& next_hop, const Interface& outbound_interface, uint8_t remaining_hops, const Interface& receiving_interface, uint8_t hops, const Bytes& destination_hash, bool validated, double proof_timeout) :
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
			double _timestamp = 0;
			Bytes _next_hop;
			// CBA TODO does this need to be a reference in order for virtual method callbacks to work?
			//const Interface& _outbound_interface;
			Interface _outbound_interface;
			uint8_t _remaining_hops = 0;
			// CBA TODO does this need to be a reference in order for virtual method callbacks to work?
			//const Interface& _receiving_interface;
			Interface _receiving_interface;
			uint8_t _hops = 0;
			Bytes _destination_hash;
			bool _validated = false;
			double _proof_timeout = 0;
		};

		// CBA TODO Analyze safety of using Inrerface references here
		class ReverseEntry {
		public:
			ReverseEntry(const Interface& receiving_interface, const Interface& outbound_interface, double timestamp) :
				_receiving_interface(receiving_interface),
				_outbound_interface(outbound_interface),
				_timestamp(timestamp)
			{
			}
		public:
			// CBA TODO does this need to be a reference in order for virtual method callbacks to work?
			//const Interface& _receiving_interface;
			Interface _receiving_interface;
			// CBA TODO does this need to be a reference in order for virtual method callbacks to work?
			//const Interface& _outbound_interface;
			Interface _outbound_interface;
			double _timestamp = 0;
		};

		// CBA TODO Analyze safety of using Inrerface references here
		class PathRequestEntry {
		public:
			PathRequestEntry(const Bytes& destination_hash, double timeout, const Interface& requesting_interface) :
				_destination_hash(destination_hash),
				_timeout(timeout),
				_requesting_interface(requesting_interface)
			{
			}
		public:
			Bytes _destination_hash;
			double _timeout = 0;
			// CBA TODO does this need to be a reference in order for virtual method callbacks to work?
			//const Interface& _requesting_interface;
			Interface _requesting_interface;
		};

	public:
		static void start(const Reticulum& reticulum_instance);
		static void loop();
		static void jobloop();
		static void jobs();
		static void transmit(Interface& interface, const Bytes& raw);
		static bool outbound(Packet& packet);
		static bool packet_filter(const Packet& packet);
		//static void inbound(const Bytes& raw, const Interface& interface = {Type::NONE});
		static void inbound(const Bytes& raw, const Interface& interface);
		static void inbound(const Bytes& raw);
		static void synthesize_tunnel(const Interface& interface);
		static void tunnel_synthesize_handler(const Bytes& data, const Packet& packet);
		static void handle_tunnel(const Bytes& tunnel_id, const Interface& interface);
		static void register_interface(Interface& interface);
		static void deregister_interface(const Interface& interface);
		static void register_destination(Destination& destination);
		static void deregister_destination(const Destination& destination);
		static void register_link(const Link& link);
		static void activate_link(Link& link);
		static void register_announce_handler(HAnnounceHandler handler);
		static void deregister_announce_handler(HAnnounceHandler handler);
		static Interface find_interface_from_hash(const Bytes& interface_hash);
		static bool should_cache(const Packet& packet);
		static void cache(const Packet& packet, bool force_cache = false);
		static Packet get_cached_packet(const Bytes& packet_hash);
		static bool cache_request_packet(const Packet& packet);
		static void cache_request(const Bytes& packet_hash, const Destination& destination);
		static bool has_path(const Bytes& destination_hash);
		static uint8_t hops_to(const Bytes& destination_hash);
		static Bytes next_hop(const Bytes& destination_hash);
		static Interface next_hop_interface(const Bytes& destination_hash);
		static bool expire_path(const Bytes& destination_hash);
		//static void request_path(const Bytes& destination_hash, const Interface& on_interface = {Type::NONE}, const Bytes& tag = {}, bool recursive = false);
		static void request_path(const Bytes& destination_hash, const Interface& on_interface, const Bytes& tag = {}, bool recursive = false);
		static void request_path(const Bytes& destination_hash);
		static void path_request_handler(const Bytes& data, const Packet& packet);
		static void path_request(const Bytes& destination_hash, bool is_from_local_client, const Interface& attached_interface, const Bytes& requestor_transport_id = {}, const Bytes& tag = {});
		static bool from_local_client(const Packet& packet);
		static bool is_local_client_interface(const Interface& interface);
		static bool interface_to_shared_instance(const Interface& interface);
		static void detach_interfaces();
		static void shared_connection_disappeared();
		static void shared_connection_reappeared();
		static void drop_announce_queues();
		static bool announce_emitted(const Packet& packet);
		static void save_packet_hashlist();
		static void save_path_table();
		static void save_tunnel_table();
		static void persist_data();
		static void exit_handler();

		static Destination find_destination_from_hash(const Bytes& destination_hash);

	private:
		// CBA MUST use references to interfaces here in order for virtul overrides for send/receive to work
#if defined(INTERFACES_SET)
		// set sorted, can use find
		//static std::set<std::reference_wrapper<const Interface>, std::less<const Interface>> _interfaces;           // All active interfaces
		static std::set<std::reference_wrapper<Interface>, std::less<Interface>> _interfaces;           // All active interfaces
#elif defined(INTERFACES_LIST)
		// list is unsorted, can't use find
		static std::list<std::reference_wrapper<Interface>> _interfaces;           // All active interfaces
#elif defined(INTERFACES_MAP)
		// map is sorted, can use find
		static std::map<Bytes, Interface&> _interfaces;           // All active interfaces
#endif
#if defined(DESTINATIONS_SET)
		static std::set<Destination> _destinations;           // All active destinations
#elif defined(DESTINATIONS_MAP)
		static std::map<Bytes, Destination> _destinations;           // All active destinations
#endif
		static std::set<Link> _pending_links;           // Links that are being established
		static std::set<Link> _active_links;           // Links that are active
		static std::set<Bytes> _packet_hashlist;           // A list of packet hashes for duplicate detection
		static std::list<PacketReceipt> _receipts;           // Receipts of all outgoing packets for proof processing

		// TODO: "destination_table" should really be renamed to "path_table"
		// Notes on memory usage: 1 megabyte of memory can store approximately
		// 55.100 path table entries or approximately 22.300 link table entries.

		static std::map<Bytes, AnnounceEntry> _announce_table;           // A table for storing announces currently waiting to be retransmitted
		static std::map<Bytes, DestinationEntry> _destination_table;           // A lookup table containing the next hop to a given destination
		static std::map<Bytes, ReverseEntry> _reverse_table;           // A lookup table for storing packet hashes used to return proofs and replies
		static std::map<Bytes, LinkEntry> _link_table;           // A lookup table containing hops for links
		static std::map<Bytes, AnnounceEntry> _held_announces;           // A table containing temporarily held announce-table entries
		static std::set<HAnnounceHandler> _announce_handlers;           // A table storing externally registered announce handlers
		//z _tunnels              = {}           // A table storing tunnels to other transport instances
		//z _announce_rate_table  = {}           // A table for keeping track of announce rates
		static std::map<Bytes, double> _path_requests;           // A table for storing path request timestamps

		static std::map<Bytes, PathRequestEntry> _discovery_path_requests;       // A table for keeping track of path requests on behalf of other nodes
		static std::set<Bytes> _discovery_pr_tags;       // A table for keeping track of tagged path requests
		static uint16_t _max_pr_tags;    // Maximum amount of unique path request tags to remember

		// Transport control destinations are used
		// for control purposes like path requests
		static std::set<Destination> _control_destinations;
		static std::set<Bytes> _control_hashes;

		// Interfaces for communicating with
		// local clients connected to a shared
		// Reticulum instance
		//static std::set<Interface> _local_client_interfaces;
		static std::set<std::reference_wrapper<const Interface>, std::less<const Interface>> _local_client_interfaces;

		static std::map<Bytes, const Interface&> _pending_local_path_requests;

		//z _local_client_rssi_cache    = []
		//z _local_client_snr_cache     = []
		static uint16_t _LOCAL_CLIENT_CACHE_MAXSIZE;

		static double _start_time;
		static bool _jobs_locked;
		static bool _jobs_running;
		static uint32_t _job_interval;
		static double _jobs_last_run;
		static double _links_last_checked;
		static uint32_t _links_check_interval;
		static double _receipts_last_checked;
		static uint32_t _receipts_check_interval;
		static double _announces_last_checked;
		static uint32_t _announces_check_interval;
		static uint32_t _hashlist_maxsize;
		static double _tables_last_culled;
		static uint32_t _tables_cull_interval;

		static Reticulum _owner;
		static Identity _identity;
	};

}