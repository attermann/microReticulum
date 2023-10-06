#pragma once

#include "Link.h"


namespace RNS {

	class Packet;
	class Destination;

	class Transport {

	public:
		// Constants
		enum types {
		BROADCAST    = 0x00,
		TRANSPORT    = 0x01,
		RELAY        = 0x02,
		TUNNEL       = 0x03,
		NONE         = 0xFF,
		};

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

		static constexpr const float LINK_TIMEOUT            = Link::STALE_TIME * 1.25;
		static const uint16_t REVERSE_TIMEOUT      = 30*60;        // Reverse table entries are removed after 30 minutes
		static const uint32_t DESTINATION_TIMEOUT  = 60*60*24*7;   // Destination table entries are removed if unused for one week
		static const uint16_t MAX_RECEIPTS         = 1024;         // Maximum number of receipts to keep track of
		static const uint8_t MAX_RATE_TIMESTAMPS   = 16;           // Maximum number of announce timestamps to keep per destination

	public:
		Transport();
		~Transport();

	public:
		static void register_destination(Destination &destination);
		static bool outbound(const Packet &packet);

	};

}