#include <unity.h>

#include <microStore/Adapters/UniversalFileSystem.h>

#include "microReticulum.h"

const char destination_table_path[] = "./destination_table";

class InInterface : public RNS::InterfaceImpl {
public:
	InInterface(const char *name = "InInterface") : RNS::InterfaceImpl(name) {
		_OUT = false;
		_IN = true;
	}
	virtual ~InInterface() {
		_name = "(deleted)";
	}
	// Incoming interface only, send_outgoing not currently implemented
	virtual bool send_outgoing(const RNS::Bytes &data) { return true; }
	// Incoming is be handled automatically by InterfaceImpl::handle_incoming called via Interface::handle_incoming from OutInterface
};

class OutInterface : public RNS::InterfaceImpl {
public:
	OutInterface(RNS::Interface& in_interface, const char *name = "OutInterface") : RNS::InterfaceImpl(name), _in_interface(in_interface) {
		_OUT = true;
		_IN = false;
	}
	virtual ~OutInterface() {
		_name = "(deleted)";
	}
	virtual bool send_outgoing(const RNS::Bytes &data) {
		HEADF(RNS::LOG_TRACE, "OutInterface.send_outgoing: data: %s", data.toHex().c_str());

		// Keep the last announce sent, so a test can replay it as if it had
		// arrived from another node
		if (data.size() > 0 && (data.data()[0] & 0x03) == RNS::Type::Packet::ANNOUNCE) {
			last_announce = data;
		}

		// Loop data back to InInterface for testing
		// NOTE: This sends the incoming data directory to RNS::Interface (which relays to Transport)
		//       and does not actually go through InInterface.
		_in_interface.handle_incoming(data);

		// Perform post-send housekeeping
		InterfaceImpl::handle_outgoing(data);

		return true;
	}
	static RNS::Bytes last_announce;
private:
	RNS::Interface& _in_interface;
};
RNS::Bytes OutInterface::last_announce;

// Test AnnounceHandler
class ExampleAnnounceHandler : public RNS::AnnounceHandler {
public:
	ExampleAnnounceHandler(const char* aspect_filter = nullptr) : AnnounceHandler(aspect_filter) {}
	virtual ~ExampleAnnounceHandler() {}
	virtual void received_announce(const RNS::Bytes& destination_hash, const RNS::Identity& announced_identity, const RNS::Bytes& app_data) {
		INFO("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
		INFOF("ExampleAnnounceHandler: destination hash: %s", destination_hash.toHex().c_str());
		if (announced_identity) {
			INFOF("ExampleAnnounceHandler: announced identity hash: %s", announced_identity.hash().toHex().c_str());
			INFOF("ExampleAnnounceHandler: announced identity app data: %s", announced_identity.app_data().toHex().c_str());
		}
        if (app_data) {
			INFOF("ExampleAnnounceHandler: app data text: \"%s\"", app_data.toString().c_str());
		}
		INFO("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
	}
};

// Overrides only the basic callback: must still be called now that Transport
// dispatches through the packet-carrying overload
class BasicAnnounceHandler : public RNS::AnnounceHandler {
public:
	BasicAnnounceHandler() : AnnounceHandler(nullptr) {}
	virtual void received_announce(const RNS::Bytes& destination_hash, const RNS::Identity& announced_identity, const RNS::Bytes& app_data) {
		++calls;
		last_destination_hash = destination_hash;
	}
	int calls = 0;
	RNS::Bytes last_destination_hash;
};

// Overrides the packet-carrying callback as well, and records what it saw
class PacketAnnounceHandler : public RNS::AnnounceHandler {
public:
	PacketAnnounceHandler() : AnnounceHandler(nullptr) {}
	virtual void received_announce(const RNS::Bytes& destination_hash, const RNS::Identity& announced_identity, const RNS::Bytes& app_data) {
		++basic_calls;
	}
	virtual void received_announce(const RNS::Bytes& destination_hash, const RNS::Identity& announced_identity, const RNS::Bytes& app_data, const RNS::Packet& packet) {
		++packet_calls;
		last_destination_hash = destination_hash;
		last_packet_destination_hash = packet.destination_hash();
		last_packet_type = packet.packet_type();
		last_receiving_interface = packet.receiving_interface().get_hash();
	}
	int basic_calls = 0;
	int packet_calls = 0;
	RNS::Bytes last_destination_hash;
	RNS::Bytes last_packet_destination_hash;
	uint8_t last_packet_type = 0xFF;
	RNS::Bytes last_receiving_interface;
};

std::shared_ptr<BasicAnnounceHandler> basic_announce_handler(new BasicAnnounceHandler());
std::shared_ptr<PacketAnnounceHandler> packet_announce_handler(new PacketAnnounceHandler());

//ExampleAnnounceHandler announce_handler((const char*)"example_utilities.announcesample.fruits");
//RNS::HAnnounceHandler announce_handler(new ExampleAnnounceHandler("example_utilities.announcesample.fruits"));
RNS::HAnnounceHandler announce_handler(new ExampleAnnounceHandler());

// Test packet receive callback
//void(*)(const Bytes& data, const Packet& packet)
void onPacket(const RNS::Bytes& data, const RNS::Packet& packet) {
	INFO("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
	INFOF("onPacket: data: %s", data.toHex().c_str());
	INFOF("onPacket: text: \"%s\"", data.toString().c_str());
	//TRACEF("onPacket: %s", packet.debugString().c_str());
	INFO("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
}

// Ping packet receive callback
//void(*)(const Bytes& data, const Packet& packet)
void onPingPacket(const RNS::Bytes& data, const RNS::Packet& packet) {
	INFO("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
	INFOF("onPingPacket: data: %s", data.toHex().c_str());
	INFOF("onPingPacket: text: \"%s\"", data.toString().c_str());
	//TRACEF("onPingPacket: %s", packet.debugString().c_str());
	INFO("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
}


RNS::Reticulum reticulum({RNS::Type::NONE});
RNS::Identity identity({RNS::Type::NONE});
RNS::Destination destination({RNS::Type::NONE});
RNS::Interface in_interface(new InInterface());
RNS::Interface out_interface(new OutInterface(in_interface));

void testReticulum() {
	HEAD("Running testReticulum...", RNS::LOG_TRACE);

	HEAD("Registering Filesystem with OS...", RNS::LOG_TRACE);
    microStore::FileSystem filesystem{microStore::Adapters::UniversalFileSystem()};
	filesystem.init();
	RNS::Utilities::OS::register_filesystem(filesystem);

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
	reticulum.transport_enabled(false);
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
	RNS::Transport::register_announce_handler(basic_announce_handler);
	RNS::Transport::register_announce_handler(packet_announce_handler);

	HEAD("Announcing destination...", RNS::LOG_TRACE);
	// test path
	//destination.announce(RNS::bytesFromString("test"), true, nullptr, RNS::bytesFromString("test_tag"));
	// test packet send
	destination.announce(RNS::bytesFromString("test"));
	// 23.9% (+0.8%)

	// The announce looped back above is for a local destination, so Transport
	// does not hand it to announce handlers. Deregister the destination and
	// replay the same announce, so it arrives as one from another node would.
	HEAD("Replaying announce as a remote one...", RNS::LOG_TRACE);
	TEST_ASSERT_TRUE(OutInterface::last_announce.size() > 0);
	RNS::Transport::deregister_destination(destination);
	in_interface.handle_incoming(OutInterface::last_announce);

	// A handler overriding only the basic callback is still called
	TEST_ASSERT_EQUAL_INT(1, basic_announce_handler->calls);
	TEST_ASSERT_TRUE(basic_announce_handler->last_destination_hash == destination.hash());

	// A handler overriding the packet-carrying callback gets the packet, and
	// is not called a second time through the basic one
	TEST_ASSERT_EQUAL_INT(1, packet_announce_handler->packet_calls);
	TEST_ASSERT_EQUAL_INT(0, packet_announce_handler->basic_calls);
	TEST_ASSERT_TRUE(packet_announce_handler->last_destination_hash == destination.hash());
	TEST_ASSERT_TRUE(packet_announce_handler->last_packet_destination_hash == destination.hash());
	TEST_ASSERT_EQUAL_UINT8(RNS::Type::Packet::ANNOUNCE, packet_announce_handler->last_packet_type);
	TEST_ASSERT_TRUE(packet_announce_handler->last_receiving_interface == in_interface.get_hash());

	RNS::Transport::deregister_announce_handler(basic_announce_handler);
	RNS::Transport::deregister_announce_handler(packet_announce_handler);

	HEAD("Destroying Reticulum instance...", RNS::LOG_TRACE);
	reticulum = {RNS::Type::NONE};

	RNS::Utilities::OS::remove_file(destination_table_path);
}


void setUp(void) {
	// set stuff up here before each test
}

void tearDown(void) {
	// clean stuff up here after each test
}

int runUnityTests(void) {
	UNITY_BEGIN();
	RUN_TEST(testReticulum);
	return UNITY_END();
}

// For native dev-platform or for some embedded frameworks
int main(void) {
	return runUnityTests();
}

#ifdef ARDUINO
// For Arduino framework
void setup() {
	// Wait ~2 seconds before the Unity test runner
	// establishes connection with a board Serial interface
	delay(2000);
	
	runUnityTests();
}
void loop() {}
#endif

// For ESP-IDF framework
void app_main() {
	runUnityTests();
}
