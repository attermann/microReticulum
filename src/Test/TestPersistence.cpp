//#include <unity.h>

#include "Transport.h"
#include "Log.h"
#include "Bytes.h"
#include "Interfaces/LoRaInterface.h"
#include "Utilities/OS.h"

#include "Utilities/Persistence.h"

//#include <ArduinoJson.h>

#include <map>
#include <vector>
#include <set>
#include <string>
#include <assert.h>
#include <stdio.h>


class TestObject {
public:
    TestObject() {}
    TestObject(const char* str, std::vector<std::string>& vec) : _str(str), _vec(vec) {}
	inline bool operator < (const TestObject& test) const { return _str < test._str; }
	inline const std::string toString() const { return std::string("TestObject(") + _str + "," + std::to_string(_vec.size()) + ")"; }
    std::string _str;
    std::vector<std::string> _vec;
};

namespace ArduinoJson {
	// ArduinoJSON serialization support for TestObject
	template <>
	struct Converter<TestObject> {
		static bool toJson(const TestObject& src, JsonVariant dst) {
			dst["str"] = src._str;
			dst["vec"] = src._vec;
			return true;
		}
		static TestObject fromJson(JsonVariantConst src) {
			TestObject dst;
			dst._str = src["str"].as<std::string>();
			dst._vec = src["vec"].as<std::vector<std::string>>();
			return dst;
		}
		static bool checkJson(JsonVariantConst src) {
			return src["str"].is<std::string>() && src["vec"].is<std::vector<std::string>>();
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

/*
void testSerializeObject() {

	Test test("bar", "fum");

	StaticJsonDocument<1024> doc;
	doc = test;

	RNS::Bytes data;
	size_t size = 1024;
	size_t length = serializeJson(doc, data.writable(size), size);
	//size_t length = serializeMsgPack(doc, data.writable(size), size);
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
		DeserializationError error = deserializeJson(doc, data.data());
		//DeserializationError error = deserializeMsgPack(doc, data.data());
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
*/
void testSerializeObject() {

	std::vector<std::string> vec({"one", "two"});
	TestObject test("test", vec);
	RNS::extreme("serializeObject: test: " + test.toString());

	StaticJsonDocument<1024> doc;
	doc = test;

	RNS::Bytes data;
	size_t size = 1024;
	size_t length = serializeJson(doc, data.writable(size), size);
	//size_t length = serializeMsgPack(doc, data.writable(size), size);
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
		DeserializationError error = deserializeJson(doc, data.data());
		//DeserializationError error = deserializeMsgPack(doc, data.data());
		if (!error) {
			//JsonObject root = doc.as<JsonObject>();
			//for (JsonPair kv : root) {
			//	RNS::extreme("key: " + std::string(kv.key().c_str()) + " value: " + kv.value().as<const char*>());
			//}
			TestObject test = doc.as<TestObject>();
			RNS::extreme("testDeserializeObject: test: " + test.toString());
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

	std::vector<std::string> tvec;
	tvec.push_back("foo");
	tvec.push_back("bar");
	std::vector<TestObject> vector;
	vector.push_back({"two", tvec});
	vector.push_back({"one", tvec});

	StaticJsonDocument<4096> doc;
	//copyArray(vector, doc.createNestedArray("vector"));
	//doc["vector"] = vector;
	//copyArray(vector, doc);
	doc = vector;

	//JsonVariant dst;
	//dst = vector;

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

			std::vector<TestObject> vector = doc.as<std::vector<TestObject>>();
			for (auto& test : vector) {
				RNS::extreme("testDeserializeVector: entry: " + test.toString());
			}

			//JsonArray arr = doc.as<JsonArray>();
			//for (JsonVariant elem : arr) {
			//	RNS::extreme("testDeserializeVector: entry: " + elem.as<TestObject>().toString());
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
const char test_set_path[] = "/test_set";
#else
const char test_set_path[] = "test_set";
#endif

void testSerializeSet() {

	std::vector<std::string> tvec;
	tvec.push_back("foo");
	tvec.push_back("bar");
	std::set<TestObject> set;
	set.insert({"two", tvec});
	set.insert({"one", tvec});

	StaticJsonDocument<4096> doc;
	//copyArray(set, doc.createNestedArray("set"));
	//doc["set"] = set;
	//copyArray(set, doc);
	//doc = set;

	//std::string test;
	//RNS::Bytes test;
	//std::set<int> test;
	std::set<RNS::Bytes> test;
	//std::map<std::string, std::string> test;
	JsonVariant dst;
	dst["test"] = test;

	RNS::Bytes data;
	size_t size = 4096;
	size_t length = serializeJson(doc, data.writable(size), size);
	//size_t length = serializeMsgPack(doc, data.writable(size), size);
	if (length < size) {
		data.resize(length);
	}
	RNS::extreme("testSerializeSet: serialized " + std::to_string(length) + " bytes");
	if (length > 0) {
		if (RNS::Utilities::OS::write_file(data, test_set_path)) {
			RNS::extreme("testSerializeSet: wrote: " + std::to_string(data.size()) + " bytes");
		}
		else {
			RNS::extreme("testSerializeSet: write failed");
			assert(false);
		}
	}
	else {
		RNS::extreme("testSerializeSet: failed to serialize");
		assert(false);
	}

}

void testDeserializeSet() {

	// Compute the required size
	const int capacity =
		JSON_ARRAY_SIZE(2) +
		2*JSON_OBJECT_SIZE(3) +
		4*JSON_OBJECT_SIZE(1);

	//StaticJsonDocument<8192> doc;
	DynamicJsonDocument doc(8192);

	RNS::Bytes data = RNS::Utilities::OS::read_file(test_set_path);
	if (data) {
		RNS::extreme("testDeserializeSet: read: " + std::to_string(data.size()) + " bytes");
		//RNS::extreme("testDeserializeSet: data: " + data.toString());
		DeserializationError error = deserializeJson(doc, data.data());
		//DeserializationError error = deserializeMsgPack(doc, data.data());
		if (!error) {
			RNS::extreme("testDeserializeSet: successfully deserialized");

			std::set<TestObject> set = doc.as<std::set<TestObject>>();
			for (auto& test : set) {
				RNS::extreme("testDeserializeSet: entry: " + test.toString());
			}

			//JsonArray arr = doc.as<JsonArray>();
			//for (JsonVariant elem : arr) {
			//	RNS::extreme("testDeserializeSet: entry: " + elem.as<TestObject>().toString());
			//}
		}
		else {
			RNS::extreme("testDeserializeSet: failed to deserialize");
			assert(false);
		}
		//assert(RNS::Utilities::OS::remove_file(test_set_path));
	}
	else {
		RNS::extreme("testDeserializeSet: read failed");
		assert(false);
	}

}


#ifdef ARDUINO
const char test_map_path[] = "/test_map";
#else
const char test_map_path[] = "test_map";
#endif

void testSerializeMap() {

	std::vector<std::string> tvec;
	tvec.push_back("foo");
	tvec.push_back("bar");
	std::map<std::string, TestObject> map;
	map.insert({"two", {"two", tvec}});
	map.insert({"one", {"one", tvec}});

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

			std::map<std::string, TestObject> map(doc.as<std::map<std::string, TestObject>>());
			for (auto& [str, test] : map) {
				RNS::extreme("testDeserializeMap: entry: " + str + " = " + test.toString());
			}

			//JsonObject root = doc.as<JsonObject>();
			//for (JsonPair kv : root) {
			//	RNS::extreme("testDeserializeMap: entry: " + std::string(kv.key().c_str()) + " = " + kv.value().as<TestObject>().toString());
			//}
		}
		else {
			RNS::extreme("testDeserializeMap: failed to deserialize");
			assert(false);
		}
		//assert(RNS::Utilities::OS::remove_file(test_map_path));
	}
	else {
		RNS::extreme("testDeserializeMap: read failed");
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
	RNS::Bytes blob;
	blob.assignHex("b10bb10b");
	std::set<RNS::Bytes> blobs({received, blob});
	RNS::Transport::DestinationEntry entry_one;
	entry_one._timestamp = 1.0;
	entry_one._received_from = received;
	entry_one._receiving_interface = lora_interface;
	entry_one._random_blobs = blobs;
	RNS::Bytes one;
	one.assignHex("1111111111111111");
	map.insert({one, entry_one});
	//RNS::Transport::DestinationEntry entry_two(2.0, empty, 1, 0.0, blobs, interface, packet);
	RNS::Transport::DestinationEntry entry_two;
	entry_two._timestamp = 2.0;
	entry_two._received_from = received;
	entry_two._receiving_interface = lora_interface;
	entry_two._random_blobs = blobs;
	RNS::Bytes two;
	two.assignHex("2222222222222222");
	map.insert({two, entry_two});

	for (auto& [hash, test] : map) {
		RNS::extreme("testSerializeDestinationTable: entry: " + hash.toHex() + " = " + test.debugString());
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
				RNS::extreme("testDeserializeDestinationTable: entry: " + hash.toHex() + " = " + test.debugString());
			}

			//JsonObject root = doc.as<JsonObject>();
			//for (JsonPair kv : root) {
			//	RNS::extreme("testDeserializeDestinationTable: entry: " + std::string(kv.key().c_str()) + " = " + kv.value().as<TestObject>().toString());
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
	testSerializeSet();
	testDeserializeSet();
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
