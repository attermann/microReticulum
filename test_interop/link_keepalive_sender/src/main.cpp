// Interop sender for the link keepalive scenario.
//
// Opens a link to a Python destination and leaves it idle. On a local link
// the keepalive interval is KEEPALIVE_MIN (5 s) and the stale time 10 s, so
// the link only stays up for HOLD_S if keepalives go out and their replies
// come back. Python then stops answering, and the link has to go stale and
// close with TIMEOUT, sending a LINKCLOSE that Python receives.
//
// Exits 0 iff the link stayed up for at least HOLD_S and then closed with
// TIMEOUT.

#include <microStore/FileSystem.h>
#include <microStore/Adapters/UniversalFileSystem.h>
#include <UDPInterface.h>

#include <microReticulum.h>

#include <stdio.h>

static const char* APP_NAME = "microreticulum_interop";
static const char* ASPECT   = "keepalive";

// Longer than the stale time plus the grace period on a local link.
static const double HOLD_S    = 20.0;
static const double TIMEOUT_S = 75.0;

static RNS::Reticulum reticulum({RNS::Type::NONE});
static RNS::Interface udp_interface(RNS::Type::NONE);
static RNS::Destination outgoing_destination({RNS::Type::NONE});
static RNS::Link active_link({RNS::Type::NONE});

static bool requested     = false;
static bool established   = false;
static bool closed        = false;
static bool went_stale    = false;
static double established_at = 0.0;
static double closed_at      = 0.0;
static uint8_t closed_reason = RNS::Type::Link::TEARDOWN_NONE;

static const char* reason_name(uint8_t r) {
	switch (r) {
		case RNS::Type::Link::TIMEOUT:            return "TIMEOUT";
		case RNS::Type::Link::INITIATOR_CLOSED:   return "INITIATOR_CLOSED";
		case RNS::Type::Link::DESTINATION_CLOSED: return "DESTINATION_CLOSED";
		default:                                  return "TEARDOWN_NONE";
	}
}

static void on_link_established(RNS::Link& link) {
	printf("[cpp] link established: %s, rtt %.3f s\n", link.hash().toHex().c_str(), link.rtt());
	established    = true;
	established_at = RNS::Utilities::OS::time();
}

static void on_link_closed(RNS::Link& link) {
	closed        = true;
	closed_at     = RNS::Utilities::OS::time();
	closed_reason = (uint8_t)link.teardown_reason();
	printf("[cpp] link closed after %.1f s: teardown_reason=%s\n",
	       closed_at - established_at, reason_name(closed_reason));
}

class AnnounceHandler : public RNS::AnnounceHandler {
public:
	AnnounceHandler() : RNS::AnnounceHandler(
	    (std::string(APP_NAME) + "." + ASPECT).c_str()) {}
	void received_announce(const RNS::Bytes& destination_hash,
	                       const RNS::Identity& announced_identity,
	                       const RNS::Bytes& app_data) override {
		if (requested) return;
		printf("[cpp] received announce: dest=%s\n", destination_hash.toHex().c_str());
		outgoing_destination = RNS::Destination(announced_identity,
		                                        RNS::Type::Destination::OUT,
		                                        RNS::Type::Destination::SINGLE,
		                                        APP_NAME, ASPECT);
		requested = true;
		active_link = RNS::Link(outgoing_destination, on_link_established, on_link_closed);
	}
};
static RNS::HAnnounceHandler announce_handler(new AnnounceHandler());

int main() {
	printf("[cpp] link_keepalive_sender starting\n");

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

	RNS::Transport::register_announce_handler(announce_handler);

	const double start = RNS::Utilities::OS::time();
	while (!closed) {
		reticulum.loop();
		const double now = RNS::Utilities::OS::time();
		if (established && !went_stale && active_link.status() == RNS::Type::Link::STALE) {
			went_stale = true;
			printf("[cpp] link went stale after %.1f s\n", now - established_at);
		}
		if (now - start > TIMEOUT_S) {
			printf("[cpp] TIMEOUT established=%d\n", established ? 1 : 0);
			break;
		}
		RNS::Utilities::OS::sleep(0.01);
	}

	RNS::Transport::deregister_interface(udp_interface);
	const bool held = closed && closed_at - established_at >= HOLD_S;
	const int rc = (established && held && closed_reason == RNS::Type::Link::TIMEOUT) ? 0 : 1;
	printf("[cpp] exit code %d (established=%d held=%d stale=%d reason=%s)\n",
	       rc, established ? 1 : 0, held ? 1 : 0, went_stale ? 1 : 0, reason_name(closed_reason));
	return rc;
}
