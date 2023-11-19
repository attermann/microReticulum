//#include <unity.h>

#include "Bytes.h"
#include "Log.h"

#include <map>
#include <assert.h>

void testBytes() {

	RNS::Bytes bytes;
	assert(!bytes);
	assert(bytes.size() == 0);
	assert(bytes.empty() == true);
	assert(bytes.data() == nullptr);

	const uint8_t prestr[] = "Hello";
	const uint8_t poststr[] = " World";

	RNS::Bytes prebuf(prestr, 5);
	assert(prebuf);
	assert(prebuf.size() == 5);
	assert(memcmp(prebuf.data(), "Hello", prebuf.size()) == 0);
	assert(!(bytes == prebuf));
	assert(bytes != prebuf);
	assert(bytes < prebuf);

	RNS::Bytes postbuf(poststr, 6);
	assert(postbuf);
	assert(postbuf.size() == 6);
	assert(memcmp(postbuf.data(), " World", postbuf.size()) == 0);
	assert(!(postbuf == bytes));
	assert(postbuf != bytes);
	assert(postbuf > bytes);

	assert(!(prebuf == postbuf));
	assert(prebuf != postbuf);

	if (prebuf == postbuf) {
		RNS::extreme("bytess are the same");
	}
	else {
		RNS::extreme("bytess are different");
	}

	bytes += prebuf + postbuf;
	assert(bytes.size() == 11);
	assert(memcmp(bytes.data(), "Hello World", bytes.size()) == 0);
	RNS::extreme("assign bytes: " + bytes.toString());
	RNS::extreme("assign prebuf: " + prebuf.toString());
	RNS::extreme("assign postbuf: " + postbuf.toString());

	bytes = "Foo";
	assert(bytes.size() == 3);
	assert(memcmp(bytes.data(), "Foo", bytes.size()) == 0);

	bytes = prebuf + postbuf;
	assert(bytes.size() == 11);
	assert(memcmp(bytes.data(), "Hello World", bytes.size()) == 0);

	// test left in range
	{
		RNS::Bytes left(bytes.left(5));
		RNS::extreme("left: " + left.toString());
		assert(left.size() == 5);
		assert(memcmp(left.data(), "Hello", left.size()) == 0);
	}
	// test left oob
	{
		RNS::Bytes left(bytes.left(20));
		RNS::extreme("oob left: " + left.toString());
		assert(left.size() == 11);
		assert(memcmp(left.data(), "Hello World", left.size()) == 0);
	}
	// test right in range
	{
		RNS::Bytes right(bytes.right(5));
		RNS::extreme("right: " + right.toString());
		assert(right.size() == 5);
		assert(memcmp(right.data(), "World", right.size()) == 0);
	}
	// test right oob
	{
		RNS::Bytes right(bytes.right(20));
		RNS::extreme("oob right: " + right.toString());
		assert(right.size() == 11);
		assert(memcmp(right.data(), "Hello World", right.size()) == 0);
	}
	// test mid in range
	{
		RNS::Bytes mid(bytes.mid(3, 5));
		RNS::extreme("mid: " + mid.toString());
		assert(mid.size() == 5);
		assert(memcmp(mid.data(), "lo Wo", mid.size()) == 0);
	}
	// test mid oob pos
	{
		RNS::Bytes mid(bytes.mid(20, 5));
		RNS::extreme("oob pos mid: " + mid.toString());
		assert(!mid);
		assert(mid.size() == 0);
	}
	// test mid oob pos
	{
		RNS::Bytes mid(bytes.mid(3, 20));
		RNS::extreme("oob len mid: " + mid.toString());
		assert(mid.size() == 8);
		assert(memcmp(mid.data(), "lo World", mid.size()) == 0);
	}
	// test mid to end variant
	{
		RNS::Bytes mid(bytes.mid(3));
		RNS::extreme("end mid: " + mid.toString());
		assert(mid.size() == 8);
		assert(memcmp(mid.data(), "lo World", mid.size()) == 0);
	}

	// test resize
	{
		RNS::Bytes shrink(bytes);
		shrink.resize(5);
		RNS::extreme("shrink: " + shrink.toString());
		assert(shrink.size() == 5);
		assert(memcmp(shrink.data(), "Hello", shrink.size()) == 0);
	}

	// stream into empty bytes
	{
		RNS::Bytes strmbuf;
		strmbuf << prebuf << postbuf;
		RNS::extreme("stream strmbuf: " + strmbuf.toString());
		RNS::extreme("stream prebuf: " + prebuf.toString());
		RNS::extreme("stream postbuf: " + postbuf.toString());
		assert(strmbuf.size() == 11);
		assert(memcmp(strmbuf.data(), "Hello World", strmbuf.size()) == 0);
		assert(prebuf.size() == 5);
		assert(memcmp(prebuf.data(), "Hello", prebuf.size()) == 0);
		assert(postbuf.size() == 6);
		assert(memcmp(postbuf.data(), " World", postbuf.size()) == 0);
	}

	// stream into populated bytes
	{
		RNS::Bytes strmbuf("Stream ");
		assert(strmbuf);
		assert(strmbuf.size() == 7);
		assert(memcmp(strmbuf.data(), "Stream ", strmbuf.size()) == 0);

		strmbuf << prebuf << postbuf;
		RNS::extreme("stream strmbuf: " + strmbuf.toString());
		RNS::extreme("stream prebuf: " + prebuf.toString());
		RNS::extreme("stream postbuf: " + postbuf.toString());
		assert(strmbuf.size() == 18);
		assert(memcmp(strmbuf.data(), "Stream Hello World", strmbuf.size()) == 0);
		assert(prebuf.size() == 5);
		assert(memcmp(prebuf.data(), "Hello", prebuf.size()) == 0);
		assert(postbuf.size() == 6);
		assert(memcmp(postbuf.data(), " World", postbuf.size()) == 0);
	}

	// stream with assignment
	// (this is a known and correct but perhaps unexpected and non-intuitive side-effect of assignment with stream)
	{
		RNS::Bytes strmbuf = prebuf << postbuf;
		RNS::extreme("stream strmbuf: " + strmbuf.toString());
		RNS::extreme("stream prebuf: " + prebuf.toString());
		RNS::extreme("stream postbuf: " + postbuf.toString());
		assert(strmbuf.size() == 11);
		assert(memcmp(strmbuf.data(), "Hello World", strmbuf.size()) == 0);
		assert(prebuf.size() == 11);
		assert(memcmp(prebuf.data(), "Hello World", prebuf.size()) == 0);
		assert(postbuf.size() == 6);
		assert(memcmp(postbuf.data(), " World", postbuf.size()) == 0);
	}

	// test creating bytes from nullptr
	{
		RNS::Bytes bytes = nullptr;
		assert(!bytes);
		assert(bytes.size() == 0);
		assert(bytes.data() == nullptr);
	}

	// test creating bytes from NONE
	{
		RNS::Bytes bytes(RNS::Bytes::NONE);
		assert(!bytes);
		assert(bytes.size() == 0);
		assert(bytes.data() == nullptr);
	}

	// TODO test comparison

}

void testCowBytes() {

	RNS::Bytes bytes1("1");
	assert(bytes1.size() == 1);
	assert(memcmp(bytes1.data(), "1", bytes1.size()) == 0);

	RNS::Bytes bytes2(bytes1);
	assert(bytes2.size() == 1);
	assert(memcmp(bytes2.data(), "1", bytes2.size()) == 0);
	assert(bytes2.data() == bytes1.data());

	RNS::Bytes bytes3(bytes2);
	assert(bytes3.size() == 1);
	assert(memcmp(bytes3.data(), "1", bytes3.size()) == 0);
	assert(bytes3.data() == bytes2.data());

	RNS::extreme("pre bytes1 ptr: " + std::to_string((uintptr_t)bytes1.data()) + " data: " + bytes1.toString());
	RNS::extreme("pre bytes2 ptr: " + std::to_string((uintptr_t)bytes2.data()) + " data: " + bytes2.toString());
	RNS::extreme("pre bytes3 ptr: " + std::to_string((uintptr_t)bytes3.data()) + " data: " + bytes3.toString());

	//bytes1.append("mississippi");
	//assert(bytes1.size() == 12);
	//assert(memcmp(bytes1.data(), "1mississippi", bytes1.size()) == 0);
	//assert(bytes1.data() != bytes2.data());

	bytes2.append("mississippi");
	assert(bytes2.size() == 12);
	assert(memcmp(bytes2.data(), "1mississippi", bytes2.size()) == 0);
	assert(bytes2.data() != bytes1.data());

	bytes3.assign("mississippi");
	assert(bytes3.size() == 11);
	assert(memcmp(bytes3.data(), "mississippi", bytes3.size()) == 0);
	assert(bytes3.data() != bytes2.data());

	RNS::extreme("post bytes1 ptr: " + std::to_string((uintptr_t)bytes1.data()) + " data: " + bytes1.toString());
	RNS::extreme("post bytes2 ptr: " + std::to_string((uintptr_t)bytes2.data()) + " data: " + bytes2.toString());
	RNS::extreme("post bytes3 ptr: " + std::to_string((uintptr_t)bytes3.data()) + " data: " + bytes3.toString());
}

void testBytesConversion() {

	{
		RNS::Bytes bytes("Hello World");
		std::string hex = bytes.toHex();
		RNS::extreme("text: \"" + bytes.toString() + "\" upper hex: \"" + hex + "\"");
		assert(hex.length() == 22);
		assert(hex.compare("48656C6C6F20576F726C64") == 0);
	}
	{
		RNS::Bytes bytes("Hello World");
		std::string hex = bytes.toHex(false);
		RNS::extreme("text: \"" + bytes.toString() + "\" lower hex: \"" + hex + "\"");
		assert(hex.length() == 22);
		assert(hex.compare("48656c6c6f20576f726c64") == 0);
	}
	{
		std::string hex("48656C6C6F20576F726C64");
		RNS::Bytes bytes;
		bytes.assignHex(hex.c_str());
		std::string text = bytes.toString();
		RNS::extreme("hex: \"" + hex + "\" text: \"" + text + "\"");
		assert(text.length() == 11);
		assert(text.compare("Hello World") == 0);
	}
	{
		std::string hex("48656c6c6f20576f726c64");
		RNS::Bytes bytes;
		bytes.assignHex(hex.c_str());
		std::string text = bytes.toString();
		RNS::extreme("hex: \"" + hex + "\" text: \"" + text + "\"");
		assert(text.length() == 11);
		assert(text.compare("Hello World") == 0);

		bytes.assignHex(hex.c_str());
		text = bytes.toString();
		RNS::extreme("hex: \"" + hex + "\" text: \"" + text + "\"");
		assert(text.length() == 11);
		assert(text.compare("Hello World") == 0);

		bytes.appendHex(hex.c_str());
		text = bytes.toString();
		RNS::extreme("hex: \"" + hex + "\" text: \"" + text + "\"");
		assert(text.length() == 22);
		assert(text.compare("Hello WorldHello World") == 0);
	}

}

void testMap()
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
