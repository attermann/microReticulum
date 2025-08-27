#include <unity.h>

#include "../common/filesystem/FileSystem.h"

#include <Interface.h>
#include <Transport.h>
#include <Log.h>
#include <Bytes.h>
#include <Utilities/OS.h>
#include <Utilities/Persistence.h>

#include <map>
#include <vector>
#include <set>
#include <string>
#include <stdio.h>

class TestInterface : public RNS::InterfaceImpl {

public:
	TestInterface(const char* name = "TestInterface") : InterfaceImpl(name) {}
	virtual ~TestInterface() {}

private:
	virtual void send_outgoing(const RNS::Bytes& data) {
		DEBUG("TestInterface.send_outgoing: data: " + data.toHex());
		// Perform post-send housekeeping
		InterfaceImpl::handle_outgoing(data);
	}
	void on_incoming(const RNS::Bytes& data) {
		DEBUG("TestInterface.on_incoming: data: " + data.toHex());
		// Pass received data on to transport
		InterfaceImpl::handle_incoming(data);
	}

};

#ifdef ARDUINO
const char test_destination_table_path[] = "/test_destination_table";
const char test_empty_destination_table_path[] = "/test_empty_destination_table";
#else
const char test_destination_table_path[] = "test_destination_table";
const char test_empty_destination_table_path[] = "test_empty_destination_table";
#endif

void testSerializeDestinationTable() {
	HEAD("testSerializeDestinationTable", RNS::LOG_TRACE);

	//RNS::Bytes empty;
	//std::set<RNS::Bytes> blobs;
	//RNS::Interface interface({RNS::Type::NONE});
	RNS::Interface test_interface(new TestInterface());
	//RNS::Packet packet({RNS::Type::NONE});
	static std::map<RNS::Bytes, RNS::Transport::DestinationEntry> map;
	//DestinationEntry(double time, const Bytes& received_from, uint8_t announce_hops, double expires, const std::set<Bytes>& random_blobs, Interface& receiving_interface, const Packet& packet) :
	//RNS::Transport::DestinationEntry entry_one(1.0, empty, 1, 0.0, blobs, interface, packet);
	RNS::Bytes received;
	received.assignHex("deadbeef");
	RNS::Bytes blob;
	blob.assignHex("b10bb10b");
	std::set<RNS::Bytes> blobs({received, blob});
	RNS::Transport::DestinationEntry entry_one;
	entry_one._timestamp = 1.0;
	entry_one._received_from = received;
	entry_one._receiving_interface = test_interface.get_hash();
	entry_one._random_blobs = blobs;
	RNS::Bytes one;
	one.assignHex("1111111111111111");
	map.insert({one, entry_one});
	//RNS::Transport::DestinationEntry entry_two(2.0, empty, 1, 0.0, blobs, interface, packet);
	RNS::Transport::DestinationEntry entry_two;
	entry_two._timestamp = 2.0;
	entry_two._received_from = received;
	entry_two._receiving_interface = test_interface.get_hash();
	entry_two._random_blobs = blobs;
	RNS::Bytes two;
	two.assignHex("2222222222222222");
	map.insert({two, entry_two});

	for (int n = 0; n < 20; n++) {
		RNS::Transport::DestinationEntry entry;
		entry._timestamp = 1.0;
		entry._received_from = received;
		entry._receiving_interface = test_interface.get_hash();
		entry._random_blobs = blobs;
		RNS::Bytes hash;
		hash.assign(std::to_string(1000000000000001 + n).c_str());
		map.insert({hash, entry});
	}

	TRACEF("testSerializeDestinationTable: map contains %d entries", map.size());
	TEST_ASSERT_EQUAL_size_t(22, map.size());
	for (auto& [hash, test] : map) {
		TRACEF("testSerializeDestinationTable: entry: %s = %s", hash.toHex().c_str(), test.debugString().c_str());
	}

	uint32_t stream_crc;
	size_t wrote = RNS::Persistence::serialize(map, test_destination_table_path, stream_crc);
	TRACEF("testSerializeDestinationTable: wrote: %d bytes", wrote);
	if (wrote == 0) {
		TRACE("testSerializeDestinationTable: write failed");
		TEST_FAIL();
	}

	uint32_t crc = RNS::Persistence::crc(map);
	TRACEF("testSerializeDestinationTable: crc: 0x%X", crc);
	TRACEF("testSerializeDestinationTable: stream crc: 0x%X", stream_crc);
	TEST_ASSERT_EQUAL_UINT32(0xFC979601, crc);
	TEST_ASSERT_EQUAL_UINT32(0xFC979601, stream_crc);

}

void testDeserializeDestinationTable() {
	HEAD("testDeserializeDestinationTable", RNS::LOG_TRACE);

	std::map<RNS::Bytes, RNS::Transport::DestinationEntry> map;
	uint32_t stream_crc;
	size_t read = RNS::Persistence::deserialize(map, test_destination_table_path, stream_crc);
	TRACEF("testDeserializeDestinationTable: deserialized %d bytes", read);
	if (read == 0) {
		TRACE("testDeserializeDestinationTable: failed to deserialize");
		TEST_FAIL();
	}

	TRACEF("testDeserializeDestinationTable: map contains %d entries", map.size());
	TEST_ASSERT_EQUAL_size_t(22, map.size());
	for (auto& [hash, test] : map) {
		TRACEF("testDeserializeDestinationTable: entry: %s = %s", hash.toHex().c_str(), test.debugString().c_str());
	}

	uint32_t crc = RNS::Persistence::crc(map);
	TRACEF("testDeserializeDestinationTable: crc: 0x%X", crc);
	TRACEF("testDeserializeDestinationTable: stream crc: 0x%X", stream_crc);
	TEST_ASSERT_EQUAL_UINT32(0xFC979601, crc);
	TEST_ASSERT_EQUAL_UINT32(0xFC979601, stream_crc);

}

void testDeserializeEmptyDestinationTable() {

	// Write empty JSON destination table
	RNS::Bytes emptyData("{}");
	TEST_ASSERT_EQUAL_size_t(2, RNS::Utilities::OS::write_file(test_empty_destination_table_path, emptyData));

	std::map<RNS::Bytes, RNS::Transport::DestinationEntry> destination_table;
	RNS::Persistence::deserialize(destination_table, test_empty_destination_table_path);
	TEST_ASSERT_EQUAL_size_t(0, destination_table.size());
}


void setUp(void) {
	// set stuff up here before each test
}

void tearDown(void) {
	// clean stuff up here after each test
}

int runUnityTests(void) {
	UNITY_BEGIN();

	// Suite-level setup
	RNS::FileSystem persistence_filesystem = new FileSystem();
	((FileSystem*)persistence_filesystem.get())->init();
	RNS::Utilities::OS::register_filesystem(persistence_filesystem);

	// Run tests
	RUN_TEST(testSerializeDestinationTable);
	RUN_TEST(testDeserializeDestinationTable);
	RUN_TEST(testDeserializeEmptyDestinationTable);

	// Suite-level teardown
	RNS::Utilities::OS::remove_file(test_destination_table_path);
	RNS::Utilities::OS::remove_file(test_empty_destination_table_path);
	RNS::Utilities::OS::deregister_filesystem();

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
