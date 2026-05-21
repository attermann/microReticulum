#include <unity.h>

#include "Resource.h"
#include "Bytes.h"
#include "Type.h"

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


void setUp(void) {}
void tearDown(void) {}

int runUnityTests(void) {
	UNITY_BEGIN();
	RUN_TEST(test_pack_unpack_round_trip);
	RUN_TEST(test_nil_request_id_round_trip);
	RUN_TEST(test_pack_segment_pagination);
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
