// Interop-test sender — drives a Resource transfer to a Python RNS peer over
// the localhost UDPInterface. Exits 0 on resource concluded COMPLETE,
// 1 on timeout or other failure.
//
// Run alongside test_interop/python_resource_receiver.py via run_resource_transfer.sh.

#include <microStore/FileSystem.h>
#include <microStore/Adapters/UniversalFileSystem.h>
#include <UDPInterface.h>

#include <microReticulum.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* APP_NAME = "microreticulum_interop";
static const char* ASPECT   = "resource";

// Deterministic test payload — 1024 bytes, repeating bytes 0x00..0xff.
// The Python receiver verifies the resource hash; the driver script may
// optionally pass an --expect-sha256 derived from this same payload.
static RNS::Bytes build_payload() {
	RNS::Bytes p;
	for (size_t i = 0; i < 1024; ++i) p.append((uint8_t)(i & 0xff));
	return p;
}

// Singletons (the C++ port uses a global static Transport, so a single
// Reticulum instance is the design).
static RNS::Reticulum reticulum({RNS::Type::NONE});
static RNS::Interface udp_interface(RNS::Type::NONE);
static RNS::Identity local_identity({RNS::Type::NONE});
static RNS::Destination outgoing_destination({RNS::Type::NONE});
static RNS::Link outgoing_link({RNS::Type::NONE});
static volatile bool transfer_done = false;
static volatile int  exit_code = 1;
static volatile bool announce_seen = false;
static double start_time = 0.0;

// Resource conclusion callback — the C++ Callbacks API uses raw function
// pointers (no std::function on NRF52).
static void on_resource_concluded(const RNS::Resource& resource) {
	if (resource.status() == RNS::Type::Resource::COMPLETE) {
		printf("[cpp] resource concluded COMPLETE: hash %s\n",
		       resource.hash().toHex().c_str());
		exit_code = 0;
	}
	else {
		printf("[cpp] resource concluded FAILED, status=%d\n",
		       (int)resource.status());
		exit_code = 1;
	}
	transfer_done = true;
}

// Link-established callback — kicks off the actual Resource send.
static void on_link_established(RNS::Link& link) {
	printf("[cpp] link established: %s\n", link.hash().toHex().c_str());

	RNS::Bytes payload = build_payload();
	printf("[cpp] sending resource: %lu bytes\n", (unsigned long)payload.size());

	RNS::Resource resource = RNS::Resource(payload, link)
		.auto_compress(false)
		.set_concluded_callback(on_resource_concluded)
		.start();
	// The Resource is captured by Link::_outgoing_resources; the local
	// stack variable can go out of scope safely (pimpl/shared_ptr).
}

// Link-closed callback — distinguish proof-completion from premature close.
static void on_link_closed(RNS::Link& link) {
	printf("[cpp] link closed\n");
}

// AnnounceHandler subclass: when an announce arrives matching our peer's
// app/aspect, build the outgoing Destination and Link.
class InteropAnnounceHandler : public RNS::AnnounceHandler {
public:
	InteropAnnounceHandler()
	  : RNS::AnnounceHandler(/*aspect_filter=*/(std::string(APP_NAME) + "." + ASPECT).c_str()) {}

	void received_announce(const RNS::Bytes& destination_hash,
	                       const RNS::Identity& announced_identity,
	                       const RNS::Bytes& app_data) override {
		if (announce_seen) return;
		announce_seen = true;

		printf("[cpp] received announce: dest=%s identity=%s\n",
		       destination_hash.toHex().c_str(),
		       announced_identity.hash().toHex().c_str());

		// Build outgoing Destination using the announced identity.
		outgoing_destination = RNS::Destination(announced_identity,
		                                        RNS::Type::Destination::OUT,
		                                        RNS::Type::Destination::SINGLE,
		                                        APP_NAME, ASPECT);

		// Open the Link. handshake() runs through the LR/LP/PROOF exchange;
		// the established callback fires once the link is ACTIVE.
		outgoing_link = RNS::Link(outgoing_destination,
		                          on_link_established, on_link_closed);
	}
};

static RNS::HAnnounceHandler announce_handler(new InteropAnnounceHandler());

int main() {
	printf("[cpp] resource_interop_sender starting\n");

	// Filesystem registration (UDPInterface and Reticulum need one).
	microStore::FileSystem filesystem{microStore::Adapters::UniversalFileSystem()};
	filesystem.init();
	RNS::Utilities::OS::register_filesystem(filesystem);

	// UDPInterface on the default port 4242 with broadcast — both processes
	// share this convention so they see each other's packets via localhost.
	udp_interface = new UDPInterface();
	udp_interface.mode(RNS::Type::Interface::MODE_GATEWAY);
	RNS::Transport::register_interface(udp_interface);
	udp_interface.start();

	reticulum = RNS::Reticulum();
	// Leaf-mode: do not rebroadcast announces. The interop test only needs
	// to learn the Python peer's destination, not act as a transport node.
	reticulum.transport_enabled(false);
	reticulum.start();

	// Local identity (random — we don't need to be reachable by Python).
	local_identity = RNS::Identity();

	RNS::Transport::register_announce_handler(announce_handler);

	printf("[cpp] waiting for announce from %s.%s...\n", APP_NAME, ASPECT);

	start_time = RNS::Utilities::OS::time();
	const double TIMEOUT_SECONDS = 60.0;

	while (!transfer_done) {
		reticulum.loop();
		const double elapsed = RNS::Utilities::OS::time() - start_time;
		if (elapsed > TIMEOUT_SECONDS) {
			printf("[cpp] TIMEOUT after %.1fs (announce_seen=%d)\n", elapsed, announce_seen ? 1 : 0);
			break;
		}
		RNS::Utilities::OS::sleep(0.01);
	}

	RNS::Transport::deregister_interface(udp_interface);
	printf("[cpp] exit code %d\n", exit_code);
	return exit_code;
}
