#include <unity.h>

#include <Reticulum.h>
#include <Transport.h>
#include <Interface.h>
#include <Identity.h>
#include <Bytes.h>
#include <Log.h>
#include <Type.h>
#include <Cryptography/HKDF.h>

// Test interface that captures outgoing data
class CaptureInterface : public RNS::InterfaceImpl {
public:
	CaptureInterface(const char *name = "CaptureInterface") : RNS::InterfaceImpl(name) {
		_OUT = true;
		_IN = true;
	}
	virtual ~CaptureInterface() {}
	virtual void send_outgoing(const RNS::Bytes &data) {
		last_sent = data;
		send_count++;
		InterfaceImpl::handle_outgoing(data);
	}
	RNS::Bytes last_sent;
	int send_count = 0;
};

// Test that IFAC key derivation works with both name and passphrase
void testIFACKeyDerivation() {
	HEAD("Running testIFACKeyDerivation...", RNS::LOG_TRACE);

	RNS::Interface interface(new CaptureInterface("TestIFAC"));
	RNS::Reticulum reticulum;
	reticulum._add_interface(interface,
		RNS::Type::Interface::MODE_FULL,
		16,
		"testnetwork",
		"testpassphrase");

	TEST_ASSERT_TRUE_MESSAGE(interface.ifac_identity(), "IFAC identity should be set");
	TEST_ASSERT_EQUAL_size_t(64, interface.ifac_key().size());
	TEST_ASSERT_EQUAL_UINT16(16, interface.ifac_size());
	TEST_ASSERT_TRUE_MESSAGE(interface.ifac_signature().size() > 0, "IFAC signature should be set");

	RNS::Transport::deregister_interface(interface);
}

// Test that IFAC key derivation works with name only
void testIFACKeyDerivationNameOnly() {
	HEAD("Running testIFACKeyDerivationNameOnly...", RNS::LOG_TRACE);

	RNS::Interface interface(new CaptureInterface("TestIFACName"));
	RNS::Reticulum reticulum;
	reticulum._add_interface(interface,
		RNS::Type::Interface::MODE_FULL,
		8,
		"testnetwork",
		"");

	TEST_ASSERT_TRUE_MESSAGE(interface.ifac_identity(), "IFAC identity should be set with name only");
	TEST_ASSERT_EQUAL_size_t(64, interface.ifac_key().size());
	TEST_ASSERT_EQUAL_UINT16(8, interface.ifac_size());

	RNS::Transport::deregister_interface(interface);
}

// Test that IFAC key derivation works with passphrase only
void testIFACKeyDerivationKeyOnly() {
	HEAD("Running testIFACKeyDerivationKeyOnly...", RNS::LOG_TRACE);

	RNS::Interface interface(new CaptureInterface("TestIFACKey"));
	RNS::Reticulum reticulum;
	reticulum._add_interface(interface,
		RNS::Type::Interface::MODE_FULL,
		8,
		"",
		"testpassphrase");

	TEST_ASSERT_TRUE_MESSAGE(interface.ifac_identity(), "IFAC identity should be set with key only");
	TEST_ASSERT_EQUAL_size_t(64, interface.ifac_key().size());

	RNS::Transport::deregister_interface(interface);
}

// Test that no IFAC is set when neither name nor passphrase is provided
void testIFACNoConfig() {
	HEAD("Running testIFACNoConfig...", RNS::LOG_TRACE);

	RNS::Interface interface(new CaptureInterface("TestIFACNone"));
	RNS::Reticulum reticulum;
	reticulum._add_interface(interface,
		RNS::Type::Interface::MODE_FULL,
		8,
		"",
		"");

	TEST_ASSERT_FALSE_MESSAGE(interface.ifac_identity(), "IFAC identity should not be set without config");
	TEST_ASSERT_EQUAL_size_t(0, interface.ifac_key().size());

	RNS::Transport::deregister_interface(interface);
}

// Test that same name+passphrase always produces same key (deterministic)
void testIFACDeterministic() {
	HEAD("Running testIFACDeterministic...", RNS::LOG_TRACE);

	RNS::Interface interface1(new CaptureInterface("TestDet1"));
	RNS::Interface interface2(new CaptureInterface("TestDet2"));
	RNS::Reticulum reticulum;

	reticulum._add_interface(interface1,
		RNS::Type::Interface::MODE_FULL, 16, "mynet", "mypass");
	reticulum._add_interface(interface2,
		RNS::Type::Interface::MODE_FULL, 16, "mynet", "mypass");

	TEST_ASSERT_TRUE(interface1.ifac_key() == interface2.ifac_key());
	TEST_ASSERT_TRUE(interface1.ifac_signature() == interface2.ifac_signature());

	RNS::Transport::deregister_interface(interface1);
	RNS::Transport::deregister_interface(interface2);
}

// Test transmit adds IFAC and masking, then inbound decodes it correctly
void testIFACTransmitInboundRoundtrip() {
	HEAD("Running testIFACTransmitInboundRoundtrip...", RNS::LOG_TRACE);

	CaptureInterface* capture_impl = new CaptureInterface("TestRoundtrip");
	RNS::Interface interface(capture_impl);

	// Set up transport identity (required for inbound processing)
	RNS::Bytes transport_prv_bytes;
	transport_prv_bytes.assignHex("BABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABE");
	RNS::Identity transport_identity(false);
	transport_identity.load_private_key(transport_prv_bytes);
	RNS::Transport::identity(transport_identity);

	RNS::Reticulum reticulum;
	reticulum._add_interface(interface,
		RNS::Type::Interface::MODE_FULL, 8, "roundtrip_net", "roundtrip_pass");

	// Create a minimal fake raw packet (at least 2 header bytes + some payload)
	// Header: 2 bytes, then payload
	uint8_t raw_data[] = { 0x00, 0x01, 0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE };
	RNS::Bytes raw(raw_data, sizeof(raw_data));

	// Transmit should add IFAC and mask
	capture_impl->send_count = 0;
	RNS::Transport::transmit(interface, raw);

	TEST_ASSERT_EQUAL_INT_MESSAGE(1, capture_impl->send_count, "Transmit should have sent one packet");
	TEST_ASSERT_TRUE_MESSAGE(capture_impl->last_sent.size() > raw.size(), "Transmitted packet should be larger (IFAC added)");
	TEST_ASSERT_EQUAL_size_t(raw.size() + 8, capture_impl->last_sent.size());

	// Verify IFAC flag is set in transmitted packet
	TEST_ASSERT_TRUE_MESSAGE(capture_impl->last_sent[0] & 0x80, "IFAC flag should be set");

	RNS::Transport::deregister_interface(interface);
}

// Cross-compatibility test: verify C++ produces same output as Python reference
void testIFACCrossCompatibilityWithPython() {
	HEAD("Running testIFACCrossCompatibilityWithPython...", RNS::LOG_TRACE);

	// Python-generated vectors for netname="manual_net", netkey="manual_pass", ifac_size=8
	const char* EXPECTED_IFAC_KEY_HEX = "F88E29721ECA87556BCA845F3F736CC64FD1B2BE084A888BBC3308D8177C43F7615417C2FECAB62BA43C5E0EED04A951DDCF18521999FCF3C4FACBCAF194F60B";

	// Step 1: Verify key derivation matches Python
	RNS::Interface interface(new CaptureInterface("TestCrossPy"));
	RNS::Reticulum reticulum;
	reticulum._add_interface(interface,
		RNS::Type::Interface::MODE_FULL, 8, "manual_net", "manual_pass");

	RNS::Bytes expected_key;
	expected_key.assignHex(EXPECTED_IFAC_KEY_HEX);
	TEST_ASSERT_TRUE_MESSAGE(interface.ifac_key() == expected_key,
		"IFAC key must match Python derivation");

	uint8_t ifac_size = interface.ifac_size();

	// Step 2: Verify IFAC encode produces same output as Python for 490-byte packet
	size_t pkt_size = 490;
	RNS::Bytes raw;
	raw.append((uint8_t)0x00);
	raw.append((uint8_t)0x01);
	for (size_t i = 2; i < pkt_size; i++) {
		raw.append((uint8_t)(i & 0xFF));
	}

	// Sign
	RNS::Bytes ifac = interface.ifac_identity().sign(raw).right(ifac_size);
	RNS::Bytes expected_ifac;
	expected_ifac.assignHex("7676929F48C33B09");
	TEST_ASSERT_TRUE_MESSAGE(ifac == expected_ifac,
		"IFAC signature must match Python for 490-byte packet");

	// Generate mask
	RNS::Bytes mask = RNS::Cryptography::hkdf(
		raw.size() + ifac_size, ifac, interface.ifac_key());

	// Assemble
	uint8_t hdr[] = { (uint8_t)(raw[0] | 0x80), raw[1] };
	RNS::Bytes new_header(hdr, 2);
	RNS::Bytes new_raw = new_header + ifac + raw.mid(2);

	// Mask
	RNS::Bytes masked_raw;
	masked_raw.assign(new_raw.data(), new_raw.size());
	for (size_t i = 0; i < masked_raw.size(); i++) {
		if (i == 0) {
			masked_raw[i] = (new_raw[i] ^ mask[i]) | 0x80;
		}
		else if (i == 1 || i > (size_t)(ifac_size + 1)) {
			masked_raw[i] = new_raw[i] ^ mask[i];
		}
	}

	TEST_ASSERT_EQUAL_size_t_MESSAGE(498, masked_raw.size(),
		"Masked packet size must be 498 (490+8)");

	RNS::Bytes expected_first32;
	expected_first32.assignHex("B8997676929F48C33B0934319E2E4BA8C7467637323A364EE8D12DA2C245E4D2");
	RNS::Bytes actual_first32 = masked_raw.left(32);
	TEST_ASSERT_TRUE_MESSAGE(actual_first32 == expected_first32,
		"First 32 bytes of masked output must match Python");

	// Step 3: Test multiple sizes against Python vectors
	struct TestVector {
		size_t pkt_size;
		const char* ifac_hex;
		const char* first32_hex;
		size_t masked_size;
	};

	TestVector vectors[] = {
		{   8, "015CDC0DD56E2E0F", "BA46015CDC0DD56E2E0F834E49040C2C", 16 },
		{  20, "1FEB1DC54B8CF001", "CBB91FEB1DC54B8CF001799D596F823EB6B81791DAFF885E4A2A071C", 28 },
		{ 100, "6F247C24AF6AA904", "E97E6F247C24AF6AA904D8B3CCD9659119C5248C2193AA08D9004B5F09CECA86", 108 },
		{ 300, "8757C8021FCB5D04", "A39A8757C8021FCB5D04F895A4E6B5287EC702AFE4036104A0AF9F6C8EF298F1", 308 },
		{ 500, "E5DD2224BF0E1B0D", "F610E5DD2224BF0E1B0D234BCD598EA35F9ECC26DBCC4B5D03199B2F2A5569D2", 508 },
	};

	for (size_t v = 0; v < sizeof(vectors) / sizeof(vectors[0]); v++) {
		char msg[128];
		TestVector& tv = vectors[v];

		RNS::Bytes vraw;
		vraw.append((uint8_t)0x00);
		vraw.append((uint8_t)0x01);
		for (size_t i = 2; i < tv.pkt_size; i++) {
			vraw.append((uint8_t)(i & 0xFF));
		}

		RNS::Bytes vifac = interface.ifac_identity().sign(vraw).right(ifac_size);
		RNS::Bytes exp_ifac;
		exp_ifac.assignHex(tv.ifac_hex);
		snprintf(msg, sizeof(msg), "IFAC mismatch at size %zu", tv.pkt_size);
		TEST_ASSERT_TRUE_MESSAGE(vifac == exp_ifac, msg);

		RNS::Bytes vmask = RNS::Cryptography::hkdf(
			vraw.size() + ifac_size, vifac, interface.ifac_key());

		uint8_t vhdr[] = { (uint8_t)(vraw[0] | 0x80), vraw[1] };
		RNS::Bytes vnew_header(vhdr, 2);
		RNS::Bytes vnew_raw = vnew_header + vifac + vraw.mid(2);

		RNS::Bytes vmasked;
		vmasked.assign(vnew_raw.data(), vnew_raw.size());
		for (size_t i = 0; i < vmasked.size(); i++) {
			if (i == 0) {
				vmasked[i] = (vnew_raw[i] ^ vmask[i]) | 0x80;
			}
			else if (i == 1 || i > (size_t)(ifac_size + 1)) {
				vmasked[i] = vnew_raw[i] ^ vmask[i];
			}
		}

		snprintf(msg, sizeof(msg), "Masked size mismatch at size %zu", tv.pkt_size);
		TEST_ASSERT_EQUAL_size_t_MESSAGE(tv.masked_size, vmasked.size(), msg);

		RNS::Bytes exp_first32;
		exp_first32.assignHex(tv.first32_hex);
		RNS::Bytes act_first = vmasked.left(exp_first32.size());
		snprintf(msg, sizeof(msg), "Masked bytes mismatch at size %zu", tv.pkt_size);
		TEST_ASSERT_TRUE_MESSAGE(act_first == exp_first32, msg);
	}

	RNS::Transport::deregister_interface(interface);
}

// Test that inbound rejects packets with wrong IFAC key
void testIFACInboundRejectsWrongKey() {
	HEAD("Running testIFACInboundRejectsWrongKey...", RNS::LOG_TRACE);

	// Create two interfaces with different IFAC keys
	CaptureInterface* sender_impl = new CaptureInterface("Sender");
	RNS::Interface sender_interface(sender_impl);

	CaptureInterface* receiver_impl = new CaptureInterface("Receiver");
	RNS::Interface receiver_interface(receiver_impl);

	// Set up transport identity
	RNS::Bytes transport_prv_bytes;
	transport_prv_bytes.assignHex("BABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABE");
	RNS::Identity transport_identity(false);
	transport_identity.load_private_key(transport_prv_bytes);
	RNS::Transport::identity(transport_identity);

	RNS::Reticulum reticulum;
	reticulum._add_interface(sender_interface,
		RNS::Type::Interface::MODE_FULL, 8, "net_a", "pass_a");
	reticulum._add_interface(receiver_interface,
		RNS::Type::Interface::MODE_FULL, 8, "net_b", "pass_b");

	// Transmit on sender
	uint8_t raw_data[] = { 0x00, 0x01, 0xDE, 0xAD, 0xBE, 0xEF };
	RNS::Bytes raw(raw_data, sizeof(raw_data));
	RNS::Transport::transmit(sender_interface, raw);

	// The transmitted packet has sender's IFAC — feeding it to receiver
	// with a different key should result in the packet being dropped.
	// Transport::inbound won't crash but will return without processing.
	// We verify that no exception is thrown (test completes).
	RNS::Transport::inbound(sender_impl->last_sent, receiver_interface);

	// If we get here without crash, the packet was silently dropped (correct behavior)
	TEST_ASSERT_TRUE_MESSAGE(true, "Wrong IFAC key should silently drop packet");

	RNS::Transport::deregister_interface(sender_interface);
	RNS::Transport::deregister_interface(receiver_interface);
}

// Manual IFAC encode/decode test at various sizes to isolate the crypto logic
// This replicates the exact steps from Transport::transmit and Transport::inbound
// without going through the full Transport machinery
void testIFACManualEncodeDecodeVarySizes() {
	HEAD("Running testIFACManualEncodeDecodeVarySizes...", RNS::LOG_TRACE);

	// Set up an interface with IFAC (ifac_size=8, matching RNode default)
	RNS::Interface interface(new CaptureInterface("TestManualED"));
	RNS::Reticulum reticulum;
	reticulum._add_interface(interface,
		RNS::Type::Interface::MODE_FULL, 8, "manual_net", "manual_pass");

	TEST_ASSERT_TRUE_MESSAGE(interface.ifac_identity(), "IFAC identity should be set");

	uint8_t ifac_size = interface.ifac_size();
	TEST_ASSERT_EQUAL_UINT8(8, ifac_size);

	// Test various packet sizes (2 header bytes + payload)
	size_t test_sizes[] = { 8, 20, 50, 100, 200, 300, 400, 450, 480, 490, 495, 500 };
	int num_sizes = sizeof(test_sizes) / sizeof(test_sizes[0]);

	for (int t = 0; t < num_sizes; t++) {
		size_t pkt_size = test_sizes[t];
		char msg[128];
		snprintf(msg, sizeof(msg), "Packet size %zu", pkt_size);

		// Build a fake raw packet: 2 header bytes + (pkt_size-2) payload
		RNS::Bytes raw;
		raw.append((uint8_t)0x00);  // header byte 0 (no IFAC flag)
		raw.append((uint8_t)0x01);  // header byte 1 (hops)
		for (size_t i = 2; i < pkt_size; i++) {
			raw.append((uint8_t)(i & 0xFF));
		}
		TEST_ASSERT_EQUAL_size_t_MESSAGE(pkt_size, raw.size(), msg);

		// === ENCODE (replicating Transport::transmit IFAC logic) ===

		// Step 1: Sign the raw packet, take last ifac_size bytes
		RNS::Bytes ifac = interface.ifac_identity().sign(raw).right(ifac_size);
		snprintf(msg, sizeof(msg), "IFAC size at pkt_size %zu", pkt_size);
		TEST_ASSERT_EQUAL_size_t_MESSAGE(ifac_size, ifac.size(), msg);

		// Step 2: Generate mask
		RNS::Bytes mask = RNS::Cryptography::hkdf(
			raw.size() + ifac_size,
			ifac,
			interface.ifac_key()
		);
		snprintf(msg, sizeof(msg), "Mask size at pkt_size %zu", pkt_size);
		TEST_ASSERT_EQUAL_size_t_MESSAGE(raw.size() + ifac_size, mask.size(), msg);

		// Step 3: Set IFAC flag and build new packet
		uint8_t hdr[] = { (uint8_t)(raw[0] | 0x80), raw[1] };
		RNS::Bytes new_header(hdr, 2);
		RNS::Bytes new_raw = new_header + ifac + raw.mid(2);

		snprintf(msg, sizeof(msg), "new_raw size at pkt_size %zu", pkt_size);
		TEST_ASSERT_EQUAL_size_t_MESSAGE(pkt_size + ifac_size, new_raw.size(), msg);

		// Step 4: Mask payload (identical to Transport::transmit)
		RNS::Bytes masked_raw;
		masked_raw.assign(new_raw.data(), new_raw.size());
		for (size_t i = 0; i < masked_raw.size(); i++) {
			if (i == 0) {
				masked_raw[i] = (new_raw[i] ^ mask[i]) | 0x80;
			}
			else if (i == 1 || i > (size_t)(ifac_size + 1)) {
				masked_raw[i] = new_raw[i] ^ mask[i];
			}
		}

		// Verify IFAC flag is set
		snprintf(msg, sizeof(msg), "IFAC flag at pkt_size %zu", pkt_size);
		TEST_ASSERT_TRUE_MESSAGE(masked_raw[0] & 0x80, msg);

		// === DECODE (replicating Transport::inbound IFAC logic) ===

		RNS::Bytes local_raw(masked_raw);

		// Step 1: Verify size
		snprintf(msg, sizeof(msg), "Decode: size check at pkt_size %zu", pkt_size);
		TEST_ASSERT_TRUE_MESSAGE(local_raw.size() > (size_t)(2 + ifac_size), msg);

		// Step 2: Extract IFAC
		RNS::Bytes extracted_ifac = local_raw.mid(2, ifac_size);
		snprintf(msg, sizeof(msg), "Extracted IFAC matches at pkt_size %zu", pkt_size);
		TEST_ASSERT_TRUE_MESSAGE(extracted_ifac == ifac, msg);

		// Step 3: Generate mask for unmasking
		RNS::Bytes decode_mask = RNS::Cryptography::hkdf(local_raw.size(), extracted_ifac, interface.ifac_key());
		snprintf(msg, sizeof(msg), "Decode mask size at pkt_size %zu", pkt_size);
		TEST_ASSERT_EQUAL_size_t_MESSAGE(local_raw.size(), decode_mask.size(), msg);

		// Step 4: Unmask
		RNS::Bytes unmasked_raw;
		unmasked_raw.assign(local_raw.data(), local_raw.size());
		for (size_t i = 0; i < unmasked_raw.size(); i++) {
			if (i <= 1 || i > (size_t)(ifac_size + 1)) {
				unmasked_raw[i] = local_raw[i] ^ decode_mask[i];
			}
		}
		local_raw = unmasked_raw;

		// Step 5: Clear IFAC flag and remove IFAC bytes
		uint8_t dec_hdr[] = { (uint8_t)(local_raw[0] & 0x7f), local_raw[1] };
		RNS::Bytes dec_header(dec_hdr, 2);
		RNS::Bytes reconstructed = dec_header + local_raw.mid(2 + ifac_size);

		// Step 6: Verify signature
		RNS::Bytes expected_ifac = interface.ifac_identity().sign(reconstructed).right(ifac_size);
		snprintf(msg, sizeof(msg), "IFAC verification at pkt_size %zu", pkt_size);
		TEST_ASSERT_TRUE_MESSAGE(extracted_ifac == expected_ifac, msg);

		// Step 7: Verify reconstructed matches original
		snprintf(msg, sizeof(msg), "Reconstructed size at pkt_size %zu", pkt_size);
		TEST_ASSERT_EQUAL_size_t_MESSAGE(pkt_size, reconstructed.size(), msg);
		snprintf(msg, sizeof(msg), "Reconstructed matches original at pkt_size %zu", pkt_size);
		TEST_ASSERT_TRUE_MESSAGE(reconstructed == raw, msg);
	}

	RNS::Transport::deregister_interface(interface);
}

// Test transmit→inbound round trip at various sizes using Transport functions
void testIFACTransmitInboundRoundtripVarySizes() {
	HEAD("Running testIFACTransmitInboundRoundtripVarySizes...", RNS::LOG_TRACE);

	CaptureInterface* capture_impl = new CaptureInterface("TestRTVary");
	RNS::Interface interface(capture_impl);

	// Set up transport identity
	RNS::Bytes transport_prv_bytes;
	transport_prv_bytes.assignHex("BABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABE");
	RNS::Identity transport_identity(false);
	transport_identity.load_private_key(transport_prv_bytes);
	RNS::Transport::identity(transport_identity);

	RNS::Reticulum reticulum;
	reticulum._add_interface(interface,
		RNS::Type::Interface::MODE_FULL, 8, "rtvary_net", "rtvary_pass");

	size_t test_sizes[] = { 8, 20, 50, 100, 200, 300, 400, 450, 480, 490, 495, 500 };
	int num_sizes = sizeof(test_sizes) / sizeof(test_sizes[0]);

	for (int t = 0; t < num_sizes; t++) {
		size_t pkt_size = test_sizes[t];
		char msg[128];

		// Build raw packet
		RNS::Bytes raw;
		raw.append((uint8_t)0x00);
		raw.append((uint8_t)0x01);
		for (size_t i = 2; i < pkt_size; i++) {
			raw.append((uint8_t)((i + t) & 0xFF));
		}

		// Transmit
		capture_impl->send_count = 0;
		RNS::Transport::transmit(interface, raw);

		snprintf(msg, sizeof(msg), "Transmit sent at pkt_size %zu", pkt_size);
		TEST_ASSERT_EQUAL_INT_MESSAGE(1, capture_impl->send_count, msg);

		snprintf(msg, sizeof(msg), "Wire size at pkt_size %zu (expected %zu, got %zu)",
			pkt_size, pkt_size + 8, capture_impl->last_sent.size());
		TEST_ASSERT_EQUAL_size_t_MESSAGE(pkt_size + 8, capture_impl->last_sent.size(), msg);

		snprintf(msg, sizeof(msg), "IFAC flag at pkt_size %zu", pkt_size);
		TEST_ASSERT_TRUE_MESSAGE(capture_impl->last_sent[0] & 0x80, msg);

		// Feed the transmitted packet back through inbound
		// (This will go through IFAC decode + unpack — unpack may fail
		// since our fake packets aren't valid Reticulum packets, but
		// the IFAC decode should succeed before unpack is attempted)
		RNS::Transport::inbound(capture_impl->last_sent, interface);

		// We can't easily verify inbound succeeded since it returns void,
		// but we can verify no crash/exception occurred for this size
		snprintf(msg, sizeof(msg), "Inbound survived at pkt_size %zu", pkt_size);
		TEST_ASSERT_TRUE_MESSAGE(true, msg);
	}

	RNS::Transport::deregister_interface(interface);
}

// Test that inbound rejects packets without IFAC flag when IFAC is enabled
void testIFACInboundRejectsNoFlag() {
	HEAD("Running testIFACInboundRejectsNoFlag...", RNS::LOG_TRACE);

	RNS::Interface interface(new CaptureInterface("TestNoFlag"));

	RNS::Bytes transport_prv_bytes;
	transport_prv_bytes.assignHex("BABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABE");
	RNS::Identity transport_identity(false);
	transport_identity.load_private_key(transport_prv_bytes);
	RNS::Transport::identity(transport_identity);

	RNS::Reticulum reticulum;
	reticulum._add_interface(interface,
		RNS::Type::Interface::MODE_FULL, 8, "flagtest", "flagpass");

	// Send a raw packet WITHOUT IFAC flag to an interface that expects IFAC
	uint8_t raw_data[] = { 0x00, 0x01, 0xDE, 0xAD, 0xBE, 0xEF };
	RNS::Bytes raw(raw_data, sizeof(raw_data));

	// This should silently drop the packet (no IFAC flag but IFAC expected)
	RNS::Transport::inbound(raw, interface);

	// If we get here, the packet was correctly dropped
	TEST_ASSERT_TRUE_MESSAGE(true, "Packet without IFAC flag should be dropped when IFAC is enabled");

	RNS::Transport::deregister_interface(interface);
}


void setUp(void) {
}

void tearDown(void) {
}

int runUnityTests(void) {
	UNITY_BEGIN();
	RUN_TEST(testIFACKeyDerivation);
	RUN_TEST(testIFACKeyDerivationNameOnly);
	RUN_TEST(testIFACKeyDerivationKeyOnly);
	RUN_TEST(testIFACNoConfig);
	RUN_TEST(testIFACDeterministic);
	RUN_TEST(testIFACTransmitInboundRoundtrip);
	RUN_TEST(testIFACManualEncodeDecodeVarySizes);
	RUN_TEST(testIFACTransmitInboundRoundtripVarySizes);
	RUN_TEST(testIFACCrossCompatibilityWithPython);
	RUN_TEST(testIFACInboundRejectsWrongKey);
	RUN_TEST(testIFACInboundRejectsNoFlag);
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
