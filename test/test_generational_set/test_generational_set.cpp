#include <unity.h>

#include "microReticulum/Bytes.h"
#include "microReticulum/Utilities/GenerationalSet.h"

#include <set>
#include <string>
#include <functional>

using RNS::Bytes;
using RNS::Utilities::GenerationalSet;

static Bytes make_hash(int n) {
	char buf[32];
	std::snprintf(buf, sizeof(buf), "hash-%08d", n);
	return Bytes(buf);
}

void test_empty_state() {
	GenerationalSet<Bytes> s;
	TEST_ASSERT_TRUE(s.empty());
	TEST_ASSERT_EQUAL_size_t(0, s.size());
	TEST_ASSERT_EQUAL_size_t(0, s.max_size());
	TEST_ASSERT_FALSE(s.contains(make_hash(1)));
	TEST_ASSERT_TRUE(s.find(make_hash(1)) == s.end());
	TEST_ASSERT_TRUE(s.begin() == s.end());
}

void test_insert_uniqueness() {
	GenerationalSet<Bytes> s;
	Bytes h = make_hash(1);

	auto r1 = s.insert(h);
	TEST_ASSERT_TRUE(r1.second);
	TEST_ASSERT_EQUAL_size_t(1, s.size());

	auto r2 = s.insert(h);
	TEST_ASSERT_FALSE(r2.second);
	TEST_ASSERT_EQUAL_size_t(1, s.size());

	TEST_ASSERT_TRUE(s.contains(h));
	TEST_ASSERT_EQUAL_size_t(1, s.count(h));
}

void test_unbounded_growth_when_max_size_zero() {
	GenerationalSet<Bytes> s;
	// max_size defaults to 0 -> no rotation
	for (int i = 0; i < 500; ++i) {
		s.insert(make_hash(i));
	}
	TEST_ASSERT_EQUAL_size_t(500, s.size());
	TEST_ASSERT_TRUE(s.contains(make_hash(0)));
	TEST_ASSERT_TRUE(s.contains(make_hash(499)));
}

void test_rotation_threshold_is_half_max_size() {
	GenerationalSet<Bytes> s;
	s.max_size(10);
	// Rotation threshold is (10+1)/2 = 5. After inserting 5 unique items,
	// the 5th insert triggers rotation: _active emptied, _previous holds 5.
	for (int i = 0; i < 4; ++i) {
		s.insert(make_hash(i));
	}
	TEST_ASSERT_EQUAL_size_t(4, s.size());

	// The 5th insert triggers rotation
	s.insert(make_hash(4));
	TEST_ASSERT_EQUAL_size_t(5, s.size());
	// All 5 should still be findable (in _previous)
	for (int i = 0; i < 5; ++i) {
		TEST_ASSERT_TRUE(s.contains(make_hash(i)));
	}
}

void test_peak_size_bounded_by_max_size() {
	GenerationalSet<Bytes> s;
	s.max_size(20);
	for (int i = 0; i < 1000; ++i) {
		s.insert(make_hash(i));
		TEST_ASSERT_LESS_OR_EQUAL_size_t(20, s.size());
	}
}

void test_sliding_window_eviction() {
	GenerationalSet<Bytes> s;
	s.max_size(10);
	// Insert 30 unique values. Threshold = 5, so rotation fires every 5 inserts.
	// After 30 inserts: 6 rotations have occurred. The last 5 are in _active,
	// the prior 5 (values 20-24) are in _previous. Values 0-19 are gone.
	for (int i = 0; i < 30; ++i) {
		s.insert(make_hash(i));
	}
	// Earliest entries should have rotated out (allow ±5 slack for rotation grain)
	for (int i = 0; i < 15; ++i) {
		TEST_ASSERT_FALSE_MESSAGE(s.contains(make_hash(i)),
			"old entry should have been rotated out");
	}
	// The newest entries should still be present
	for (int i = 25; i < 30; ++i) {
		TEST_ASSERT_TRUE_MESSAGE(s.contains(make_hash(i)),
			"recent entry should still be present");
	}
}

void test_find_searches_both_generations() {
	GenerationalSet<Bytes> s;
	s.max_size(10);
	// Insert 5 to push into _previous via rotation, then insert 2 more in _active
	for (int i = 0; i < 5; ++i) s.insert(make_hash(i));
	s.insert(make_hash(100));
	s.insert(make_hash(101));

	// 0-4 should be in _previous, 100/101 in _active. All findable.
	for (int i = 0; i < 5; ++i) {
		auto it = s.find(make_hash(i));
		TEST_ASSERT_TRUE(it != s.end());
		TEST_ASSERT_TRUE(*it == make_hash(i));
	}
	TEST_ASSERT_TRUE(s.find(make_hash(100)) != s.end());
	TEST_ASSERT_TRUE(s.find(make_hash(101)) != s.end());
	TEST_ASSERT_TRUE(s.find(make_hash(999)) == s.end());
}

void test_iteration_visits_all() {
	GenerationalSet<Bytes> s;
	s.max_size(10);
	for (int i = 0; i < 7; ++i) {
		s.insert(make_hash(i));
	}
	TEST_ASSERT_EQUAL_size_t(7, s.size());

	std::set<Bytes> visited;
	for (auto it = s.begin(); it != s.end(); ++it) {
		visited.insert(*it);
	}
	TEST_ASSERT_EQUAL_size_t(7, visited.size());
	for (int i = 0; i < 7; ++i) {
		TEST_ASSERT_TRUE(visited.find(make_hash(i)) != visited.end());
	}
}

void test_erase_by_value() {
	GenerationalSet<Bytes> s;
	s.max_size(10);
	for (int i = 0; i < 7; ++i) {
		s.insert(make_hash(i));
	}
	TEST_ASSERT_EQUAL_size_t(7, s.size());

	TEST_ASSERT_EQUAL_size_t(1, s.erase(make_hash(0)));
	TEST_ASSERT_EQUAL_size_t(0, s.erase(make_hash(0)));
	TEST_ASSERT_EQUAL_size_t(6, s.size());
	TEST_ASSERT_FALSE(s.contains(make_hash(0)));

	TEST_ASSERT_EQUAL_size_t(1, s.erase(make_hash(6)));
	TEST_ASSERT_EQUAL_size_t(5, s.size());
}

void test_no_lru_promotion() {
	GenerationalSet<Bytes> s;
	s.max_size(10);
	// Insert 5 to fill _active and trigger rotation -> _previous has [0..4]
	for (int i = 0; i < 5; ++i) s.insert(make_hash(i));
	// Now insert(0) — already in _previous. Should be reported as duplicate,
	// NOT promoted to _active.
	auto r = s.insert(make_hash(0));
	TEST_ASSERT_FALSE(r.second);
	TEST_ASSERT_EQUAL_size_t(5, s.size());
	// _active should still be empty after the rotation
	// Insert 4 more — should fill _active without triggering rotation
	for (int i = 10; i < 14; ++i) {
		s.insert(make_hash(i));
	}
	TEST_ASSERT_EQUAL_size_t(9, s.size()); // 5 in previous + 4 in active
}

void test_clear() {
	GenerationalSet<Bytes> s;
	s.max_size(10);
	for (int i = 0; i < 20; ++i) {
		s.insert(make_hash(i));
	}
	TEST_ASSERT_FALSE(s.empty());
	s.clear();
	TEST_ASSERT_TRUE(s.empty());
	TEST_ASSERT_EQUAL_size_t(0, s.size());
	TEST_ASSERT_TRUE(s.begin() == s.end());
}

void test_max_size_chaining_fluent() {
	GenerationalSet<Bytes> s;
	auto& ref = s.max_size(42);
	TEST_ASSERT_EQUAL(&s, &ref);
	TEST_ASSERT_EQUAL_size_t(42, s.max_size());
}

void test_custom_compare_int() {
	// Verify the class instantiates and works for non-Bytes types too.
	GenerationalSet<int, std::greater<int>> s;
	s.max_size(10);
	for (int i = 0; i < 5; ++i) s.insert(i);
	TEST_ASSERT_TRUE(s.contains(0));
	TEST_ASSERT_TRUE(s.contains(4));
	TEST_ASSERT_FALSE(s.contains(5));

	// Iteration within each generation should follow the greater<int> order
	// (descending). Active has [0..4] sorted descending.
	auto it = s.begin();
	int prev = *it;
	++it;
	for (; it != s.end(); ++it) {
		TEST_ASSERT_GREATER_THAN(*it, prev);
		prev = *it;
	}
}

void test_drop_in_substitutes_for_set_in_patterns() {
	// Mirror the Transport.cpp call-site patterns to confirm drop-in semantics.
	GenerationalSet<Bytes> s;
	s.max_size(100);

	Bytes h = make_hash(7);

	// duplicate detection idiom: container.find(x) == container.end()
	TEST_ASSERT_TRUE(s.find(h) == s.end());

	// insertion (return value typically ignored at call sites)
	s.insert(h);

	// duplicate detection after insert
	TEST_ASSERT_TRUE(s.find(h) != s.end());

	// idempotent re-insert
	s.insert(h);
	TEST_ASSERT_EQUAL_size_t(1, s.size());
}

void setUp(void) {}
void tearDown(void) {}

int runUnityTests(void) {
	UNITY_BEGIN();
	RUN_TEST(test_empty_state);
	RUN_TEST(test_insert_uniqueness);
	RUN_TEST(test_unbounded_growth_when_max_size_zero);
	RUN_TEST(test_rotation_threshold_is_half_max_size);
	RUN_TEST(test_peak_size_bounded_by_max_size);
	RUN_TEST(test_sliding_window_eviction);
	RUN_TEST(test_find_searches_both_generations);
	RUN_TEST(test_iteration_visits_all);
	RUN_TEST(test_erase_by_value);
	RUN_TEST(test_no_lru_promotion);
	RUN_TEST(test_clear);
	RUN_TEST(test_max_size_chaining_fluent);
	RUN_TEST(test_custom_compare_int);
	RUN_TEST(test_drop_in_substitutes_for_set_in_patterns);
	return UNITY_END();
}

int main(void) {
	return runUnityTests();
}

#ifdef ARDUINO
void setup() { delay(2000); runUnityTests(); }
void loop() {}
#endif

void app_main() {
	runUnityTests();
}
