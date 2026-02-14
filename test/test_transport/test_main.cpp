#include <unity.h>

#include "../common/filesystem/FileSystem.h"

#include <Reticulum.h>
#include <Transport.h>
#include <Identity.h>
#include <Destination.h>
#include <Packet.h>
#include <Log.h>
#include <Bytes.h>
#include <Utilities/OS.h>

// ============================================================================
// Test infrastructure - loopback interfaces
// ============================================================================

static int packets_received = 0;

class InInterface : public RNS::InterfaceImpl {
public:
	InInterface(const char *name = "InInterface") : RNS::InterfaceImpl(name) {
		_OUT = false;
		_IN = true;
	}
	virtual ~InInterface() { _name = "(deleted)"; }
	virtual void send_outgoing(const RNS::Bytes &data) {}
};

class OutInterface : public RNS::InterfaceImpl {
public:
	OutInterface(RNS::Interface& in_interface, const char *name = "OutInterface")
		: RNS::InterfaceImpl(name), _in_interface(in_interface) {
		_OUT = true;
		_IN = false;
	}
	virtual ~OutInterface() { _name = "(deleted)"; }
	virtual void send_outgoing(const RNS::Bytes &data) {
		_in_interface.handle_incoming(data);
		InterfaceImpl::handle_outgoing(data);
	}
private:
	RNS::Interface& _in_interface;
};

// Global test state
RNS::Reticulum test_reticulum({RNS::Type::NONE});
RNS::Identity test_identity({RNS::Type::NONE});
RNS::Destination test_dest({RNS::Type::NONE});
RNS::Interface in_interface(new InInterface());
RNS::Interface out_interface(new OutInterface(in_interface));

void onTestPacket(const RNS::Bytes& data, const RNS::Packet& packet) {
	packets_received++;
}

// ============================================================================
// Setup: Initialize Reticulum with loopback interfaces
// ============================================================================

bool rns_initialized = false;

void initRNS() {
	if (rns_initialized) return;

	RNS::FileSystem reticulum_filesystem = new FileSystem();
	((FileSystem*)reticulum_filesystem.get())->init();
	RNS::Utilities::OS::register_filesystem(reticulum_filesystem);

	RNS::Transport::register_interface(in_interface);
	RNS::Transport::register_interface(out_interface);

	// Transport identity
	RNS::Bytes transport_prv;
	transport_prv.assignHex("BABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABE");
	RNS::Identity transport_identity(false);
	transport_identity.load_private_key(transport_prv);
	RNS::Transport::identity(transport_identity);

	test_reticulum = RNS::Reticulum();
	test_reticulum.transport_enabled(true);
	test_reticulum.start();

	// Single identity and destination (same pattern as test_rns_loopback)
	test_identity = RNS::Identity(false);
	RNS::Bytes prv;
	prv.assignHex("E0D43398EDC974EBA9F4A83463691A08F4D306D4E56BA6B275B8690A2FBD9852E9EBE7C03BC45CAEC9EF8E78C830037210BFB9986F6CA2DEE2B5C28D7B4DE6B0");
	test_identity.load_private_key(prv);

	test_dest = RNS::Destination(test_identity, RNS::Type::Destination::IN,
		RNS::Type::Destination::SINGLE, "test", "transport");
	test_dest.set_packet_callback(onTestPacket);
	test_dest.set_proof_strategy(RNS::Type::Destination::PROVE_ALL);

	rns_initialized = true;
}

// ============================================================================
// Bytes edge case tests
// ============================================================================

void test_assignHex_even_length() {
	// Normal case: even-length hex string
	RNS::Bytes bytes;
	bytes.assignHex("48656C6C6F");  // "Hello"
	TEST_ASSERT_EQUAL_size_t(5, bytes.size());
	TEST_ASSERT_EQUAL_MEMORY("Hello", bytes.data(), 5);
}

void test_assignHex_odd_length() {
	// Odd-length hex should be truncated to even (drop trailing nibble)
	RNS::Bytes bytes;
	bytes.assignHex("ABC");  // 3 chars - only "AB" should be decoded
	TEST_ASSERT_EQUAL_size_t(1, bytes.size());
	TEST_ASSERT_EQUAL_UINT8(0xAB, bytes.data()[0]);
}

void test_assignHex_single_char() {
	// Single char hex can't form a complete byte - should produce empty
	RNS::Bytes bytes;
	bytes.assignHex("A");
	TEST_ASSERT_EQUAL_size_t(0, bytes.size());
}

void test_assignHex_empty() {
	RNS::Bytes bytes;
	bytes.assignHex("");
	TEST_ASSERT_EQUAL_size_t(0, bytes.size());
}

void test_appendHex_odd_length() {
	// appendHex with odd length should also truncate to even
	RNS::Bytes bytes;
	bytes.assignHex("4142");  // "AB"
	TEST_ASSERT_EQUAL_size_t(2, bytes.size());

	bytes.appendHex("434");  // 3 chars - only "43" ('C') should be appended
	TEST_ASSERT_EQUAL_size_t(3, bytes.size());
	TEST_ASSERT_EQUAL_UINT8(0x43, bytes.data()[2]);
}

void test_hex_roundtrip_stability() {
	// toHex always produces even length, so roundtrip should be safe
	RNS::Bytes original("Hello World");
	std::string hex = original.toHex();
	TEST_ASSERT_TRUE(hex.length() % 2 == 0);  // Must be even

	RNS::Bytes restored;
	restored.assignHex(hex.c_str());
	TEST_ASSERT_EQUAL_size_t(original.size(), restored.size());
	TEST_ASSERT_EQUAL_MEMORY(original.data(), restored.data(), original.size());
}

void test_mid_large_len() {
	// mid() with len that extends past end should clamp
	RNS::Bytes bytes("Hello World");

	RNS::Bytes mid = bytes.mid(3, 100);
	TEST_ASSERT_EQUAL_size_t(8, mid.size());
	TEST_ASSERT_EQUAL_MEMORY("lo World", mid.data(), 8);

	// SIZE_MAX len should also clamp, not integer-overflow and crash
	RNS::Bytes mid2 = bytes.mid(3, SIZE_MAX);
	TEST_ASSERT_EQUAL_size_t(8, mid2.size());
	TEST_ASSERT_EQUAL_MEMORY("lo World", mid2.data(), 8);
}

void test_mid_zero_size_bytes() {
	RNS::Bytes empty;
	RNS::Bytes mid = empty.mid(0, 5);
	TEST_ASSERT_FALSE(mid);
	TEST_ASSERT_EQUAL_size_t(0, mid.size());
}

// ============================================================================
// Transport receipt lifecycle tests
// ============================================================================

void test_receipt_creation_and_culling() {
	// Send packets and verify the send path + receipt creation + culling doesn't crash.
	// Note: loopback only delivers announces, not data packets, so we test
	// the outbound path, receipt creation, and jobs() culling cycle.
	initRNS();

	const int NUM_PACKETS = 10;
	for (int i = 0; i < NUM_PACKETS; i++) {
		RNS::Bytes payload("Test packet " + std::to_string(i));
		RNS::Packet packet(test_dest, payload);
		packet.send();
	}

	// Run Transport jobs to process receipts (check interval = 1.0s)
	RNS::Utilities::OS::sleep(1.5);
	test_reticulum.loop();

	// Run again to trigger receipt timeout and culling
	RNS::Utilities::OS::sleep(1.5);
	test_reticulum.loop();

	TEST_ASSERT_TRUE_MESSAGE(true, "Receipt creation and culling did not crash");
}

void test_receipt_rapid_fire() {
	// Simulate Maps-like behavior: send many packets in rapid succession.
	// This stresses Transport::outbound(), receipt list growth, and
	// the jobs() culling path.
	initRNS();

	const int NUM_PACKETS = 50;
	for (int i = 0; i < NUM_PACKETS; i++) {
		std::string msg = "Tile request z=10 x=" + std::to_string(i) + " y=42";
		RNS::Bytes payload(msg.c_str());
		RNS::Packet packet(test_dest, payload);
		packet.send();
	}

	// Run multiple job cycles to process and cull all receipts
	for (int cycle = 0; cycle < 5; cycle++) {
		RNS::Utilities::OS::sleep(1.5);
		test_reticulum.loop();
	}

	TEST_ASSERT_TRUE_MESSAGE(true, "Rapid-fire send did not crash");
}

void test_receipt_timeout_callback() {
	// Test that receipt timeout callbacks fire correctly
	initRNS();

	static bool timeout_called = false;
	static bool delivery_called = false;

	// Create a destination that we can't actually reach (no loopback for this one)
	RNS::Identity remote_id(true);  // Random identity
	RNS::Destination unreachable_dest(remote_id, RNS::Type::Destination::OUT,
		RNS::Type::Destination::SINGLE, "test", "unreachable");

	RNS::Bytes payload("This should timeout");
	RNS::Packet packet(unreachable_dest, payload);
	packet.send();
	RNS::PacketReceipt receipt = packet.receipt();
	receipt.set_timeout(1);  // 1 second timeout
	receipt.set_timeout_callback([](const RNS::PacketReceipt& r) {
		timeout_called = true;
	});
	receipt.set_delivery_callback([](const RNS::PacketReceipt& r) {
		delivery_called = true;
	});

	// Wait for timeout
	for (int i = 0; i < 5; i++) {
		RNS::Utilities::OS::sleep(0.5);
		test_reticulum.loop();
	}

	TEST_ASSERT_TRUE_MESSAGE(timeout_called, "Timeout callback should have fired");
	TEST_ASSERT_FALSE_MESSAGE(delivery_called, "Delivery callback should NOT have fired");
}

// ============================================================================
// Transport hashlist tests
// ============================================================================

void test_hashlist_basic() {
	// Send enough packets that the hashlist should grow
	initRNS();

	const int NUM_PACKETS = 30;
	for (int i = 0; i < NUM_PACKETS; i++) {
		RNS::Bytes payload("Hash test " + std::to_string(i));
		RNS::Packet packet(test_dest, payload);
		packet.send();
	}

	// Run jobs to process and potentially cull hashlist
	test_reticulum.loop();

	TEST_ASSERT_TRUE_MESSAGE(true, "Hashlist operations did not crash");
}

void test_hashlist_overflow() {
	// Send more packets than _hashlist_maxsize (100) to trigger culling
	initRNS();

	const int NUM_PACKETS = 150;  // > maxsize of 100
	for (int i = 0; i < NUM_PACKETS; i++) {
		RNS::Bytes payload("Overflow " + std::to_string(i) + " padding to make unique");
		RNS::Packet packet(test_dest, payload);
		packet.send();

		// Periodically run jobs to trigger culling mid-stream
		if (i % 25 == 0) {
			test_reticulum.loop();
		}
	}

	// Final jobs run
	test_reticulum.loop();
	RNS::Utilities::OS::sleep(1.5);
	test_reticulum.loop();

	TEST_ASSERT_TRUE_MESSAGE(true, "Hashlist overflow and culling did not crash");
}

// ============================================================================
// Packet send/receive cycle stress test
// ============================================================================

void test_send_receive_cycle() {
	// Single packet send through outbound path, then jobs cycle
	initRNS();

	RNS::Bytes payload("Cycle test payload");
	RNS::Packet packet(test_dest, payload);
	packet.send();

	test_reticulum.loop();
	RNS::Utilities::OS::sleep(1.5);
	test_reticulum.loop();

	TEST_ASSERT_TRUE_MESSAGE(true, "Send/jobs cycle did not crash");
}

void test_send_receive_stress() {
	// Stress test: 100 packets with periodic jobs() processing.
	// Tests that outbound + hashlist + receipt list handles volume.
	initRNS();

	const int NUM_CYCLES = 100;
	for (int i = 0; i < NUM_CYCLES; i++) {
		std::string msg = "Stress " + std::to_string(i);
		RNS::Bytes payload(msg.c_str());
		RNS::Packet packet(test_dest, payload);
		packet.send();

		// Run jobs every 10 packets
		if (i % 10 == 0) {
			test_reticulum.loop();
		}
	}

	// Final processing - let receipts timeout and get culled
	for (int i = 0; i < 3; i++) {
		RNS::Utilities::OS::sleep(1.5);
		test_reticulum.loop();
	}

	printf("[TEST] Sent %d packets without crash\n", NUM_CYCLES);
	TEST_ASSERT_TRUE_MESSAGE(true, "Stress test did not crash");
}

void test_interleaved_send_and_jobs() {
	// Interleave packet sends with Transport::jobs() to stress the
	// _jobs_locked/_jobs_running coordination and receipt processing
	initRNS();

	for (int round = 0; round < 20; round++) {
		// Send a burst of 5 packets
		for (int i = 0; i < 5; i++) {
			std::string msg = "Burst " + std::to_string(round) + "." + std::to_string(i);
			RNS::Bytes payload(msg.c_str());
			RNS::Packet packet(test_dest, payload);
			packet.send();
		}

		// Immediately run jobs (tests state after outbound batch)
		test_reticulum.loop();
	}

	// Let receipts settle and get culled
	for (int i = 0; i < 3; i++) {
		RNS::Utilities::OS::sleep(1.5);
		test_reticulum.loop();
	}

	printf("[TEST] Burst test: 20 rounds x 5 packets completed\n");
	TEST_ASSERT_TRUE_MESSAGE(true, "Interleaved send/jobs did not crash");
}

// ============================================================================
// Identity/Destination lifecycle tests
// ============================================================================

void test_identity_create_destroy_cycle() {
	// Rapidly create and destroy identities to stress crypto allocation
	for (int i = 0; i < 20; i++) {
		RNS::Identity id(true);  // Generate new keypair
		TEST_ASSERT_TRUE(id);
		RNS::Bytes hash = id.hash();
		TEST_ASSERT_TRUE(hash.size() > 0);
		std::string hex = hash.toHex();
		TEST_ASSERT_TRUE(hex.length() > 0);
	}
	TEST_ASSERT_TRUE_MESSAGE(true, "Identity create/destroy cycle did not crash");
}

void test_destination_packet_cycle() {
	// Create destinations, send packets, destroy - repeat
	initRNS();

	for (int cycle = 0; cycle < 10; cycle++) {
		RNS::Identity temp_id(true);
		RNS::Destination temp_dest(temp_id, RNS::Type::Destination::IN,
			RNS::Type::Destination::SINGLE, "test", "temp");
		temp_dest.set_proof_strategy(RNS::Type::Destination::PROVE_ALL);

		// Announce
		std::string announce_data = "test_cycle_" + std::to_string(cycle);
		temp_dest.announce(RNS::bytesFromString(announce_data.c_str()));

		test_reticulum.loop();
		// temp_dest goes out of scope here
	}

	TEST_ASSERT_TRUE_MESSAGE(true, "Destination lifecycle did not crash");
}

// ============================================================================
// Test runner
// ============================================================================

void setUp(void) {}
void tearDown(void) {}

int runUnityTests(void) {
	UNITY_BEGIN();

	// Bytes edge cases
	RUN_TEST(test_assignHex_even_length);
	RUN_TEST(test_assignHex_odd_length);
	RUN_TEST(test_assignHex_single_char);
	RUN_TEST(test_assignHex_empty);
	RUN_TEST(test_appendHex_odd_length);
	RUN_TEST(test_hex_roundtrip_stability);
	RUN_TEST(test_mid_large_len);
	RUN_TEST(test_mid_zero_size_bytes);

	// Transport receipt lifecycle
	RUN_TEST(test_receipt_creation_and_culling);
	RUN_TEST(test_receipt_rapid_fire);
	RUN_TEST(test_receipt_timeout_callback);

	// Transport hashlist
	RUN_TEST(test_hashlist_basic);
	RUN_TEST(test_hashlist_overflow);

	// Packet send/receive
	RUN_TEST(test_send_receive_cycle);
	RUN_TEST(test_send_receive_stress);
	RUN_TEST(test_interleaved_send_and_jobs);

	// Identity/Destination lifecycle
	RUN_TEST(test_identity_create_destroy_cycle);
	RUN_TEST(test_destination_packet_cycle);

	return UNITY_END();
}

int main(void) {
	return runUnityTests();
}

#ifdef ARDUINO
void setup() {
	delay(2000);
	runUnityTests();
}
void loop() {}
#endif

void app_main() {
	runUnityTests();
}
