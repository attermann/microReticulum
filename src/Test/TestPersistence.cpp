//#include <unity.h>

#include "Transport.h"
#include "Interfaces/LoRaInterface.h"
#include "Utilities/Persistence.h"
#include "Utilities/OS.h"
#include "Log.h"
#include "Bytes.h"

#include <ArduinoJson.h>

#include <map>
#include <vector>
#include <string>
#include <assert.h>
#include <stdio.h>


class Test {
public:
    Test() {}
    Test(const char* foo, const char* fee) : _foo(foo), _fee(fee) {}
	const std::string toString() { return std::string("Test(") + _foo + "," + _fee + ")"; }
    std::string _foo;
    std::string _fee;
};

namespace ArduinoJson {
	template <>
	struct Converter<Test> {
		static bool toJson(const Test& src, JsonVariant dst) {
			dst["foo"] = src._foo;
			dst["fee"] = src._fee;
			return true;
		}

		static Test fromJson(JsonVariantConst src) {
			return Test(src["foo"], src["fee"]);
		}

		static bool checkJson(JsonVariantConst src) {
			return src["foo"].is<std::string>() && src["fee"].is<std::string>();
		}
	};
}


#ifdef ARDUINO
const char test_file_path[] = "/test_file";
#else
const char test_file_path[] = "test_file";
#endif
const char test_file_data[] = "test data";

void testWrite() {
	RNS::Bytes data(test_file_data);
	if (RNS::Utilities::OS::write_file(data, test_file_path)) {
		RNS::extreme("wrote: " + std::to_string(data.size()) + " bytes");
	}
	else {
		RNS::extreme("write failed");
		assert(false);
	}
}

void testRead() {
	RNS::Bytes data = RNS::Utilities::OS::read_file(test_file_path);
	if (data) {
		RNS::extreme("read: " + std::to_string(data.size()) + " bytes");
		RNS::extreme("data: " + data.toString());
		assert(data.size() == strlen(test_file_data));
		assert(memcmp(data.data(), test_file_data, strlen(test_file_data)) == 0);
		//assert(RNS::Utilities::OS::remove_file(test_file_path));
	}
	else {
		RNS::extreme("read failed");
		assert(false);
	}
}


#ifdef ARDUINO
const char test_object_path[] = "/test_object";
#else
const char test_object_path[] = "test_object";
#endif

void testSerializeObject() {

	Test test("bar", "fum");

	StaticJsonDocument<1024> doc;
	doc["foo"] = test._foo;
	doc["fee"] = test._fee;

	RNS::Bytes data;
	size_t size = 1024;
	//size_t length = serializeJson(doc, data.writable(size), size);
	size_t length = serializeMsgPack(doc, data.writable(size), size);
	if (length < size) {
		data.resize(length);
	}
	RNS::extreme("serialized " + std::to_string(length) + " bytes");
	if (length > 0) {
		if (RNS::Utilities::OS::write_file(data, test_object_path)) {
			RNS::extreme("wrote: " + std::to_string(data.size()) + " bytes");
		}
		else {
			RNS::extreme("write failed");
			assert(false);
		}
	}
	else {
		RNS::extreme("failed to serialize");
		assert(false);
	}

}

void testDeserializeObject() {

	//StaticJsonDocument<8192> doc;
	DynamicJsonDocument doc(8192);

	RNS::Bytes data = RNS::Utilities::OS::read_file(test_object_path);
	if (data) {
		RNS::extreme("read: " + std::to_string(data.size()) + " bytes");
		//RNS::extreme("data: " + data.toString());
		//DeserializationError error = deserializeJson(doc, data.data());
		DeserializationError error = deserializeMsgPack(doc, data.data());
		if (!error) {
			JsonObject root = doc.as<JsonObject>();
			for (JsonPair kv : root) {
				RNS::extreme("key: " + std::string(kv.key().c_str()) + " value: " + kv.value().as<const char*>());
			}
		}
		else {
			RNS::extreme("failed to deserialize");
			assert(false);
		}
		//assert(RNS::Utilities::OS::remove_file(test_object_path));
	}
	else {
		RNS::extreme("read failed");
		assert(false);
	}

}


#ifdef ARDUINO
const char test_vector_path[] = "/test_vector";
#else
const char test_vector_path[] = "test_vector";
#endif

void testSerializeVector() {

	//std::vector<std::string> vector;
	//vector.push_back("foo");
	//vector.push_back("bar");
	std::vector<Test> vector;
	vector.push_back({"bar1", "fum1"});
	vector.push_back({"bar2", "fum2"});

	StaticJsonDocument<4096> doc;
	//copyArray(vector, doc.createNestedArray("vector"));
	//doc["vector"] = vector;
	//copyArray(vector, doc);
	doc = vector;

/*
	DynamicJsonBuffer jsonBuffer;
	JsonObject& root = jsonBuffer.createObject();

	//JsonObject& weather = root.createNestedObject("weather");
	//weather["temperature"] = 12;
	//weather["condition"] = "cloudy";

	//JsonArray& coords = root.createNestedArray("coords");
	//coords.add(48.7507371, 7);
	//coords.add(2.2625587, 7);
	JsonArray& arr = root.createNestedArray("vec");
	for (auto entry : vec) {
		arr.add(
	}
*/

	RNS::Bytes data;
	size_t size = 4096;
	size_t length = serializeJson(doc, data.writable(size), size);
	//size_t length = serializeMsgPack(doc, data.writable(size), size);
	if (length < size) {
		data.resize(length);
	}
	RNS::extreme("testSerializeVector: serialized " + std::to_string(length) + " bytes");
	if (length > 0) {
		if (RNS::Utilities::OS::write_file(data, test_vector_path)) {
			RNS::extreme("testSerializeVector: wrote: " + std::to_string(data.size()) + " bytes");
		}
		else {
			RNS::extreme("testSerializeVector: write failed");
			assert(false);
		}
	}
	else {
		RNS::extreme("testSerializeVector: failed to serialize");
		assert(false);
	}

}

void testDeserializeVector() {

	// Compute the required size
	const int capacity =
		JSON_ARRAY_SIZE(2) +
		2*JSON_OBJECT_SIZE(3) +
		4*JSON_OBJECT_SIZE(1);

	//StaticJsonDocument<8192> doc;
	DynamicJsonDocument doc(8192);

	RNS::Bytes data = RNS::Utilities::OS::read_file(test_vector_path);
	if (data) {
		RNS::extreme("testDeserializeVector: read: " + std::to_string(data.size()) + " bytes");
		//RNS::extreme("testDeserializeVector: data: " + data.toString());
		DeserializationError error = deserializeJson(doc, data.data());
		//DeserializationError error = deserializeMsgPack(doc, data.data());
		if (!error) {
			RNS::extreme("testDeserializeVector: successfully deserialized");

			std::vector<Test> vector = doc.as<std::vector<Test>>();
			for (auto& test : vector) {
				RNS::extreme("testDeserializeVector: entry: " + test.toString());
			}

			//JsonArray arr = doc.as<JsonArray>();
			//for (JsonVariant elem : arr) {
			//	RNS::extreme("testDeserializeVector: entry: " + elem.as<Test>().toString());
			//}
		}
		else {
			RNS::extreme("testDeserializeVector: failed to deserialize");
			assert(false);
		}
		//assert(RNS::Utilities::OS::remove_file(test_vector_path));
	}
	else {
		RNS::extreme("testDeserializeVector: read failed");
		assert(false);
	}

}


#ifdef ARDUINO
const char test_map_path[] = "/test_map";
#else
const char test_map_path[] = "test_map";
#endif

void testSerializeMap() {

	//std::map<std::string, std::string> map;
	//map.insert({"foo", "bar"});
	//map.insert({"fee", "fum"});
	std::map<std::string, Test> map;
	map.insert({"one", {"bar1", "fum1"}});
	map.insert({"two", {"bar2", "fum2"}});

	StaticJsonDocument<4096> doc;
	//copyArray(map, doc.createNestedArray("map"));
	//doc["map"] = map;
	//copyArray(map, doc);
	doc = map;

	RNS::Bytes data;
	size_t size = 4096;
	size_t length = serializeJson(doc, data.writable(size), size);
	//size_t length = serializeMsgPack(doc, data.writable(size), size);
	if (length < size) {
		data.resize(length);
	}
	RNS::extreme("testSerializeMap: serialized " + std::to_string(length) + " bytes");
	if (length > 0) {
		if (RNS::Utilities::OS::write_file(data, test_map_path)) {
			RNS::extreme("testSerializeMap: wrote: " + std::to_string(data.size()) + " bytes");
		}
		else {
			RNS::extreme("testSerializeMap: write failed");
			assert(false);
		}
	}
	else {
		RNS::extreme("testSerializeMap: failed to serialize");
		assert(false);
	}

}

void testDeserializeMap() {

	// Compute the required size
	const int capacity =
		JSON_ARRAY_SIZE(2) +
		2*JSON_OBJECT_SIZE(3) +
		4*JSON_OBJECT_SIZE(1);

	//StaticJsonDocument<8192> doc;
	DynamicJsonDocument doc(8192);

	RNS::Bytes data = RNS::Utilities::OS::read_file(test_map_path);
	if (data) {
		RNS::extreme("testDeserializeMap: read: " + std::to_string(data.size()) + " bytes");
		//RNS::extreme("testDeserializeMap: data: " + data.toString());
		DeserializationError error = deserializeJson(doc, data.data());
		//DeserializationError error = deserializeMsgPack(doc, data.data());
		if (!error) {
			RNS::extreme("testDeserializeMap: successfully deserialized");

			std::map<std::string, Test> map(doc.as<std::map<std::string, Test>>());
			for (auto& [str, test] : map) {
				RNS::extreme("testDeserializeMap: entry: " + str + " = " + test.toString());
			}

			//JsonObject root = doc.as<JsonObject>();
			//for (JsonPair kv : root) {
			//	RNS::extreme("testDeserializeMap: entry: " + std::string(kv.key().c_str()) + " = " + kv.value().as<Test>().toString());
			//}
		}
		else {
			RNS::extreme("testDeserializeMap: failed to deserialize");
			assert(false);
		}
		//assert(RNS::Utilities::OS::remove_file(test_map_path));
	}
	else {
		RNS::extreme("testDeserializeMap: testDeserializeVector: read failed");
		assert(false);
	}

}

#ifdef ARDUINO
const char test_destination_table_path[] = "/test_destination_table";
#else
const char test_destination_table_path[] = "test_destination_table";
#endif

void testSerializeDestinationTable() {

	//RNS::Bytes empty;
	//std::set<RNS::Bytes> blobs;
	//RNS::Interface interface({RNS::Type::NONE});
	RNS::Interfaces::LoRaInterface lora_interface; 
	//RNS::Packet packet({RNS::Type::NONE});
	static std::map<RNS::Bytes, RNS::Transport::DestinationEntry> map;
	//DestinationEntry(double time, const Bytes& received_from, uint8_t announce_hops, double expires, const std::set<Bytes>& random_blobs, Interface& receiving_interface, const Packet& packet) :
	//RNS::Transport::DestinationEntry entry_one(1.0, empty, 1, 0.0, blobs, interface, packet);
	RNS::Bytes received;
	received.assignHex("deadbeef");
	RNS::Transport::DestinationEntry entry_one;
	entry_one._timestamp = 1.0;
	entry_one._received_from = received;
	entry_one._receiving_interface = lora_interface;
	RNS::Bytes one;
	one.assignHex("1111111111111111");
	map.insert({one, entry_one});
	//RNS::Transport::DestinationEntry entry_two(2.0, empty, 1, 0.0, blobs, interface, packet);
	RNS::Transport::DestinationEntry entry_two;
	entry_two._timestamp = 2.0;
	entry_two._received_from = received;
	entry_two._receiving_interface = lora_interface;
	RNS::Bytes two;
	two.assignHex("2222222222222222");
	map.insert({two, entry_two});

	for (auto& [hash, test] : map) {
		RNS::extreme("testSerializeDestinationTable: entry: " + hash.toHex() + " = (" + std::to_string(test._timestamp) + "," + test._received_from.toHex() + ")");
	}

	StaticJsonDocument<4096> doc;
	doc = map;

	RNS::Bytes data;
	size_t size = 4096;
	size_t length = serializeJson(doc, data.writable(size), size);
	//size_t length = serializeMsgPack(doc, data.writable(size), size);
	if (length < size) {
		data.resize(length);
	}
	RNS::extreme("testSerializeDestinationTable: serialized " + std::to_string(length) + " bytes");
	if (length > 0) {
		if (RNS::Utilities::OS::write_file(data, test_destination_table_path)) {
			RNS::extreme("testSerializeDestinationTable: wrote: " + std::to_string(data.size()) + " bytes");
		}
		else {
			RNS::extreme("testSerializeDestinationTable: write failed");
			assert(false);
		}
	}
	else {
		RNS::extreme("testSerializeDestinationTable: failed to serialize");
		assert(false);
	}

}

void testDeserializeDestinationTable() {

	// Compute the required size
	const int capacity =
		JSON_ARRAY_SIZE(2) +
		2*JSON_OBJECT_SIZE(3) +
		4*JSON_OBJECT_SIZE(1);

	//StaticJsonDocument<8192> doc;
	DynamicJsonDocument doc(8192);

	RNS::Bytes data = RNS::Utilities::OS::read_file(test_destination_table_path);
	if (data) {
		RNS::extreme("testDeserializeDestinationTable: read: " + std::to_string(data.size()) + " bytes");
		//RNS::extreme("testDeserializeVector: data: " + data.toString());
		DeserializationError error = deserializeJson(doc, data.data());
		//DeserializationError error = deserializeMsgPack(doc, data.data());
		if (!error) {
			RNS::extreme("testDeserializeDestinationTable: successfully deserialized");

			static std::map<RNS::Bytes, RNS::Transport::DestinationEntry> map(doc.as<std::map<RNS::Bytes, RNS::Transport::DestinationEntry>>());
			for (auto& [hash, test] : map) {
				RNS::extreme("testDeserializeDestinationTable: entry: " + hash.toHex() + " = (" + std::to_string(test._timestamp) + "," + test._received_from.toHex() + ")");
			}

			//JsonObject root = doc.as<JsonObject>();
			//for (JsonPair kv : root) {
			//	RNS::extreme("testDeserializeDestinationTable: entry: " + std::string(kv.key().c_str()) + " = " + kv.value().as<Test>().toString());
			//}
		}
		else {
			RNS::extreme("testDeserializeDestinationTable: failed to deserialize");
			assert(false);
		}
		//assert(RNS::Utilities::OS::remove_file(test_destination_table_path));
	}
	else {
		RNS::extreme("testDeserializeDestinationTable: read failed");
		assert(false);
	}

}


void testPersistence() {
	RNS::head("Running testPersistence...", RNS::LOG_EXTREME);
	testWrite();
	testRead();
	testSerializeObject();
	testDeserializeObject();
	testSerializeVector();
	testDeserializeVector();
	testSerializeMap();
	testDeserializeMap();
	testSerializeDestinationTable();
	testDeserializeDestinationTable();
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
	RUN_TEST(testCollections);
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
