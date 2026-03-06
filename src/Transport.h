#pragma once

#include "Packet.h"
#include "Bytes.h"
#include "Type.h"
#include "Utilities/Memory.h"

#include <map>
#include <vector>
#include <list>
#include <set>
#include <array>
#include <memory>
#include <functional>
#include <stdint.h>

#ifndef RNS_LEAN_PATH_TABLE
	#define RNS_LEAN_PATH_TABLE 1
#endif

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

	public:

		using InterfaceTable = std::map<Bytes, Interface&>;
		using DestinationTable = std::map<Bytes, Destination>;

		class Callbacks {
		public:
			using receive_packet = void(*)(const Bytes& raw, const Interface& interface);
			using transmit_packet = void(*)(const Bytes& raw, const Interface& interface);
			using filter_packet = bool(*)(const Packet& packet);
		public:
			receive_packet _receive_packet = nullptr;
			transmit_packet _transmit_packet = nullptr;
			filter_packet _filter_packet = nullptr;
		friend class Transport;
		};

		class PacketEntry {
		public:
			PacketEntry() {}
			PacketEntry(const Bytes& raw, double sent_at, const Bytes& destination_hash) :
				_raw(raw),
				_sent_at(sent_at),
				_destination_hash(destination_hash)
			{
			}
			PacketEntry(const Packet& packet) :
				_raw(packet.raw()),
				_sent_at(packet.sent_at()),
				_destination_hash(packet.destination_hash())
			{
			}
		public:
			Bytes _raw;
			double _sent_at = 0;
			Bytes _destination_hash;
			bool _cached = false;
#ifndef NDEBUG
			inline std::string debugString() const {
				std::string dump;
				dump = "PacketEntry: destination_hash=" + _destination_hash.toHex() +
					" sent_at=" + std::to_string(_sent_at);
				return dump;
			}
#endif
		};
		using PacketTable = std::map<Bytes, PacketEntry>;

		// CBA TODO Analyze safety of using Inrerface references here
		// CBA TODO Analyze safety of using Packet references here
		class DestinationEntry {
		public:
			DestinationEntry() {}
			DestinationEntry(double timestamp, const Bytes& received_from, uint8_t announce_hops, double expires, const std::set<Bytes>& random_blobs, const Interface& receiving_interface, const Packet& announce_packet) :
				_timestamp(timestamp),
				_received_from(received_from),
				_hops(announce_hops),
				_expires(expires),
				_receiving_interface(receiving_interface),
#if !RNS_LEAN_PATH_TABLE
				_announce_packet(announce_packet),
#endif
				_random_blobs(random_blobs)
			{
				if (receiving_interface) _receiving_interface_hash = receiving_interface.get_hash();
				if (announce_packet) _announce_packet_hash = announce_packet.get_hash();
			}
			DestinationEntry(double timestamp, const Bytes& received_from, uint8_t announce_hops, double expires, const std::set<Bytes>& random_blobs, const Bytes& receiving_interface_hash, const Bytes& announce_packet_hash) :
				_timestamp(timestamp),
				_received_from(received_from),
				_hops(announce_hops),
				_expires(expires),
				_random_blobs(random_blobs),
				_receiving_interface_hash(receiving_interface_hash),
				_announce_packet_hash(announce_packet_hash)
			{
			}
			inline operator bool() const {
				return (_receiving_interface_hash && _announce_packet_hash);
			}
		public:
			// Lazy load receiving_interface and announce_packet
			inline Interface& receiving_interface() {
				if (!_receiving_interface) _receiving_interface = find_interface_from_hash(_receiving_interface_hash);
				return _receiving_interface;
			}
#if RNS_LEAN_PATH_TABLE
			// Lazy load transient announce packet
			inline Packet announce_packet() {
				Packet announce_packet = get_cached_packet(_announce_packet_hash);
				if (announce_packet) {
					// Announce packet is cached in packed state
					// so we need to unpack it before accessing.
					announce_packet.unpack();
					// We increase the hops, since reading a packet
					// from cache is equivalent to receiving it again
					// over an interface. It is cached with it's non-
					// increased hop-count.
					announce_packet.hops(announce_packet.hops() + 1);
				}
				return announce_packet;
			}
#else
			// Lazy load and cache announce packet
			inline Packet& announce_packet() {
				if (!_announce_packet) _announce_packet = get_cached_packet(_announce_packet_hash);
				if (_announce_packet) {
					// Announce packet is cached in packed state
					// so we need to unpack it before accessing.
					_announce_packet.unpack();
					// We increase the hops, since reading a packet
					// from cache is equivalent to receiving it again
					// over an interface. It is cached with it's non-
					// increased hop-count.
					_announce_packet.hops(_announce_packet.hops() + 1);
				}
				return _announce_packet;
			}
#endif
			// Getters and setters for receiving_interface_hash and announce_packet_hash
			inline Bytes receiving_interface_hash() const { if (_receiving_interface) return _receiving_interface.get_hash(); return _receiving_interface_hash; }
			inline void receiving_interface_hash(const Bytes& hash) { if (hash == receiving_interface_hash()) return; _receiving_interface_hash = hash; _receiving_interface = {Type::NONE}; }
#if RNS_LEAN_PATH_TABLE
			inline Bytes announce_packet_hash() const { return _announce_packet_hash; }
			inline void announce_packet_hash(const Bytes& hash) { _announce_packet_hash = hash; }
#else
			inline Bytes announce_packet_hash() const { if (_announce_packet) return _announce_packet.get_hash(); return _announce_packet_hash; }
			inline void announce_packet_hash(const Bytes& hash) { if (hash == announce_packet_hash()) return; _announce_packet_hash = hash; _announce_packet = {Type::NONE}; }
#endif
		public:
			double _timestamp = 0;
			Bytes _received_from;
			uint8_t _hops = 0;
			double _expires = 0;
			std::set<Bytes> _random_blobs;
			inline bool operator < (const DestinationEntry& entry) const {
				// sort by ascending timestamp (oldest entries at the top)
				return _timestamp < entry._timestamp;
			}
		private:
			Interface _receiving_interface = {Type::NONE};
			Bytes _receiving_interface_hash;
#if !RNS_LEAN_PATH_TABLE
			//const Packet& _announce_packet;
			Packet _announce_packet = {Type::NONE};
#endif
			Bytes _announce_packet_hash;
		public:
#ifndef NDEBUG
			inline std::string debugString() const {
				std::string dump;
				dump = "DestinationEntry: timestamp=" + std::to_string(_timestamp) +
					" received_from=" + _received_from.toHex() +
					" hops=" + std::to_string(_hops) +
					" expires=" + std::to_string(_expires) +
					//" random_blobs=" + _random_blobs +
					" receiving_interface=" + receiving_interface_hash().toHex() +
					" announce_packet=" + announce_packet_hash().toHex();
				dump += " random_blobs=(";
				for (auto& blob : _random_blobs) {
					dump += blob.toHex() + ",";
				}
				dump += ")";
				return dump;
			}
#endif
		};
		//using PathTable = std::map<Bytes, DestinationEntry>;
		using PathTable = std::map<Bytes, DestinationEntry, std::less<Bytes>, Utilities::Memory::ContainerAllocator<std::pair<const Bytes, DestinationEntry>>>;

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
			const Bytes _received_from;
			uint8_t _hops = 0;
			// CBA Storing packet reference causes memory issues, presumably because orignal packet is being destroyed
			//  MUST use instance instad of reference!!!
			//const Packet& _packet;
			const Packet _packet = {Type::NONE};
			uint8_t _local_rebroadcasts = 0;
			bool _block_rebroadcasts = false;
			const Interface _attached_interface = {Type::NONE};
		};
		//using AnnounceTable = std::map<Bytes, AnnounceEntry>;
		using AnnounceTable = std::map<Bytes, AnnounceEntry, std::less<Bytes>, Utilities::Memory::ContainerAllocator<std::pair<const Bytes, AnnounceEntry>>>;

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
			const Bytes _next_hop;
			const Interface _outbound_interface = {Type::NONE};
			uint8_t _remaining_hops = 0;
			Interface _receiving_interface = {Type::NONE};
			uint8_t _hops = 0;
			const Bytes _destination_hash;
			bool _validated = false;
			double _proof_timeout = 0;
		};
		using LinkTable = std::map<Bytes, LinkEntry>;

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
			Interface _receiving_interface = {Type::NONE};
			const Interface _outbound_interface = {Type::NONE};
			double _timestamp = 0;
		};
		using ReverseTable = std::map<Bytes, ReverseEntry>;

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
			const Bytes _destination_hash;
			double _timeout = 0;
			const Interface _requesting_interface = {Type::NONE};
		};
		using PathRequestTable = std::map<Bytes, PathRequestEntry>;

/*
		// CBA TODO Analyze safety of using Inrerface references here
		class SerialisedEntry {
		public:
			SerialisedEntry(const Bytes& destination_hash, double timestamp, const Bytes& received_from, uint8_t announce_hops, double expires, const std::set<Bytes>& random_blobs, Interface& receiving_interface, const Packet& packet) :
				_destination_hash(destination_hash),
				_timestamp(timestamp),
				_hops(announce_hops),
				_expires(expires),
				_random_blobs(random_blobs),
				_receiving_interface(receiving_interface),
				_announce_packet(packet)
			{
			}
		public:
			const Bytes _destination_hash;
			double _timestamp = 0;
			const Bytes _received_from;
			uint8_t _hops = 0;
			double _expires = 0;
			std::set<Bytes> _random_blobs;
			Interface _receiving_interface = {Type::NONE};
			Packet _announce_packet = {Type::NONE};
		};
*/

		// CBA TODO Analyze safety of using Inrerface references here
		class TunnelEntry {
		public:
			TunnelEntry(const Bytes& tunnel_id, const Bytes& interface_hash, double expires) :
				_tunnel_id(tunnel_id),
				_interface_hash(interface_hash),
				_expires(expires)
			{
			}
		public:
			const Bytes _tunnel_id;
			const Bytes _interface_hash;
			PathTable _serialised_paths;
			double _expires = 0;
		};
		using TunnelTable = std::map<Bytes, TunnelEntry>;

		class RateEntry {
		public:
			RateEntry(double now) :
				_last(now)
			{
				_timestamps.push_back(now);
			}
		public:
			double _last = 0.0;
			double _rate_violations = 0.0;
			double _blocked_until = 0.0;
			std::vector<double> _timestamps;
		};
		using RateTable = std::map<Bytes, RateEntry>;

	public:
		static void start(const Reticulum& reticulum_instance);
		static void loop();
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
		inline static const InterfaceTable& get_interfaces() { return _interfaces; }
		static void register_destination(Destination& destination);
		static void deregister_destination(const Destination& destination);
		static void register_link(Link& link);
		static void activate_link(Link& link);
		static void register_announce_handler(HAnnounceHandler handler);
		static void deregister_announce_handler(HAnnounceHandler handler);
		static bool is_interface_from_hash(const Bytes& interface_hash);
		static Interface find_interface_from_hash(const Bytes& interface_hash);
		static bool should_cache_packet(const Packet& packet);
		static bool cache_packet(const Packet& packet, bool force_cache = false);
		static bool is_cached_packet(const Bytes& packet_hash);
		static Packet get_cached_packet(const Bytes& packet_hash);
		static bool clear_cached_packet(const Bytes& packet_hash);
		static bool cache_request_packet(const Packet& packet);
		static void cache_request(const Bytes& packet_hash, const Destination& destination);
		static DestinationEntry& get_path(const Bytes& destination_hash);
		static bool remove_path(const Bytes& destination_hash);
		static bool has_path(const Bytes& destination_hash);
		static uint8_t hops_to(const Bytes& destination_hash);
		static Bytes next_hop(const Bytes& destination_hash);
		static Interface next_hop_interface(const Bytes& destination_hash);
		static uint32_t next_hop_interface_bitrate(const Bytes& destination_hash);
		static uint16_t next_hop_interface_hw_mtu(const Bytes& destination_hash);
		static double next_hop_per_bit_latency(const Bytes& destination_hash);
		static double next_hop_per_byte_latency(const Bytes& destination_hash);
		static double first_hop_timeout(const Bytes& destination_hash);
		static double extra_link_proof_timeout(const Interface& interface);
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
		static uint64_t announce_emitted(const Packet& packet);
		static void write_packet_hashlist();
		static bool read_path_table();
		static bool write_path_table();
		static void read_tunnel_table();
		static void write_tunnel_table();
		static void persist_data();
		static void clean_caches();
		static void dump_stats();
		static void exit_handler();

		static uint16_t remove_reverse_entries(const std::vector<Bytes>& hashes);
		static uint16_t remove_links(const std::vector<Bytes>& hashes);
		static uint16_t remove_paths(const std::vector<Bytes>& hashes);
		static uint16_t remove_discovery_path_requests(const std::vector<Bytes>& hashes);
		static uint16_t remove_tunnels(const std::vector<Bytes>& hashes);

		static Destination find_destination_from_hash(const Bytes& destination_hash);

		// CBA
		static void cull_path_table();
		static void cull_announce_table();

		// getters/setters
		static inline void set_receive_packet_callback(Callbacks::receive_packet callback) { _callbacks._receive_packet = callback; }
		static inline void set_transmit_packet_callback(Callbacks::transmit_packet callback) { _callbacks._transmit_packet = callback; }
		static inline void set_filter_packet_callback(Callbacks::filter_packet callback) { _callbacks._filter_packet = callback; }
		static inline const Reticulum& reticulum() { return _owner; }
		static inline const Identity& identity() { return _identity; }
		inline static uint16_t path_table_maxsize() { return _path_table_maxsize; }
		inline static void path_table_maxsize(uint16_t path_table_maxsize) { _path_table_maxsize = path_table_maxsize; }
		inline static uint16_t announce_table_maxsize() { return _announce_table_maxsize; }
		inline static void announce_table_maxsize(uint16_t announce_table_maxsize) { _announce_table_maxsize = announce_table_maxsize; }
		inline static uint16_t hashlist_maxsize() { return _hashlist_maxsize; }
		inline static void hashlist_maxsize(uint16_t hashlist_maxsize) { _hashlist_maxsize = hashlist_maxsize; }
		inline static uint16_t max_pr_tags() { return _max_pr_tags; }
		inline static void max_pr_tags(uint16_t max_pr_tags) { _max_pr_tags = max_pr_tags; }
		inline static uint16_t path_table_maxpersist() { return _path_table_maxpersist; }
		inline static void path_table_maxpersist(uint16_t path_table_maxpersist) { _path_table_maxpersist = path_table_maxpersist; }
		// CBA TEST
		static inline void identity(Identity& identity) { _identity = identity; }

		inline static const PathTable& get_path_table() { return _path_table; }
		inline static const RateTable& get_announce_rate_table() { return _announce_rate_table; }
		inline static const LinkTable& get_link_table() { return _link_table; }

	private:
		// CBA MUST use references to interfaces here in order for virtul overrides for send/receive to work
		// map is sorted, can use find
		static InterfaceTable _interfaces;			// All active interfaces
		static DestinationTable _destinations;		// All active destinations
		// CBA TODO: Reconsider using std::set for enforcing uniqueness. Maybe consider std::map keyed on hash instead
		static std::set<Link> _pending_links;		// Links that are being established
		static std::set<Link> _active_links;		// Links that are active
		static std::set<Bytes> _packet_hashlist;	// A list of packet hashes for duplicate detection
		static std::list<PacketReceipt> _receipts;	// Receipts of all outgoing packets for proof processing

		// TODO: "destination_table" should really be renamed to "path_table"
		// Notes on memory usage: 1 megabyte of memory can store approximately
		// 55.100 path table entries or approximately 22.300 link table entries.

		static AnnounceTable _announce_table;	// A table for storing announces currently waiting to be retransmitted
		static PathTable _path_table;			// A lookup table containing the next hop to a given destination
		static ReverseTable _reverse_table;		// A lookup table for storing packet hashes used to return proofs and replies
		static LinkTable _link_table;			// A lookup table containing hops for links
		static AnnounceTable _held_announces;	// A table containing temporarily held announce-table entries
		static TunnelTable _tunnels;			// A table storing tunnels to other transport instances
		static RateTable _announce_rate_table;	// A table for keeping track of announce rates
		static std::set<HAnnounceHandler> _announce_handlers;	// A table storing externally registered announce handlers
		static std::map<Bytes, double> _path_requests;	// A table for storing path request timestamps

		static PathRequestTable _discovery_path_requests;	// A table for keeping track of path requests on behalf of other nodes
		static std::set<Bytes> _discovery_pr_tags;	// A table for keeping track of tagged path requests

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

		// CBA
		static PacketTable _packet_table;           // A lookup table containing announce packets for known paths

		//z _local_client_rssi_cache    = []
		//z _local_client_snr_cache     = []
		static uint16_t _LOCAL_CLIENT_CACHE_MAXSIZE;

		static double _start_time;
		static bool _jobs_locked;
		static bool _jobs_running;
		static float _job_interval;
		static double _jobs_last_run;
		static double _links_last_checked;
		static float _links_check_interval;
		static double _receipts_last_checked;
		static float _receipts_check_interval;
		static double _announces_last_checked;
		static float _announces_check_interval;
		static double _tables_last_culled;
		static float _tables_cull_interval;
		static bool _saving_path_table;
		static uint16_t _hashlist_maxsize;
		static uint16_t _max_pr_tags;

		// CBA
		static uint16_t _path_table_maxsize;
		static uint16_t _path_table_maxpersist;
		static double _last_saved;
		static float _save_interval;
		static uint32_t _path_table_crc;
		static uint16_t _announce_table_maxsize;

		static Reticulum _owner;
		static Identity _identity;

		// CBA
		static Callbacks _callbacks;

		// CBA Stats
		static uint32_t _packets_sent;
		static uint32_t _packets_received;
		static uint32_t _destinations_added;
		static size_t _last_memory;
		static size_t _last_psram;
		static size_t _last_flash;

		// CBA Block re-entrance
		static bool cleaning_caches;
	};

	template <typename M, typename S> 
	void MapToValues(const M& m, S& s) {
		for (typename M::const_iterator it = m.begin(); it != m.end(); ++it) {
			s.insert(it->second);
		}
	}

	template <typename M, typename S> 
	void MapToPairs(const M& m, S& s) {
		for (typename M::const_iterator it = m.begin(); it != m.end(); ++it) {
			s.push_back(*it);
		}
	}
}
