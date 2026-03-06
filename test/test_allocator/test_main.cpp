#include <unity.h>

#include <Bytes.h>
#include <Utilities/OS.h>
#include <Utilities/Memory.h>

#include <map>
#include <string>
#include <scoped_allocator>

//#define VECTOR_INTERNAL_CONTENT
//#define VECTOR_EXTERNAL_CONTENT
//#define BYTES_INTERNAL_CONTENT
#define BYTES_EXTERNAL_CONTENT

class TestObject {
public:
	TestObject() {}
	TestObject(const char* str, std::vector<std::string>& vec, RNS::Bytes& buf) : _str(str), _vec(vec), _external_buf(buf) {
#ifdef VECTOR_INTERNAL_CONTENT
		// CBA The following vector entries use psram even though they're created internally (not part of base object)
		_vec.push_back("fee");
		_vec.push_back("fum");
#endif
#ifdef BYTES_INTERNAL_CONTENT
		// CBA The following Bytes content use psram even though they're created internally (not part of base object)
		_internal_buf.assign("This is a Bytes buffer allocated INSIDE of TestObject");
#endif
	}
	inline bool operator < (const TestObject& test) const { return _str < test._str; }

	inline const std::string toString() const { return std::string("TestObject(") + _str + "," + std::to_string(_vec.size()) + "," + _internal_buf.toString() + "," + _external_buf.toString() + ")"; }

	// CBA All of the following empty containers are included in the memory allocated for std::map and use psram
	std::string _str;
	std::vector<std::string> _vec;
	RNS::Bytes _internal_buf;
	RNS::Bytes _external_buf;
};


void test_map_allocator() {

	// CBA TODO Figure out how to selectively use PSRAM for Bytes and Packet objects.

	// Trigger creation of both heap and psram pool before starting
	RNS::Utilities::Memory::pool_init(RNS::Utilities::Memory::heap_pool_info);
	RNS::Utilities::Memory::pool_init(RNS::Utilities::Memory::altheap_pool_info);
	//RNS::Utilities::Memory::dump_heap_stats();
	HEAD("Initial Stats:", RNS::LOG_TRACE);
	RNS::Utilities::Memory::dump_basic_pool_stats();
	//RNS::Utilities::Memory::dump_basic_allocator_stats();

	{
		//using TestMap = std::map<Bytes, TestObject, std::less<Bytes>, RNS::Utilities::Memory::ContainerAllocator<std::pair<const Bytes, TestObject>>>;
		using TestMap = std::map<std::string, TestObject, std::less<std::string>, RNS::Utilities::Memory::ContainerAllocator<std::pair<const std::string, TestObject>>>;
		//using TestAlloc = RNS::Utilities::Memory::ContainerAllocator<std::pair<const std::string, TestObject>>;
		//using TestStringAlloc = RNS::Utilities::Memory::ContainerAllocator<char>;
		//using TestMap = std::map<std::string, TestObject, std::less<std::string>, std::scoped_allocator_adaptor<TestAlloc, TestStringAlloc>>;

		HEAD("Post-map Stats:", RNS::LOG_TRACE);
		RNS::Utilities::Memory::dump_basic_pool_stats();
		//RNS::Utilities::Memory::dump_basic_allocator_stats();

		// CBA The following empty vector object is included in the memory allocated for std::map and uses psram
		std::vector<std::string> vec;
#ifdef VECTOR_EXTERNAL_CONTENT
		// CBA The following vector entries use psram
		vec.push_back("foo");
		vec.push_back("bar");
#endif

		// CBA The following empty Bytes object is included in the memory allocated for std::map and uses psram
		RNS::Bytes buf;
#ifdef BYTES_EXTERNAL_CONTENT
		// CBA The following Bytes content use psram
		buf.assign("This is a Bytes buffer allocated OUTSIDE of TestObject");
#endif

		TestMap map;
		map.insert({"two", {"two", vec, buf}});
		map.insert({"one", {"one", vec, buf}});

		HEAD("Post-insert Stats:", RNS::LOG_TRACE);
		RNS::Utilities::Memory::dump_basic_pool_stats();
		//RNS::Utilities::Memory::dump_basic_allocator_stats();

		TEST_ASSERT_EQUAL(2, map.size());
		for (const auto& [key, entry] : map) {
			TRACEF("key: %s, entry: %s", key.c_str(), entry.toString().c_str());
		}
	}

	HEAD("Post-free Stats:", RNS::LOG_TRACE);
	RNS::Utilities::Memory::dump_basic_pool_stats();
	//RNS::Utilities::Memory::dump_basic_allocator_stats();

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

	// Run tests
	RUN_TEST(test_map_allocator);

	// Suite-level teardown

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
