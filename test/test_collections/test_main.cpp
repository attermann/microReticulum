#include <unity.h>

#include "Bytes.h"
#include "Log.h"

#include <map>
#include <string>

void testBytesMap()
{
	const uint8_t prestr[] = "Hello";
	const uint8_t poststr[] = "World";

	RNS::Bytes prebuf(prestr, 5);
	TEST_ASSERT_EQUAL_size_t(5, prebuf.size());
	TEST_ASSERT_EQUAL_MEMORY("Hello", prebuf.data(), prebuf.size());

	RNS::Bytes postbuf(poststr, 5);
	TEST_ASSERT_EQUAL_size_t(5, postbuf.size());
	TEST_ASSERT_EQUAL_MEMORY("World", postbuf.data(), postbuf.size());

	std::map<RNS::Bytes, std::string> map;
	map.insert({prebuf, "hello"});
	map.insert({postbuf, "world"});
	TEST_ASSERT_EQUAL_size_t(2, map.size());

	auto preit = map.find(prebuf);
	TEST_ASSERT_NOT_EQUAL(map.end(), preit);
	TEST_ASSERT_EQUAL_STRING("hello", (*preit).second.c_str());
	if (preit != map.end()) {
		TRACE(std::string("found prebuf: ") + (*preit).second);
	}

	auto postit = map.find(postbuf);
	TEST_ASSERT_NOT_EQUAL(map.end(), postit);
	TEST_ASSERT_EQUAL_STRING("world", (*postit).second.c_str());
	if (postit != map.end()) {
		TRACE(std::string("found postbuf: ") + (*postit).second);
	}

	const uint8_t newstr[] = "World";
	RNS::Bytes newbuf(newstr, 5);
	TEST_ASSERT_EQUAL_size_t(5, newbuf.size());
	TEST_ASSERT_EQUAL_MEMORY("World", newbuf.data(), newbuf.size());
	auto newit = map.find(newbuf);
	TEST_ASSERT_NOT_EQUAL(map.end(), newit);
	TEST_ASSERT_EQUAL_STRING("world", (*newit).second.c_str());
	if (newit != map.end()) {
		TRACE(std::string("found newbuf: ") + (*newit).second);
	}

	std::string str = map["World"];
	TEST_ASSERT_EQUAL_size_t(5, str.size());
	TEST_ASSERT_EQUAL_STRING("world", str.c_str());
}

class Entry {
	public:
	// CBA for some reason default constructor is required for map index reference/asign to work
	//  ("error: no matching constructor for initialization of 'Entry'" results otherwise)
    Entry() { TRACE("Entry: default constructor"); }
    Entry(const Entry& entry) : _hash(entry._hash) { TRACE("Entry: copy constructor"); }
    Entry(const char* hash) : _hash(hash) { TRACE("Entry: hash constructor"); }
    RNS::Bytes _hash;
};

void testOldMap() {

	std::map<RNS::Bytes, Entry> entries;
	std::map<RNS::Bytes, Entry> other_entries;

	{
		Entry some_entry("some hash");
		entries.insert({some_entry._hash, some_entry});
		Entry some_other_entry("some other hash");
		other_entries.insert({some_entry._hash, some_entry});
		other_entries.insert({some_other_entry._hash, some_other_entry});
	}

	TEST_ASSERT_EQUAL_size_t(1, entries.size());
	TEST_ASSERT_EQUAL_size_t(2, other_entries.size());
	for (auto& pair : other_entries) {
		if (entries.find(pair.first) == entries.end()) {
			entries[pair.first] = other_entries[pair.first];
		}
	}
	for (auto& pair : entries) {
		TRACE("entries: " + pair.second._hash.toString());
	}
	TEST_ASSERT_EQUAL_size_t(2, entries.size());
	TEST_ASSERT_EQUAL_size_t(2, other_entries.size());

}

void testNewMap() {

	std::map<RNS::Bytes, Entry> entries;
	std::map<RNS::Bytes, Entry> other_entries;

	{
		Entry some_entry("some hash");
		entries.insert({some_entry._hash, some_entry});
		Entry some_other_entry("some other hash");
		other_entries.insert({some_entry._hash, some_entry});
		other_entries.insert({some_other_entry._hash, some_other_entry});
	}

	TEST_ASSERT_EQUAL_size_t(1, entries.size());
	TEST_ASSERT_EQUAL_size_t(2, other_entries.size());
	for (auto& [hash, entry] : other_entries) {
		if (entries.find(hash) == entries.end()) {
			entries[hash] = other_entries[hash];
		}
	}
	for (auto& [hash, entry] : entries) {
		TRACE("entries: " + entry._hash.toString());
	}
	TEST_ASSERT_EQUAL_size_t(2, entries.size());
	TEST_ASSERT_EQUAL_size_t(2, other_entries.size());

}


void setUp(void) {
	// set stuff up here before each test
}

void tearDown(void) {
	// clean stuff up here after each test
}

int runUnityTests(void) {
	UNITY_BEGIN();
	RUN_TEST(testBytesMap);
	RUN_TEST(testOldMap);
	RUN_TEST(testNewMap);
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
