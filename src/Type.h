#pragma once

#include "Log.h"
#include "Cryptography/Fernet.h"

#include <stdint.h>

namespace RNS { namespace Type {

	// generic empty object constructor type
	enum NoneConstructor {
		NONE
	};

	namespace Persistence {

		//static const uint16_t DOCUMENT_MAXSIZE = 1024;
		static const uint16_t DOCUMENT_MAXSIZE = 8192;
		//static const uint16_t DOCUMENT_MAXSIZE = 16384;
		static const uint16_t BUFFER_MAXSIZE = Persistence::DOCUMENT_MAXSIZE * 1.5;	// Json write buffer of 1.5 times document seems to be sufficient

	}

	namespace Reticulum {

		/*
		The MTU that Reticulum adheres to, and will expect other peers to
		adhere to. By default, the MTU is 507 bytes. In custom RNS network
		implementations, it is possible to change this value, but doing so will
		completely break compatibility with all other RNS networks. An identical
		MTU is a prerequisite for peers to communicate in the same network.

		Unless you really know what you are doing, the MTU should be left at
		the default value.

		Future minimum will probably be locked in at 251 bytes to support
		networks with segments of different MTUs. Absolute minimum is 219.
		*/
		static const uint16_t MTU = 500;

		static const uint16_t MAX_QUEUED_ANNOUNCES = 16384;
		static const uint32_t QUEUED_ANNOUNCE_LIFE = 60*60*24;

		static const uint8_t ANNOUNCE_CAP = 2;
		/*
		The maximum percentage of interface bandwidth that, at any given time,
		may be used to propagate announces. If an announce was scheduled for
		broadcasting on an interface, but doing so would exceed the allowed
		bandwidth allocation, the announce will be queued for transmission
		when there is bandwidth available.

		Reticulum will always prioritise propagating announces with fewer
		hops, ensuring that distant, large networks with many peers on fast
		links don't overwhelm the capacity of smaller networks on slower
		mediums. If an announce remains queued for an extended amount of time,
		it will eventually be dropped.

		This value will be applied by default to all created interfaces,
		but it can be configured individually on a per-interface basis.
		*/

		static const uint16_t MINIMUM_BITRATE = 500;

		// TODO: To reach the 300bps level without unreasonably impacting
		// performance on faster links, we need a mechanism for setting
		// this value more intelligently. One option could be inferring it
		// from interface speed, but a better general approach would most
		// probably be to let Reticulum somehow continously build a map of
		// per-hop latencies and use this map for the timeout calculation. 
		static const uint8_t DEFAULT_PER_HOP_TIMEOUT = 6;

		static const uint16_t HASHLENGTH = 256;	// In bits
		// Length of truncated hashes in bits.
		static const uint16_t TRUNCATED_HASHLENGTH = 128;	// In bits

		static const uint16_t HEADER_MINSIZE   = 2+1+(TRUNCATED_HASHLENGTH/8)*1;	// In bytes
		static const uint16_t HEADER_MAXSIZE   = 2+1+(TRUNCATED_HASHLENGTH/8)*2;	// In bytes
		static const uint16_t IFAC_MIN_SIZE    = 1;
		//z IFAC_SALT        = bytes.fromhex("adf54d882c9a9b80771eb4995d702d4a3e733391b2a0f53f416d9f907e55cff8")

		static const uint16_t MDU              = MTU - HEADER_MAXSIZE - IFAC_MIN_SIZE;

		static const uint32_t RESOURCE_CACHE   = 60*60*24;
		// CBA TEST
		//static const uint16_t JOB_INTERVAL     = 60*5;
		static const uint16_t JOB_INTERVAL     = 60;
		// CBA TEST
		//static const uint16_t CLEAN_INTERVAL   = 60*15;
		static const uint16_t CLEAN_INTERVAL   = 60;
		// CBA MCU
		//static const uint16_t PERSIST_INTERVAL = 60*60*12;
		// CBA TEST
		//static const uint16_t PERSIST_INTERVAL = 60*60;
		static const uint16_t PERSIST_INTERVAL = 60;
		static const uint16_t GRACIOUS_PERSIST_INTERVAL = 60*5;

		static const uint8_t DESTINATION_LENGTH = TRUNCATED_HASHLENGTH/8;	// In bytes

		// CBA MCU
		//static const uint8_t FILEPATH_MAXSIZE = 64;
		static const uint8_t FILEPATH_MAXSIZE = 96;

	}

	namespace Identity {

		//static const char CURVE[] = "Curve25519";
		static constexpr const char* CURVE = "Curve25519";
		// The curve used for Elliptic Curve DH key exchanges

		static const uint16_t KEYSIZE     = 256*2;
		// X25519 key size in bits. A complete key is the concatenation of a 256 bit encryption key, and a 256 bit signing key.

		// Non-configurable constants
		static const uint8_t FERNET_OVERHEAD           = Cryptography::Fernet::FERNET_OVERHEAD;
		static const uint8_t AES128_BLOCKSIZE           = 16;          // In bytes
		static const uint16_t HASHLENGTH                = Reticulum::HASHLENGTH;	// In bits
		static const uint16_t SIGLENGTH                 = KEYSIZE;     // In bits

		static const uint8_t NAME_HASH_LENGTH     = 80;
		static const uint8_t RANDOM_HASH_LENGTH     = 80;
		static const uint16_t TRUNCATED_HASHLENGTH = Reticulum::TRUNCATED_HASHLENGTH;	// In bits
		// Constant specifying the truncated hash length (in bits) used by Reticulum
		// for addressable hashes and other purposes. Non-configurable.

	}

	namespace Destination {

		enum types {
			SINGLE     = 0x00,
			GROUP      = 0x01,
			PLAIN      = 0x02,
			LINK       = 0x03,
		};

		enum proof_strategies {
			PROVE_NONE = 0x21,
			PROVE_APP  = 0x22,
			PROVE_ALL  = 0x23,
		};

		enum request_policies {
			ALLOW_NONE = 0x00,
			ALLOW_ALL  = 0x01,
			ALLOW_LIST = 0x02,
		};

		enum directions {
			IN         = 0x11,
			OUT        = 0x12,
		};

		const uint8_t PR_TAG_WINDOW = 30;

	}

	namespace Link {

		// The curve used for Elliptic Curve DH key exchanges
		static constexpr const char* CURVE = Identity::CURVE;

		static const uint16_t ECPUBSIZE         = 32+32;
		static const uint8_t KEYSIZE           = 32;

		//static const uint16_t MDU = floor((Reticulum::MTU-Reticulum::IFAC_MIN_SIZE-Reticulum::HEADER_MINSIZE-Identity::FERNET_OVERHEAD)/Identity::AES128_BLOCKSIZE)*Identity::AES128_BLOCKSIZE - 1;
		static const uint16_t MDU = ((Reticulum::MTU-Reticulum::IFAC_MIN_SIZE-Reticulum::HEADER_MINSIZE-Identity::FERNET_OVERHEAD)/Identity::AES128_BLOCKSIZE)*Identity::AES128_BLOCKSIZE - 1;

		// Timeout for link establishment in seconds per hop to destination.
		static const uint8_t ESTABLISHMENT_TIMEOUT_PER_HOP = Reticulum::DEFAULT_PER_HOP_TIMEOUT;

		// RTT timeout factor used in link timeout calculation.
		static const uint16_t TRAFFIC_TIMEOUT_FACTOR = 6;
		static const uint16_t KEEPALIVE_TIMEOUT_FACTOR = 4;
		// Grace period in seconds used in link timeout calculation.
		static const uint8_t STALE_GRACE = 2;
		// Interval for sending keep-alive packets on established links in seconds.
		static const uint16_t KEEPALIVE = 360;
		/*
		If no traffic or keep-alive packets are received within this period, the
		link will be marked as stale, and a final keep-alive packet will be sent.
		If after this no traffic or keep-alive packets are received within ``RTT`` *
		``KEEPALIVE_TIMEOUT_FACTOR`` + ``STALE_GRACE``, the link is considered timed out,
		and will be torn down.
		*/
		static const uint16_t STALE_TIME = 2*KEEPALIVE;

		enum status {
			PENDING   = 0x00,
			HANDSHAKE = 0x01,
			ACTIVE    = 0x02,
			STALE     = 0x03,
			CLOSED    = 0x04
		};

		enum teardown_reason {
			TIMEOUT            = 0x01,
			INITIATOR_CLOSED   = 0x02,
			DESTINATION_CLOSED = 0x03,
		};

		enum resource_strategy {
			ACCEPT_NONE = 0x00,
			ACCEPT_APP  = 0x01,
			ACCEPT_ALL  = 0x02,
		};

	}

	namespace Interface {

		// Interface mode definitions
		enum modes {
			MODE_NONE           = 0x00,
			MODE_FULL           = 0x01,
			MODE_POINT_TO_POINT = 0x04,
			MODE_ACCESS_POINT   = 0x08,
			MODE_ROAMING        = 0x10,
			MODE_BOUNDARY       = 0x20,
			MODE_GATEWAY        = 0x40,
		};

	}

	namespace Packet {

		// Packet types
		enum types {
			DATA         = 0x00,     // Data packets
			ANNOUNCE     = 0x01,     // Announces
			LINKREQUEST  = 0x02,     // Link requests
			PROOF        = 0x03,      // Proofs
		};

		// Header types
		enum header_types {
			HEADER_1     = 0x00,     // Normal header format
			HEADER_2     = 0x01,      // Header format used for packets in transport
		};

		// Packet context types
		enum context_types {
			CONTEXT_NONE   = 0x00,   // Generic data packet
			RESOURCE       = 0x01,   // Packet is part of a resource
			RESOURCE_ADV   = 0x02,   // Packet is a resource advertisement
			RESOURCE_REQ   = 0x03,   // Packet is a resource part request
			RESOURCE_HMU   = 0x04,   // Packet is a resource hashmap update
			RESOURCE_PRF   = 0x05,   // Packet is a resource proof
			RESOURCE_ICL   = 0x06,   // Packet is a resource initiator cancel message
			RESOURCE_RCL   = 0x07,   // Packet is a resource receiver cancel message
			CACHE_REQUEST  = 0x08,   // Packet is a cache request
			REQUEST        = 0x09,   // Packet is a request
			RESPONSE       = 0x0A,   // Packet is a response to a request
			PATH_RESPONSE  = 0x0B,   // Packet is a response to a path request
			COMMAND        = 0x0C,   // Packet is a command
			COMMAND_STATUS = 0x0D,   // Packet is a status of an executed command
			CHANNEL        = 0x0E,   // Packet contains link channel data
			KEEPALIVE      = 0xFA,   // Packet is a keepalive packet
			LINKIDENTIFY   = 0xFB,   // Packet is a link peer identification proof
			LINKCLOSE      = 0xFC,   // Packet is a link close message
			LINKPROOF      = 0xFD,   // Packet is a link packet proof
			LRRTT          = 0xFE,   // Packet is a link request round-trip time measurement
			LRPROOF        = 0xFF,    // Packet is a link request proof
		};

		// This is used to calculate allowable
		// payload sizes
		static const uint16_t HEADER_MAXSIZE = Reticulum::HEADER_MAXSIZE;
		static const uint16_t MDU            = Reticulum::MDU;

		// With an MTU of 500, the maximum of data we can
		// send in a single encrypted packet is given by
		// the below calculation; 383 bytes.
		//static const uint16_t ENCRYPTED_MDU  = floor((Reticulum::MDU-Identity::FERNET_OVERHEAD-Identity::KEYSIZE/16)/Identity::AES128_BLOCKSIZE)*Identity::AES128_BLOCKSIZE - 1;
		//static const uint16_t ENCRYPTED_MDU;
		static const uint16_t ENCRYPTED_MDU  = ((Reticulum::MDU-Identity::FERNET_OVERHEAD-Identity::KEYSIZE/16)/Identity::AES128_BLOCKSIZE)*Identity::AES128_BLOCKSIZE - 1;
		// The maximum size of the payload data in a single encrypted packet 
		static const uint16_t PLAIN_MDU      = MDU;
		// The maximum size of the payload data in a single unencrypted packet

		static const uint8_t TIMEOUT_PER_HOP = Reticulum::DEFAULT_PER_HOP_TIMEOUT;

	}

	namespace PacketReceipt {

		// Receipt status constants
		enum Status {
			FAILED    = 0x00,
			SENT      = 0x01,
			DELIVERED = 0x02,
			CULLED    = 0xFF
		};

		static const uint16_t EXPL_LENGTH = Identity::HASHLENGTH / 8 + Identity::SIGLENGTH / 8;
		static const uint16_t IMPL_LENGTH = Identity::SIGLENGTH / 8;

	}

	namespace Transport {

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

		enum state {
			STATE_UNKNOWN        = 0x00,
			STATE_UNRESPONSIVE   = 0x01,
			STATE_RESPONSIVE     = 0x02,
		};

		static constexpr const char* APP_NAME = "rnstransport";

		// Maximum amount of hops that Reticulum will transport a packet.
		static const uint8_t PATHFINDER_M    = 128;       // Max hops

		static const uint8_t PATHFINDER_R      = 1;          // Retransmit retries
		static const uint8_t PATHFINDER_G      = 5;          // Retry grace period
		static constexpr const float PATHFINDER_RW     = 0.5;        // Random window for announce rebroadcast

		// TODO: Calculate an optimal number for this in
		// various situations
		static const uint8_t LOCAL_REBROADCASTS_MAX = 2;          // How many local rebroadcasts of an announce is allowed

		static const uint8_t PATH_REQUEST_TIMEOUT = 15;           // Default timuout for client path requests in seconds
		static constexpr const float PATH_REQUEST_GRACE     = 0.35;         // Grace time before a path announcement is made, allows directly reachable peers to respond first
		static const uint8_t PATH_REQUEST_RW      = 2;            // Path request random window
		static const uint8_t PATH_REQUEST_MI      = 5;            // Minimum interval in seconds for automated path requests

		static constexpr const float LINK_TIMEOUT  = Link::STALE_TIME * 1.25;
		static const uint16_t REVERSE_TIMEOUT      = 30*60;        // Reverse table entries are removed after 30 minutes
		// CBA MCU
		//static const uint16_t MAX_RECEIPTS         = 1024;         // Maximum number of receipts to keep track of
		static const uint16_t MAX_RECEIPTS         = 20;         // Maximum number of receipts to keep track of
		static const uint8_t MAX_RATE_TIMESTAMPS   = 16;           // Maximum number of announce timestamps to keep per destination
		static const uint8_t PERSIST_RANDOM_BLOBS  = 32;           // Maximum number of random blobs per destination to persist to disk
		static const uint8_t MAX_RANDOM_BLOBS      = 64;           // Maximum number of random blobs per destination to keep in memory

		// CBA MCU
		//static const uint32_t DESTINATION_TIMEOUT = 60*60*24*7;   // Destination table entries are removed if unused for one week
		//static const uint32_t PATHFINDER_E      = 60*60*24*7; // Path expiration of one week
		//static const uint32_t AP_PATH_TIME      = 60*60*24;   // Path expiration of one day for Access Point paths
		//static const uint32_t ROAMING_PATH_TIME = 60*60*6;    // Path expiration of 6 hours for Roaming paths
		static const uint32_t DESTINATION_TIMEOUT = 60*60*24*1;   // Destination table entries are removed if unused for one day
		static const uint32_t PATHFINDER_E      = 60*60*24*1; // Path expiration of one day
		static const uint32_t AP_PATH_TIME      = 60*60*6;   // Path expiration of 6 hours for Access Point paths
		static const uint32_t ROAMING_PATH_TIME = 60*60*1;    // Path expiration of 1 hour for Roaming paths

		static const uint16_t LOCAL_CLIENT_CACHE_MAXSIZE = 512;
	}

} }
