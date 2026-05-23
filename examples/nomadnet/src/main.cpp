/*
 * Copyright (c) 2026 Chad Attermann
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */

/*
##########################################################
# This RNS example demonstrates a minimal NomadNet-      #
# compatible page server and client. The server          #
# announces a destination with aspect "nomadnetwork.node"#
# and registers a request handler that serves a single   #
# hardcoded page at "/page/index.mu". The client takes a #
# destination hash (and optional page path) on the       #
# command line, establishes a Link, issues one page      #
# request, prints the returned bytes, and exits.         #
#                                                        #
# The aspect tuple ("nomadnetwork", "node") and the      #
# "/page/..." path convention mirror the reference       #
# Python NomadNet node (nomadnet/Node.py), so this       #
# example is wire-compatible with Python NomadNet        #
# page browsing. No LXMF is required — LXMF is only used #
# by NomadNet for the separate messaging feature.        #
##########################################################
*/

#include <microStore/FileSystem.h>
#include <microStore/Adapters/UniversalFileSystem.h>
#include <UDPInterface.h>
#include <MsgPack.h>

#include <Reticulum.h>
#include <Interface.h>
#include <Link.h>
#include <Identity.h>
#include <Destination.h>
#include <Packet.h>
#include <Transport.h>
#include <Log.h>
#include <Bytes.h>
#include <Type.h>
#include <Utilities/OS.h>

#ifdef ARDUINO
#include <Arduino.h>
#else
#include <termios.h>
#include <fcntl.h>
#include <stdio.h>
#include <signal.h>
#endif

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <string>

// NomadNet-compatible aspect: full aspect string is "nomadnetwork.node".
#define APP_NAME          "nomadnetwork"
#define APP_ASPECT        "node"
#define DEFAULT_PAGE_PATH "/page/index.mu"

// Human-readable node name. Sent as raw UTF-8 in the announce app_data,
// matching nomadnet/Node.py:217-222. This is what other NomadNet
// clients (e.g. the Python `nomadnet` text browser) show in their
// directory / site listing for this node. Change this string to rename
// the node; rebuild required.
#define NODE_NAME         "microReticulum NomadNet Example Node"

RNS::Reticulum reticulum;
RNS::Interface udp_interface({RNS::Type::NONE});


/*
##########################################################
#### Server Part #########################################
##########################################################
*/

// Hardcoded page content served at /page/index.mu. Real NomadNet pages
// use the .mu micron markup format, but we don't parse it here — we just
// emit text that a NomadNet browser (or this example's client) can
// display verbatim.
static const char* INDEX_PAGE =
	">microReticulum NomadNet example\n"
	"\n"
	"Hello from the microReticulum C++ NomadNet example node.\n"
	"\n"
	"This page is served at `/page/index.mu` by a minimal server\n"
	"that registers a single request handler on a Destination with\n"
	"the NomadNet aspect (\"nomadnetwork.node\").\n";

// Request handler for /page/index.mu. Signature is fixed by
// RNS::RequestHandler::response_generator (see src/Destination.h).
//
// The Link::request contract is that the return value must already be
// msgpack-encoded bytes — Link::handle_request splices it verbatim into
// the response envelope (Link.cpp:994). Python's RNS auto-encodes
// arbitrary return values; in C++ we encode manually here.
RNS::Bytes serve_index(
	const RNS::Bytes& path,
	const RNS::Bytes& data,
	const RNS::Bytes& request_id,
	const RNS::Bytes& link_id,
	const RNS::Identity& remote_identity,
	double requested_at
) {
	RNS::logf(RNS::LOG_NOTICE, "Serving %s to link <%s>",
		path.toString().c_str(), link_id.toHex().c_str());
	const size_t page_len = strlen(INDEX_PAGE);
	MsgPack::Packer packer;
	packer.packBinary(reinterpret_cast<const uint8_t*>(INDEX_PAGE), page_len);
	return RNS::Bytes(packer.data(), packer.size());
}

void server_link_established(RNS::Link& link) {
	RNS::logf(RNS::LOG_NOTICE, "Client link established <%s>", link.hash().toHex().c_str());
}

void server_loop(RNS::Destination& destination) {
	RNS::logf(RNS::LOG_NOTICE,
		"NomadNet example node <%s> running, serving %s",
		destination.hash().toHex().c_str(), DEFAULT_PAGE_PATH);
	RNS::log("Hit enter to re-announce (Ctrl-C to quit)");

	const RNS::Bytes node_name_app_data(NODE_NAME);

	while (true) {
		reticulum.loop();
		// Non-blocking input
		char ch;
		while (read(STDIN_FILENO, &ch, 1) > 0) {
			if (ch == '\n') {
				destination.announce(node_name_app_data);
				RNS::logf(RNS::LOG_NOTICE, "Sent announce from %s as \"%s\"",
					destination.hash().toHex().c_str(), NODE_NAME);
			}
		}
	}
}

void server() {

	// Randomly generate a fresh identity for this example run.
	RNS::Identity server_identity = RNS::Identity();

	// Create an IN/SINGLE destination on the NomadNet aspect, so
	// clients (this example, or a Python NomadNet browser) can find
	// us by aspect/announce and open a Link.
	RNS::Destination server_destination = RNS::Destination(
		server_identity,
		RNS::Type::Destination::IN,
		RNS::Type::Destination::SINGLE,
		APP_NAME,
		APP_ASPECT
	);

	// Logging-only — not strictly required for request/response to work.
	server_destination.set_link_established_callback(server_link_established);

	// Register the page handler. ALLOW_ALL because page browsing is open
	// to anyone who can reach the node, just like a Python NomadNet
	// node's default policy.
	server_destination.register_request_handler(
		RNS::Bytes(DEFAULT_PAGE_PATH),
		serve_index,
		RNS::Type::Destination::ALLOW_ALL
	);

	// Announce once at startup so a client that's already listening can
	// discover us immediately. The node name is sent as the announce
	// app_data (plain UTF-8 bytes), matching nomadnet/Node.py:217-222 —
	// this is what other NomadNet clients show in their site listing.
	const RNS::Bytes node_name_app_data(NODE_NAME);
	server_destination.announce(node_name_app_data);
	RNS::logf(RNS::LOG_NOTICE, "Sent startup announce from %s as \"%s\"",
		server_destination.hash().toHex().c_str(), NODE_NAME);

	server_loop(server_destination);
}


/*
##########################################################
#### Client Part #########################################
##########################################################
*/

// Path the client should request once the Link is up. Set in client()
// before the Link is created; read inside the on-established callback,
// which can't capture local state (it's a C-style function pointer).
static std::string requested_path;

// Set by the response/failure callbacks so the client main loop can
// observe completion and tear the Link down cleanly.
static volatile bool request_done = false;
static volatile bool request_ok   = false;

// Decode a msgpack `bin` value into `out`. Returns true on success.
//
// The nomadnet response delivered to on_response by microReticulum is
// the msgpack-encoded `response` value from the server-side
// `umsgpack.packb([request_id, response])` envelope. nomadnet's
// serve_page returns Python `bytes`, which always serializes as msgpack
// `bin` — so unwrapping with bin_t<uint8_t> is sufficient for the
// NomadNet use case. (Mirrors the same Unpacker pattern that
// Link.cpp:1251-1255 uses to decode single-packet RESPONSE envelopes.)
static bool msgpack_unwrap_bin(const RNS::Bytes& packed, RNS::Bytes& out) {
	if (packed.size() < 1) return false;
	MsgPack::Unpacker u;
	u.feed(packed.data(), packed.size());
	if (!u.isBin()) return false;
	MsgPack::bin_t<uint8_t> bin;
	u.deserialize(bin);
	out = RNS::Bytes(bin.data(), bin.size());
	return true;
}

// Write the raw response bytes (which we couldn't parse cleanly) to a
// temp file. Returns the file path on success, empty string on failure.
// Used for the "compressed response" path — see on_request_failed().
static std::string write_response_temp_file(const RNS::Bytes& request_id,
                                            const RNS::Bytes& payload) {
	std::string filename = "/tmp/nomadnet_";
	filename += request_id.toHex();
	filename += ".bin";
	FILE* f = fopen(filename.c_str(), "wb");
	if (!f) return std::string();
	if (payload.size() > 0) {
		fwrite(payload.data(), 1, payload.size(), f);
	}
	fclose(f);
	return filename;
}

void on_response(const RNS::RequestReceipt& receipt) {
	const RNS::Bytes request_id = receipt.get_request_id();
	RNS::Bytes page_bytes = const_cast<RNS::RequestReceipt&>(receipt).get_response();

	RNS::logf(RNS::LOG_NOTICE,
		"Response received for request <%s>: %lu page_bytes (msgpack-encoded)",
		request_id.toHex().c_str(),
		(unsigned long)page_bytes.size());

	// page_bytes is the still-msgpack-encoded `response` element of the
	// server's umsgpack.packb([request_id, response]) envelope. For
	// NomadNet pages, `response` is the file content serialized as
	// msgpack `bin`. Unwrap the bin marker + length prefix to recover
	// the actual page content.
	RNS::Bytes page_content;
	if (!msgpack_unwrap_bin(page_bytes, page_content)) {
		RNS::logf(RNS::LOG_ERROR,
			"Response payload for <%s> is not a msgpack bin value — "
			"first byte 0x%02x. Saving raw bytes for inspection.",
			request_id.toHex().c_str(),
			page_bytes.size() > 0 ? page_bytes.data()[0] : 0);
		const std::string saved = write_response_temp_file(request_id, page_bytes);
		if (!saved.empty()) {
			RNS::logf(RNS::LOG_NOTICE, "Raw response bytes written to %s", saved.c_str());
		}
		request_ok = true;
		request_done = true;
		return;
	}

	RNS::logf(RNS::LOG_NOTICE,
		"Unwrapped page content: %lu bytes",
		(unsigned long)page_content.size());

	printf("\n----- BEGIN %s -----\n", requested_path.c_str());
	fwrite(page_content.data(), 1, page_content.size(), stdout);
	if (page_content.size() == 0 || page_content.data()[page_content.size() - 1] != '\n') {
		printf("\n");
	}
	printf("----- END %s -----\n\n", requested_path.c_str());
	fflush(stdout);
	request_ok = true;
	request_done = true;
}

void on_request_failed(const RNS::RequestReceipt& receipt) {
	// Several distinct failure modes funnel through this callback with
	// no structured reason on the receipt:
	//   - The server has no handler for `requested_path` (request
	//     reached the server but its `register_request_handler` map
	//     didn't match).
	//   - The link timed out before the response arrived (timeout
	//     value passed to link.request()).
	//   - The response was sent as a *compressed* resource — standard
	//     nomadnet servers bz2-compress responses by default if the
	//     compressed size is smaller. The C++ Reticulum port doesn't
	//     decompress bz2, so Resource::assemble() in the library rejects
	//     the resource as CORRUPT and the application sees only this
	//     failure callback. No way to distinguish the bz2 case from a
	//     real timeout without a library-side flag we don't have.
	// If you control the nomadnet server, register page handlers with
	// `auto_compress=False` (nomadnet/Node.py around the
	// `register_request_handler("/page/index.mu", ...)` calls) to bypass
	// the issue. Otherwise the page is not retrievable from this client
	// until microReticulum gains bz2 decompression support.
	RNS::logf(RNS::LOG_ERROR,
		"Request for %s failed (timeout, missing handler, or bz2-compressed "
		"response — bz2 not supported on the C++ port; patch the server with "
		"auto_compress=False if you control it)",
		requested_path.c_str());
	request_done = true;
}

void on_link_established(RNS::Link& link) {
	RNS::logf(RNS::LOG_NOTICE, "Link established, requesting %s", requested_path.c_str());
	// NomadNet page fetches don't carry a payload, but the
	// request envelope on the wire is a msgpack 3-array
	// [timestamp, path_hash, data] (see Link.cpp pack_request_envelope
	// and RNS/Link.py:489-491). Python's umsgpack.unpackb() on the
	// receiving side expects all three elements to be present and valid
	// msgpack — so for "no payload" we must send msgpack nil (0xC0),
	// not zero bytes. Passing {Bytes::NONE} appends nothing after the
	// array header that promised 3 elements, and Python's unpackb
	// silently throws "insufficient data" with an empty str(e).
	static const uint8_t MSGPACK_NIL = 0xC0;
	const RNS::Bytes nil_payload(&MSGPACK_NIL, 1);
	link.request(
		RNS::Bytes(requested_path),
		nil_payload,
		on_response,
		on_request_failed,
		nullptr,
		10.0
	);
}

void on_link_closed(RNS::Link& link) {
	if (link.teardown_reason() == RNS::Type::Link::TIMEOUT) {
		RNS::log("Link timed out");
	}
	else if (link.teardown_reason() == RNS::Type::Link::DESTINATION_CLOSED) {
		RNS::log("Link closed by server");
	}
	else {
		RNS::log("Link closed");
	}
	// If the link closed before any request callback fired (e.g. the
	// LINK_REQUEST timed out before the server's LINK_PROOF arrived),
	// release the main loop so the client exits instead of hanging.
	request_done = true;
}

void client(const char* destination_hexhash, const char* path) {
	requested_path = path;

	// Decode the hex destination hash from argv.
	RNS::Bytes destination_hash;
	try {
		int dest_len = (RNS::Type::Reticulum::TRUNCATED_HASHLENGTH/8)*2;
		if (strlen(destination_hexhash) != (size_t)dest_len) {
			throw std::invalid_argument("Destination length is invalid, must be "+std::to_string(dest_len)+" hexadecimal characters ("+std::to_string(dest_len/2)+" bytes).");
		}
		destination_hash.assignHex(destination_hexhash);
	}
	catch (const std::exception& e) {
		RNS::log("Invalid destination entered. Check your input!", RNS::LOG_ERROR);
		return;
	}

	// Path discovery: ask the network for a path to the destination if
	// we don't already know one, then spin until an announce arrives.
	if (!RNS::Transport::has_path(destination_hash)) {
		RNS::log("Destination is not yet known. Requesting path and waiting for announce to arrive...");
		RNS::Transport::request_path(destination_hash);
		while (!RNS::Transport::has_path(destination_hash)) {
			reticulum.loop();
			RNS::Utilities::OS::sleep(0.1);
		}
	}

	// Recall the server's identity from the path table.
	RNS::Identity server_identity = RNS::Identity::recall(destination_hash);

	RNS::log("Establishing link with NomadNet node...");

	RNS::Destination server_destination = RNS::Destination(
		server_identity,
		RNS::Type::Destination::OUT,
		RNS::Type::Destination::SINGLE,
		APP_NAME,
		APP_ASPECT
	);

	RNS::Link link = RNS::Link(server_destination);
	link.set_link_established_callback(on_link_established);
	link.set_link_closed_callback(on_link_closed);

	// Pump the stack until the response (or failure) callback fires.
	while (!request_done) {
		reticulum.loop();
		RNS::Utilities::OS::sleep(0.05);
	}

	// Tear the Link down so the server sees a clean close, then drain
	// a few hundred ms of events so the teardown packet actually leaves
	// the wire before main() returns.
	if (link && link.status() != RNS::Type::Link::CLOSED) {
		link.teardown();
	}
	const double cleanup_until = RNS::Utilities::OS::time() + 0.5;
	while (RNS::Utilities::OS::time() < cleanup_until) {
		reticulum.loop();
		RNS::Utilities::OS::sleep(0.05);
	}

	if (!request_ok) {
		RNS::log("Page request did not complete successfully", RNS::LOG_ERROR);
	}
}


/*
##########################################################
#### Program Startup #####################################
##########################################################
*/

void cleanup_handler(int signum) {
	if (signum == SIGINT) {
		printf("\nCtrl+C detected. Performing cleanup...\n");
		printf("Cleanup complete. Exiting.\n");
		_exit(0);
	}
}

static void usage(const char* argv0) {
	printf("\nUsage:\n");
	printf("  %s -s | --server                Run as a NomadNet page server\n", argv0);
	printf("  %s <destination_hash> [<path>]  Run as a client; fetch <path> (default %s)\n",
		argv0, DEFAULT_PAGE_PATH);
	printf("\nOptional flags:\n");
	printf("  --log_trace                     Enable trace-level logging\n\n");
}

int main(int argc, char *argv[]) {

	bool log_trace = false;
	for (int i = 1; i < argc; ++i) {
		if (argv[i] && strcasecmp(argv[i], "--log_trace") == 0) {
			log_trace = true;
			for (int j = i; j < argc - 1; ++j) {
				argv[j] = argv[j + 1];
			}
			--argc;
			--i;
		}
	}

#if defined(RNS_MEM_LOG)
	RNS::loglevel(RNS::LOG_MEM);
#else
	if (log_trace) {
		RNS::loglevel(RNS::LOG_TRACE);
	}
	else {
		RNS::loglevel(RNS::LOG_NOTICE);
	}
#endif

	signal(SIGINT, cleanup_handler);

	// Non-blocking stdin so the server loop can poll for Enter without
	// stalling the Reticulum event pump.
	int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
	fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

	try {
		microStore::FileSystem filesystem{microStore::Adapters::UniversalFileSystem()};
		filesystem.init();
		RNS::Utilities::OS::register_filesystem(filesystem);

		udp_interface = new UDPInterface();
		udp_interface.mode(RNS::Type::Interface::MODE_GATEWAY);
		RNS::Transport::register_interface(udp_interface);
		udp_interface.start();

		reticulum.transport_enabled(false);
		reticulum.probe_destination_enabled(true);
		reticulum.start();
	}
	catch (const std::exception& e) {
		ERRORF("!!! Exception in Reticulum startup: %s", e.what());
	}

	if (argc <= 1) {
		usage(argv[0]);
		return -1;
	}
	if (strcmp(argv[1], "--server") == 0 || strcmp(argv[1], "-s") == 0) {
		server();
	}
	else if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
		usage(argv[0]);
		return 0;
	}
	else {
		const char* path = (argc >= 3) ? argv[2] : DEFAULT_PAGE_PATH;
		client(argv[1], path);
	}

	printf("\nLoop exited. Performing cleanup...\n");
	printf("Cleanup complete. Exiting.\n");

	return 0;
}
