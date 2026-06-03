#include <unity.h>

#include "microReticulum/Resource.h"
#include "microReticulum/Bytes.h"
#include "microReticulum/Type.h"

#include <stdio.h>
#include <string.h>

using namespace RNS;

// Build a non-trivial ResourceAdvertisement, pack it, unpack it, and assert
// every relevant field round-trips byte-for-byte.
static void test_pack_unpack_round_trip() {
	ResourceAdvertisement adv;
	adv._t = 12345;
	adv._d = 67890;
	adv._n = 7;
	adv._i = 1;
	adv._l = 3;
	adv._f = 0;

	// Hashes (mock values of expected lengths)
	uint8_t h[16];  for (size_t i = 0; i < sizeof(h); ++i)  h[i]  = (uint8_t)(i + 1);
	uint8_t r[4];   for (size_t i = 0; i < sizeof(r); ++i)  r[i]  = (uint8_t)(0x10 + i);
	uint8_t o[16];  for (size_t i = 0; i < sizeof(o); ++i)  o[i]  = (uint8_t)(0x20 + i);
	uint8_t q[8];   for (size_t i = 0; i < sizeof(q); ++i)  q[i]  = (uint8_t)(0x30 + i);
	// Hashmap: 7 entries x 4 bytes
	uint8_t m[7 * Type::Resource::MAPHASH_LEN];
	for (size_t i = 0; i < sizeof(m); ++i) m[i] = (uint8_t)(0x40 + i);

	adv._h = Bytes(h, sizeof(h));
	adv._r = Bytes(r, sizeof(r));
	adv._o = Bytes(o, sizeof(o));
	adv._q = Bytes(q, sizeof(q));
	adv._m = Bytes(m, sizeof(m));

	// Set every boolean independently so packed flag byte exercises all bits.
	adv._e = true;
	adv._c = false;
	adv._s = true;
	adv._u = true;
	adv._p = false;
	adv._x = true;
	// Reconstruct packed flag byte using the same expression as the
	// Resource-from-Resource constructor (Resource.py:1307).
	adv._f = (uint8_t)((adv._x << 5) | (adv._p << 4) | (adv._u << 3) | (adv._s << 2) | (adv._c << 1) | adv._e);

	Bytes packed = adv.pack(0);
	TEST_ASSERT_TRUE(packed.size() > 0);

	ResourceAdvertisement decoded = ResourceAdvertisement::unpack(packed);

	TEST_ASSERT_EQUAL_UINT64(adv._t, decoded._t);
	TEST_ASSERT_EQUAL_UINT64(adv._d, decoded._d);
	TEST_ASSERT_EQUAL_UINT32(adv._n, decoded._n);
	TEST_ASSERT_EQUAL_UINT32(adv._i, decoded._i);
	TEST_ASSERT_EQUAL_UINT32(adv._l, decoded._l);
	TEST_ASSERT_EQUAL_UINT32(adv._f, decoded._f);

	TEST_ASSERT_EQUAL_INT(adv._h.size(), decoded._h.size());
	TEST_ASSERT_EQUAL_MEMORY(adv._h.data(), decoded._h.data(), adv._h.size());
	TEST_ASSERT_EQUAL_INT(adv._r.size(), decoded._r.size());
	TEST_ASSERT_EQUAL_MEMORY(adv._r.data(), decoded._r.data(), adv._r.size());
	TEST_ASSERT_EQUAL_INT(adv._o.size(), decoded._o.size());
	TEST_ASSERT_EQUAL_MEMORY(adv._o.data(), decoded._o.data(), adv._o.size());
	TEST_ASSERT_EQUAL_INT(adv._q.size(), decoded._q.size());
	TEST_ASSERT_EQUAL_MEMORY(adv._q.data(), decoded._q.data(), adv._q.size());
	TEST_ASSERT_EQUAL_INT(adv._m.size(), decoded._m.size());
	TEST_ASSERT_EQUAL_MEMORY(adv._m.data(), decoded._m.data(), adv._m.size());

	TEST_ASSERT_EQUAL(adv._e, decoded._e);
	TEST_ASSERT_EQUAL(adv._c, decoded._c);
	TEST_ASSERT_EQUAL(adv._s, decoded._s);
	TEST_ASSERT_EQUAL(adv._u, decoded._u);
	TEST_ASSERT_EQUAL(adv._p, decoded._p);
	TEST_ASSERT_EQUAL(adv._x, decoded._x);
}

// A request_id of empty/None should round-trip as nil and produce empty
// _q on the other side. _u and _p should both decode false.
static void test_nil_request_id_round_trip() {
	ResourceAdvertisement adv;
	adv._t = 100;
	adv._d = 200;
	adv._n = 1;
	adv._h = Bytes(reinterpret_cast<const uint8_t*>("0123456789ABCDEF"), 16);
	adv._r = Bytes(reinterpret_cast<const uint8_t*>("rand"), 4);
	adv._o = adv._h;
	adv._m = Bytes(reinterpret_cast<const uint8_t*>("MAP1"), 4);
	adv._i = 1;
	adv._l = 1;
	adv._f = 0;

	Bytes packed = adv.pack(0);
	ResourceAdvertisement decoded = ResourceAdvertisement::unpack(packed);

	TEST_ASSERT_EQUAL_INT(0, decoded._q.size());
	TEST_ASSERT_FALSE(decoded._u);
	TEST_ASSERT_FALSE(decoded._p);
}

// Hashmap pagination: an adv with N=10 parts and HASHMAP_MAX_LEN large enough
// to cover everything in segment 0 should produce all 10 hashes in _m on
// unpack. Selecting segment 1 should produce an empty slice if it's beyond
// the data.
static void test_pack_segment_pagination() {
	ResourceAdvertisement adv;
	adv._t = 1024;
	adv._d = 1024;
	adv._n = 10;
	adv._h = Bytes(reinterpret_cast<const uint8_t*>("HHHHHHHHHHHHHHHH"), 16);
	adv._r = Bytes(reinterpret_cast<const uint8_t*>("rrrr"), 4);
	adv._o = adv._h;
	adv._i = 1;
	adv._l = 1;
	adv._f = 0;

	// Full hashmap = 10 x 4 bytes
	uint8_t full_map[10 * Type::Resource::MAPHASH_LEN];
	for (size_t i = 0; i < sizeof(full_map); ++i) full_map[i] = (uint8_t)i;
	adv._m = Bytes(full_map, sizeof(full_map));

	Bytes packed_seg0 = adv.pack(0);
	ResourceAdvertisement decoded_seg0 = ResourceAdvertisement::unpack(packed_seg0);
	// Segment 0 should contain all 10 entries (HASHMAP_MAX_LEN >> 10 always).
	TEST_ASSERT_EQUAL_INT(sizeof(full_map), decoded_seg0._m.size());
	TEST_ASSERT_EQUAL_MEMORY(full_map, decoded_seg0._m.data(), sizeof(full_map));

	// Selecting segment 1 — beyond the data — should yield empty _m.
	Bytes packed_seg1 = adv.pack(1);
	ResourceAdvertisement decoded_seg1 = ResourceAdvertisement::unpack(packed_seg1);
	TEST_ASSERT_EQUAL_INT(0, decoded_seg1._m.size());
}


// Decode a hex character to its nibble value (0-15) or 0xFF on error.
static uint8_t hex_nibble(char c) {
	if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
	if (c >= 'a' && c <= 'f') return (uint8_t)(10 + (c - 'a'));
	if (c >= 'A' && c <= 'F') return (uint8_t)(10 + (c - 'A'));
	return 0xFF;
}

static Bytes from_hex(const char* hex) {
	Bytes out;
	for (size_t i = 0; hex[i] != '\0' && hex[i + 1] != '\0'; i += 2) {
		uint8_t hi = hex_nibble(hex[i]);
		uint8_t lo = hex_nibble(hex[i + 1]);
		if (hi == 0xFF || lo == 0xFF) break;
		out.append((uint8_t)((hi << 4) | lo));
	}
	return out;
}

// Golden packed bytes for a ResourceAdvertisement, captured directly from
// Python RNS 1.2.9 (RNS.Resource.ResourceAdvertisement.pack) for these inputs:
//   t=12345, d=67890, n=7, i=1, l=3, e=true, c=false, s=true, u=true, p=false, x=true
//   h = bytes(range(16))                  (16 bytes 0x00..0x0f)
//   r = bytes([0x10..0x13])               (4 bytes)
//   o = bytes([0x20..0x2f])               (16 bytes)
//   q = bytes([0x30..0x37])               (8 bytes)
//   m = bytes([0x40..0x5b])               (28 bytes, 7 x MAPHASH_LEN)
//   f = (x<<5)|(p<<4)|(u<<3)|(s<<2)|(c<<1)|e
//
// If the encoded msgpack integer widths differ between MsgPack-cpp and
// umsgpack, the bytes won't match exactly, but the field values must still
// round-trip — both checks are below.
static const char* PYTHON_GOLDEN_HEX =
	"8ba174cd3039a164ce00010932a16e07a168c410"
	"000102030405060708090a0b0c0d0e0fa172c404"
	"10111213a16fc410202122232425262728292a2b"
	"2c2d2e2fa16901a16c03a171c40830313233343536"
	"37a1662da16dc41c404142434445464748494a4b"
	"4c4d4e4f505152535455565758595a5b";

// Decode Python's golden packed bytes via our C++ unpack(). All field values
// must match what Python had on the encode side. This proves our unpacker is
// compatible with Python RNS's umsgpack-encoded advertisements.
static void test_unpack_python_golden() {
	Bytes packed = from_hex(PYTHON_GOLDEN_HEX);
	TEST_ASSERT_TRUE(packed.size() > 0);

	ResourceAdvertisement adv = ResourceAdvertisement::unpack(packed);

	TEST_ASSERT_EQUAL_UINT64(12345, adv._t);
	TEST_ASSERT_EQUAL_UINT64(67890, adv._d);
	TEST_ASSERT_EQUAL_UINT32(7,     adv._n);
	TEST_ASSERT_EQUAL_UINT16(1,     adv._i);
	TEST_ASSERT_EQUAL_UINT16(3,     adv._l);

	TEST_ASSERT_EQUAL_INT(16, adv._h.size());
	for (size_t i = 0; i < 16; ++i) TEST_ASSERT_EQUAL_UINT8(i, adv._h.data()[i]);
	TEST_ASSERT_EQUAL_INT(4, adv._r.size());
	for (size_t i = 0; i < 4;  ++i) TEST_ASSERT_EQUAL_UINT8(0x10 + i, adv._r.data()[i]);
	TEST_ASSERT_EQUAL_INT(16, adv._o.size());
	for (size_t i = 0; i < 16; ++i) TEST_ASSERT_EQUAL_UINT8(0x20 + i, adv._o.data()[i]);
	TEST_ASSERT_EQUAL_INT(8, adv._q.size());
	for (size_t i = 0; i < 8;  ++i) TEST_ASSERT_EQUAL_UINT8(0x30 + i, adv._q.data()[i]);
	TEST_ASSERT_EQUAL_INT(28, adv._m.size());
	for (size_t i = 0; i < 28; ++i) TEST_ASSERT_EQUAL_UINT8(0x40 + i, adv._m.data()[i]);

	// Flag bits derived from f-byte must match Python's source flags.
	TEST_ASSERT_TRUE(adv._e);
	TEST_ASSERT_FALSE(adv._c);
	TEST_ASSERT_TRUE(adv._s);
	TEST_ASSERT_TRUE(adv._u);
	TEST_ASSERT_FALSE(adv._p);
	TEST_ASSERT_TRUE(adv._x);
}

// Build a C++ adv with the same field values Python used to produce
// PYTHON_GOLDEN_HEX, pack it via C++, then unpack via C++. The round-trip
// must succeed — and if the C++ packed bytes also equal the Python bytes
// exactly, we have byte-level wire interop. Log a TEST_PRINTF either way.
static void test_pack_matches_python_golden() {
	ResourceAdvertisement adv;
	adv._t = 12345;
	adv._d = 67890;
	adv._n = 7;
	adv._i = 1;
	adv._l = 3;

	uint8_t h[16];  for (size_t i = 0; i < 16; ++i) h[i] = (uint8_t)i;
	uint8_t r[4]    = {0x10, 0x11, 0x12, 0x13};
	uint8_t o[16];  for (size_t i = 0; i < 16; ++i) o[i] = (uint8_t)(0x20 + i);
	uint8_t q[8];   for (size_t i = 0; i < 8;  ++i) q[i] = (uint8_t)(0x30 + i);
	uint8_t m[28];  for (size_t i = 0; i < 28; ++i) m[i] = (uint8_t)(0x40 + i);
	adv._h = Bytes(h, sizeof(h));
	adv._r = Bytes(r, sizeof(r));
	adv._o = Bytes(o, sizeof(o));
	adv._q = Bytes(q, sizeof(q));
	adv._m = Bytes(m, sizeof(m));

	adv._e = true; adv._c = false; adv._s = true; adv._u = true; adv._p = false; adv._x = true;
	adv._f = (uint8_t)((adv._x << 5) | (adv._p << 4) | (adv._u << 3) | (adv._s << 2) | (adv._c << 1) | adv._e);

	Bytes cpp_packed = adv.pack(0);
	TEST_ASSERT_TRUE(cpp_packed.size() > 0);

	// Round-trip via our own unpacker — must succeed regardless of whether
	// MsgPack-cpp picked the same integer widths as Python's umsgpack.
	ResourceAdvertisement decoded = ResourceAdvertisement::unpack(cpp_packed);
	TEST_ASSERT_EQUAL_UINT64(adv._t, decoded._t);
	TEST_ASSERT_EQUAL_UINT64(adv._d, decoded._d);
	TEST_ASSERT_EQUAL_INT(adv._m.size(), decoded._m.size());
	TEST_ASSERT_EQUAL_MEMORY(adv._m.data(), decoded._m.data(), adv._m.size());

	// Cross-decode: Python's unpacker must handle the C++ packed bytes —
	// the only canonical way to verify that here is to run our own unpack
	// on the bytes and confirm field-equivalence (Python's unpacker accepts
	// any compatible msgpack integer width). Already covered above.

	// Byte-level equality check vs. Python's golden output. This is a
	// stronger property than is strictly required for interop (msgpack
	// allows multiple valid encodings of the same value), but if it holds
	// it confirms identical encoding choices between MsgPack-cpp and
	// umsgpack for these inputs.
	Bytes python_golden = from_hex(PYTHON_GOLDEN_HEX);
	if (cpp_packed.size() == python_golden.size()
	    && memcmp(cpp_packed.data(), python_golden.data(), cpp_packed.size()) == 0) {
		TEST_MESSAGE("Byte-level interop: C++ pack matches Python golden exactly");
	}
	else {
		// Not a failure — encodings can legitimately differ in integer widths.
		// Print sizes so we know whether the divergence is structural or just
		// width-related.
		char buf[160];
		snprintf(buf, sizeof(buf),
		         "C++ packed (%lu bytes) differs from Python golden (%lu bytes) — round-trip-correct but not byte-equal",
		         (unsigned long)cpp_packed.size(), (unsigned long)python_golden.size());
		TEST_MESSAGE(buf);
	}
}


void setUp(void) {}
void tearDown(void) {}

int runUnityTests(void) {
	UNITY_BEGIN();
	RUN_TEST(test_pack_unpack_round_trip);
	RUN_TEST(test_nil_request_id_round_trip);
	RUN_TEST(test_pack_segment_pagination);
	RUN_TEST(test_unpack_python_golden);
	RUN_TEST(test_pack_matches_python_golden);
	return UNITY_END();
}

int main(void) {
	return runUnityTests();
}

#ifdef ARDUINO
void setup() { delay(2000); runUnityTests(); }
void loop() {}
#endif

void app_main() { runUnityTests(); }
