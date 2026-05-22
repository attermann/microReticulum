// Interop-test sender for basic-packet-exchange.
//
// Flow:
//   1. Set up own Reticulum + UDPInterface + IN-direction Destination.
//   2. Register a packet callback that validates Python's inbound packet
//      against an expected pattern.
//   3. Announce our destination so Python can learn its hash.
//   4. AnnounceHandler waits for Python's announce, builds an OUT
//      Destination, and sends a deterministic packet to it.
//   5. Exit 0 iff BOTH directions completed (Python heard our packet AND
//      we heard Python's packet) before the timeout.

#include <microStore/FileSystem.h>
#include <microStore/Adapters/UniversalFileSystem.h>
#include <UDPInterface.h>

#include <Reticulum.h>
#include <Identity.h>
#include <Destination.h>
#include <Packet.h>
#include <Transport.h>
#include <Interface.h>
#include <Log.h>
#include <Bytes.h>
#include <Type.h>
#include <Utilities/OS.h>

#include <stdio.h>

static const char* APP_NAME = "microreticulum_interop";
static const char* ASPECT   = "packet";

// 256-byte pattern for C++ -> Python: bytes 0x00..0xff.
static RNS::Bytes build_cpp_to_python_payload() {
	RNS::Bytes p;
	for (size_t i = 0; i < 256; ++i) p.append((uint8_t)(i & 0xff));
	return p;
}
// 256-byte pattern for Python -> C++: reverse, bytes 0xff..0x00. Distinct
// so any directional mix-up is obvious.
static RNS::Bytes build_python_to_cpp_payload() {
	RNS::Bytes p;
	for (size_t i = 0; i < 256; ++i) p.append((uint8_t)(0xff - (i & 0xff)));
	return p;
}

static RNS::Reticulum reticulum({RNS::Type::NONE});
static RNS::Interface udp_interface(RNS::Type::NONE);
static RNS::Identity  local_identity({RNS::Type::NONE});
static RNS::Destination local_destination({RNS::Type::NONE});
static RNS::Destination outgoing_destination({RNS::Type::NONE});
static volatile bool cpp_received_packet = false;
static volatile bool cpp_sent_packet     = false;
static volatile bool announce_seen       = false;

static bool bytes_equal(const RNS::Bytes& a, const RNS::Bytes& b) {
	return a.size() == b.size()
	    && memcmp(a.data(), b.data(), a.size()) == 0;
}

// Local packet callback — fires when Python's packet arrives.
static void on_local_packet(const RNS::Bytes& data, const RNS::Packet& packet) {
	RNS::Bytes expected = build_python_to_cpp_payload();
	const bool ok = bytes_equal(data, expected);
	printf("[cpp] packet received: %lu bytes, match=%s\n",
	       (unsigned long)data.size(), ok ? "yes" : "no");
	if (ok) cpp_received_packet = true;
}

class InteropAnnounceHandler : public RNS::AnnounceHandler {
public:
	InteropAnnounceHandler()
	  : RNS::AnnounceHandler((std::string(APP_NAME) + "." + ASPECT).c_str()) {}

	void received_announce(const RNS::Bytes& destination_hash,
	                       const RNS::Identity& announced_identity,
	                       const RNS::Bytes& app_data) override {
		if (announce_seen) return;
		// Ignore our own announce echoed back via the broadcast/loopback path.
		if (destination_hash == local_destination.hash()) return;
		announce_seen = true;

		printf("[cpp] received announce: dest=%s\n",
		       destination_hash.toHex().c_str());

		outgoing_destination = RNS::Destination(announced_identity,
		                                        RNS::Type::Destination::OUT,
		                                        RNS::Type::Destination::SINGLE,
		                                        APP_NAME, ASPECT);

		RNS::Bytes payload = build_cpp_to_python_payload();
		try {
			RNS::Packet(outgoing_destination, payload).send();
			printf("[cpp] packet sent: %lu bytes\n", (unsigned long)payload.size());
			cpp_sent_packet = true;
		}
		catch (const std::exception& e) {
			printf("[cpp] packet send failed: %s\n", e.what());
		}
	}
};
static RNS::HAnnounceHandler announce_handler(new InteropAnnounceHandler());

int main() {
	printf("[cpp] packet_interop_sender starting\n");

	microStore::FileSystem filesystem{microStore::Adapters::UniversalFileSystem()};
	filesystem.init();
	RNS::Utilities::OS::register_filesystem(filesystem);

	udp_interface = new UDPInterface();
	udp_interface.mode(RNS::Type::Interface::MODE_GATEWAY);
	RNS::Transport::register_interface(udp_interface);
	udp_interface.start();

	reticulum = RNS::Reticulum();
	reticulum.transport_enabled(false);
	reticulum.start();

	local_identity = RNS::Identity();
	local_destination = RNS::Destination(local_identity,
	                                     RNS::Type::Destination::IN,
	                                     RNS::Type::Destination::SINGLE,
	                                     APP_NAME, ASPECT);
	local_destination.set_packet_callback(on_local_packet);
	local_destination.set_proof_strategy(RNS::Type::Destination::PROVE_ALL);

	RNS::Transport::register_announce_handler(announce_handler);

	// Announce so Python can discover our destination.
	local_destination.announce();
	printf("[cpp] announced own destination %s\n",
	       local_destination.hash().toHex().c_str());

	const double TIMEOUT_S = 30.0;
	const double start = RNS::Utilities::OS::time();
	double last_re_announce = start;

	while (true) {
		reticulum.loop();
		if (cpp_received_packet && cpp_sent_packet) break;
		const double now = RNS::Utilities::OS::time();
		if (now - start > TIMEOUT_S) {
			printf("[cpp] TIMEOUT (received=%d sent=%d announce_seen=%d)\n",
			       cpp_received_packet ? 1 : 0,
			       cpp_sent_packet     ? 1 : 0,
			       announce_seen       ? 1 : 0);
			break;
		}
		// Re-announce every 3s in case the first announce raced Python's
		// startup.
		if (now - last_re_announce >= 3.0 && !announce_seen) {
			local_destination.announce();
			last_re_announce = now;
		}
		RNS::Utilities::OS::sleep(0.01);
	}

	RNS::Transport::deregister_interface(udp_interface);
	const int rc = (cpp_received_packet && cpp_sent_packet) ? 0 : 1;
	printf("[cpp] exit code %d\n", rc);
	return rc;
}
