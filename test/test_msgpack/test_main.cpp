#include <unity.h>

#define MSGPACK_DEBUGLOG_ENABLE 0
#define DEBUGLOG_DEFAULT_LOG_LEVEL_TRACE
#include <MsgPack.h>
#include "Bytes.h"

#include <string.h>
#include <stdint.h>

// Use MsgPack's native binary type
using BinaryBuffer = MsgPack::bin_t<uint8_t>;

void setUp(void) {
    // set stuff up here before each test
}

void tearDown(void) {
    // clean stuff up here after each test
}

void test_msgpack_array_packing(void) {
    // Test data setup
    float testFloat = 3.14159f;
    
    // Create binary buffers using MsgPack's bin_t type
    BinaryBuffer buffer1;
    buffer1.resize(5);
    memcpy(buffer1.data(), "Hello", 5);
    
    BinaryBuffer buffer2;
    buffer2.resize(5);
    memcpy(buffer2.data(), "World", 5);
    
    // Pack the array using MsgPack
    MsgPack::Packer packer;
    packer.to_array(testFloat, buffer1, buffer2);
    
    // Get the packed data
    size_t packed_size = packer.size();
    const uint8_t* packed_data = packer.data();
    
    // Basic validation - should have data
    TEST_ASSERT_GREATER_THAN(0, packed_size);
    TEST_ASSERT_NOT_NULL(packed_data);
    
    // Expected MsgPack format:
    // - Array header (fixarray with 3 elements): 0x93
    // - Float (float32): 0xca + 4 bytes for 3.14159f
    // - Binary 1 (bin8): 0xc4 + length(5) + "Hello" 
    // - Binary 2 (bin8): 0xc4 + length(5) + "World"
    
    // Verify array header - fixarray with 3 elements
    TEST_ASSERT_EQUAL_HEX8(0x93, packed_data[0]);
    
    // Verify float marker (float32)
    TEST_ASSERT_EQUAL_HEX8(0xca, packed_data[1]);
    
    // Extract and verify the float value by unpacking
    MsgPack::Unpacker unpacker;
    unpacker.feed(packed_data, packed_size);
    
    float unpacked_float;
    MsgPack::bin_t<uint8_t> unpacked_buffer1;
    MsgPack::bin_t<uint8_t> unpacked_buffer2;
    
    // Unpack the array
    bool unpack_success = unpacker.from_array(unpacked_float, unpacked_buffer1, unpacked_buffer2);
    TEST_ASSERT_TRUE(unpack_success);
    
    // Verify unpacked values match original data
    TEST_ASSERT_EQUAL_FLOAT(testFloat, unpacked_float);
    
    // Verify binary buffers
    TEST_ASSERT_EQUAL_size_t(buffer1.size(), unpacked_buffer1.size());
    TEST_ASSERT_EQUAL_MEMORY(buffer1.data(), unpacked_buffer1.data(), buffer1.size());
    
    TEST_ASSERT_EQUAL_size_t(buffer2.size(), unpacked_buffer2.size()); 
    TEST_ASSERT_EQUAL_MEMORY(buffer2.data(), unpacked_buffer2.data(), buffer2.size());
    
    // Additional validation: verify total packed size makes sense
    // Array header(1) + float marker(1) + float data(4) + 
    // bin1 header(1) + bin1 length(1) + bin1 data(5) +
    // bin2 header(1) + bin2 length(1) + bin2 data(5) = 20 bytes
    TEST_ASSERT_EQUAL_size_t(20, packed_size);
    
    // Verify specific binary markers for the Hello and World strings
    // After array header(1) + float marker(1) + float data(4) = offset 6
    TEST_ASSERT_EQUAL_HEX8(0xc4, packed_data[6]);  // bin8 marker for first buffer
    TEST_ASSERT_EQUAL_HEX8(5, packed_data[7]);     // length of "Hello"
    TEST_ASSERT_EQUAL_MEMORY("Hello", &packed_data[8], 5);
    
    // After first buffer: offset 6 + 1 + 1 + 5 = 13
    TEST_ASSERT_EQUAL_HEX8(0xc4, packed_data[13]); // bin8 marker for second buffer  
    TEST_ASSERT_EQUAL_HEX8(5, packed_data[14]);    // length of "World"
    TEST_ASSERT_EQUAL_MEMORY("World", &packed_data[15], 5);
}

void test_msgpack_empty_buffer(void) {
    // Test with empty buffer
    float testFloat = -42.0f;
    BinaryBuffer emptyBuffer; // Empty buffer
    
    BinaryBuffer normalBuffer;
    normalBuffer.resize(4);
    memcpy(normalBuffer.data(), "Test", 4);
    
    MsgPack::Packer packer;
    packer.to_array(testFloat, emptyBuffer, normalBuffer);
    
    size_t packed_size = packer.size();
    const uint8_t* packed_data = packer.data();
    
    TEST_ASSERT_GREATER_THAN(0, packed_size);
    TEST_ASSERT_NOT_NULL(packed_data);
    
    // Verify by unpacking
    MsgPack::Unpacker unpacker;
    unpacker.feed(packed_data, packed_size);
    
    float unpacked_float;
    MsgPack::bin_t<uint8_t> unpacked_empty;
    MsgPack::bin_t<uint8_t> unpacked_normal;
    
    bool unpack_success = unpacker.from_array(unpacked_float, unpacked_empty, unpacked_normal);
    TEST_ASSERT_TRUE(unpack_success);
    
    TEST_ASSERT_EQUAL_FLOAT(testFloat, unpacked_float);
    TEST_ASSERT_EQUAL_size_t(0, unpacked_empty.size());
    TEST_ASSERT_EQUAL_size_t(4, unpacked_normal.size());
    TEST_ASSERT_EQUAL_MEMORY("Test", unpacked_normal.data(), 4);
}

void test_msgpack_binary_content(void) {
    // Test with binary data (not just strings)
    float testFloat = 1.0f;
    uint8_t binaryData1[] = {0x00, 0xFF, 0xAA, 0x55};
    uint8_t binaryData2[] = {0x01, 0x02, 0x03};
    
    BinaryBuffer buffer1;
    buffer1.resize(sizeof(binaryData1));
    memcpy(buffer1.data(), binaryData1, sizeof(binaryData1));
    
    BinaryBuffer buffer2;
    buffer2.resize(sizeof(binaryData2));
    memcpy(buffer2.data(), binaryData2, sizeof(binaryData2));
    
    MsgPack::Packer packer;
    packer.to_array(testFloat, buffer1, buffer2);
    
    size_t packed_size = packer.size();
    const uint8_t* packed_data = packer.data();
    
    TEST_ASSERT_GREATER_THAN(0, packed_size);
    TEST_ASSERT_NOT_NULL(packed_data);
    
    // Verify by unpacking
    MsgPack::Unpacker unpacker;
    unpacker.feed(packed_data, packed_size);
    
    float unpacked_float;
    MsgPack::bin_t<uint8_t> unpacked_buffer1;
    MsgPack::bin_t<uint8_t> unpacked_buffer2;
    
    bool unpack_success = unpacker.from_array(unpacked_float, unpacked_buffer1, unpacked_buffer2);
    TEST_ASSERT_TRUE(unpack_success);
    
    TEST_ASSERT_EQUAL_FLOAT(testFloat, unpacked_float);
    TEST_ASSERT_EQUAL_size_t(4, unpacked_buffer1.size());
    TEST_ASSERT_EQUAL_MEMORY(binaryData1, unpacked_buffer1.data(), 4);
    TEST_ASSERT_EQUAL_size_t(3, unpacked_buffer2.size());
    TEST_ASSERT_EQUAL_MEMORY(binaryData2, unpacked_buffer2.data(), 3);
}

void test_pack_bytes_array_python() {
    static const unsigned char binary[] = {
        0x93, 0xcb, 0x40, 0x5e, 0xdd, 0x2f, 0x1a, 0x9f,
        0xbe, 0x77, 0xc4, 0x06, 0x66, 0x69, 0x72, 0x73, 
        0x74, 0xab, 0xc4, 0x07, 0x73, 0x65, 0x63, 0x6f,
        0x6e, 0x64, 0xcd,
    };
	double time = 123.456;
    // Test that array containing bytes matches python equivalent
	const RNS::Bytes one("first\xab");
	const RNS::Bytes two("second\xcd");
    MsgPack::Packer packer;
	packer.to_array(time, one, two);
	RNS::Bytes data(packer.data(), packer.size());
    TEST_ASSERT_EQUAL_size_t(27, packer.size());
    TEST_ASSERT_EQUAL_MEMORY(binary, packer.data(), packer.size());
}

void test_unpack_bytes_array_python() {
    static const unsigned char binary[] = {
        0x93, 0xcb, 0x40, 0x5e, 0xdd, 0x2f, 0x1a, 0x9f,
        0xbe, 0x77, 0xc4, 0x06, 0x66, 0x69, 0x72, 0x73, 
        0x74, 0xab, 0xc4, 0x07, 0x73, 0x65, 0x63, 0x6f,
        0x6e, 0x64, 0xcd,
    };
    MsgPack::Unpacker unpacker;
    unpacker.feed(binary, sizeof(binary));
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
    TEST_ASSERT_EQUAL_FLOAT(123.456, time);
    TEST_ASSERT_EQUAL_MEMORY("first\xab", one.data(), one.size());
    TEST_ASSERT_EQUAL_MEMORY("second\xcd", two.data(), two.size());
}

void test_pack_float_python() {
    static const unsigned char binary[] = {
        0xcb, 0x40, 0x5e, 0xdd, 0x2f, 0x1a, 0x9f, 0xbe,
        0x77
    };
	double time = 123.456;
    MsgPack::Packer packer;
	packer.serialize(time);
	RNS::Bytes data(packer.data(), packer.size());
    TEST_ASSERT_EQUAL_size_t(9, packer.size());
    TEST_ASSERT_EQUAL_MEMORY(binary, packer.data(), packer.size());
}

void test_unpack_float_python() {
    static const unsigned char binary[] = {
        0xcb, 0x40, 0x5e, 0xdd, 0x2f, 0x1a, 0x9f, 0xbe,
        0x77
    };
    MsgPack::Unpacker unpacker;
    unpacker.feed(binary, sizeof(binary));
    double time;
    unpacker.deserialize(time);
    TEST_ASSERT_EQUAL_FLOAT(123.456, time);
}

int runUnityTests(void) {
    UNITY_BEGIN();
    RUN_TEST(test_msgpack_array_packing);
    RUN_TEST(test_msgpack_empty_buffer);
    RUN_TEST(test_msgpack_binary_content);
    RUN_TEST(test_pack_bytes_array_python);
    RUN_TEST(test_unpack_bytes_array_python);
    RUN_TEST(test_pack_float_python);
    RUN_TEST(test_unpack_float_python);
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
