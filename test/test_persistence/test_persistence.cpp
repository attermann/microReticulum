#include <unity.h>

#include <microStore/Adapters/UniversalFileSystem.h>

#include <Interface.h>
#include <Transport.h>
#include <Log.h>
#include <Bytes.h>
#include <Utilities/OS.h>
#include <Utilities/Persistence.h>

//#include <ArduinoJson.h>
#include <MsgPack.h>

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
		DEBUGF("TestInterface.send_outgoing: data: %s", data.toHex().c_str());
		// Perform post-send housekeeping
		InterfaceImpl::handle_outgoing(data);
	}
	void on_incoming(const RNS::Bytes& data) {
		DEBUGF("TestInterface.on_incoming: data: %s", data.toHex().c_str());
		// Pass received data on to transport
		InterfaceImpl::handle_incoming(data);
	}

};


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
	if (RNS::Utilities::OS::write_file(test_file_path, data) == data.size()) {
		TRACEF("wrote: %lu bytes", data.size());
	}
	else {
		TRACE("write failed");
		TEST_FAIL();
	}
}

void testRead() {
	RNS::Bytes data;
	if (RNS::Utilities::OS::read_file(test_file_path, data) > 0) {
		TRACEF("read: %lu bytes", data.size());
		TRACEF("data: %s", data.toString().c_str());
		TEST_ASSERT_EQUAL_size_t(strlen(test_file_data), data.size());
		TEST_ASSERT_NOT_NULL(data.data());
		TEST_ASSERT_EQUAL_MEMORY(test_file_data, data.data(), strlen(test_file_data));
		//assert(RNS::Utilities::OS::remove_file(test_file_path));
	}
	else {
		TRACE("read failed");
		TEST_FAIL();
	}
}


#ifdef ARDUINO
const char test_object_path[] = "/test_object";
const char test_array_path[] = "/test_array";
const char test_series_path[] = "/test_series";
#else
const char test_object_path[] = "test_object";
const char test_array_path[] = "test_array";
const char test_series_path[] = "test_series";
#endif

/*
void testSerializeObject() {

	Test test("bar", "fum");

	StaticJsonDocument<1024> doc;
	doc.set(test);

	RNS::Bytes data;
	size_t size = 1024;
	size_t length = serializeJson(doc, data.writable(size), size);
	//size_t length = serializeMsgPack(doc, data.writable(size), size);
	if (length < size) {
		data.resize(length);
	}
	TRACEF("serialized %lu bytes", length);
	if (length > 0) {
		if (RNS::Utilities::OS::write_file(test_object_path, data) == data.size()) {
			TRACEF("wrote: %lu bytes", data.size());
		}
		else {
			TRACE("write failed");
			TEST_FAIL();
		}
	}
	else {
		TRACE("failed to serialize");
		TEST_FAIL();
	}

}

void testDeserializeObject() {

	//StaticJsonDocument<8192> doc;
	DynamicJsonDocument doc(1024);

	RNS::Bytes data;
	if (RNS::Utilities::OS::read_file(test_object_path, data) > 0) {
	if (data) {
		TRACEF("read: %lu bytes", data.size());
		//TRACEF("data: %s", data.toString().c_str());
		DeserializationError error = deserializeJson(doc, data.data());
		//DeserializationError error = deserializeMsgPack(doc, data.data());
		if (!error) {
			JsonObject root = doc.as<JsonObject>();
			for (JsonPair kv : root) {
				TRACEF("key: %s value: %s", kv.key().c_str(), kv.value().as<const char*>());
			}
		}
		else {
			TRACE("testDeserializeObject: failed to deserialize");
			TEST_FAIL();
		}
		//assert(RNS::Utilities::OS::remove_file(test_object_path));
	}
	else {
		TRACE("read failed");
		TEST_FAIL();
	}

}
*/
void testSerializeObject() {

	std::vector<std::string> vec({"one", "two"});
	TestObject test("test", vec);
	TRACEF("testSerializeObject: test: %s", test.toString().c_str());

	JsonDocument doc;
	doc.set(test);

	RNS::Bytes data;
	size_t size = 1024;
	size_t length = serializeJson(doc, data.writable(size), size);
	//size_t length = serializeMsgPack(doc, data.writable(size), size);
	if (length < size) {
		data.resize(length);
	}
	TRACEF("testSerializeObject: serialized %lu bytes", length);
	if (length > 0) {
		if (RNS::Utilities::OS::write_file(test_object_path, data) == data.size()) {
			TRACEF("testSerializeObject: wrote: %lu bytes", data.size());
		}
		else {
			TRACE("testSerializeObject: write failed");
			TEST_FAIL();
		}
	}
	else {
		TRACE("testSerializeObject: failed to serialize");
		TEST_FAIL();
	}

}

void testDeserializeObject() {

	JsonDocument doc;

	RNS::Bytes data;
	if (RNS::Utilities::OS::read_file(test_object_path, data) > 0) {
		TRACEF("testDeserializeObject: read: %lu bytes", data.size());
		//TRACEF("data: %s", data.toString().c_str());
		DeserializationError error = deserializeJson(doc, data.data());
		//DeserializationError error = deserializeMsgPack(doc, data.data());
		if (!error) {
			//JsonObject root = doc.as<JsonObject>();
			//for (JsonPair kv : root) {
			//	TRACEF("key: %s value: %s", kv.key().c_str(), kv.value().as<const char*>());
			//}
			TestObject test = doc.as<TestObject>();
			TRACEF("testDeserializeObject: test: %s", test.toString().c_str());
		}
		else {
			TRACE("testDeserializeObject: failed to deserialize");
			TEST_FAIL();
		}
		//assert(RNS::Utilities::OS::remove_file(test_object_path));
	}
	else {
		TRACE("testDeserializeObject: read failed");
		TEST_FAIL();
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

	JsonDocument doc;
	//copyArray(vector, doc.createNestedArray("vector"));
	//doc["vector"] = vector;
	//copyArray(vector, doc);
	doc.set(vector);

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
	TRACEF("testSerializeVector: serialized %lu bytes", length);
	if (length > 0) {
		if (RNS::Utilities::OS::write_file(test_vector_path, data) == data.size()) {
			TRACEF("testSerializeVector: wrote: %lu bytes", data.size());
		}
		else {
			TRACE("testSerializeVector: write failed");
			TEST_FAIL();
		}
	}
	else {
		TRACE("testSerializeVector: failed to serialize");
		TEST_FAIL();
	}

}

void testDeserializeVector() {

	JsonDocument doc;

	RNS::Bytes data;
	if (RNS::Utilities::OS::read_file(test_vector_path, data) > 0) {
		TRACEF("testDeserializeVector: read: %lu bytes", data.size());
		//TRACEF("testDeserializeVector: data: %s", data.toString().c_str());
		DeserializationError error = deserializeJson(doc, data.data());
		//DeserializationError error = deserializeMsgPack(doc, data.data());
		if (!error) {
			TRACE("testDeserializeVector: successfully deserialized");

			std::vector<TestObject> vector = doc.as<std::vector<TestObject>>();
			for (auto& test : vector) {
				TRACEF("testDeserializeVector: entry: %s", test.toString().c_str());
			}

			//JsonArray arr = doc.as<JsonArray>();
			//for (JsonVariant elem : arr) {
			//	TRACEF("testDeserializeVector: entry: %s", elem.as<TestObject>().toString().c_str());
			//}
		}
		else {
			TRACE("testDeserializeVector: failed to deserialize");
			TEST_FAIL();
		}
		//assert(RNS::Utilities::OS::remove_file(test_vector_path));
	}
	else {
		TRACE("testDeserializeVector: read failed");
		TEST_FAIL();
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

	JsonDocument doc;
	//copyArray(set, doc.createNestedArray("set"));
	//doc["set"] = set;
	//copyArray(set, doc);
	//doc.set(set);

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
	TRACEF("testSerializeSet: serialized %lu bytes", length);
	if (length > 0) {
		if (RNS::Utilities::OS::write_file(test_set_path, data) == data.size()) {
			TRACEF("testSerializeSet: wrote: %lu bytes", data.size());
		}
		else {
			TRACE("testSerializeSet: write failed");
			TEST_FAIL();
		}
	}
	else {
		TRACE("testSerializeSet: failed to serialize");
		TEST_FAIL();
	}

}

void testDeserializeSet() {

	JsonDocument doc;

	RNS::Bytes data;
	if (RNS::Utilities::OS::read_file(test_set_path, data) > 0) {
		TRACEF("testDeserializeSet: read: %lu bytes", data.size());
		//TRACEF("testDeserializeSet: data: %s", data.toString().c_str());
		DeserializationError error = deserializeJson(doc, data.data());
		//DeserializationError error = deserializeMsgPack(doc, data.data());
		if (!error) {
			TRACE("testDeserializeSet: successfully deserialized");

			std::set<TestObject> set = doc.as<std::set<TestObject>>();
			for (auto& test : set) {
				TRACEF("testDeserializeSet: entry: %s", test.toString().c_str());
			}

			//JsonArray arr = doc.as<JsonArray>();
			//for (JsonVariant elem : arr) {
			//	TRACEF("testDeserializeSet: entry: %s", elem.as<TestObject>().toString().c_str());
			//}
		}
		else {
			TRACE("testDeserializeSet: failed to deserialize");
			TEST_FAIL();
		}
		//assert(RNS::Utilities::OS::remove_file(test_set_path));
	}
	else {
		TRACE("testDeserializeSet: read failed");
		TEST_FAIL();
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

	JsonDocument doc;
	//copyArray(map, doc.createNestedArray("map"));
	//doc["map"] = map;
	//copyArray(map, doc);
	doc.set(map);

	RNS::Bytes data;
	size_t size = 4096;
	size_t length = serializeJson(doc, data.writable(size), size);
	//size_t length = serializeMsgPack(doc, data.writable(size), size);
	if (length < size) {
		data.resize(length);
	}
	TRACEF("testSerializeMap: serialized %lu bytes", length);
	if (length > 0) {
		if (RNS::Utilities::OS::write_file(test_map_path, data) == data.size()) {
			TRACEF("testSerializeMap: wrote: %lu bytes", data.size());
		}
		else {
			TRACE("testSerializeMap: write failed");
			TEST_FAIL();
		}
	}
	else {
		TRACE("testSerializeMap: failed to serialize");
		TEST_FAIL();
	}

}

void testDeserializeMap() {

	JsonDocument doc;

	RNS::Bytes data;
	if (RNS::Utilities::OS::read_file(test_map_path, data) > 0) {
		TRACEF("testDeserializeMap: read: %lu bytes", data.size());
		//TRACEF("testDeserializeMap: data: %s", data.toString().c_str());
		DeserializationError error = deserializeJson(doc, data.data());
		//DeserializationError error = deserializeMsgPack(doc, data.data());
		if (!error) {
			TRACE("testDeserializeMap: successfully deserialized");

			std::map<std::string, TestObject> map(doc.as<std::map<std::string, TestObject>>());
			for (auto& [str, test] : map) {
				TRACEF("testDeserializeMap: entry: %s = %s", str.c_str(), test.toString().c_str());
			}

			//JsonObject root = doc.as<JsonObject>();
			//for (JsonPair kv : root) {
			//	TRACEF("testDeserializeMap: entry: %s = %s", kv.key().c_str(), kv.value().as<TestObject>().toString().c_str());
			//}
		}
		else {
			TRACE("testDeserializeMap: failed to deserialize");
			TEST_FAIL();
		}
		//assert(RNS::Utilities::OS::remove_file(test_map_path));
	}
	else {
		TRACE("testDeserializeMap: read failed");
		TEST_FAIL();
	}

}

#ifdef ARDUINO
const char test_path_table_path[] = "/test_path_table";
const char test_empty_path_table_path[] = "/test_empty_path_table";
#else
const char test_path_table_path[] = "test_path_table";
const char test_empty_path_table_path[] = "test_empty_path_table";
#endif

void testSerializeDestinationTable() {

	//RNS::Bytes empty;
	//std::set<RNS::Bytes> blobs;
	//RNS::Interface interface({RNS::Type::NONE});
	RNS::Interface test_interface(new TestInterface());
	//RNS::Packet packet({RNS::Type::NONE});
	static std::map<RNS::Bytes, RNS::Persistence::DestinationEntry> map;
	//DestinationEntry(double time, const Bytes& received_from, uint8_t announce_hops, double expires, const std::set<Bytes>& random_blobs, Interface& receiving_interface, const Packet& packet) :
	//RNS::Persistence::DestinationEntry entry_one(1.0, empty, 1, 0.0, blobs, interface, packet);
	RNS::Bytes received;
	received.assignHex("deadbeef");
	RNS::Bytes blob;
	blob.assignHex("b10bb10b");
	std::set<RNS::Bytes> blobs({received, blob});
	RNS::Persistence::DestinationEntry entry_one;
	entry_one._timestamp = 1.0;
	entry_one._received_from = received;
	entry_one._random_blobs = blobs;
	entry_one._receiving_interface = test_interface;
	RNS::Bytes one;
	one.assignHex("1111111111111111");
	map.insert({one, entry_one});
	//RNS::Persistence::DestinationEntry entry_two(2.0, empty, 1, 0.0, blobs, interface, packet);
	RNS::Persistence::DestinationEntry entry_two;
	entry_two._timestamp = 2.0;
	entry_two._received_from = received;
	entry_two._random_blobs = blobs;
	entry_two._receiving_interface = test_interface;
	RNS::Bytes two;
	two.assignHex("2222222222222222");
	map.insert({two, entry_two});

	for (int n = 0; n < 20; n++) {
		RNS::Persistence::DestinationEntry entry;
		entry._timestamp = 1.0;
		entry._received_from = received;
		entry._random_blobs = blobs;
		entry._receiving_interface = test_interface;
		RNS::Bytes hash;
		hash.assign(std::to_string(1000000000000001 + n).c_str());
		map.insert({hash, entry});
	}

	for (auto& [hash, test] : map) {
		TRACEF("testSerializeDestinationTable: entry: %s = %s", hash.toHex().c_str(), test.debugString().c_str());
	}

	JsonDocument doc;
	doc.set(map);
	TRACEF("testSerializeDestinationTable: doc size %lu", doc.size());

	RNS::Bytes data;
	size_t size = 32768;
	//size_t size = BUFFER_SIZE;
	TRACEF("testSerializeDestinationTable: buffer size %lu bytes", size);
	size_t length = serializeJson(doc, data.writable(size), size);
	//size_t length = serializeMsgPack(doc, data.writable(size), size);
	if (length < size) {
		data.resize(length);
	}
	TRACEF("testSerializeDestinationTable: serialized %lu bytes", length);
	if (length > 0) {
		TRACEF("testSerializeDestinationTable: json: %s", data.toString().c_str());
		if (RNS::Utilities::OS::write_file(test_path_table_path, data) == data.size()) {
			TRACEF("testSerializeDestinationTable: wrote: %lu bytes", data.size());
		}
		else {
			TRACE("testSerializeDestinationTable: write failed");
			TEST_FAIL();
		}
	}
	else {
		TRACE("testSerializeDestinationTable: failed to serialize");
		TEST_FAIL();
	}

}

void testDeserializeDestinationTable() {

	RNS::Bytes data;
	if (RNS::Utilities::OS::read_file(test_path_table_path, data) > 0) {
		TRACEF("testDeserializeDestinationTable: read: %lu bytes", data.size());
		TRACEF("testDeserializeDestinationTable: json: %s", data.toString().c_str());
		JsonDocument doc;

		//TRACEF("testDeserializeVector: data: %s", data.toString().c_str());
		DeserializationError error = deserializeJson(doc, data.data());
		//DeserializationError error = deserializeMsgPack(doc, data.data());
		TRACEF("testDeserializeDestinationTable: doc size %lu", doc.size());
		if (!error) {
			TRACEF("testDeserializeDestinationTable: successfully deserialized %lu bytes", data.size());
			//TRACEF("testDeserializeDestinationTable: json: %s", data.toString().c_str());

			static std::map<RNS::Bytes, RNS::Persistence::DestinationEntry> map(doc.as<std::map<RNS::Bytes, RNS::Persistence::DestinationEntry>>());
			TEST_ASSERT_EQUAL_size_t(22, map.size());
			for (auto& [hash, test] : map) {
				TRACEF("testDeserializeDestinationTable: entry: %s = %s", hash.toHex().c_str(), test.debugString().c_str());
			}

			//JsonObject root = doc.as<JsonObject>();
			//for (JsonPair kv : root) {
			//	TRACEF("testDeserializeDestinationTable: entry: %s = %s", kv.key().c_str(), kv.value().as<TestObject>().toString().c_str());
			//}
		}
		else {
			TRACEF("testDeserializeDestinationTable: failed to deserialize: %s", error.c_str());
			TEST_FAIL();
		}
		//assert(RNS::Utilities::OS::remove_file(test_path_table_path));
	}
	else {
		TRACE("testDeserializeDestinationTable: read failed");
		TEST_FAIL();
	}

}

void testDeserializeEmptyDestinationTable() {

	// Write empty JSON destination table
	RNS::Bytes emptyData("{}");
	TEST_ASSERT_EQUAL_size_t(2, RNS::Utilities::OS::write_file(test_empty_path_table_path, emptyData));

	RNS::Bytes data;
	if (RNS::Utilities::OS::read_file(test_empty_path_table_path, data) > 0) {
		TRACEF("testDeserializeEmptyDestinationTable: read: %lu bytes", data.size());
		JsonDocument doc;

		//TRACEF("testDeserializeVector: data: %s", data.toString().c_str());
		DeserializationError error = deserializeJson(doc, data.data());
		//DeserializationError error = deserializeMsgPack(doc, data.data());
		TRACEF("testDeserializeEmptyDestinationTable: doc size %lu", doc.size());
		if (!error) {
			TRACEF("testDeserializeEmptyDestinationTable: successfully deserialized %lu bytes", data.size());
			TRACEF("testDeserializeEmptyDestinationTable: json: %s", data.toString().c_str());

			static std::map<RNS::Bytes, RNS::Persistence::DestinationEntry> map(doc.as<std::map<RNS::Bytes, RNS::Persistence::DestinationEntry>>());
			TEST_ASSERT_EQUAL_size_t(0, map.size());
			for (auto& [hash, test] : map) {
				TRACEF("testDeserializeEmptyDestinationTable: entry: %s = %s", hash.toHex().c_str(), test.debugString().c_str());
			}

			//JsonObject root = doc.as<JsonObject>();
			//for (JsonPair kv : root) {
			//	TRACEF("testDeserializeEmptyDestinationTable: entry: %s = %s", kv.key().c_str(), kv.value().as<TestObject>().toString().c_str());
			//}
		}
		else {
			TRACE("testDeserializeEmptyDestinationTable: failed to deserialize");
			TEST_FAIL();
		}
		//assert(RNS::Utilities::OS::remove_file(test_empty_path_table_path));
	}
	else {
		TRACE("testDeserializeEmptyDestinationTable: read failed");
		TEST_FAIL();
	}

}


void testSerializeTimeOffset() {
	uint64_t offset = RNS::Utilities::OS::ltime();
	TRACEF("Writing time offset of %llu to file /time_offset", offset);
	RNS::Bytes buf((uint8_t*)&offset, sizeof(offset));
	RNS::Utilities::OS::write_file("/time_offset", buf);
}


class TestOtherObject {
public:
    TestOtherObject() {}
    TestOtherObject(const char* str, std::vector<std::string>& vec) : _str(str), _vec(vec) {}
	inline bool operator < (const TestOtherObject& test) const { return _str < test._str; }
	inline const std::string toString() const { return std::string("TestOtherObject(") + _str + "," + std::to_string(_vec.size()) + ")"; }
    std::string _str;
    std::vector<std::string> _vec;
};

struct TestStruct {
	int foo;
	int bar;
};


void testJsonMsgpackSerializeObject() {

	std::vector<std::string> vec({"one", "two"});
	TestObject test("test", vec);
	TRACEF("testJsonMsgpackSerializeObject: test: %s", test.toString().c_str());

	JsonDocument doc;
	doc.set(test);

	RNS::Bytes data;
	size_t size = 1024;
	size_t length = serializeMsgPack(doc, data.writable(size), size);
	if (length < size) {
		data.resize(length);
	}
	TRACEF("testJsonMsgpackSerializeObject: serialized %lu bytes", length);
	if (length > 0) {
		if (RNS::Utilities::OS::write_file(test_object_path, data) == data.size()) {
			TRACEF("testJsonMsgpackSerializeObject: wrote: %lu bytes", data.size());
		}
		else {
			TRACE("testJsonMsgpackSerializeObject: write failed");
			TEST_FAIL();
		}
	}
	else {
		TRACE("testJsonMsgpackSerializeObject: failed to serialize");
		TEST_FAIL();
	}

}

void testJsonMsgpackDeserializeObject() {

	JsonDocument doc;

	RNS::Bytes data;
	if (RNS::Utilities::OS::read_file(test_object_path, data) > 0) {
		TRACEF("testJsonMsgpackDeserializeObject: read: %lu bytes", data.size());
		//TRACEF("data: %s", data.toString().c_str());
		DeserializationError error = deserializeMsgPack(doc, data.data());
		if (!error) {
			//JsonObject root = doc.as<JsonObject>();
			//for (JsonPair kv : root) {
			//	TRACEF("key: %s value: %s", kv.key().c_str(), kv.value().as<const char*>());
			//}
			TestObject test = doc.as<TestObject>();
			TRACEF("testJsonMsgpackDeserializeObject: test: %s", test.toString().c_str());
		}
		else {
			TRACE("testJsonMsgpackDeserializeObject: failed to deserialize");
			TEST_FAIL();
		}
		//assert(RNS::Utilities::OS::remove_file(test_object_path));
	}
	else {
		TRACE("testJsonMsgpackDeserializeObject: read failed");
		TEST_FAIL();
	}

}

void testJsonMsgpackSerializeArray() {

	// NOTE: Following is creating a msgpack that appears in Python like:
	// [123.456, [102, 105, 114, 115, 116, 171], [115, 101, 99, 111, 110, 100, 205]]

	double time = 123.456;
	// CBA Need this version writing byte-arrays to match the python version also writing byte-arrays (25 bytes)
	const RNS::Bytes one("first\xab");
	const RNS::Bytes two("second\xcd");
	//TRACEF("testJsonMsgpackSerializeArray: time=%f one=%s two=%s", time, one.toString().c_str(), two.toString().c_str());
	// CBA This version writing strings exactly matches the python version also writing strings (23 bytes)
	//const std::string one("first");
	//const std::string two("second");
	//TRACEF("testJsonMsgpackSerializeArray: time=%f one=%s two=%s", time, one.c_str(), two.c_str());

	JsonDocument doc;

	JsonArray array = doc.to<JsonArray>();
	array.add(time);
	//array.add(one);
	//array.add(two);
	const JsonArray a_one = doc.createNestedArray();
	for (uint8_t byte : one.collection()) {
		a_one.add(byte);
	}
	const JsonArray a_two = doc.createNestedArray();
	for (uint8_t byte : two.collection()) {
		a_two.add(byte);
	}

	RNS::Bytes data;
	size_t size = 1024;
	size_t length = serializeMsgPack(doc, data.writable(size), size);
	if (length < size) {
		data.resize(length);
	}
	TRACEF("testJsonMsgpackSerializeArray: serialized %lu bytes", length);
	if (length > 0) {
		if (RNS::Utilities::OS::write_file(test_array_path, data) == data.size()) {
			TRACEF("testJsonMsgpackSerializeArray: wrote: %lu bytes", data.size());
		}
		else {
			TRACE("testJsonMsgpackSerializeArray: write failed");
			TEST_FAIL();
		}
	}
	else {
		TRACE("testJsonMsgpackSerializeArray: failed to serialize");
		TEST_FAIL();
	}

}

void testJsonMsgpackDeserializeArray() {

	JsonDocument doc;

	RNS::Bytes data;
	if (RNS::Utilities::OS::read_file(test_array_path, data) > 0) {
		TRACEF("testJsonMsgpackDeserializeArray: read: %lu bytes", data.size());
		//TRACEF("data: %s", data.toString().c_str());
		DeserializationError error = deserializeMsgPack(doc, data.data(), data.size());
		if (!error) {
			JsonArray array = doc.as<JsonArray>();
			double time = array[0].as<double>();
			//std::string one(array[1].as<std::string>());
			//RNS::Bytes one(array[1].as<RNS::Bytes>());
			RNS::Bytes one(array[1].as<std::vector<uint8_t>>());
			//std::string two(array[2].as<std::string>());
			//RNS::Bytes two(array[2].as<RNS::Bytes>());
			RNS::Bytes two(array[2].as<std::vector<uint8_t>>());
			TRACEF("testJsonMsgpackDeserializeArray: time=%f one=%s two=%s", time, one.toString().c_str(), two.toString().c_str());
			//TRACEF("testJsonMsgpackDeserializeArray: time=%f one=%s two=%s", time, one.c_str(), two.c_str());
			TEST_ASSERT_NOT_NULL(one.data());
			TEST_ASSERT_EQUAL_MEMORY("first\xab", one.data(), one.size());
			TEST_ASSERT_NOT_NULL(two.data());
			TEST_ASSERT_EQUAL_MEMORY("second\xcd", two.data(), two.size());
		}
		else {
			TRACE("testJsonMsgpackDeserializeArray: failed to deserialize");
			TEST_FAIL();
		}
		//assert(RNS::Utilities::OS::remove_file(test_array_path));
	}
	else {
		TRACE("testJsonMsgpackDeserializeArray: read failed");
		TEST_FAIL();
	}

}


void testMsgpackSerializeObject() {

	std::vector<std::string> vec({"one", "two"});
	TestObject test("test", vec);
	TRACEF("testMsgpackSerializeObject: test: %s", test.toString().c_str());

	JsonDocument doc;

	RNS::Bytes data;
	size_t size = 1024;
	size_t length = serializeMsgPack(doc, data.writable(size), size);
	if (length < size) {
		data.resize(length);
	}
	TRACEF("testMsgpackSerializeObject: serialized %lu bytes", length);
	if (length > 0) {
		if (RNS::Utilities::OS::write_file(test_object_path, data) == data.size()) {
			TRACEF("testMsgpackSerializeObject: wrote: %lu bytes", data.size());
		}
		else {
			TRACE("testMsgpackSerializeObject: write failed");
			TEST_FAIL();
		}
	}
	else {
		TRACE("testMsgpackSerializeObject: failed to serialize");
		TEST_FAIL();
	}

}

void testMsgpackDeserializeObject() {

	JsonDocument doc;

	RNS::Bytes data;
	if (RNS::Utilities::OS::read_file(test_object_path, data) > 0) {
		TRACEF("testMsgpackDeserializeObject: read: %lu bytes", data.size());
		//TRACEF("data: %s", data.toString().c_str());
		DeserializationError error = deserializeMsgPack(doc, data.data());
		if (!error) {
			//JsonObject root = doc.as<JsonObject>();
			//for (JsonPair kv : root) {
			//	TRACEF("key: %s value: %s", kv.key().c_str(), kv.value().as<const char*>());
			//}
			TestObject test = doc.as<TestObject>();
			TRACEF("testMsgpackDeserializeObject: test: %s", test.toString().c_str());
		}
		else {
			TRACE("testMsgpackDeserializeObject: failed to deserialize");
			TEST_FAIL();
		}
		//assert(RNS::Utilities::OS::remove_file(test_object_path));
	}
	else {
		TRACE("testMsgpackDeserializeObject: read failed");
		TEST_FAIL();
	}

}

void testMsgpackSerializeArray() {

	// NOTE: Following is creating a msgpack that appears in Python like:

	double time = 123.456;
	// CBA Need this version writing byte-arrays to match the python version also writing byte-arrays (27 bytes)
	const RNS::Bytes one("first\xab");
	const RNS::Bytes two("second\xcd");
	//MsgPack::bin_t<uint8_t> one("first\xab");
	//MsgPack::bin_t<uint8_t> two("second\xcd");
	//TRACEF("testMsgpackSerializeArray: time=%f one=%s two=%s", time, one.toString().c_str(), two.toString().c_str());
	// CBA This version writing strings exactly matches the python version also writing strings (23 bytes)
	//const std::string one("first");
	//const std::string two("second");
	//TRACEF("testMsgpackSerializeArray: time=%f one=%s two=%s", time, one.c_str(), two.c_str());

    MsgPack::Packer packer;
	//packer.packArraySize(3);
	//packer.pack(time);
	//packer.pack(one);
	//packer.pack(two);
	packer.to_array(time, one, two);
	//packer.to_array(time, one.collection(), two.collection());
	RNS::Bytes data(packer.data(), packer.size());

	TRACEF("testMsgpackSerializeArray: serialized %lu bytes", data.size());
	if (packer.size() > 0) {
		if (RNS::Utilities::OS::write_file(test_array_path, data) == data.size()) {
			TRACEF("testMsgpackSerializeArray: wrote: %lu bytes", data.size());
		}
		else {
			TRACE("testMsgpackSerializeArray: write failed");
			TEST_FAIL();
		}
	}
	else {
		TRACE("testMsgpackSerializeArray: failed to serialize");
		TEST_FAIL();
	}

}

void testMsgpackDeserializeArray() {

	JsonDocument doc;

	RNS::Bytes data;
	if (RNS::Utilities::OS::read_file(test_array_path, data) > 0) {
		TRACEF("testMsgpackDeserializeArray: read: %lu bytes", data.size());
		//TRACEF("data: %s", data.toString().c_str());
		MsgPack::Unpacker unpacker;
		unpacker.feed(data.data(), data.size());
		double time;
		// CBA NOTE: Can't currently deserialize directly into Bytes
		//RNS::Bytes one;
		//RNS::Bytes two;
		//unpacker.from_array(time, one, two);
		MsgPack::bin_t<uint8_t> bin_one;
		MsgPack::bin_t<uint8_t> bin_two;
		unpacker.from_array(time, bin_one, bin_two);
		RNS::Bytes one(bin_one);
		RNS::Bytes two(bin_two);
		TRACEF("testMsgpackDeserializeArray: time=%f one=%s two=%s", time, one.toString().c_str(), two.toString().c_str());
		//TRACEF("testMsgpackDeserializeArray: time=%f one=%s two=%s", time, one.c_str(), two.c_str());
		//assert(RNS::Utilities::OS::remove_file(test_array_path));
		TEST_ASSERT_NOT_NULL(one.data());
		TEST_ASSERT_EQUAL_MEMORY("first\xab", one.data(), one.size());
		TEST_ASSERT_NOT_NULL(two.data());
		TEST_ASSERT_EQUAL_MEMORY("second\xcd", two.data(), two.size());
	}
	else {
		TRACE("testMsgpackDeserializeArray: read failed");
		TEST_FAIL();
	}

}

void testMsgpackSerializeSeries() {

	// NOTE: Following is creating a msgpack that appears in Python like:

	double time = 123.456;
	// CBA Need this version writing byte-arrays to match the python version also writing byte-arrays (27 bytes)
	const RNS::Bytes one("first\xab");
	const RNS::Bytes two("second\xcd");
	//TRACEF("testMsgpackSerializeSeries: time=%f one=%s two=%s", time, one.toString().c_str(), two.toString().c_str());

    MsgPack::Packer packer;
	packer.serialize(time, one, two);
	//packer.serialize(time, one.collection(), two.collection());
	RNS::Bytes data(packer.data(), packer.size());

	TRACEF("testMsgpackSerializeSeries: serialized %lu bytes", data.size());
	if (packer.size() > 0) {
		if (RNS::Utilities::OS::write_file(test_series_path, data) == data.size()) {
			TRACEF("testMsgpackSerializeSeries: wrote: %lu bytes", data.size());
		}
		else {
			TRACE("testMsgpackSerializeSeries: write failed");
			TEST_FAIL();
		}
	}
	else {
		TRACE("testMsgpackSerializeSeries: failed to serialize");
		TEST_FAIL();
	}

}

void testMsgpackDeserializeSeries() {

	JsonDocument doc;

	RNS::Bytes data;
	if (RNS::Utilities::OS::read_file(test_series_path, data) > 0) {
		TRACEF("testMsgpackDeserializeSeries: read: %lu bytes", data.size());
		//TRACEF("data: %s", data.toString().c_str());
		MsgPack::Unpacker unpacker;
		unpacker.feed(data.data(), data.size());
		double time;
		MsgPack::bin_t<uint8_t> bin_one;
		MsgPack::bin_t<uint8_t> bin_two;
		unpacker.deserialize(time, bin_one, bin_two);
		RNS::Bytes one(bin_one);
		RNS::Bytes two(bin_two);
		TRACEF("testMsgpackDeserializeSeries: time=%f one=%s two=%s", time, one.toString().c_str(), two.toString().c_str());
		//TRACEF("testMsgpackDeserializeSeries: time=%f one=%s two=%s", time, one.c_str(), two.c_str());
		//assert(RNS::Utilities::OS::remove_file(test_series_path));
		TEST_ASSERT_NOT_NULL(one.data());
		TEST_ASSERT_EQUAL_MEMORY("first\xab", one.data(), one.size());
		TEST_ASSERT_NOT_NULL(two.data());
		TEST_ASSERT_EQUAL_MEMORY("second\xcd", two.data(), two.size());
	}
	else {
		TRACE("testMsgpackDeserializeSeries: read failed");
		TEST_FAIL();
	}

}


void test_codec_destination_entry() {

	// CBA Must register interface in order for deserializing to find it
	RNS::Interface test_interface(new TestInterface());
	RNS::Transport::register_interface(test_interface);

	RNS::Bytes raw;
	raw.assignHex("eaeaeaea");
	RNS::Packet test_packet(raw);

	RNS::Bytes received;
	received.assignHex("deadbeefdeadbeefdeadbeefdeadbeef");

	RNS::Bytes blob;
	blob.assignHex("b10bb10b");
	std::set<RNS::Bytes> blobs({received, blob});

	microStore::Codec<RNS::Persistence::DestinationEntry> codec;

	RNS::Persistence::DestinationEntry pre_entry;
	pre_entry._timestamp = 1.0;
	pre_entry._hops = 5;
	pre_entry._expires = 2.0;
	pre_entry._received_from = received;
	pre_entry._random_blobs = blobs;
	pre_entry._receiving_interface = test_interface;
	pre_entry._announce_packet = test_packet;
	RNS::Bytes hash;
	hash.assignHex("1111111111111111");
	TRACEF("test_codec_destination_entry: pre-entry: %s", pre_entry.debugString().c_str());
	std::vector<uint8_t> buffer = codec.encode(pre_entry);

	TRACEF("test_codec_destination_entry: buffer: len=%zu", buffer.size());

	RNS::Persistence::DestinationEntry post_entry;
	codec.decode(buffer, post_entry);
	TRACEF("test_codec_destination_entry: post-entry: %s", post_entry.debugString().c_str());

	TEST_ASSERT_EQUAL_FLOAT(pre_entry._timestamp, post_entry._timestamp);
	TEST_ASSERT_EQUAL_FLOAT(pre_entry._hops, post_entry._hops);
	TEST_ASSERT_EQUAL_FLOAT(pre_entry._expires, post_entry._expires);
	TEST_ASSERT_EQUAL_MEMORY(pre_entry._received_from.data(), post_entry._received_from.data(), pre_entry._received_from.size());
	TEST_ASSERT_EQUAL_MEMORY(pre_entry._receiving_interface.get_hash().data(), post_entry._receiving_interface.get_hash().data(), pre_entry._receiving_interface.get_hash().size());
	TEST_ASSERT_EQUAL_MEMORY(pre_entry._announce_packet.get_hash().data(), post_entry._announce_packet.get_hash().data(), pre_entry._announce_packet.get_hash().size());

}


void setUp(void) {
	// set stuff up here before each test
#ifdef ARDUINO
	Serial.begin(115200);
	while (!Serial) { ; }  // important for USB boards
	delay(1000);
#endif
}

void tearDown(void) {
	// clean stuff up here after each test
}

int runUnityTests(void) {
	UNITY_BEGIN();

	// Suite-level setup
    microStore::FileSystem filesystem{microStore::Adapters::UniversalFileSystem()};
	filesystem.init();
	RNS::Utilities::OS::register_filesystem(filesystem);

	// Run tests
	RUN_TEST(testWrite);
	RUN_TEST(testRead);
	RUN_TEST(testSerializeObject);
	RUN_TEST(testDeserializeObject);
	RUN_TEST(testSerializeVector);
	RUN_TEST(testDeserializeVector);
	RUN_TEST(testSerializeSet);
	RUN_TEST(testDeserializeSet);
	RUN_TEST(testSerializeMap);
	RUN_TEST(testDeserializeMap);

	RUN_TEST(testSerializeTimeOffset);

	RUN_TEST(testSerializeDestinationTable);
	RUN_TEST(testDeserializeDestinationTable);
	RUN_TEST(testDeserializeEmptyDestinationTable);

	RUN_TEST(testJsonMsgpackSerializeObject);
	RUN_TEST(testJsonMsgpackDeserializeObject);
	RUN_TEST(testJsonMsgpackSerializeArray);
	RUN_TEST(testJsonMsgpackDeserializeArray);

	RUN_TEST(testMsgpackSerializeObject);
	RUN_TEST(testMsgpackDeserializeObject);
	RUN_TEST(testMsgpackSerializeArray);
	RUN_TEST(testMsgpackDeserializeArray);
	RUN_TEST(testMsgpackSerializeSeries);
	RUN_TEST(testMsgpackDeserializeSeries);

	RUN_TEST(test_codec_destination_entry);

	// Suite-level teardown
	RNS::Utilities::OS::remove_file(test_file_path);
	RNS::Utilities::OS::remove_file(test_object_path);
	RNS::Utilities::OS::remove_file(test_map_path);
	RNS::Utilities::OS::remove_file(test_vector_path);
	RNS::Utilities::OS::remove_file(test_set_path);
	RNS::Utilities::OS::remove_file(test_path_table_path);
	RNS::Utilities::OS::remove_file(test_empty_path_table_path);
	RNS::Utilities::OS::remove_file(test_array_path);
	RNS::Utilities::OS::remove_file(test_series_path);
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
