// Interop-test sender for link request/response RPC.
//
// Flow:
//   1. Wait for Python's announce, build Link.
//   2. On link established, send a 200-byte msgpack-encoded payload via
//      Link::request("/echo", encoded_data, on_response, on_failed).
//   3. Python's registered handler echoes the bytes back unchanged.
//   4. on_response fires with the raw response bytes (from
//      RequestReceipt::get_response()). Verify byte-equal to original.
//   5. Tear down the link (cleanup), exit 0 on success.

#include <microStore/FileSystem.h>
#include <microStore/Adapters/UniversalFileSystem.h>
#include <UDPInterface.h>
#include <MsgPack.h>

#include <Reticulum.h>
#include <Identity.h>
#include <Destination.h>
#include <Link.h>
#include <Transport.h>
#include <Interface.h>
#include <Log.h>
#include <Bytes.h>
#include <Type.h>
#include <Utilities/OS.h>

#include <stdio.h>

static const char* APP_NAME = "microreticulum_interop";
static const char* ASPECT   = "request";

// 200-byte deterministic payload — small enough to fit in a single link
// packet so we exercise the RPC path, not the resource-as-request path.
static RNS::Bytes build_payload() {
	RNS::Bytes p;
	for (size_t i = 0; i < 200; ++i) p.append((uint8_t)(i & 0xff));
	return p;
}

static RNS::Reticulum reticulum({RNS::Type::NONE});
static RNS::Interface udp_interface(RNS::Type::NONE);
static RNS::Identity local_identity({RNS::Type::NONE});
static RNS::Destination outgoing_destination({RNS::Type::NONE});
static RNS::Link active_link({RNS::Type::NONE});
static volatile bool announce_seen     = false;
static volatile bool response_received = false;
static volatile bool response_ok       = false;
static volatile bool request_failed    = false;

static void on_response(const RNS::RequestReceipt& receipt) {
	RNS::Bytes response = const_cast<RNS::RequestReceipt&>(receipt).get_response();
	RNS::Bytes expected = build_payload();
	const bool ok = (response.size() == expected.size()
	              && memcmp(response.data(), expected.data(), expected.size()) == 0);
	printf("[cpp] response received: %lu bytes, match=%s\n",
	       (unsigned long)response.size(), ok ? "yes" : "no");
	response_received = true;
	response_ok       = ok;
}

static void on_failed(const RNS::RequestReceipt& receipt) {
	printf("[cpp] request failed\n");
	request_failed = true;
}

static void on_link_closed(RNS::Link& link) {
	printf("[cpp] link closed\n");
}

static void on_link_established(RNS::Link& link) {
	printf("[cpp] link established: %s\n", link.hash().toHex().c_str());

	// Msgpack-encode the payload as the third array element of the
	// request envelope. The Link::request() contract is that `data` is
	// already-msgpack-encoded bytes spliced verbatim into the envelope
	// (see Link.cpp:65-72 pack_request_envelope and the comment at :570).
	RNS::Bytes payload = build_payload();
	MsgPack::Packer p;
	p.packBinary(payload.data(), payload.size());
	RNS::Bytes encoded(p.data(), p.size());

	printf("[cpp] sending request to /echo: %lu bytes\n",
	       (unsigned long)payload.size());
	link.request(RNS::Bytes("/echo"), encoded,
	             on_response, on_failed, /*progress=*/nullptr,
	             /*timeout=*/10.0);
}

class AnnounceHandler : public RNS::AnnounceHandler {
public:
	AnnounceHandler() : RNS::AnnounceHandler(
	    (std::string(APP_NAME) + "." + ASPECT).c_str()) {}
	void received_announce(const RNS::Bytes& destination_hash,
	                       const RNS::Identity& announced_identity,
	                       const RNS::Bytes& app_data) override {
		if (announce_seen) return;
		announce_seen = true;
		printf("[cpp] received announce: dest=%s\n",
		       destination_hash.toHex().c_str());
		outgoing_destination = RNS::Destination(announced_identity,
		                                        RNS::Type::Destination::OUT,
		                                        RNS::Type::Destination::SINGLE,
		                                        APP_NAME, ASPECT);
		active_link = RNS::Link(outgoing_destination,
		                        on_link_established, on_link_closed);
	}
};
static RNS::HAnnounceHandler announce_handler(new AnnounceHandler());

int main() {
	printf("[cpp] request_interop_sender starting\n");

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

	const double TIMEOUT_S = 30.0;
	const double start = RNS::Utilities::OS::time();

	while (true) {
		reticulum.loop();
		if (response_received || request_failed) {
			// Tear down the link to let Python see a clean close.
			if (active_link && active_link.status() != RNS::Type::Link::CLOSED) {
				active_link.teardown();
			}
			break;
		}
		if (RNS::Utilities::OS::time() - start > TIMEOUT_S) {
			printf("[cpp] TIMEOUT (announce=%d resp=%d failed=%d)\n",
			       announce_seen?1:0, response_received?1:0, request_failed?1:0);
			break;
		}
		RNS::Utilities::OS::sleep(0.01);
	}

	// Give the teardown packet a moment to leave the wire.
	const double cleanup_until = RNS::Utilities::OS::time() + 0.5;
	while (RNS::Utilities::OS::time() < cleanup_until) {
		reticulum.loop();
		RNS::Utilities::OS::sleep(0.01);
	}

	RNS::Transport::deregister_interface(udp_interface);
	const int rc = response_ok ? 0 : 1;
	printf("[cpp] exit code %d\n", rc);
	return rc;
}
