//#include <unity.h>

#include "Reticulum.h"
#include "Transport.h"
#include "Log.h"

#include <assert.h>

class TestInterface : public RNS::Interface {

public:
	TestInterface(const char* name = "TestInterface") : Interface(name) {}
	virtual ~TestInterface() {}

	virtual inline std::string toString() const { return "TestInterface[" + name() + "]"; }

private:
	virtual void on_incoming(const RNS::Bytes& data) {
		RNS::debug(toString() + ".on_incoming: data: " + data.toHex());
		Interface::on_incoming(data);
	}
	virtual void on_outgoing(const RNS::Bytes& data) {
		RNS::debug(toString() + ".on_outgoing: data: " + data.toHex());
		Interface::on_outgoing(data);
	}

};

class InInterface : public RNS::Interface {
public:
	InInterface() : RNS::Interface("InInterface") {
		OUT(false);
		IN(true);
	}
	InInterface(const char *name) : RNS::Interface(name) {
		OUT(false);
		IN(true);
	}
	virtual ~InInterface() {
		name("(deleted)");
	}
	virtual void on_incoming(const RNS::Bytes &data) {
		HEAD("InInterface.on_incoming: data: " + data.toHex(), RNS::LOG_TRACE);
		RNS::Interface::on_incoming(data);
	}
	virtual inline std::string toString() const { return "InInterface[" + name() + "]"; }
};

class OutInterface : public RNS::Interface {
public:
	OutInterface(InInterface& in_interface) : RNS::Interface("OutInterface"), _in_interface(in_interface) {
		OUT(true);
		IN(false);
	}
	OutInterface(InInterface& in_interface, const char *name) : RNS::Interface(name), _in_interface(in_interface) {
		OUT(true);
		IN(false);
	}
	virtual ~OutInterface() {
		name("(deleted)");
	}
	virtual void on_outgoing(const RNS::Bytes &data) {
		HEAD("OutInterface.on_outgoing: data: " + data.toHex(), RNS::LOG_TRACE);
		_in_interface.on_incoming(data);
		RNS::Interface::on_outgoing(data);
	}
	virtual inline std::string toString() const { return "OutInterface[" + name() + "]"; }
private:
	InInterface& _in_interface;
};

// Test AnnounceHandler
class ExampleAnnounceHandler : public RNS::AnnounceHandler {
public:
	ExampleAnnounceHandler(const char* aspect_filter = nullptr) : AnnounceHandler(aspect_filter) {}
	virtual ~ExampleAnnounceHandler() {}
	virtual void received_announce(const RNS::Bytes& destination_hash, const RNS::Identity& announced_identity, const RNS::Bytes& app_data) {
		INFO("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
		INFO("ExampleAnnounceHandler: destination hash: " + destination_hash.toHex());
		if (announced_identity) {
			INFO("ExampleAnnounceHandler: announced identity hash: " + announced_identity.hash().toHex());
			INFO("ExampleAnnounceHandler: announced identity app data: " + announced_identity.app_data().toHex());
		}
        if (app_data) {
			INFO("ExampleAnnounceHandler: app data text: \"" + app_data.toString() + "\"");
		}
		INFO("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
	}
};

//ExampleAnnounceHandler announce_handler((const char*)"example_utilities.announcesample.fruits");
//RNS::HAnnounceHandler announce_handler(new ExampleAnnounceHandler("example_utilities.announcesample.fruits"));
RNS::HAnnounceHandler announce_handler(new ExampleAnnounceHandler());

// Test packet receive callback
void onPacket(const RNS::Bytes& data, const RNS::Packet& packet) {
	INFO("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
	INFO("onPacket: data: " + data.toHex());
	INFO("onPacket: text: \"" + data.toString() + "\"");
	//TRACE("onPacket: " + packet.debugString());
	INFO("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
}

// Ping packet receive callback
void onPingPacket(const RNS::Bytes& data, const RNS::Packet& packet) {
	INFO("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
	INFO("onPingPacket: data: " + data.toHex());
	INFO("onPingPacket: text: \"" + data.toString() + "\"");
	//TRACE("onPingPacket: " + packet.debugString());
	INFO("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
}


RNS::Reticulum reticulum({RNS::Type::NONE});
RNS::Identity identity({RNS::Type::NONE});
RNS::Destination destination({RNS::Type::NONE});
InInterface in_interface;
OutInterface out_interface(in_interface);

void testReticulum() {
	HEAD("Running testReticulum...", RNS::LOG_TRACE);

	HEAD("Registering Interface instances with Transport...", RNS::LOG_TRACE);
	RNS::Transport::register_interface(in_interface);
	RNS::Transport::register_interface(out_interface);

	RNS::Bytes transport_prv_bytes;
#ifdef ARDUINO
	transport_prv_bytes.assignHex("CAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFECAFE");
#else
	transport_prv_bytes.assignHex("BABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABEBABE");
#endif
	RNS::Identity transport_identity(false);
	transport_identity.load_private_key(transport_prv_bytes);
	RNS::Transport::identity(transport_identity);

	HEAD("Creating Reticulum instance...", RNS::LOG_TRACE);
    reticulum = RNS::Reticulum();
	reticulum.transport_enabled(true);
	reticulum.probe_destination_enabled(true);
	reticulum.start();

	HEAD("Creating Identity instance...", RNS::LOG_TRACE);
	// new identity
	identity = RNS::Identity(false);
	RNS::Bytes prv_bytes;
#ifdef ARDUINO
	prv_bytes.assignHex("78E7D93E28D55871608FF13329A226CABC3903A357388A035B360162FF6321570B092E0583772AB80BC425F99791DF5CA2CA0A985FF0415DAB419BBC64DDFAE8");
#else
	prv_bytes.assignHex("E0D43398EDC974EBA9F4A83463691A08F4D306D4E56BA6B275B8690A2FBD9852E9EBE7C03BC45CAEC9EF8E78C830037210BFB9986F6CA2DEE2B5C28D7B4DE6B0");
#endif
	identity.load_private_key(prv_bytes);
	// 22.6% (+0.7%)

	HEAD("Creating Destination instance...", RNS::LOG_TRACE);
	//RNS::Destination destination(identity, RNS::Type::Destination::IN, RNS::Type::Destination::SINGLE, "app", "aspects");
	destination = RNS::Destination(identity, RNS::Type::Destination::IN, RNS::Type::Destination::SINGLE, "app", "aspects");
	// 23.0% (+0.4%)

	// Register DATA packet callback
	HEAD("Registering packet callback with Destination...", RNS::LOG_TRACE);
	destination.set_packet_callback(onPacket);
	destination.set_proof_strategy(RNS::Type::Destination::PROVE_ALL);

	{
		// Register PING packet callback
		HEAD("Creating PING Destination instance...", RNS::LOG_TRACE);
		RNS::Destination ping_destination(identity, RNS::Type::Destination::IN, RNS::Type::Destination::SINGLE, "example_utilities", "echo.request");

		HEAD("Registering packet callback with PING Destination...", RNS::LOG_TRACE);
		ping_destination.set_packet_callback(onPingPacket);
		ping_destination.set_proof_strategy(RNS::Type::Destination::PROVE_ALL);
	}

	HEAD("Registering announce handler with Transport...", RNS::LOG_TRACE);
	RNS::Transport::register_announce_handler(announce_handler);

	HEAD("Announcing destination...", RNS::LOG_TRACE);
	// test path
	//destination.announce(RNS::bytesFromString("test"), true, nullptr, RNS::bytesFromString("test_tag"));
	// test packet send
	destination.announce(RNS::bytesFromString("test"));
	// 23.9% (+0.8%)

	reticulum = {RNS::Type::NONE};
}

/*
void setUp(void) {
	// set stuff up here
}

void tearDown(void) {
	// clean stuff up here
}

int runUnityTests(void) {
	UNITY_BEGIN();
	RUN_TEST(testBytes);
	RUN_TEST(testCowBytes);
	RUN_TEST(testBytesConversion);
	RUN_TEST(testMap);
	return UNITY_END();
}

// For native dev-platform or for some embedded frameworks
int main(void) {
	return runUnityTests();
}

// For Arduino framework
void setup() {
	// Wait ~2 seconds before the Unity test runner
	// establishes connection with a board Serial interface
	delay(2000);

	runUnityTests();
}
void loop() {}

// For ESP-IDF framework
void app_main() {
	runUnityTests();
}
*/
