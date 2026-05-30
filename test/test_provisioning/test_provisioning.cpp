#include <unity.h>

#include <microStore/Adapters/UniversalFileSystem.h>

#include "Provisioning/Provisioning.h"
#include "Provisioning/Codec.h"
#include "Provisioning/Ids.h"

#include "Utilities/OS.h"
#include "Bytes.h"
#include "Log.h"

#define MSGPACK_DEBUGLOG_ENABLE 0
#include <MsgPack.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/stat.h>

using namespace RNS;
using namespace RNS::Provisioning;

// ---------------------------------------------------------------------------
// Test fixtures
// ---------------------------------------------------------------------------

// Each test uses an isolated directory under /tmp so files from one test
// don't leak into another. The fixture is the per-process counter below.
static int g_test_counter = 0;
static std::string g_test_root;

static std::string make_test_root() {
	char buf[256];
	int pid = (int)getpid();
	snprintf(buf, sizeof(buf), "/tmp/rns_prov_test_%d_%d", pid, ++g_test_counter);
	mkdir(buf, 0755);
	return std::string(buf);
}

static void rm_rf(const std::string& path) {
	std::string cmd = "rm -rf '" + path + "'";
	int rc = system(cmd.c_str());
	(void)rc;
}

// A small "custom" namespace used by most tests — independent of the
// built-ins so we don't depend on Reticulum runtime state.
static constexpr uint16_t CUSTOM_NS_ID = 9100;
static constexpr uint16_t CUSTOM_BOOL  = 1;
static constexpr uint16_t CUSTOM_INT   = 2;
static constexpr uint16_t CUSTOM_FLOAT = 3;
static constexpr uint16_t CUSTOM_STR   = 4;
static constexpr uint16_t CUSTOM_BYTES = 5;
static constexpr uint16_t CUSTOM_REBOOT_INT = 6;	// FF_REBOOT_REQUIRED

// Tracks whether the live setter for CUSTOM_INT was invoked, and what
// value it last saw — for live_vs_reboot_apply.
static int    g_live_int_setter_count = 0;
static int64_t g_live_int_setter_value = 0;
static int    g_reboot_int_setter_count = 0;

static void reset_setter_counters() {
	g_live_int_setter_count = 0;
	g_live_int_setter_value = 0;
	g_reboot_int_setter_count = 0;
}

static void register_custom_namespace() {
	Manager::instance()
		.namespace_("custom", CUSTOM_NS_ID)
		.field_bool ("enabled",      CUSTOM_BOOL,  FF_LIVE_APPLY,      false)
		.field_int  ("level",        CUSTOM_INT,   FF_LIVE_APPLY,      5, 0, 100,
			[](const Value& v) { ++g_live_int_setter_count; g_live_int_setter_value = v.as_int(); return true; })
		.field_float("ratio",        CUSTOM_FLOAT, FF_LIVE_APPLY,      0.5, 0.0, 1.0)
		.field_string("label",       CUSTOM_STR,   FF_LIVE_APPLY,      "default", 32)
		.field_bytes ("blob",        CUSTOM_BYTES, FF_LIVE_APPLY,      Bytes(), 64)
		.field_int  ("frequency_hz", CUSTOM_REBOOT_INT, FF_REBOOT_REQUIRED, 915000000, 100000000, 1000000000,
			[](const Value& v) { ++g_reboot_int_setter_count; (void)v; return true; })
		.end();
}

static void fresh_provisioning(const std::string& root) {
	Manager::instance().end();
	register_custom_namespace();	// must register before begin()
	Manager::instance().begin(root.c_str());
}

void setUp(void) {
	g_test_root = make_test_root();
	reset_setter_counters();
}

void tearDown(void) {
	Manager::instance().end();
	rm_rf(g_test_root);
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

void test_register_and_lookup(void) {
	fresh_provisioning(g_test_root);
	auto& reg = Manager::instance().registry();

	// Built-ins registered automatically.
	TEST_ASSERT_NOT_NULL(reg.find(Ns::Reticulum::Id));
	TEST_ASSERT_NOT_NULL(reg.find("reticulum"));
	TEST_ASSERT_NOT_NULL(reg.find(Ns::Transport::Id));

	// Custom namespace registered explicitly.
	const Namespace* ns = reg.find(CUSTOM_NS_ID);
	TEST_ASSERT_NOT_NULL(ns);
	TEST_ASSERT_NOT_NULL(reg.find("custom"));
	TEST_ASSERT_NOT_NULL(ns->find_field(CUSTOM_INT));
	TEST_ASSERT_NOT_NULL(ns->find_field("level"));
	TEST_ASSERT_NULL(ns->find_field((uint16_t)9999));
}

void test_default_values(void) {
	fresh_provisioning(g_test_root);
	auto& p = Manager::instance();

	Value v_bool = p.field(CUSTOM_NS_ID, CUSTOM_BOOL);
	TEST_ASSERT_EQUAL(Type::Bool, (int)v_bool.type());
	TEST_ASSERT_FALSE(v_bool.as_bool());

	Value v_int = p.field(CUSTOM_NS_ID, CUSTOM_INT);
	TEST_ASSERT_EQUAL_INT64(5, v_int.as_int());
}

void test_set_draft_independent_of_working(void) {
	fresh_provisioning(g_test_root);
	auto& p = Manager::instance();

	TEST_ASSERT_TRUE(p.field(CUSTOM_NS_ID, CUSTOM_INT, Value((int64_t)42)));
	// Working still holds default.
	TEST_ASSERT_EQUAL_INT64(5,  p.field(CUSTOM_NS_ID, CUSTOM_INT, Manager::Source::Working).as_int());
	// Draft holds new.
	TEST_ASSERT_EQUAL_INT64(42, p.field(CUSTOM_NS_ID, CUSTOM_INT, Manager::Source::Draft).as_int());
	// Effective overlays.
	TEST_ASSERT_EQUAL_INT64(42, p.field(CUSTOM_NS_ID, CUSTOM_INT, Manager::Source::Effective).as_int());
	// Live setter not invoked yet — only fires at commit.
	TEST_ASSERT_EQUAL(0, g_live_int_setter_count);
}

void test_commit_promotes_and_invokes_live_setter(void) {
	fresh_provisioning(g_test_root);
	auto& p = Manager::instance();

	TEST_ASSERT_TRUE(p.field(CUSTOM_NS_ID, CUSTOM_INT, Value((int64_t)42)));
	TEST_ASSERT_TRUE(p.commit(CUSTOM_NS_ID));

	TEST_ASSERT_EQUAL_INT64(42, p.field(CUSTOM_NS_ID, CUSTOM_INT).as_int());
	TEST_ASSERT_EQUAL(1, g_live_int_setter_count);
	TEST_ASSERT_EQUAL_INT64(42, g_live_int_setter_value);
	// Draft was cleared.
	TEST_ASSERT_FALSE(p.field(CUSTOM_NS_ID, CUSTOM_INT, Manager::Source::Draft).type() != Type::None);
}

void test_discard_clears_draft(void) {
	fresh_provisioning(g_test_root);
	auto& p = Manager::instance();

	p.field(CUSTOM_NS_ID, CUSTOM_INT, Value((int64_t)42));
	TEST_ASSERT_TRUE(p.discard(CUSTOM_NS_ID));

	TEST_ASSERT_EQUAL_INT64(5, p.field(CUSTOM_NS_ID, CUSTOM_INT).as_int());
	TEST_ASSERT_EQUAL(Type::None,
		(int)p.field(CUSTOM_NS_ID, CUSTOM_INT, Manager::Source::Draft).type());
	TEST_ASSERT_EQUAL(0, g_live_int_setter_count);
}

void test_reload_after_reboot(void) {
	// Commit a change, simulate restart, expect value to survive.
	fresh_provisioning(g_test_root);
	auto& p = Manager::instance();

	p.field(CUSTOM_NS_ID, CUSTOM_INT, Value((int64_t)77));
	p.field(CUSTOM_NS_ID, CUSTOM_STR, Value("hello"));
	TEST_ASSERT_TRUE(p.commit());

	// Reset the counter so we can isolate the reload's behaviour from
	// the commit that just ran.
	reset_setter_counters();

	// Simulate reboot: tear down and re-init using the same storage root.
	fresh_provisioning(g_test_root);
	auto& p2 = Manager::instance();

	TEST_ASSERT_EQUAL_INT64(77, p2.field(CUSTOM_NS_ID, CUSTOM_INT).as_int());
	TEST_ASSERT_EQUAL_STRING("hello",
		p2.field(CUSTOM_NS_ID, CUSTOM_STR).as_string().c_str());
	// needs_reboot is reset on a fresh boot.
	TEST_ASSERT_FALSE(p2.needs_reboot());
	// Boot-time apply DOES push the loaded working value into the runtime
	// static via the field's setter — without that step, persistence would
	// be decorative (working map updated but runtime statics unchanged).
	// Expect exactly one setter call with the value loaded from disk.
	TEST_ASSERT_EQUAL(1, g_live_int_setter_count);
	TEST_ASSERT_EQUAL_INT64(77, g_live_int_setter_value);
}

void test_constraint_violation_rejected(void) {
	fresh_provisioning(g_test_root);
	auto& p = Manager::instance();

	// Out-of-range for [0, 100].
	TEST_ASSERT_FALSE(p.field(CUSTOM_NS_ID, CUSTOM_INT, Value((int64_t)500)));
	// Working unchanged.
	TEST_ASSERT_EQUAL_INT64(5, p.field(CUSTOM_NS_ID, CUSTOM_INT).as_int());
	// No draft entry created.
	TEST_ASSERT_EQUAL(Type::None,
		(int)p.field(CUSTOM_NS_ID, CUSTOM_INT, Manager::Source::Draft).type());

	// Wrong type also rejected.
	TEST_ASSERT_FALSE(p.field(CUSTOM_NS_ID, CUSTOM_INT, Value("oops")));
}

void test_live_vs_reboot_apply(void) {
	fresh_provisioning(g_test_root);
	auto& p = Manager::instance();

	TEST_ASSERT_FALSE(p.draft_has_reboot());
	TEST_ASSERT_FALSE(p.needs_reboot());

	// Live field — draft_has_reboot stays false.
	p.field(CUSTOM_NS_ID, CUSTOM_INT, Value((int64_t)42));
	TEST_ASSERT_FALSE(p.draft_has_reboot());

	// Reboot-required field — draft_has_reboot flips on.
	p.field(CUSTOM_NS_ID, CUSTOM_REBOOT_INT, Value((int64_t)868000000));
	TEST_ASSERT_TRUE(p.draft_has_reboot());

	TEST_ASSERT_TRUE(p.commit());
	// Live setter was called; reboot setter was NOT.
	TEST_ASSERT_EQUAL(1, g_live_int_setter_count);
	TEST_ASSERT_EQUAL(0, g_reboot_int_setter_count);
	// needs_reboot reflects the committed REBOOT_REQUIRED field.
	TEST_ASSERT_TRUE(p.needs_reboot());
	// Draft has been cleared, so draft_has_reboot is now false.
	TEST_ASSERT_FALSE(p.draft_has_reboot());
}

void test_reboot_callback_fires_once(void) {
	fresh_provisioning(g_test_root);
	auto& p = Manager::instance();

	int callback_count = 0;
	p.on_reboot_requested([&callback_count]() { ++callback_count; });

	p.field(CUSTOM_NS_ID, CUSTOM_REBOOT_INT, Value((int64_t)868000000));
	TEST_ASSERT_TRUE(p.commit());
	TEST_ASSERT_EQUAL(1, callback_count);

	// A second commit of another reboot field should NOT fire again
	// (needs_reboot is sticky until reboot or factory_reset).
	p.field(CUSTOM_NS_ID, CUSTOM_REBOOT_INT, Value((int64_t)869000000));
	TEST_ASSERT_TRUE(p.commit());
	TEST_ASSERT_EQUAL(1, callback_count);
}

void test_boot_apply_fires_reboot_required_setter(void) {
	// Commit a REBOOT_REQUIRED field. The setter must NOT fire on commit
	// (existing post-boot semantic) but MUST fire once on the next boot
	// when apply_loaded_to_runtime() walks the registry.
	fresh_provisioning(g_test_root);
	auto& p = Manager::instance();

	p.field(CUSTOM_NS_ID, CUSTOM_REBOOT_INT, Value((int64_t)868000000));
	TEST_ASSERT_TRUE(p.commit());
	// REBOOT_REQUIRED setter is NOT called by commit().
	TEST_ASSERT_EQUAL(0, g_reboot_int_setter_count);
	TEST_ASSERT_TRUE(p.needs_reboot());

	// Simulate reboot. Reset counters so the boot apply is isolated.
	reset_setter_counters();
	fresh_provisioning(g_test_root);
	auto& p2 = Manager::instance();

	// Boot apply fires the REBOOT_REQUIRED setter exactly once.
	TEST_ASSERT_EQUAL(1, g_reboot_int_setter_count);
	// And needs_reboot is cleared on the fresh boot — the reboot already
	// happened, the deferred-apply just executed.
	TEST_ASSERT_FALSE(p2.needs_reboot());
	// Working value still reflects what was on disk.
	TEST_ASSERT_EQUAL_INT64(868000000, p2.field(CUSTOM_NS_ID, CUSTOM_REBOOT_INT).as_int());
}

void test_unknown_field_ignored_on_load(void) {
	// Hand-craft a MsgPack file with an extra unknown field and verify it
	// loads cleanly with the unknown id silently dropped.
	fresh_provisioning(g_test_root);
	auto& p = Manager::instance();

	p.field(CUSTOM_NS_ID, CUSTOM_INT, Value((int64_t)33));
	TEST_ASSERT_TRUE(p.commit());

	// Inject an unknown id into the file by rewriting it manually.
	std::string path = g_test_root + "/custom.msgpack";
	MsgPack::Packer packer;
	packer.serialize(MsgPack::map_size_t(2));
	packer.serialize((uint16_t)CUSTOM_INT);
	packer.serialize((int64_t)77);
	packer.serialize((uint16_t)9999);	// unknown field id
	packer.serialize((int64_t)12345);
	Bytes raw(packer.data(), packer.size());
	Utilities::OS::write_file(path.c_str(), raw);

	// Reload.
	fresh_provisioning(g_test_root);
	auto& p2 = Manager::instance();

	TEST_ASSERT_EQUAL_INT64(77, p2.field(CUSTOM_NS_ID, CUSTOM_INT).as_int());
}

void test_atomic_write_resilience(void) {
	// Write a normal file, then drop a stray .tmp and ensure load_all
	// removes it and still loads the real file.
	fresh_provisioning(g_test_root);
	auto& p = Manager::instance();
	p.field(CUSTOM_NS_ID, CUSTOM_INT, Value((int64_t)21));
	TEST_ASSERT_TRUE(p.commit());

	// Plant a garbage .tmp (mimicking an interrupted save).
	std::string tmp_path = g_test_root + "/custom.msgpack.tmp";
	Bytes garbage("not valid msgpack");
	Utilities::OS::write_file(tmp_path.c_str(), garbage);
	TEST_ASSERT_TRUE(Utilities::OS::file_exists(tmp_path.c_str()));

	fresh_provisioning(g_test_root);
	auto& p2 = Manager::instance();

	TEST_ASSERT_EQUAL_INT64(21, p2.field(CUSTOM_NS_ID, CUSTOM_INT).as_int());
	// .tmp was cleaned up on load.
	TEST_ASSERT_FALSE(Utilities::OS::file_exists(tmp_path.c_str()));
}

void test_factory_reset(void) {
	fresh_provisioning(g_test_root);
	auto& p = Manager::instance();

	p.field(CUSTOM_NS_ID, CUSTOM_INT, Value((int64_t)42));
	p.commit();
	TEST_ASSERT_EQUAL_INT64(42, p.field(CUSTOM_NS_ID, CUSTOM_INT).as_int());

	TEST_ASSERT_TRUE(p.factory_reset());
	TEST_ASSERT_EQUAL_INT64(5, p.field(CUSTOM_NS_ID, CUSTOM_INT).as_int());
	TEST_ASSERT_FALSE(Utilities::OS::file_exists((g_test_root + "/custom.msgpack").c_str()));

	// Survives reboot.
	fresh_provisioning(g_test_root);
	TEST_ASSERT_EQUAL_INT64(5, Manager::instance().field(CUSTOM_NS_ID, CUSTOM_INT).as_int());
}

void test_by_name_accessors(void) {
	fresh_provisioning(g_test_root);
	auto& p = Manager::instance();

	TEST_ASSERT_TRUE(p.field("custom", "level", Value((int64_t)17)));
	TEST_ASSERT_EQUAL_INT64(17, p.field("custom", "level", Manager::Source::Draft).as_int());

	TEST_ASSERT_TRUE(p.commit("custom"));
	TEST_ASSERT_EQUAL_INT64(17, p.field("custom", "level").as_int());

	// Unknown ns/field returns a None value rather than throwing.
	Value missing = p.field("nope", "nada");
	TEST_ASSERT_EQUAL(Type::None, (int)missing.type());
}

void test_duplicate_field_id_rejected(void) {
	// Register a fresh namespace with a duplicate field id; second add
	// must fail (Namespace::add_field returns false).
	Manager::instance().end();
	auto b = Manager::instance()
		.namespace_("dup", 9200)
		.field_int("a", 1, FF_LIVE_APPLY, 0, 0, 10)
		.field_int("b", 1, FF_LIVE_APPLY, 0, 0, 10);
	// Pull the underlying namespace to inspect.
	Manager::instance().begin(g_test_root.c_str());
	const Namespace* ns = Manager::instance().registry().find((uint16_t)9200);
	TEST_ASSERT_NOT_NULL(ns);
	TEST_ASSERT_EQUAL(1u, ns->fields().size());	// duplicate dropped
}

// ---------------------------------------------------------------------------
// Wire round-trip tests — exercise handle_message()
// ---------------------------------------------------------------------------

// Build a wire envelope [op, seq, payload] where payload-packing is
// delegated to a lambda. Returns the encoded bytes.
template <typename F>
static Bytes make_request(uint8_t op, uint64_t seq, F&& pack_payload) {
	MsgPack::Packer p;
	p.serialize(MsgPack::arr_size_t(3));
	p.serialize(op);
	p.serialize(seq);
	pack_payload(p);
	return Bytes(p.data(), p.size());
}

void test_wire_get_info(void) {
	fresh_provisioning(g_test_root);
	auto& p = Manager::instance();

	Bytes req = make_request((uint8_t)Op::GetInfo, 42, [](MsgPack::Packer& pk) {
		MsgPack::object::nil_t n;
		pk.serialize(n);
	});
	Bytes resp = p.handle_message(req);
	TEST_ASSERT_GREATER_THAN(0, resp.size());

	MsgPack::Unpacker u;
	u.feed(resp.data(), resp.size());
	TEST_ASSERT_TRUE(u.isArray());
	const size_t n = u.unpackArraySize();
	TEST_ASSERT_EQUAL(3, n);

	uint8_t op = 0; u.deserialize(op);
	uint64_t seq = 0; u.deserialize(seq);
	TEST_ASSERT_EQUAL((uint8_t)Op::GetInfo, op);
	TEST_ASSERT_EQUAL(42, seq);

	// Payload should be a map with at least the schema version.
	TEST_ASSERT_TRUE(u.isMap());
}

void test_wire_set_state_then_commit(void) {
	fresh_provisioning(g_test_root);
	auto& p = Manager::instance();

	// SetState: { CUSTOM_NS_ID: { CUSTOM_INT: 88 } }
	Bytes req = make_request((uint8_t)Op::SetState, 1, [&](MsgPack::Packer& pk) {
		pk.serialize(MsgPack::map_size_t(1));
		pk.serialize((uint16_t)CUSTOM_NS_ID);
		pk.serialize(MsgPack::map_size_t(1));
		pk.serialize((uint16_t)CUSTOM_INT);
		pk.serialize((int64_t)88);
	});
	Bytes resp = p.handle_message(req);
	TEST_ASSERT_GREATER_THAN(0, resp.size());
	// Working still default, draft has 88.
	TEST_ASSERT_EQUAL_INT64(5, p.field(CUSTOM_NS_ID, CUSTOM_INT).as_int());
	TEST_ASSERT_EQUAL_INT64(88,
		p.field(CUSTOM_NS_ID, CUSTOM_INT, Manager::Source::Draft).as_int());

	// Commit
	Bytes commit_req = make_request((uint8_t)Op::Commit, 2, [](MsgPack::Packer& pk) {
		MsgPack::object::nil_t n;
		pk.serialize(n);
	});
	Bytes commit_resp = p.handle_message(commit_req);
	TEST_ASSERT_GREATER_THAN(0, commit_resp.size());
	TEST_ASSERT_EQUAL_INT64(88, p.field(CUSTOM_NS_ID, CUSTOM_INT).as_int());
	TEST_ASSERT_EQUAL(1, g_live_int_setter_count);
}

void test_wire_set_state_constraint_error(void) {
	fresh_provisioning(g_test_root);
	auto& p = Manager::instance();

	Bytes req = make_request((uint8_t)Op::SetState, 1, [&](MsgPack::Packer& pk) {
		pk.serialize(MsgPack::map_size_t(1));
		pk.serialize((uint16_t)CUSTOM_NS_ID);
		pk.serialize(MsgPack::map_size_t(1));
		pk.serialize((uint16_t)CUSTOM_INT);
		pk.serialize((int64_t)999);	// out of range
	});
	Bytes resp = p.handle_message(req);

	MsgPack::Unpacker u;
	u.feed(resp.data(), resp.size());
	u.unpackArraySize();
	uint8_t op = 0; u.deserialize(op);
	uint64_t seq = 0; u.deserialize(seq);
	TEST_ASSERT_EQUAL((uint8_t)Op::SetState, op);

	// Payload should contain an errors array.
	TEST_ASSERT_TRUE(u.isMap());
	const size_t n = u.unpackMapSize();
	bool found_errors = false;
	for (size_t i = 0; i < n; ++i) {
		uint16_t key = 0;
		int64_t k = 0; u.deserialize(k);
		key = (uint16_t)k;
		if (key == Key::FieldErrors && u.isArray()) {
			const size_t en = u.unpackArraySize();
			TEST_ASSERT_GREATER_THAN(0, en);
			found_errors = true;
			// Consume remaining elements
			for (size_t j = 0; j < en; ++j) {
				u.unpackArraySize();
				int64_t a, b, c; u.deserialize(a); u.deserialize(b); u.deserialize(c);
			}
		}
		else if (u.isUInt() || u.isInt())   { int64_t v; u.deserialize(v); }
		else if (u.isBool())                { bool b; u.deserialize(b); }
		else                                { u.unpackArraySize(); }
	}
	TEST_ASSERT_TRUE(found_errors);
	// Live setter NOT invoked.
	TEST_ASSERT_EQUAL(0, g_live_int_setter_count);
}

void test_wire_get_capabilities(void) {
	fresh_provisioning(g_test_root);
	auto& p = Manager::instance();

	Bytes req = make_request((uint8_t)Op::GetCapabilities, 1, [](MsgPack::Packer& pk) {
		MsgPack::object::nil_t n;
		pk.serialize(n);
	});
	Bytes resp = p.handle_message(req);

	MsgPack::Unpacker u;
	u.feed(resp.data(), resp.size());
	u.unpackArraySize();
	uint8_t op = 0; u.deserialize(op);
	uint64_t seq = 0; u.deserialize(seq);
	TEST_ASSERT_EQUAL((uint8_t)Op::GetCapabilities, op);
	TEST_ASSERT_TRUE(u.isArray());
	const size_t cap = u.unpackArraySize();
	TEST_ASSERT_GREATER_OR_EQUAL(3, cap);	// reticulum + transport + identity + custom
}

void test_wire_factory_reset(void) {
	fresh_provisioning(g_test_root);
	auto& p = Manager::instance();
	p.field(CUSTOM_NS_ID, CUSTOM_INT, Value((int64_t)42));
	p.commit();

	Bytes req = make_request((uint8_t)Op::FactoryReset, 1, [](MsgPack::Packer& pk) {
		MsgPack::object::nil_t n;
		pk.serialize(n);
	});
	Bytes resp = p.handle_message(req);
	TEST_ASSERT_GREATER_THAN(0, resp.size());
	TEST_ASSERT_EQUAL_INT64(5, p.field(CUSTOM_NS_ID, CUSTOM_INT).as_int());
}

void test_wire_unknown_op_error(void) {
	fresh_provisioning(g_test_root);
	auto& p = Manager::instance();

	Bytes req = make_request(250, 7, [](MsgPack::Packer& pk) {
		MsgPack::object::nil_t n;
		pk.serialize(n);
	});
	Bytes resp = p.handle_message(req);

	MsgPack::Unpacker u;
	u.feed(resp.data(), resp.size());
	u.unpackArraySize();
	uint8_t op = 0; u.deserialize(op);
	uint64_t seq = 0; u.deserialize(seq);
	TEST_ASSERT_EQUAL((uint8_t)Op::Error, op);
	TEST_ASSERT_EQUAL(7, seq);
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

int runUnityTests(void) {
	UNITY_BEGIN();

	// Suite setup: register the filesystem once.
	microStore::FileSystem filesystem{microStore::Adapters::UniversalFileSystem()};
	filesystem.init();
	Utilities::OS::register_filesystem(filesystem);

	RUN_TEST(test_register_and_lookup);
	RUN_TEST(test_default_values);
	RUN_TEST(test_set_draft_independent_of_working);
	RUN_TEST(test_commit_promotes_and_invokes_live_setter);
	RUN_TEST(test_discard_clears_draft);
	RUN_TEST(test_reload_after_reboot);
	RUN_TEST(test_constraint_violation_rejected);
	RUN_TEST(test_live_vs_reboot_apply);
	RUN_TEST(test_reboot_callback_fires_once);
	RUN_TEST(test_boot_apply_fires_reboot_required_setter);
	RUN_TEST(test_unknown_field_ignored_on_load);
	RUN_TEST(test_atomic_write_resilience);
	RUN_TEST(test_factory_reset);
	RUN_TEST(test_by_name_accessors);
	RUN_TEST(test_duplicate_field_id_rejected);
	RUN_TEST(test_wire_get_info);
	RUN_TEST(test_wire_set_state_then_commit);
	RUN_TEST(test_wire_set_state_constraint_error);
	RUN_TEST(test_wire_get_capabilities);
	RUN_TEST(test_wire_factory_reset);
	RUN_TEST(test_wire_unknown_op_error);

	Utilities::OS::deregister_filesystem();
	return UNITY_END();
}

int main(void) {
	return runUnityTests();
}

#ifdef ARDUINO
void setup() { delay(2000); runUnityTests(); }
void loop() {}
#endif
