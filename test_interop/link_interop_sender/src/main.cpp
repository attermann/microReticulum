// Interop-test sender for link lifecycle (setup + teardown, both polarities).
//
// Test flow:
//   Pass 1 (C++ initiates teardown):
//     - Wait for Python's announce
//     - Build Link to Python's destination
//     - Wait for established callback (both sides should fire)
//     - 1s later, call link.teardown()
//     - Verify closed callback fires with teardown_reason = INITIATOR_CLOSED
//   Pass 2 (Python initiates teardown):
//     - Build a second Link
//     - Wait for established callback
//     - Do NOT tear down; wait for Python to call teardown()
//     - Verify closed callback fires with teardown_reason = DESTINATION_CLOSED
//   Exit 0 iff both passes succeed.

#include <microStore/FileSystem.h>
#include <microStore/Adapters/UniversalFileSystem.h>
#include <UDPInterface.h>

#include <microReticulum.h>

#include <stdio.h>

static const char* APP_NAME = "microreticulum_interop";
static const char* ASPECT   = "link";

static RNS::Reticulum reticulum({RNS::Type::NONE});
static RNS::Interface udp_interface(RNS::Type::NONE);
static RNS::Identity local_identity({RNS::Type::NONE});
static RNS::Destination outgoing_destination({RNS::Type::NONE});
static RNS::Link active_link({RNS::Type::NONE});

enum class Phase { WAIT_ANNOUNCE, PASS1_ESTABLISH, PASS1_TEARDOWN,
                   PASS2_ESTABLISH, PASS2_AWAIT_REMOTE_TEARDOWN, DONE };
static volatile Phase phase = Phase::WAIT_ANNOUNCE;
static volatile bool pass1_established = false;
static volatile bool pass1_closed_ok   = false;
static volatile bool pass2_established = false;
static volatile bool pass2_closed_ok   = false;
static double pass1_established_at = 0.0;

static const char* reason_name(uint8_t r) {
	switch (r) {
		case RNS::Type::Link::TIMEOUT:            return "TIMEOUT";
		case RNS::Type::Link::INITIATOR_CLOSED:   return "INITIATOR_CLOSED";
		case RNS::Type::Link::DESTINATION_CLOSED: return "DESTINATION_CLOSED";
		default:                                  return "TEARDOWN_NONE";
	}
}

static void on_link_established(RNS::Link& link) {
	if (phase == Phase::PASS1_ESTABLISH) {
		printf("[cpp] pass1 link established: %s\n", link.hash().toHex().c_str());
		pass1_established      = true;
		pass1_established_at   = RNS::Utilities::OS::time();
		phase                  = Phase::PASS1_TEARDOWN;
	}
	else if (phase == Phase::PASS2_ESTABLISH) {
		printf("[cpp] pass2 link established: %s\n", link.hash().toHex().c_str());
		pass2_established = true;
		phase             = Phase::PASS2_AWAIT_REMOTE_TEARDOWN;
	}
	else {
		printf("[cpp] unexpected link established (phase=%d)\n", (int)phase);
	}
}

static void on_link_closed(RNS::Link& link) {
	const uint8_t r = (uint8_t)link.teardown_reason();
	if (phase == Phase::PASS1_TEARDOWN) {
		printf("[cpp] pass1 link closed: teardown_reason=%s\n", reason_name(r));
		// We initiated the teardown — C++ side should see INITIATOR_CLOSED.
		pass1_closed_ok = (r == RNS::Type::Link::INITIATOR_CLOSED);
		// Move on to pass 2 by establishing another link.
		printf("[cpp] starting pass2: new link\n");
		active_link = RNS::Link(outgoing_destination,
		                        on_link_established, on_link_closed);
		phase = Phase::PASS2_ESTABLISH;
	}
	else if (phase == Phase::PASS2_AWAIT_REMOTE_TEARDOWN) {
		printf("[cpp] pass2 link closed: teardown_reason=%s\n", reason_name(r));
		// Python initiated this teardown — C++ side should see DESTINATION_CLOSED.
		pass2_closed_ok = (r == RNS::Type::Link::DESTINATION_CLOSED);
		phase = Phase::DONE;
	}
	else {
		printf("[cpp] unexpected link closed (phase=%d, reason=%s)\n",
		       (int)phase, reason_name(r));
	}
}

class AnnounceHandler : public RNS::AnnounceHandler {
public:
	AnnounceHandler() : RNS::AnnounceHandler(
	    (std::string(APP_NAME) + "." + ASPECT).c_str()) {}
	void received_announce(const RNS::Bytes& destination_hash,
	                       const RNS::Identity& announced_identity,
	                       const RNS::Bytes& app_data) override {
		if (phase != Phase::WAIT_ANNOUNCE) return;
		printf("[cpp] received announce: dest=%s\n",
		       destination_hash.toHex().c_str());
		outgoing_destination = RNS::Destination(announced_identity,
		                                        RNS::Type::Destination::OUT,
		                                        RNS::Type::Destination::SINGLE,
		                                        APP_NAME, ASPECT);
		phase = Phase::PASS1_ESTABLISH;
		printf("[cpp] starting pass1: new link\n");
		active_link = RNS::Link(outgoing_destination,
		                        on_link_established, on_link_closed);
	}
};
static RNS::HAnnounceHandler announce_handler(new AnnounceHandler());

int main() {
	printf("[cpp] link_interop_sender starting\n");

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
	RNS::Transport::register_announce_handler(announce_handler);

	const double TIMEOUT_S = 45.0;
	const double start = RNS::Utilities::OS::time();

	while (true) {
		reticulum.loop();
		const double now = RNS::Utilities::OS::time();

		// Trigger our own teardown 1s after pass1 established.
		if (phase == Phase::PASS1_TEARDOWN
		    && pass1_established
		    && now - pass1_established_at >= 1.0
		    && !pass1_closed_ok /* still active */) {
			// Only fire once — guard via active_link.status() != CLOSED.
			if (active_link && active_link.status() != RNS::Type::Link::CLOSED) {
				printf("[cpp] pass1 calling link.teardown()\n");
				active_link.teardown();
			}
		}

		if (phase == Phase::DONE) break;
		if (now - start > TIMEOUT_S) {
			printf("[cpp] TIMEOUT phase=%d p1est=%d p1cls=%d p2est=%d p2cls=%d\n",
			       (int)phase, pass1_established?1:0, pass1_closed_ok?1:0,
			       pass2_established?1:0, pass2_closed_ok?1:0);
			break;
		}
		RNS::Utilities::OS::sleep(0.01);
	}

	RNS::Transport::deregister_interface(udp_interface);
	const int rc = (pass1_established && pass1_closed_ok
	             && pass2_established && pass2_closed_ok) ? 0 : 1;
	printf("[cpp] exit code %d (p1est=%d p1cls=%d p2est=%d p2cls=%d)\n",
	       rc, pass1_established?1:0, pass1_closed_ok?1:0,
	       pass2_established?1:0, pass2_closed_ok?1:0);
	return rc;
}
