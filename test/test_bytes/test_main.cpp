#include <unity.h>

#include <Utilities/OS.h>
#include "Bytes.h"
#include "Log.h"

void testBytesDefault(const RNS::Bytes& bytes = {}) {
	TEST_ASSERT_FALSE(bytes);
	TEST_ASSERT_EQUAL_size_t(0, bytes.size());
	TEST_ASSERT_NULL(bytes.data());
}

RNS::Bytes ref("Test");
const RNS::Bytes& testBytesReference() {
	// NOTE: Can NOT return local instance as reference!!!
	//RNS::Bytes ref("Test");
	TRACE("returning...");
	return ref;
}

void testBytesMain() {

	RNS::Bytes bytes;
	TEST_ASSERT_FALSE(bytes);
	TEST_ASSERT_EQUAL_size_t(0, bytes.size());
	TEST_ASSERT_TRUE(bytes.empty());
	TEST_ASSERT_NULL(bytes.data());

	const uint8_t prestr[] = "Hello";
	const uint8_t poststr[] = " World";

	const RNS::Bytes prebuf(prestr, 5);
	TEST_ASSERT_TRUE(prebuf);
	TEST_ASSERT_EQUAL_size_t(5, prebuf.size());
	TEST_ASSERT_EQUAL_MEMORY("Hello", prebuf.data(), prebuf.size());
	TEST_ASSERT_FALSE(bytes == prebuf);
	TEST_ASSERT_TRUE(bytes != prebuf);
	TEST_ASSERT_TRUE(bytes < prebuf);

	const RNS::Bytes postbuf(poststr, 6);
	TEST_ASSERT_TRUE(postbuf);
	TEST_ASSERT_EQUAL_size_t(6, postbuf.size());
	TEST_ASSERT_EQUAL_MEMORY(" World", postbuf.data(), postbuf.size());
	TEST_ASSERT_FALSE(postbuf == bytes);
	TEST_ASSERT_TRUE(postbuf != bytes);
	TEST_ASSERT_TRUE(postbuf > bytes);

	TEST_ASSERT_FALSE(prebuf == postbuf);
	TEST_ASSERT_TRUE(prebuf != postbuf);

	if (prebuf == postbuf) {
		TRACE("bytess are the same");
	}
	else {
		TRACE("bytess are different");
	}

	// test string assignment
	bytes = "Foo";
	TEST_ASSERT_EQUAL_size_t(3, bytes.size());
	TEST_ASSERT_EQUAL_MEMORY("Foo", bytes.data(), bytes.size());

	// test string concat with addition
	bytes += prebuf + postbuf;
	TEST_ASSERT_EQUAL_size_t(14, bytes.size());
	TEST_ASSERT_EQUAL_MEMORY("FooHello World", bytes.data(), bytes.size());
	TRACE("assign bytes: " + bytes.toString());
	TRACE("assign prebuf: " + prebuf.toString());
	TRACE("assign postbuf: " + postbuf.toString());

	// test string assignment with addition
	bytes = prebuf + postbuf;
	TEST_ASSERT_EQUAL_size_t(11, bytes.size());
	TEST_ASSERT_EQUAL_MEMORY("Hello World", bytes.data(), bytes.size());

	// test string constructor
	{
		RNS::Bytes str("Foo");
		TEST_ASSERT_EQUAL_size_t(3, str.size());
		TEST_ASSERT_EQUAL_MEMORY("Foo", str.data(), str.size());
	}

	// test left in range
	{
		RNS::Bytes left(bytes.left(5));
		TRACE("left: " + left.toString());
		TEST_ASSERT_EQUAL_size_t(5, left.size());
		TEST_ASSERT_EQUAL_MEMORY("Hello", left.data(), left.size());
	}
	// test left oob
	{
		RNS::Bytes left(bytes.left(20));
		TRACE("oob left: " + left.toString());
		TEST_ASSERT_EQUAL_size_t(11, left.size());
		TEST_ASSERT_EQUAL_MEMORY("Hello World", left.data(), left.size());
	}
	// test right in range
	{
		RNS::Bytes right(bytes.right(5));
		TRACE("right: " + right.toString());
		TEST_ASSERT_EQUAL_size_t(5, right.size());
		TEST_ASSERT_EQUAL_MEMORY("World", right.data(), right.size());
	}
	// test right oob
	{
		RNS::Bytes right(bytes.right(20));
		TRACE("oob right: " + right.toString());
		TEST_ASSERT_EQUAL_size_t(11, right.size());
		TEST_ASSERT_EQUAL_MEMORY("Hello World", right.data(), right.size());
	}
	// test mid in range
	{
		RNS::Bytes mid(bytes.mid(3, 5));
		TRACE("mid: " + mid.toString());
		TEST_ASSERT_EQUAL_size_t(5, mid.size());
		TEST_ASSERT_EQUAL_MEMORY("lo Wo", mid.data(), mid.size());
	}
	// test mid oob pos
	{
		RNS::Bytes mid(bytes.mid(20, 5));
		TRACE("oob pos mid: " + mid.toString());
		TEST_ASSERT_FALSE(mid);
		TEST_ASSERT_EQUAL_size_t(0, mid.size());
	}
	// test mid oob pos
	{
		RNS::Bytes mid(bytes.mid(3, 20));
		TRACE("oob len mid: " + mid.toString());
		TEST_ASSERT_EQUAL_size_t(8, mid.size());
		TEST_ASSERT_EQUAL_MEMORY("lo World", mid.data(), mid.size());
	}
	// test mid to end variant
	{
		RNS::Bytes mid(bytes.mid(3));
		TRACE("end mid: " + mid.toString());
		TEST_ASSERT_EQUAL_size_t(8, mid.size());
		TEST_ASSERT_EQUAL_MEMORY("lo World", mid.data(), mid.size());
	}

	// test resize
	HEAD("TestBytes: resize", RNS::LOG_TRACE);
	{
		RNS::Bytes shrink(bytes);
		shrink.resize(5);
		TRACE("shrink: " + shrink.toString());
		TEST_ASSERT_EQUAL_size_t(5, shrink.size());
		TEST_ASSERT_EQUAL_MEMORY("Hello", shrink.data(), shrink.size());
	}

	// test trim
	HEAD("TestBytes: trim", RNS::LOG_TRACE);
	{
		RNS::Bytes bytes("Hello World");
		TRACE("orig: " + bytes.toString());
		TEST_ASSERT_EQUAL_size_t(11, bytes.size());
		TEST_ASSERT_EQUAL_MEMORY("Hello World", bytes.data(), bytes.size());
		bytes = bytes.left(5);
		TRACE("trim: " + bytes.toString());
		TEST_ASSERT_EQUAL_size_t(5, bytes.size());
		TEST_ASSERT_EQUAL_MEMORY("Hello", bytes.data(), bytes.size());
	}

	// test creating bytes from default
	{
		RNS::Bytes bytes;
		TEST_ASSERT_FALSE(bytes);
		TEST_ASSERT_EQUAL_size_t(0, bytes.size());
		TEST_ASSERT_NULL(bytes.data());
	}

	// test creating bytes from nullptr
	{
		RNS::Bytes bytes = nullptr;
		TEST_ASSERT_FALSE(bytes);
		TEST_ASSERT_EQUAL_size_t(0, bytes.size());
		TEST_ASSERT_NULL(bytes.data());
	}

	// test creating bytes from NONE
	{
		RNS::Bytes bytes(RNS::Bytes::NONE);
		TEST_ASSERT_FALSE(bytes);
		TEST_ASSERT_EQUAL_size_t(0, bytes.size());
		TEST_ASSERT_NULL(bytes.data());
	}

	// test creating bytes from empty
	{
		RNS::Bytes bytes = {};
		TEST_ASSERT_FALSE(bytes);
		TEST_ASSERT_EQUAL_size_t(0, bytes.size());
		TEST_ASSERT_NULL(bytes.data());
	}

	// function default argument
	HEAD("TestBytes: function default argument", RNS::LOG_TRACE);
	testBytesDefault();

	// function reference return
	HEAD("TestBytes: function reference return", RNS::LOG_TRACE);
	{
		RNS::Bytes test = testBytesReference();
		TRACE("returned");
		TEST_ASSERT_TRUE(test);
		TEST_ASSERT_EQUAL_size_t(4, test.size());
		TEST_ASSERT_EQUAL_MEMORY("Test", test.data(), test.size());
	}

	// TODO test comparison

}

void testCowBytes() {

	RNS::Bytes bytes1("1");
	TEST_ASSERT_EQUAL_size_t(1, bytes1.size());
	TEST_ASSERT_EQUAL_MEMORY("1", bytes1.data(), bytes1.size());

	RNS::Bytes bytes2(bytes1);
	TEST_ASSERT_EQUAL_size_t(1, bytes2.size());
	TEST_ASSERT_EQUAL_MEMORY("1", bytes2.data(), bytes2.size());
	TEST_ASSERT_EQUAL_PTR(bytes1.data(), bytes2.data());

	RNS::Bytes bytes3(bytes2);
	TEST_ASSERT_EQUAL_size_t(1, bytes3.size());
	TEST_ASSERT_EQUAL_MEMORY("1", bytes3.data(), bytes3.size());
	TEST_ASSERT_EQUAL_PTR(bytes2.data(), bytes3.data());

	TRACE("pre bytes1 ptr: " + std::to_string((uintptr_t)bytes1.data()) + " data: " + bytes1.toString());
	TRACE("pre bytes2 ptr: " + std::to_string((uintptr_t)bytes2.data()) + " data: " + bytes2.toString());
	TRACE("pre bytes3 ptr: " + std::to_string((uintptr_t)bytes3.data()) + " data: " + bytes3.toString());

	//bytes1.append("mississippi");
	//assert(bytes1.size() == 12);
	//assert(memcmp(bytes1.data(), "1mississippi", bytes1.size()) == 0);
	//assert(bytes1.data() != bytes2.data());

	bytes2.append("mississippi");
	TEST_ASSERT_EQUAL_size_t(12, bytes2.size());
	TEST_ASSERT_EQUAL_MEMORY("1mississippi", bytes2.data(), bytes2.size());
	TEST_ASSERT_NOT_EQUAL(bytes1.data(), bytes2.data());

	bytes3.assign("mississippi");
	TEST_ASSERT_EQUAL_size_t(11, bytes3.size());
	TEST_ASSERT_EQUAL_MEMORY("mississippi", bytes3.data(), bytes3.size());
	TEST_ASSERT_NOT_EQUAL(bytes2.data(), bytes3.data());

	TRACE("post bytes1 ptr: " + std::to_string((uintptr_t)bytes1.data()) + " data: " + bytes1.toString());
	TRACE("post bytes2 ptr: " + std::to_string((uintptr_t)bytes2.data()) + " data: " + bytes2.toString());
	TRACE("post bytes3 ptr: " + std::to_string((uintptr_t)bytes3.data()) + " data: " + bytes3.toString());
}

void testBytesConversion() {

	{
		RNS::Bytes bytes("Hello World");
		std::string hex = bytes.toHex(true);
		TRACE("text: \"" + bytes.toString() + "\" upper hex: \"" + hex + "\"");
		TEST_ASSERT_EQUAL_size_t(22, hex.length());
		TEST_ASSERT_EQUAL_STRING("48656C6C6F20576F726C64", hex.c_str());
	}
	{
		RNS::Bytes bytes("Hello World");
		std::string hex = bytes.toHex(false);
		TRACE("text: \"" + bytes.toString() + "\" lower hex: \"" + hex + "\"");
		TEST_ASSERT_EQUAL_size_t(22, hex.length());
		TEST_ASSERT_EQUAL_STRING("48656c6c6f20576f726c64", hex.c_str());
	}
	{
		std::string hex("48656C6C6F20576F726C64");
		RNS::Bytes bytes;
		bytes.assignHex(hex.c_str());
		std::string text = bytes.toString();
		TRACE("hex: \"" + hex + "\" text: \"" + text + "\"");
		TEST_ASSERT_EQUAL_size_t(11, text.length());
		TEST_ASSERT_EQUAL_STRING("Hello World", text.c_str());
	}
	{
		std::string hex("48656c6c6f20576f726c64");
		RNS::Bytes bytes;
		bytes.assignHex(hex.c_str());
		std::string text = bytes.toString();
		TRACE("hex: \"" + hex + "\" text: \"" + text + "\"");
		TEST_ASSERT_EQUAL_size_t(11, text.length());
		TEST_ASSERT_EQUAL_STRING("Hello World", text.c_str());

		// overwrite
		bytes.assignHex(hex.c_str());
		text = bytes.toString();
		TRACE("hex: \"" + hex + "\" text: \"" + text + "\"");
		TEST_ASSERT_EQUAL_size_t(11, text.length());
		TEST_ASSERT_EQUAL_STRING("Hello World", text.c_str());

		// apend
		bytes.appendHex(hex.c_str());
		text = bytes.toString();
		TRACE("hex: \"" + hex + "\" text: \"" + text + "\"");
		TEST_ASSERT_EQUAL_size_t(22, text.length());
		TEST_ASSERT_EQUAL_STRING("Hello WorldHello World", text.c_str());
	}

}

void testBytesResize() {
	TRACE("Testing downsize of Bytes with initial capacity");

	RNS::Bytes bytes(1024);
	TEST_ASSERT_FALSE(bytes);
	TEST_ASSERT_EQUAL_size_t(0, bytes.size());
	TEST_ASSERT_EQUAL_size_t(1024, bytes.capacity());
	TEST_ASSERT_NOT_NULL(bytes.data());

	uint8_t* buffer = bytes.writable(bytes.capacity());
	TEST_ASSERT_TRUE(bytes);
	TEST_ASSERT_EQUAL_size_t(1024, bytes.size());
	TEST_ASSERT_EQUAL_size_t(1024, bytes.capacity());

	TEST_ASSERT_NOT_NULL(buffer);
	memcpy(buffer, "Hello World", 11);
	bytes.resize(11);
	TEST_ASSERT_EQUAL_size_t(11, bytes.size());
	TEST_ASSERT_EQUAL_size_t(1024, bytes.capacity());
}

void testBytesStream() {

	const uint8_t prestr[] = "Hello";
	const uint8_t poststr[] = " World";

	const RNS::Bytes prebuf(prestr, 5);
	TEST_ASSERT_TRUE(prebuf);
	TEST_ASSERT_EQUAL_size_t(5, prebuf.size());
	TEST_ASSERT_EQUAL_MEMORY("Hello", prebuf.data(), prebuf.size());

	const RNS::Bytes postbuf(poststr, 6);
	TEST_ASSERT_TRUE(postbuf);
	TEST_ASSERT_EQUAL_size_t(6, postbuf.size());
	TEST_ASSERT_EQUAL_MEMORY(" World", postbuf.data(), postbuf.size());

	// stream into empty bytes
	HEAD("TestBytes: stream into empty bytes", RNS::LOG_TRACE);
	{
		RNS::Bytes strmbuf;
		strmbuf << prebuf << postbuf;
		TRACE("Results:");
		TRACEF("stream prebuf: %s", prebuf.toString().c_str());
		TRACEF("stream postbuf: %s", postbuf.toString().c_str());
		TRACEF("stream strmbuf: %s", strmbuf.toString().c_str());
		TEST_ASSERT_EQUAL_size_t(11, strmbuf.size());
		TEST_ASSERT_EQUAL_MEMORY("Hello World", strmbuf.data(), strmbuf.size());
		TEST_ASSERT_EQUAL_size_t(5, prebuf.size());
		TEST_ASSERT_EQUAL_MEMORY("Hello", prebuf.data(), prebuf.size());
		TEST_ASSERT_EQUAL_size_t(6, postbuf.size());
		TEST_ASSERT_EQUAL_MEMORY(" World", postbuf.data(), postbuf.size());
	}

	// stream into reserved empty bytes
	HEAD("TestBytes: stream into reserved empty bytes", RNS::LOG_TRACE);
	{
		RNS::Bytes strmbuf(256);
		//strmbuf << prebuf << postbuf;
		strmbuf << prebuf;
		strmbuf << postbuf;
		TRACE("Results:");
		//TRACEF("stream prebuf: %s", prebuf.toString().c_str());
		//TRACEF("stream postbuf: %s", postbuf.toString().c_str());
		//TRACEF("stream strmbuf: %s", strmbuf.toString().c_str());
		TEST_ASSERT_EQUAL_size_t(11, strmbuf.size());
		TEST_ASSERT_EQUAL_MEMORY("Hello World", strmbuf.data(), strmbuf.size());
		TEST_ASSERT_EQUAL_size_t(5, prebuf.size());
		TEST_ASSERT_EQUAL_MEMORY("Hello", prebuf.data(), prebuf.size());
		TEST_ASSERT_EQUAL_size_t(6, postbuf.size());
		TEST_ASSERT_EQUAL_MEMORY(" World", postbuf.data(), postbuf.size());
	}

	// stream into populated bytes
	HEAD("TestBytes: stream into populated bytes", RNS::LOG_TRACE);
	{
		RNS::Bytes strmbuf("Stream ");
		TEST_ASSERT_TRUE(strmbuf);
		TEST_ASSERT_EQUAL_size_t(7, strmbuf.size());
		TEST_ASSERT_EQUAL_MEMORY("Stream ", strmbuf.data(), strmbuf.size());

		strmbuf << prebuf << postbuf;
		TRACE("Results:");
		TRACEF("stream prebuf: %s", prebuf.toString().c_str());
		TRACEF("stream postbuf: %s", postbuf.toString().c_str());
		TRACEF("stream strmbuf: %s", strmbuf.toString().c_str());
		TEST_ASSERT_EQUAL_size_t(18, strmbuf.size());
		TEST_ASSERT_EQUAL_MEMORY("Stream Hello World", strmbuf.data(), strmbuf.size());
		TEST_ASSERT_EQUAL_size_t(5, prebuf.size());
		TEST_ASSERT_EQUAL_MEMORY("Hello", prebuf.data(), prebuf.size());
		TEST_ASSERT_EQUAL_size_t(6, postbuf.size());
		TEST_ASSERT_EQUAL_MEMORY(" World", postbuf.data(), postbuf.size());
	}

	// stream with assignment
	HEAD("TestBytes: stream with assignment", RNS::LOG_TRACE);
	// NOTE: This test demonstrates a side-effect of the stream insertion operator!!!
	//       When pre is volatile (non-const) then it gets updated in the process of inserting both pre and post.
	//       This is a known and correct but perhaps unexpected and non-intuitive side-effect of assignment with stream.
	//       To counter this side-effect, intermediate Bytes in a stream insertion chain must be const to avoid being modified.
	{
		// NOTE pre must be volatile in order for below stream with assignment to work (since it gets modified)
		RNS::Bytes pre("Hello");
		TEST_ASSERT_EQUAL_size_t(5, pre.size());
		const RNS::Bytes post(" World");
		TEST_ASSERT_EQUAL_size_t(6, post.size());
		RNS::Bytes strmbuf = pre << post;
		TRACE("Results:");
		TRACEF("stream pre: %s", prebuf.toString().c_str());
		TRACEF("stream post: %s", postbuf.toString().c_str());
		TRACEF("stream strmbuf: %s", strmbuf.toString().c_str());
		TEST_ASSERT_EQUAL_size_t(11, strmbuf.size());
		TEST_ASSERT_EQUAL_MEMORY("Hello World", strmbuf.data(), strmbuf.size());
		TEST_ASSERT_EQUAL_size_t(11, pre.size());
		TEST_ASSERT_EQUAL_MEMORY("Hello World", pre.data(), pre.size());
		TEST_ASSERT_EQUAL_size_t(6, post.size());
		TEST_ASSERT_EQUAL_MEMORY(" World", post.data(), post.size());
	}

}

void testBytesReserve() {

	RNS::Bytes src("Hello");
	uint8_t hops = 32;

	{
		RNS::Bytes dst = src.left(1);
		dst << hops;
		dst << src.mid(2);
		TRACEF("non-reserve: %s", dst.toString().c_str());
		TEST_ASSERT_EQUAL_size_t(5, dst.size());
		TEST_ASSERT_EQUAL_MEMORY("H llo", dst.data(), dst.size());
	}

	{
		RNS::Bytes dst(512);
		dst << src.left(1);
		dst << hops;
		dst << src.mid(2);
		TRACEF("reserve: %s", dst.toString().c_str());
		TEST_ASSERT_EQUAL_size_t(5, dst.size());
		TEST_ASSERT_EQUAL_MEMORY("H llo", dst.data(), dst.size());
	}

}

void testFind() {
	HEAD("testFind:", RNS::LOG_TRACE);
	RNS::Bytes src("Hello");
	TEST_ASSERT_EQUAL_INT(0, src.find("H"));
	TEST_ASSERT_EQUAL_INT(-1, src.find(1, "H"));
	TEST_ASSERT_EQUAL_INT(2, src.find("ll"));
	TEST_ASSERT_EQUAL_INT(2, src.find(1, "ll"));
	TEST_ASSERT_EQUAL_INT(2, src.find(2, "ll"));
	TEST_ASSERT_EQUAL_INT(-1, src.find(3, "ll"));
	TEST_ASSERT_EQUAL_INT(-1, src.find(32, "ll"));
	TEST_ASSERT_EQUAL_INT(-1, src.find("foo"));
}

void testCompare() {
	HEAD("testCompare:", RNS::LOG_TRACE);
	RNS::Bytes bytes("Hello\x20World");
	//RNS::Bytes bytes("Hello World");
	TRACEF("testCompare: %s", bytes.toString().c_str());
	TEST_ASSERT_TRUE(bytes == RNS::Bytes("Hello World"));
	TEST_ASSERT_TRUE(bytes == RNS::Bytes("Hello\x20World"));
	TEST_ASSERT_TRUE(bytes == "Hello World");
	TEST_ASSERT_TRUE(bytes == "Hello\x20World");
}

void testConcat() {
	HEAD("testConcat:", RNS::LOG_TRACE);

	const RNS::Bytes bytes1("Hello");
	const RNS::Bytes bytes2(" World");

	{
		TRACE("testConcat: prefix");
		RNS::Bytes bytes("Prefix ");
	}

	// CBA NOTE: Addition-assignment operator is the only case in this test that involves
	//           an extra/intermediate object creation and should therfore be avoided.
	{
		TRACE("testConcat: prefix addition-assignment with addition");
		RNS::Bytes bytes("Prefix ");
		bytes += bytes1 + bytes2;
		//TRACEF("testConcat: bytes: %s", bytes.toString().c_str());
		TEST_ASSERT_EQUAL_size_t(18, bytes.size());
		TEST_ASSERT_EQUAL_MEMORY("Prefix Hello World", bytes.data(), bytes.size());
		TEST_ASSERT_EQUAL_MEMORY("Hello", bytes1.data(), bytes1.size());
		TEST_ASSERT_EQUAL_MEMORY(" World", bytes2.data(), bytes2.size());
	}

	// CBA NOTE: Extreme care must be excercised when using stream insertion since intermediate
	//           Bytes that are volatile (non-const) will be modified in the process!!!
	{
		TRACE("testConcat: prefix stream");
		RNS::Bytes bytes("Prefix ");
		bytes << bytes1 << bytes2;
		//TRACEF("testConcat: bytes: %s", bytes.toString().c_str());
		TEST_ASSERT_EQUAL_size_t(18, bytes.size());
		TEST_ASSERT_EQUAL_MEMORY("Prefix Hello World", bytes.data(), bytes.size());
		TEST_ASSERT_EQUAL_MEMORY("Hello", bytes1.data(), bytes1.size());
		TEST_ASSERT_EQUAL_MEMORY(" World", bytes2.data(), bytes2.size());
	}

	{
		TRACE("testConcat: assignment with addition");
		const RNS::Bytes bytes = bytes1 + bytes2;
		//TRACEF("testConcat: bytes: %s", bytes.toString().c_str());
		TEST_ASSERT_EQUAL_size_t(11, bytes.size());
		TEST_ASSERT_EQUAL_MEMORY("Hello World", bytes.data(), bytes.size());
		TEST_ASSERT_EQUAL_MEMORY("Hello", bytes1.data(), bytes1.size());
		TEST_ASSERT_EQUAL_MEMORY(" World", bytes2.data(), bytes2.size());
	}

	// CBA NOTE: Following construct with assignment using stream insertion is NOT WORKING!!!
	//           Using volatile (non-const) Bytes instead does work in this fashion, but that
	//           has side-effects as stated above.
	// CBA NOTE: Also curious why Bytes constructor with capacity is being invoked here.
/*
	{
		TRACE("testConcat: assignment with stream");
		const RNS::Bytes bytes = bytes1 << bytes2;
		//TRACEF("testConcat: bytes: %s", bytes.toString().c_str());
		TEST_ASSERT_EQUAL_size_t(11, bytes.size());
		TEST_ASSERT_EQUAL_MEMORY("Hello World", bytes.data(), bytes.size());
		TEST_ASSERT_EQUAL_MEMORY("Hello", bytes1.data(), bytes1.size());
		TEST_ASSERT_EQUAL_MEMORY(" World", bytes2.data(), bytes2.size());
	}
*/

	{
		TRACE("testConcat: empty with separate stream");
		RNS::Bytes bytes;
		bytes << bytes1;
		bytes << bytes2;
		//TRACEF("testConcat: bytes: %s", bytes.toString().c_str());
		TEST_ASSERT_EQUAL_size_t(11, bytes.size());
		TEST_ASSERT_EQUAL_MEMORY("Hello World", bytes.data(), bytes.size());
		TEST_ASSERT_EQUAL_MEMORY("Hello", bytes1.data(), bytes1.size());
		TEST_ASSERT_EQUAL_MEMORY(" World", bytes2.data(), bytes2.size());
	}

	{
		TRACE("testConcat: construct with addition");
		const RNS::Bytes bytes(bytes1 + bytes2);
		//TRACEF("testConcat: bytes: %s", bytes.toString().c_str());
		TEST_ASSERT_EQUAL_size_t(11, bytes.size());
		TEST_ASSERT_EQUAL_MEMORY("Hello World", bytes.data(), bytes.size());
		TEST_ASSERT_EQUAL_MEMORY("Hello", bytes1.data(), bytes1.size());
		TEST_ASSERT_EQUAL_MEMORY(" World", bytes2.data(), bytes2.size());
	}

}

void testIndex() {
	HEAD("testConcat:", RNS::LOG_TRACE);

	const RNS::Bytes bytes("Hello");
	TEST_ASSERT_EQUAL_UINT8('H', bytes[0]);
	TEST_ASSERT_EQUAL_UINT8('e', bytes[1]);
	TEST_ASSERT_EQUAL_UINT8('l', bytes[2]);
	TEST_ASSERT_EQUAL_UINT8('l', bytes[3]);
	TEST_ASSERT_EQUAL_UINT8('o', bytes[4]);
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
	RNS::Utilities::OS::dump_heap_stats();
	size_t pre_memory = RNS::Utilities::OS::heap_available();
	TRACEF("testBytes: pre-mem: %u", pre_memory);

	// Run tests
	RUN_TEST(testBytesMain);
	RUN_TEST(testCowBytes);
	RUN_TEST(testBytesConversion);
	RUN_TEST(testBytesResize);
	RUN_TEST(testBytesStream);
	RUN_TEST(testBytesReserve);
	RUN_TEST(testFind);
	RUN_TEST(testCompare);
	RUN_TEST(testConcat);
	RUN_TEST(testIndex);

	// Suite-level teardown
	size_t post_memory = RNS::Utilities::OS::heap_available();
	size_t diff_memory = (int)pre_memory - (int)post_memory;
	TRACEF("testBytes: post-mem: %u", post_memory);
	TRACEF("testBytes: diff-mem: %u", diff_memory);
	TEST_ASSERT_EQUAL_size_t(0, diff_memory);

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
