#include <unity.h>

#include "microReticulum.h"

// Reference vectors produced by Python RNS 1.5.4 itself: the IFAC key
// derivation of Reticulum's interface setup, and RNS.Transport.transmit()
// masking a packet on an interface with ifac_size 8 (LoRa/RNode default).
static const char* NETNAME = "t114-test";
static const char* NETKEY  = "correct horse battery";
static const char* KEY_HEX =
	"fd8c2d34e726f864a29dbcc21699ba7e5a71be366739dcc81a8bbf800a535744"
	"717704cfedbd9b122e9b377989af898cecfbc1f2ecda9e285bcc2d266b390821";
static const char* RAW_HEX =
	"0100867d6e042dcce3ed972c2d0f4f2123a9002f5d209d6de938eec6f269f5e6a90858";
static const char* MASKED_HEX =
	"8711f21f1698735aba0cf40cbdbebb25afb3577006ea611e6c3a472cd552c8f6e085bd2a526db83728c2bb";

class NullInterface : public RNS::InterfaceImpl {
public:
	NullInterface() : RNS::InterfaceImpl("NullInterface") { _OUT = true; _IN = true; }
	virtual bool send_outgoing(const RNS::Bytes& data) { return true; }
};

static RNS::Bytes hex(const char* h) {
	RNS::Bytes b;
	b.assignHex(h);
	return b;
}

static RNS::Interface ifac_interface() {
	RNS::Interface iface(new NullInterface());
	iface.setup_ifac(NETNAME, NETKEY);
	return iface;
}

void testKeyDerivationMatchesPython() {
	RNS::Interface iface = ifac_interface();
	TEST_ASSERT_TRUE((bool)iface.ifac_identity());
	TEST_ASSERT_EQUAL_STRING(KEY_HEX, iface.ifac_key().toHex().c_str());
}

void testMaskMatchesPython() {
	RNS::Interface iface = ifac_interface();
	RNS::Bytes masked = RNS::Transport::ifac_mask(iface, hex(RAW_HEX));
	TEST_ASSERT_EQUAL_STRING(MASKED_HEX, masked.toHex().c_str());
}

void testUnmaskRecoversPythonPacket() {
	RNS::Interface iface = ifac_interface();
	RNS::Bytes raw = RNS::Transport::ifac_unmask(iface, hex(MASKED_HEX));
	TEST_ASSERT_EQUAL_STRING(RAW_HEX, raw.toHex().c_str());
}

void testTamperedOrForeignPacketsAreRejected() {
	RNS::Interface iface = ifac_interface();
	// One flipped payload bit: the code no longer authenticates.
	RNS::Bytes tampered = hex(MASKED_HEX);
	tampered[tampered.size() - 1] ^= 0x01;
	TEST_ASSERT_FALSE((bool)RNS::Transport::ifac_unmask(iface, tampered));
	// Another network's key.
	RNS::Interface other(new NullInterface());
	other.setup_ifac("another-net", NETKEY);
	TEST_ASSERT_FALSE((bool)RNS::Transport::ifac_unmask(other, hex(MASKED_HEX)));
	// No IFAC flag, or too short.
	TEST_ASSERT_FALSE((bool)RNS::Transport::ifac_unmask(iface, hex(RAW_HEX)));
	TEST_ASSERT_FALSE((bool)RNS::Transport::ifac_unmask(iface, hex("8711f21f16")));
}

void testEmptyNameAndKeyLeaveIfacOff() {
	RNS::Interface iface(new NullInterface());
	iface.setup_ifac("", nullptr);
	TEST_ASSERT_FALSE((bool)iface.ifac_identity());
}

void setUp(void) {}
void tearDown(void) {}

int runUnityTests(void) {
	UNITY_BEGIN();
	RUN_TEST(testKeyDerivationMatchesPython);
	RUN_TEST(testMaskMatchesPython);
	RUN_TEST(testUnmaskRecoversPythonPacket);
	RUN_TEST(testTamperedOrForeignPacketsAreRejected);
	RUN_TEST(testEmptyNameAndKeyLeaveIfacOff);
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
