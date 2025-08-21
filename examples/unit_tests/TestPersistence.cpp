//#include <unity.h>

#include "FileSystem.h"

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
#include <assert.h>
#include <stdio.h>

RNS::FileSystem persistence_filesystem(RNS::Type::NONE);

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
		TRACE("wrote: " + std::to_string(data.size()) + " bytes");
	}
	else {
		TRACE("write failed");
		assert(false);
	}
}

void testRead() {
	RNS::Bytes data;
	if (RNS::Utilities::OS::read_file(test_file_path, data) > 0) {
		TRACE("read: " + std::to_string(data.size()) + " bytes");
		TRACE("data: " + data.toString());
		assert(data.size() == strlen(test_file_data));
		assert(memcmp(data.data(), test_file_data, strlen(test_file_data)) == 0);
		//assert(RNS::Utilities::OS::remove_file(test_file_path));
	}
	else {
		TRACE("read failed");
		assert(false);
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
	doc = test;

	RNS::Bytes data;
	size_t size = 1024;
	size_t length = serializeJson(doc, data.writable(size), size);
	//size_t length = serializeMsgPack(doc, data.writable(size), size);
	if (length < size) {
		data.resize(length);
	}
	TRACE("serialized " + std::to_string(length) + " bytes");
	if (length > 0) {
		if (RNS::Utilities::OS::write_file(test_object_path, data) == data.size()) {
			TRACE("wrote: " + std::to_string(data.size()) + " bytes");
		}
		else {
			TRACE("write failed");
			assert(false);
		}
	}
	else {
		TRACE("failed to serialize");
		assert(false);
	}

}

void testDeserializeObject() {

	//StaticJsonDocument<8192> doc;
	DynamicJsonDocument doc(1024);

	RNS::Bytes data;
	if (RNS::Utilities::OS::read_file(test_object_path, data) > 0) {
	if (data) {
		TRACE("read: " + std::to_string(data.size()) + " bytes");
		//TRACE("data: " + data.toString());
		DeserializationError error = deserializeJson(doc, data.data());
		//DeserializationError error = deserializeMsgPack(doc, data.data());
		if (!error) {
			JsonObject root = doc.as<JsonObject>();
			for (JsonPair kv : root) {
				TRACE("key: " + std::string(kv.key().c_str()) + " value: " + kv.value().as<const char*>());
			}
		}
		else {
			TRACE("testDeserializeObject: failed to deserialize");
			assert(false);
		}
		//assert(RNS::Utilities::OS::remove_file(test_object_path));
	}
	else {
		TRACE("read failed");
		assert(false);
	}

}
*/
void testSerializeObject() {

	std::vector<std::string> vec({"one", "two"});
	TestObject test("test", vec);
	TRACE("testSerializeObject: test: " + test.toString());

	StaticJsonDocument<1024> doc;
	doc = test;

	RNS::Bytes data;
	size_t size = 1024;
	size_t length = serializeJson(doc, data.writable(size), size);
	//size_t length = serializeMsgPack(doc, data.writable(size), size);
	if (length < size) {
		data.resize(length);
	}
	TRACE("testSerializeObject: serialized " + std::to_string(length) + " bytes");
	if (length > 0) {
		if (RNS::Utilities::OS::write_file(test_object_path, data) == data.size()) {
			TRACE("testSerializeObject: wrote: " + std::to_string(data.size()) + " bytes");
		}
		else {
			TRACE("testSerializeObject: write failed");
			assert(false);
		}
	}
	else {
		TRACE("testSerializeObject: failed to serialize");
		assert(false);
	}

}

void testDeserializeObject() {

	//StaticJsonDocument<8192> doc;
	DynamicJsonDocument doc(1024);

	RNS::Bytes data;
	if (RNS::Utilities::OS::read_file(test_object_path, data) > 0) {
		TRACE("testDeserializeObject: read: " + std::to_string(data.size()) + " bytes");
		//TRACE("data: " + data.toString());
		DeserializationError error = deserializeJson(doc, data.data());
		//DeserializationError error = deserializeMsgPack(doc, data.data());
		if (!error) {
			//JsonObject root = doc.as<JsonObject>();
			//for (JsonPair kv : root) {
			//	TRACE("key: " + std::string(kv.key().c_str()) + " value: " + kv.value().as<const char*>());
			//}
			TestObject test = doc.as<TestObject>();
			TRACE("testDeserializeObject: test: " + test.toString());
		}
		else {
			TRACE("testDeserializeObject: failed to deserialize");
			assert(false);
		}
		//assert(RNS::Utilities::OS::remove_file(test_object_path));
	}
	else {
		TRACE("testDeserializeObject: read failed");
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
	TRACE("testSerializeVector: serialized " + std::to_string(length) + " bytes");
	if (length > 0) {
		if (RNS::Utilities::OS::write_file(test_vector_path, data) == data.size()) {
			TRACE("testSerializeVector: wrote: " + std::to_string(data.size()) + " bytes");
		}
		else {
			TRACE("testSerializeVector: write failed");
			assert(false);
		}
	}
	else {
		TRACE("testSerializeVector: failed to serialize");
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
	DynamicJsonDocument doc(1024);

	RNS::Bytes data;
	if (RNS::Utilities::OS::read_file(test_vector_path, data) > 0) {
		TRACE("testDeserializeVector: read: " + std::to_string(data.size()) + " bytes");
		//TRACE("testDeserializeVector: data: " + data.toString());
		DeserializationError error = deserializeJson(doc, data.data());
		//DeserializationError error = deserializeMsgPack(doc, data.data());
		if (!error) {
			TRACE("testDeserializeVector: successfully deserialized");

			std::vector<TestObject> vector = doc.as<std::vector<TestObject>>();
			for (auto& test : vector) {
				TRACE("testDeserializeVector: entry: " + test.toString());
			}

			//JsonArray arr = doc.as<JsonArray>();
			//for (JsonVariant elem : arr) {
			//	TRACE("testDeserializeVector: entry: " + elem.as<TestObject>().toString());
			//}
		}
		else {
			TRACE("testDeserializeVector: failed to deserialize");
			assert(false);
		}
		//assert(RNS::Utilities::OS::remove_file(test_vector_path));
	}
	else {
		TRACE("testDeserializeVector: read failed");
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
	TRACE("testSerializeSet: serialized " + std::to_string(length) + " bytes");
	if (length > 0) {
		if (RNS::Utilities::OS::write_file(test_set_path, data) == data.size()) {
			TRACE("testSerializeSet: wrote: " + std::to_string(data.size()) + " bytes");
		}
		else {
			TRACE("testSerializeSet: write failed");
			assert(false);
		}
	}
	else {
		TRACE("testSerializeSet: failed to serialize");
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
	DynamicJsonDocument doc(1024);

	RNS::Bytes data;
	if (RNS::Utilities::OS::read_file(test_set_path, data) > 0) {
		TRACE("testDeserializeSet: read: " + std::to_string(data.size()) + " bytes");
		//TRACE("testDeserializeSet: data: " + data.toString());
		DeserializationError error = deserializeJson(doc, data.data());
		//DeserializationError error = deserializeMsgPack(doc, data.data());
		if (!error) {
			TRACE("testDeserializeSet: successfully deserialized");

			std::set<TestObject> set = doc.as<std::set<TestObject>>();
			for (auto& test : set) {
				TRACE("testDeserializeSet: entry: " + test.toString());
			}

			//JsonArray arr = doc.as<JsonArray>();
			//for (JsonVariant elem : arr) {
			//	TRACE("testDeserializeSet: entry: " + elem.as<TestObject>().toString());
			//}
		}
		else {
			TRACE("testDeserializeSet: failed to deserialize");
			assert(false);
		}
		//assert(RNS::Utilities::OS::remove_file(test_set_path));
	}
	else {
		TRACE("testDeserializeSet: read failed");
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
	TRACE("testSerializeMap: serialized " + std::to_string(length) + " bytes");
	if (length > 0) {
		if (RNS::Utilities::OS::write_file(test_map_path, data) == data.size()) {
			TRACE("testSerializeMap: wrote: " + std::to_string(data.size()) + " bytes");
		}
		else {
			TRACE("testSerializeMap: write failed");
			assert(false);
		}
	}
	else {
		TRACE("testSerializeMap: failed to serialize");
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
	DynamicJsonDocument doc(1024);

	RNS::Bytes data;
	if (RNS::Utilities::OS::read_file(test_map_path, data) > 0) {
		TRACE("testDeserializeMap: read: " + std::to_string(data.size()) + " bytes");
		//TRACE("testDeserializeMap: data: " + data.toString());
		DeserializationError error = deserializeJson(doc, data.data());
		//DeserializationError error = deserializeMsgPack(doc, data.data());
		if (!error) {
			TRACE("testDeserializeMap: successfully deserialized");

			std::map<std::string, TestObject> map(doc.as<std::map<std::string, TestObject>>());
			for (auto& [str, test] : map) {
				TRACE("testDeserializeMap: entry: " + str + " = " + test.toString());
			}

			//JsonObject root = doc.as<JsonObject>();
			//for (JsonPair kv : root) {
			//	TRACE("testDeserializeMap: entry: " + std::string(kv.key().c_str()) + " = " + kv.value().as<TestObject>().toString());
			//}
		}
		else {
			TRACE("testDeserializeMap: failed to deserialize");
			assert(false);
		}
		//assert(RNS::Utilities::OS::remove_file(test_map_path));
	}
	else {
		TRACE("testDeserializeMap: read failed");
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

	for (auto& [hash, test] : map) {
		TRACE("testSerializeDestinationTable: entry: " + hash.toHex() + " = " + test.debugString());
	}

	//StaticJsonDocument<1024> doc;
	//doc = map;
	TRACE("testSerializeDestinationTable: JSON_OBJECT_SIZE " + std::to_string(JSON_OBJECT_SIZE(1)) + " bytes");
	TRACE("testSerializeDestinationTable: JSON_ARRAY_SIZE " + std::to_string(JSON_ARRAY_SIZE(1)) + " bytes");
	int calcsize = 0;
	for (auto& [destination_hash, destination_entry] : map) {
		// entry
		calcsize += JSON_ARRAY_SIZE(1);
		// map key
		calcsize += JSON_OBJECT_SIZE(1);
		// entry (+1)
		calcsize += JSON_OBJECT_SIZE(7+1);
		// blobs
		calcsize += JSON_ARRAY_SIZE(destination_entry._random_blobs.size());
	}
	TRACE("testSerializeDestinationTable: calcsize " + std::to_string(calcsize) + " bytes");
	const int BUFFER_SIZE = (JSON_OBJECT_SIZE(9) + JSON_ARRAY_SIZE(2)) * map.size() + JSON_ARRAY_SIZE(map.size());
	TRACE("testSerializeDestinationTable: BUFFER_SIZE " + std::to_string(BUFFER_SIZE) + " bytes");
	//DynamicJsonDocument doc(32768);
	//DynamicJsonDocument doc(BUFFER_SIZE);
	DynamicJsonDocument doc(calcsize);
	doc.set(map);
	TRACE("testSerializeDestinationTable: doc size " + std::to_string(doc.size()));
	TRACE("testSerializeDestinationTable: doc memory " + std::to_string(doc.memoryUsage()));

	RNS::Bytes data;
	// /size_t size = 1024;
	//size_t size = calcsize;
	size_t size = doc.memoryUsage();
	TRACE("testSerializeDestinationTable: buffer size " + std::to_string(size) + " bytes");
	size_t length = serializeJson(doc, data.writable(size), size);
	//size_t length = serializeMsgPack(doc, data.writable(size), size);
	if (length < size) {
		data.resize(length);
	}
	TRACE("testSerializeDestinationTable: serialized " + std::to_string(length) + " bytes");
	if (length > 0) {
		TRACE("json: " + data.toString());
		if (RNS::Utilities::OS::write_file(test_destination_table_path, data) == data.size()) {
			TRACE("testSerializeDestinationTable: wrote: " + std::to_string(data.size()) + " bytes");
		}
		else {
			TRACE("testSerializeDestinationTable: write failed");
			assert(false);
		}
	}
	else {
		TRACE("testSerializeDestinationTable: failed to serialize");
		assert(false);
	}

}

void testDeserializeDestinationTable() {

	RNS::Bytes data;
	if (RNS::Utilities::OS::read_file(test_destination_table_path, data) > 0) {
		TRACE("testDeserializeDestinationTable: read: " + std::to_string(data.size()) + " bytes");
		//StaticJsonDocument<8192> doc;
		//DynamicJsonDocument doc(1024);
		int calcsize = data.size() * 2.0;
		TRACE("testDeserializeDestinationTable: calcsize " + std::to_string(calcsize) + " bytes");
		DynamicJsonDocument doc(calcsize);

		//TRACE("testDeserializeVector: data: " + data.toString());
		DeserializationError error = deserializeJson(doc, data.data());
		//DeserializationError error = deserializeMsgPack(doc, data.data());
		TRACE("testDeserializeDestinationTable: doc size " + std::to_string(doc.size()));
		TRACE("testDeserializeDestinationTable: doc memory " + std::to_string(doc.memoryUsage()));
		if (!error) {
			TRACE("testDeserializeDestinationTable: successfully deserialized " + std::to_string(data.size()) + " bytes");
			TRACE("json: " + data.toString());

			static std::map<RNS::Bytes, RNS::Transport::DestinationEntry> map(doc.as<std::map<RNS::Bytes, RNS::Transport::DestinationEntry>>());
			for (auto& [hash, test] : map) {
				TRACE("testDeserializeDestinationTable: entry: " + hash.toHex() + " = " + test.debugString());
			}

			//JsonObject root = doc.as<JsonObject>();
			//for (JsonPair kv : root) {
			//	TRACE("testDeserializeDestinationTable: entry: " + std::string(kv.key().c_str()) + " = " + kv.value().as<TestObject>().toString());
			//}
		}
		else {
			TRACE("testDeserializeDestinationTable: failed to deserialize");
			assert(false);
		}
		//assert(RNS::Utilities::OS::remove_file(test_destination_table_path));
	}
	else {
		TRACE("testDeserializeDestinationTable: read failed");
		assert(false);
	}

}

void testPersistenceSerializeDestinationTable() {
	HEAD("testPersistenceSerializeDestinationTable", RNS::LOG_TRACE);

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

	TRACEF("testPersistenceSerializeDestinationTable: map contains %d entries", map.size());
	assert(22 == map.size());
	for (auto& [hash, test] : map) {
		TRACEF("testPersistenceSerializeDestinationTable: entry: %s = %s", hash.toHex().c_str(), test.debugString().c_str());
	}

	uint32_t stream_crc;
	size_t wrote = RNS::Persistence::serialize(map, test_destination_table_path, stream_crc);
	TRACEF("testPersistenceSerializeDestinationTable: wrote: %d bytes", wrote);
	if (wrote == 0) {
		TRACE("testPersistenceSerializeDestinationTable: write failed");
		assert(false);
	}

	uint32_t crc = RNS::Persistence::crc(map);
	TRACEF("testPersistenceSerializeDestinationTable: crc: 0x%X", crc);
	TRACEF("testPersistenceSerializeDestinationTable: stream crc: 0x%X", stream_crc);
	assert(0xFC979601 == crc);
	assert(0xFC979601 == stream_crc);

}

void testPersistenceDeserializeDestinationTable() {
	HEAD("testPersistenceDeserializeDestinationTable", RNS::LOG_TRACE);

	std::map<RNS::Bytes, RNS::Transport::DestinationEntry> map;
	uint32_t stream_crc;
	size_t read = RNS::Persistence::deserialize(map, test_destination_table_path, stream_crc);
	TRACEF("testPersistenceDeserializeDestinationTable: deserialized %d bytes", read);
	if (read == 0) {
		TRACE("testPersistenceDeserializeDestinationTable: failed to deserialize");
		assert(false);
	}

	TRACEF("testPersistenceDeserializeDestinationTable: map contains %d entries", map.size());
	assert(22 == map.size());
	for (auto& [hash, test] : map) {
		TRACEF("testPersistenceDeserializeDestinationTable: entry: %s = %s", hash.toHex().c_str(), test.debugString().c_str());
	}

	uint32_t crc = RNS::Persistence::crc(map);
	TRACEF("testPersistenceDeserializeDestinationTable: crc: 0x%X", crc);
	TRACEF("testPersistenceDeserializeDestinationTable: stream crc: 0x%X", stream_crc);
	assert(0xFC979601 == crc);
	assert(0xFC979601 == stream_crc);

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
	TRACE("testJsonMsgpackSerializeObject: test: " + test.toString());

	StaticJsonDocument<1024> doc;
	doc = test;

	RNS::Bytes data;
	size_t size = 1024;
	size_t length = serializeMsgPack(doc, data.writable(size), size);
	if (length < size) {
		data.resize(length);
	}
	TRACE("testJsonMsgpackSerializeObject: serialized " + std::to_string(length) + " bytes");
	if (length > 0) {
		if (RNS::Utilities::OS::write_file(test_object_path, data) == data.size()) {
			TRACE("testJsonMsgpackSerializeObject: wrote: " + std::to_string(data.size()) + " bytes");
		}
		else {
			TRACE("testJsonMsgpackSerializeObject: write failed");
			assert(false);
		}
	}
	else {
		TRACE("testJsonMsgpackSerializeObject: failed to serialize");
		assert(false);
	}

}

void testJsonMsgpackDeserializeObject() {

	//StaticJsonDocument<8192> doc;
	DynamicJsonDocument doc(1024);

	RNS::Bytes data;
	if (RNS::Utilities::OS::read_file(test_object_path, data) > 0) {
		TRACE("testJsonMsgpackDeserializeObject: read: " + std::to_string(data.size()) + " bytes");
		//TRACE("data: " + data.toString());
		DeserializationError error = deserializeMsgPack(doc, data.data());
		if (!error) {
			//JsonObject root = doc.as<JsonObject>();
			//for (JsonPair kv : root) {
			//	TRACE("key: " + std::string(kv.key().c_str()) + " value: " + kv.value().as<const char*>());
			//}
			TestObject test = doc.as<TestObject>();
			TRACE("testJsonMsgpackDeserializeObject: test: " + test.toString());
		}
		else {
			TRACE("testJsonMsgpackDeserializeObject: failed to deserialize");
			assert(false);
		}
		//assert(RNS::Utilities::OS::remove_file(test_object_path));
	}
	else {
		TRACE("testJsonMsgpackDeserializeObject: read failed");
		assert(false);
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

	StaticJsonDocument<1024> doc;

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
	TRACE("testJsonMsgpackSerializeArray: serialized " + std::to_string(length) + " bytes");
	if (length > 0) {
		if (RNS::Utilities::OS::write_file(test_array_path, data) == data.size()) {
			TRACE("testJsonMsgpackSerializeArray: wrote: " + std::to_string(data.size()) + " bytes");
		}
		else {
			TRACE("testJsonMsgpackSerializeArray: write failed");
			assert(false);
		}
	}
	else {
		TRACE("testJsonMsgpackSerializeArray: failed to serialize");
		assert(false);
	}

}

void testJsonMsgpackDeserializeArray() {

	StaticJsonDocument<1024> doc;

	RNS::Bytes data;
	if (RNS::Utilities::OS::read_file(test_array_path, data) > 0) {
		TRACE("testJsonMsgpackDeserializeArray: read: " + std::to_string(data.size()) + " bytes");
		//TRACE("data: " + data.toString());
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
			assert(one == "first\xab");
			assert(two == "second\xcd");
		}
		else {
			TRACE("testJsonMsgpackDeserializeArray: failed to deserialize");
			assert(false);
		}
		//assert(RNS::Utilities::OS::remove_file(test_array_path));
	}
	else {
		TRACE("testJsonMsgpackDeserializeArray: read failed");
		assert(false);
	}

}


void testMsgpackSerializeObject() {

	std::vector<std::string> vec({"one", "two"});
	TestObject test("test", vec);
	TRACE("testMsgpackSerializeObject: test: " + test.toString());

	StaticJsonDocument<1024> doc;
	doc = test;

	RNS::Bytes data;
	size_t size = 1024;
	size_t length = serializeMsgPack(doc, data.writable(size), size);
	if (length < size) {
		data.resize(length);
	}
	TRACE("testMsgpackSerializeObject: serialized " + std::to_string(length) + " bytes");
	if (length > 0) {
		if (RNS::Utilities::OS::write_file(test_object_path, data) == data.size()) {
			TRACE("testMsgpackSerializeObject: wrote: " + std::to_string(data.size()) + " bytes");
		}
		else {
			TRACE("testMsgpackSerializeObject: write failed");
			assert(false);
		}
	}
	else {
		TRACE("testMsgpackSerializeObject: failed to serialize");
		assert(false);
	}

}

void testMsgpackDeserializeObject() {

	//StaticJsonDocument<8192> doc;
	DynamicJsonDocument doc(1024);

	RNS::Bytes data;
	if (RNS::Utilities::OS::read_file(test_object_path, data) > 0) {
		TRACE("testMsgpackDeserializeObject: read: " + std::to_string(data.size()) + " bytes");
		//TRACE("data: " + data.toString());
		DeserializationError error = deserializeMsgPack(doc, data.data());
		if (!error) {
			//JsonObject root = doc.as<JsonObject>();
			//for (JsonPair kv : root) {
			//	TRACE("key: " + std::string(kv.key().c_str()) + " value: " + kv.value().as<const char*>());
			//}
			TestObject test = doc.as<TestObject>();
			TRACE("testMsgpackDeserializeObject: test: " + test.toString());
		}
		else {
			TRACE("testMsgpackDeserializeObject: failed to deserialize");
			assert(false);
		}
		//assert(RNS::Utilities::OS::remove_file(test_object_path));
	}
	else {
		TRACE("testMsgpackDeserializeObject: read failed");
		assert(false);
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

	TRACE("testMsgpackSerializeArray: serialized " + std::to_string(data.size()) + " bytes");
	if (packer.size() > 0) {
		if (RNS::Utilities::OS::write_file(test_array_path, data) == data.size()) {
			TRACE("testMsgpackSerializeArray: wrote: " + std::to_string(data.size()) + " bytes");
		}
		else {
			TRACE("testMsgpackSerializeArray: write failed");
			assert(false);
		}
	}
	else {
		TRACE("testMsgpackSerializeArray: failed to serialize");
		assert(false);
	}

}

void testMsgpackDeserializeArray() {

	StaticJsonDocument<1024> doc;

	RNS::Bytes data;
	if (RNS::Utilities::OS::read_file(test_array_path, data) > 0) {
		TRACE("testMsgpackDeserializeArray: read: " + std::to_string(data.size()) + " bytes");
		//TRACE("data: " + data.toString());
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
		assert(one == "first\xab");
		assert(two == "second\xcd");
	}
	else {
		TRACE("testMsgpackDeserializeArray: read failed");
		assert(false);
	}

}

void testMsgpackSerializeSeries() {

	// NOTE: Following is creating a msgpack that appears in Python like:

	double time = 123.456;
	// CBA Need this version writing byte-arrays to match the python version also writing byte-arrays (27 bytes)
	//const RNS::Bytes one("first\xab");
	//const RNS::Bytes two("second\xcd");
	//MsgPack::bin_t<uint8_t> one("first\xab");
	//MsgPack::bin_t<uint8_t> two("second\xcd");
	//TRACEF("testMsgpackSerializeSeries: time=%f one=%s two=%s", time, one.toString().c_str(), two.toString().c_str());
	// CBA This version writing strings exactly matches the python version also writing strings (23 bytes)
	const std::string one("first");
	const std::string two("second");
	//TRACEF("testMsgpackSerializeSeries: time=%f one=%s two=%s", time, one.c_str(), two.c_str());

    MsgPack::Packer packer;
	packer.serialize(time, one, two);
	//packer.serialize(time, one.collection(), two.collection());
	RNS::Bytes data(packer.data(), packer.size());

	TRACE("testMsgpackSerializeSeries: serialized " + std::to_string(data.size()) + " bytes");
	if (packer.size() > 0) {
		if (RNS::Utilities::OS::write_file(test_series_path, data) == data.size()) {
			TRACE("testMsgpackSerializeSeries: wrote: " + std::to_string(data.size()) + " bytes");
		}
		else {
			TRACE("testMsgpackSerializeSeries: write failed");
			assert(false);
		}
	}
	else {
		TRACE("testMsgpackSerializeSeries: failed to serialize");
		assert(false);
	}

}

void testMsgpackDeserializeSeries() {

	StaticJsonDocument<1024> doc;

	RNS::Bytes data;
	if (RNS::Utilities::OS::read_file(test_series_path, data) > 0) {
		TRACE("testMsgpackDeserializeSeries: read: " + std::to_string(data.size()) + " bytes");
		//TRACE("data: " + data.toString());
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
		assert(one == "first\xab");
		assert(two == "second\xcd");
	}
	else {
		TRACE("testMsgpackDeserializeSeries: read failed");
		assert(false);
	}

}


void testPersistence() {
	HEAD("Running testPersistence...", RNS::LOG_TRACE);

	persistence_filesystem = new FileSystem();
	((FileSystem*)persistence_filesystem.get())->init();
	RNS::Utilities::OS::register_filesystem(persistence_filesystem);

/*
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
*/

/*
	testSerializeTimeOffset();

	testSerializeDestinationTable();
	testDeserializeDestinationTable();

	// CBA Following not currently working on native due to incomplete streaming implementation
#ifdef ARDUINO
	testPersistenceSerializeDestinationTable();
#endif

	// CBA Following not currently working on native due to incomplete streaming implementation
#ifdef ARDUINO
	testPersistenceDeserializeDestinationTable();
#endif
*/

	//testJsonMsgpackSerializeObject();
	//testJsonMsgpackDeserializeObject();
	//testJsonMsgpackSerializeArray();
	//testJsonMsgpackDeserializeArray();

	//testMsgpackSerializeObject();
	//testMsgpackDeserializeObject();
	testMsgpackSerializeArray();
	testMsgpackDeserializeArray();
	//testMsgpackSerializeSeries();
	//testMsgpackDeserializeSeries();

	RNS::Utilities::OS::deregister_filesystem();

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
