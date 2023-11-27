//#include <unity.h>

#include "Bytes.h"
#include "Log.h"

#include <assert.h>

//void testBytesDefault(const RNS::Bytes& bytes = {RNS::Bytes::NONE}) {
void testBytesDefault(const RNS::Bytes& bytes = {}) {
	assert(!bytes);
	assert(bytes.size() == 0);
	assert(bytes.data() == nullptr);
}

RNS::Bytes ref("Test");
const RNS::Bytes& testBytesReference() {
	// NOTE: Can NOT return local instance as reference!!!
	//RNS::Bytes ref("Test");
	RNS::extreme("returning...");
	return ref;
}

void testBytesMain() {

	RNS::Bytes bytes;
	assert(!bytes);
	assert(bytes.size() == 0);
	assert(bytes.empty() == true);
	assert(bytes.data() == nullptr);

	const uint8_t prestr[] = "Hello";
	const uint8_t poststr[] = " World";

	const RNS::Bytes prebuf(prestr, 5);
	assert(prebuf);
	assert(prebuf.size() == 5);
	assert(memcmp(prebuf.data(), "Hello", prebuf.size()) == 0);
	assert(!(bytes == prebuf));
	assert(bytes != prebuf);
	assert(bytes < prebuf);

	const RNS::Bytes postbuf(poststr, 6);
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

	// test string assignment
	bytes = "Foo";
	assert(bytes.size() == 3);
	assert(memcmp(bytes.data(), "Foo", bytes.size()) == 0);

	// test string addition
	bytes = prebuf + postbuf;
	assert(bytes.size() == 11);
	assert(memcmp(bytes.data(), "Hello World", bytes.size()) == 0);

	// test string constructor
	{
		RNS::Bytes str("Foo");
		assert(str.size() == 3);
		assert(memcmp(str.data(), "Foo", str.size()) == 0);
	}

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
		RNS::extreme("stream prebuf: " + prebuf.toString());
		RNS::extreme("stream postbuf: " + postbuf.toString());
		RNS::extreme("stream strmbuf: " + strmbuf.toString());
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
		RNS::extreme("stream prebuf: " + prebuf.toString());
		RNS::extreme("stream postbuf: " + postbuf.toString());
		RNS::extreme("stream strmbuf: " + strmbuf.toString());
		assert(strmbuf.size() == 18);
		assert(memcmp(strmbuf.data(), "Stream Hello World", strmbuf.size()) == 0);
		assert(prebuf.size() == 5);
		assert(memcmp(prebuf.data(), "Hello", prebuf.size()) == 0);
		assert(postbuf.size() == 6);
		assert(memcmp(postbuf.data(), " World", postbuf.size()) == 0);
	}

	// stream with assignment
	// (side effect of also updating pre - this is a known and correct but perhaps unexpected and non-intuitive side-effect of assignment with stream)
	{
		// NOTE pre must be volatile in order for below stream with assignment to work (since it gets modified)
		RNS::Bytes pre("Hello");
		assert(pre.size() == 5);
		const RNS::Bytes post(" World");
		assert(post.size() == 6);
		RNS::Bytes strmbuf = pre << post;
		RNS::extreme("stream pre: " + prebuf.toString());
		RNS::extreme("stream post: " + postbuf.toString());
		RNS::extreme("stream strmbuf: " + strmbuf.toString());
		assert(strmbuf.size() == 11);
		assert(memcmp(strmbuf.data(), "Hello World", strmbuf.size()) == 0);
		assert(pre.size() == 11);
		assert(memcmp(pre.data(), "Hello World", pre.size()) == 0);
		assert(post.size() == 6);
		assert(memcmp(post.data(), " World", post.size()) == 0);
	}

	// test creating bytes from default
	{
		RNS::Bytes bytes;
		assert(!bytes);
		assert(bytes.size() == 0);
		assert(bytes.data() == nullptr);
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
		RNS::Bytes bytes({RNS::Bytes::NONE});
		assert(!bytes);
		assert(bytes.size() == 0);
		assert(bytes.data() == nullptr);
	}

	// test creating bytes from empty
	{
		RNS::Bytes bytes = {};
		assert(!bytes);
		assert(bytes.size() == 0);
		assert(bytes.data() == nullptr);
	}

	// function default argument
	RNS::extreme("TestBytes: function default argument");
	testBytesDefault();

	// function reference return
	RNS::extreme("TestBytes: function reference return");
	{
		RNS::Bytes test = testBytesReference();
		RNS::extreme("returned");
		assert(test);
		assert(test.size() == 4);
		assert(memcmp(test.data(), "Test", test.size()) == 0);
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

void testBytes() {
	RNS::head("Running testBytes...", RNS::LOG_EXTREME);
	testBytesMain();
	testCowBytes();
	testBytesConversion();
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
