//#include <unity.h>

#include "Bytes.h"
#include "Log.h"

#include <map>
#include <string>
#include <assert.h>

void testBytesMap()
{
	const uint8_t prestr[] = "Hello";
	const uint8_t poststr[] = "World";

	RNS::Bytes prebuf(prestr, 5);
	assert(prebuf.size() == 5);
	assert(memcmp(prebuf.data(), "Hello", prebuf.size()) == 0);

	RNS::Bytes postbuf(poststr, 5);
	assert(postbuf.size() == 5);
	assert(memcmp(postbuf.data(), "World", postbuf.size()) == 0);

	std::map<RNS::Bytes, std::string> map;
	map.insert({prebuf, "hello"});
	map.insert({postbuf, "world"});
	assert(map.size() == 2);

	auto preit = map.find(prebuf);
	assert(preit != map.end());
	assert((*preit).second.compare("hello") == 0);
	if (preit != map.end()) {
		RNS::extreme(std::string("found prebuf: ") + (*preit).second);
	}

	auto postit = map.find(postbuf);
	assert(postit != map.end());
	assert((*postit).second.compare("world") == 0);
	if (postit != map.end()) {
		RNS::extreme(std::string("found postbuf: ") + (*postit).second);
	}

	const uint8_t newstr[] = "World";
	RNS::Bytes newbuf(newstr, 5);
	assert(newbuf.size() == 5);
	assert(memcmp(newbuf.data(), "World", newbuf.size()) == 0);
	auto newit = map.find(newbuf);
	assert(newit != map.end());
	assert((*newit).second.compare("world") == 0);
	if (newit != map.end()) {
		RNS::extreme(std::string("found newbuf: ") + (*newit).second);
	}

	std::string str = map["World"];
	assert(str.size() == 5);
	assert(str.compare("world") == 0);
}

class Entry {
	public:
	// CBA for some reason default constructor is required for map index reference/asign to work
	//  ("error: no matching constructor for initialization of 'Entry'" results otherwise)
    Entry() { RNS::extreme("Entry: default constructor"); }
    Entry(const Entry& entry) : _hash(entry._hash) { RNS::extreme("Entry: copy constructor"); }
    Entry(const char* hash) : _hash(hash) { RNS::extreme("Entry: hash constructor"); }
    RNS::Bytes _hash;
};

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

	assert(entries.size() == 1);
	assert(other_entries.size() == 2);
	for (auto& [hash, entry] : other_entries) {
		if (entries.find(hash) == entries.end()) {
			entries[hash] = other_entries[hash];
		}
	}
	for (auto& [hash, entry] : entries) {
		RNS::extreme("entries: " + entry._hash.toString());
	}
	assert(entries.size() == 2);
	assert(other_entries.size() == 2);

}

void testCollections() {
	RNS::head("Running testCollections...", RNS::LOG_EXTREME);
	testBytesMap();
	testNewMap();
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
