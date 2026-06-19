#include <unity.h>

#include <microStore/Adapters/UniversalFileSystem.h>

#include "microReticulum/Provisioning/Provisioning.h"
#include "microReticulum/Provisioning/Codec.h"
#include "microReticulum/Provisioning/Ids.h"

#include "microReticulum/Reticulum.h"
#include "microReticulum/Transport.h"
#include "microReticulum/Identity.h"
#include "microReticulum/Utilities/OS.h"
#include "microReticulum/Bytes.h"
#include "microReticulum/Log.h"

#include <set>
#include <vector>

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
static constexpr uint16_t CUSTOM_METRIC     = 7;	// FF_READ_ONLY + getter (metric)
static constexpr uint16_t CUSTOM_COMMAND    = 8;	// FF_WRITE_ONLY + setter (command)
static constexpr uint16_t CUSTOM_VOID_CMD   = 9;	// FF_WRITE_ONLY + no-arg setter (command_void)

// Tracks whether the live setter for CUSTOM_INT was invoked, and what
// value it last saw — for live_vs_reboot_apply.
static int    g_live_int_setter_count = 0;
static int64_t g_live_int_setter_value = 0;
static int    g_reboot_int_setter_count = 0;

// Simulated runtime statics for the custom namespace, so the registered
// getters return live state (mirrors how built-in fields wire to
// Reticulum::transport_enabled() etc.).
static int64_t g_custom_int_runtime    = 5;
static int64_t g_custom_reboot_runtime = 915000000;
// Live runtime value behind the read-only metric field.
static int64_t g_custom_metric_runtime = 0;
// Trace of command invocations: count + last argument seen.
static int     g_command_invocations   = 0;
static int64_t g_last_command_arg      = 0;
// Trace of command_void invocations (no arg, just a count).
static int     g_void_command_invocations = 0;

static void reset_setter_counters() {
	g_live_int_setter_count = 0;
	g_live_int_setter_value = 0;
	g_reboot_int_setter_count = 0;
	g_command_invocations = 0;
	g_last_command_arg = 0;
	g_void_command_invocations = 0;
}

static void reset_runtime_state() {
	g_custom_int_runtime    = 5;
	g_custom_reboot_runtime = 915000000;
	g_custom_metric_runtime = 0;
	// Clear the Transport-side statics that field setters write into.
	// Without these resets, state leaks between tests because the
	// singleton's Transport statics are process-global.
	RNS::Transport::remote_management_allowed(std::set<Bytes>{});
	RNS::Identity none_id(RNS::Type::NONE);
	RNS::Transport::identity(none_id);
}

static void register_custom_namespace() {
	Provisioner::instance()
		.namespace_("custom", CUSTOM_NS_ID)
		.field_bool ("enabled",      CUSTOM_BOOL,  FF_LIVE_APPLY,      false)
		.field_int  ("level",        CUSTOM_INT,   FF_LIVE_APPLY,      5, 0, 100,
			[](const Value& v) {
				++g_live_int_setter_count;
				g_live_int_setter_value = v.as_int();
				g_custom_int_runtime = v.as_int();
				return true;
			},
			[]() { return g_custom_int_runtime; })
		.field_float("ratio",        CUSTOM_FLOAT, FF_LIVE_APPLY,      0.5, 0.0, 1.0)
		.field_string("label",       CUSTOM_STR,   FF_LIVE_APPLY,      "default", 32)
		.field_bytes ("blob",        CUSTOM_BYTES, FF_LIVE_APPLY,      Bytes(), 64)
		.field_int  ("frequency_hz", CUSTOM_REBOOT_INT, FF_REBOOT_REQUIRED, 915000000, 100000000, 1000000000,
			[](const Value& v) {
				++g_reboot_int_setter_count;
				g_custom_reboot_runtime = v.as_int();
				return true;
			},
			[]() { return g_custom_reboot_runtime; })
		// Read-only metric: app reports live runtime via the getter.
		.metric_int("packet_count", CUSTOM_METRIC,
			[]() { return g_custom_metric_runtime; })
		// Write-only command: SET_STATE+COMMIT triggers the action once.
		.command_int("reboot_in_seconds", CUSTOM_COMMAND, 0, 3600,
			[](const Value& v) {
				++g_command_invocations;
				g_last_command_arg = v.as_int();
				return true;
			})
		// Argument-less command: SET_STATE+COMMIT fires the no-arg setter.
		.command_void("reboot_now", CUSTOM_VOID_CMD,
			[]() {
				++g_void_command_invocations;
				return true;
			})
		.end();
}

static void fresh_provisioning(const std::string& root) {
	Provisioner::instance().end();
	register_custom_namespace();	// must register before begin()
	Provisioner::instance().begin(root.c_str());
}

void setUp(void) {
	g_test_root = make_test_root();
	reset_setter_counters();
	reset_runtime_state();
}

void tearDown(void) {
	Provisioner::instance().end();
	rm_rf(g_test_root);
}

// Build a wire envelope [op, seq, payload] where payload-packing is
// delegated to a lambda. Returns the encoded bytes. Declared up here so
// non-wire tests can also build framed requests (e.g. for SECRET-field
// exclusion checks via GET_STATE).
template <typename F>
static Bytes make_request(uint8_t op, uint64_t seq, F&& pack_payload) {
	MsgPack::Packer p;
	p.serialize(MsgPack::arr_size_t(3));
	p.serialize(op);
	p.serialize(seq);
	pack_payload(p);
	return Bytes(p.data(), p.size());
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

void test_register_and_lookup(void) {
	fresh_provisioning(g_test_root);
	auto& reg = Provisioner::instance().registry();

	// Built-ins registered automatically.
	TEST_ASSERT_NOT_NULL(reg.find(Ns::Reticulum::Id));
	TEST_ASSERT_NOT_NULL(reg.find("Reticulum"));
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
	auto& p = Provisioner::instance();

	Value v_bool = p.field(CUSTOM_NS_ID, CUSTOM_BOOL);
	TEST_ASSERT_EQUAL(Provisioning::Type::Bool, (int)v_bool.type());
	TEST_ASSERT_FALSE(v_bool.as_bool());

	Value v_int = p.field(CUSTOM_NS_ID, CUSTOM_INT);
	TEST_ASSERT_EQUAL_INT64(5, v_int.as_int());
}

void test_set_draft_independent_of_working(void) {
	fresh_provisioning(g_test_root);
	auto& p = Provisioner::instance();

	TEST_ASSERT_TRUE(p.field(CUSTOM_NS_ID, CUSTOM_INT, Value((int64_t)42)));
	// Working still holds default.
	TEST_ASSERT_EQUAL_INT64(5,  p.field(CUSTOM_NS_ID, CUSTOM_INT, Provisioner::Source::Working).as_int());
	// Draft holds new.
	TEST_ASSERT_EQUAL_INT64(42, p.field(CUSTOM_NS_ID, CUSTOM_INT, Provisioner::Source::Draft).as_int());
	// Effective overlays.
	TEST_ASSERT_EQUAL_INT64(42, p.field(CUSTOM_NS_ID, CUSTOM_INT, Provisioner::Source::Effective).as_int());
	// Live setter not invoked yet — only fires at commit.
	TEST_ASSERT_EQUAL(0, g_live_int_setter_count);
}

void test_commit_promotes_and_invokes_live_setter(void) {
	fresh_provisioning(g_test_root);
	auto& p = Provisioner::instance();

	TEST_ASSERT_TRUE(p.field(CUSTOM_NS_ID, CUSTOM_INT, Value((int64_t)42)));
	TEST_ASSERT_TRUE(p.commit(CUSTOM_NS_ID));

	TEST_ASSERT_EQUAL_INT64(42, p.field(CUSTOM_NS_ID, CUSTOM_INT).as_int());
	TEST_ASSERT_EQUAL(1, g_live_int_setter_count);
	TEST_ASSERT_EQUAL_INT64(42, g_live_int_setter_value);
	// Draft was cleared.
	TEST_ASSERT_FALSE(p.field(CUSTOM_NS_ID, CUSTOM_INT, Provisioner::Source::Draft).type() != Provisioning::Type::None);
}

void test_discard_clears_draft(void) {
	fresh_provisioning(g_test_root);
	auto& p = Provisioner::instance();

	p.field(CUSTOM_NS_ID, CUSTOM_INT, Value((int64_t)42));
	TEST_ASSERT_TRUE(p.discard(CUSTOM_NS_ID));

	TEST_ASSERT_EQUAL_INT64(5, p.field(CUSTOM_NS_ID, CUSTOM_INT).as_int());
	TEST_ASSERT_EQUAL(Provisioning::Type::None,
		(int)p.field(CUSTOM_NS_ID, CUSTOM_INT, Provisioner::Source::Draft).type());
	TEST_ASSERT_EQUAL(0, g_live_int_setter_count);
}

void test_reload_after_reboot(void) {
	// Commit a change, simulate restart, expect value to survive.
	fresh_provisioning(g_test_root);
	auto& p = Provisioner::instance();

	p.field(CUSTOM_NS_ID, CUSTOM_INT, Value((int64_t)77));
	p.field(CUSTOM_NS_ID, CUSTOM_STR, Value("hello"));
	TEST_ASSERT_TRUE(p.commit());

	// Reset the counter so we can isolate the reload's behaviour from
	// the commit that just ran.
	reset_setter_counters();
	// Also reset the simulated runtime static to its declaration default.
	// A real power-cycle reboot reinitialises statics to their declared
	// values; without this, the apply path's redundancy check (skip-if-
	// runtime-already-matches via getter) would correctly observe that the
	// in-process global still holds the post-commit value and skip the
	// setter — defeating the test's intent.
	g_custom_int_runtime = 5;

	// Simulate reboot: tear down and re-init using the same storage root.
	fresh_provisioning(g_test_root);
	auto& p2 = Provisioner::instance();

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
	auto& p = Provisioner::instance();

	// Out-of-range for [0, 100].
	TEST_ASSERT_FALSE(p.field(CUSTOM_NS_ID, CUSTOM_INT, Value((int64_t)500)));
	// Working unchanged.
	TEST_ASSERT_EQUAL_INT64(5, p.field(CUSTOM_NS_ID, CUSTOM_INT).as_int());
	// No draft entry created.
	TEST_ASSERT_EQUAL(Provisioning::Type::None,
		(int)p.field(CUSTOM_NS_ID, CUSTOM_INT, Provisioner::Source::Draft).type());

	// Wrong type also rejected.
	TEST_ASSERT_FALSE(p.field(CUSTOM_NS_ID, CUSTOM_INT, Value("oops")));
}

void test_live_vs_reboot_apply(void) {
	fresh_provisioning(g_test_root);
	auto& p = Provisioner::instance();

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

void test_reboot_required_callback_fires_once(void) {
	fresh_provisioning(g_test_root);
	auto& p = Provisioner::instance();

	int callback_count = 0;
	p.on_reboot_required([&callback_count]() { ++callback_count; });

	p.field(CUSTOM_NS_ID, CUSTOM_REBOOT_INT, Value((int64_t)868000000));
	TEST_ASSERT_TRUE(p.commit());
	TEST_ASSERT_EQUAL(1, callback_count);

	// A second commit of another reboot field should NOT fire again
	// (needs_reboot is sticky until reboot or factory_reset).
	p.field(CUSTOM_NS_ID, CUSTOM_REBOOT_INT, Value((int64_t)869000000));
	TEST_ASSERT_TRUE(p.commit());
	TEST_ASSERT_EQUAL(1, callback_count);
}

void test_getter_reflects_direct_setter(void) {
	// Simulates the scenario where app code mutates a runtime static
	// (e.g. Reticulum::transport_enabled(true)) outside of Provisioning's
	// commit path. The field's registered getter reads the live static,
	// so Provisioning::field() must reflect the new value immediately —
	// no drift between direct setters and Provisioning reads.
	fresh_provisioning(g_test_root);
	auto& p = Provisioner::instance();

	// Default visible at startup.
	TEST_ASSERT_EQUAL_INT64(5, p.field(CUSTOM_NS_ID, CUSTOM_INT).as_int());

	// Direct mutation of the simulated runtime static.
	g_custom_int_runtime = 42;
	TEST_ASSERT_EQUAL_INT64(42, p.field(CUSTOM_NS_ID, CUSTOM_INT).as_int());

	// Another direct mutation; getter keeps tracking.
	g_custom_int_runtime = 17;
	TEST_ASSERT_EQUAL_INT64(17, p.field(CUSTOM_NS_ID, CUSTOM_INT).as_int());

	// Effective source overlays draft on top of the live value.
	TEST_ASSERT_TRUE(p.field(CUSTOM_NS_ID, CUSTOM_INT, Value((int64_t)99)));
	TEST_ASSERT_EQUAL_INT64(17, p.field(CUSTOM_NS_ID, CUSTOM_INT, Provisioner::Source::Working).as_int());
	TEST_ASSERT_EQUAL_INT64(99, p.field(CUSTOM_NS_ID, CUSTOM_INT, Provisioner::Source::Draft).as_int());
	TEST_ASSERT_EQUAL_INT64(99, p.field(CUSTOM_NS_ID, CUSTOM_INT, Provisioner::Source::Effective).as_int());
}

void test_set_draft_dedups_against_live_runtime(void) {
	// Regression: when a direct setter mutates the runtime after the
	// add_field seed (e.g. RNode_Firmware.ino:798 calling
	// reticulum.transport_enabled(true) after init_provisioning()),
	// set_draft must dedup against the LIVE runtime via the getter, not
	// the stale working-map snapshot. Otherwise a user submitting the
	// runtime's actual current value (legitimate, because they want to
	// commit-through-Provisioning so persistence catches up) gets the
	// draft silently erased and the working map stays stale on disk.
	fresh_provisioning(g_test_root);
	auto& p = Provisioner::instance();

	// Simulate the firmware mutating the runtime AFTER provisioning
	// registration. Working map is now stale relative to the runtime.
	g_custom_int_runtime = 73;

	// User submits the runtime's current value (73). With dedup against
	// effective() the comparison correctly observes runtime == 73, the
	// new draft equals the live state, and the draft is dropped — no
	// pending change is needed.
	TEST_ASSERT_TRUE(p.field(CUSTOM_NS_ID, CUSTOM_INT, Value((int64_t)73)));
	TEST_ASSERT_EQUAL(Provisioning::Type::None,
		(int)p.field(CUSTOM_NS_ID, CUSTOM_INT, Provisioner::Source::Draft).type());

	// User submits a different value (50). Draft must be stored and
	// committable so the runtime can be updated through the normal flow.
	TEST_ASSERT_TRUE(p.field(CUSTOM_NS_ID, CUSTOM_INT, Value((int64_t)50)));
	TEST_ASSERT_EQUAL_INT64(50,
		p.field(CUSTOM_NS_ID, CUSTOM_INT, Provisioner::Source::Draft).as_int());
	TEST_ASSERT_TRUE(p.commit(CUSTOM_NS_ID));
	TEST_ASSERT_EQUAL_INT64(50, g_custom_int_runtime);
}

void test_reboot_required_dedups_against_working(void) {
	// FF_REBOOT_REQUIRED fields intentionally let runtime lag working
	// between commit and reboot — the setter doesn't fire on commit. The
	// dedup must compare against working (the pending value), NOT the
	// live runtime, so that submitting the *old* runtime value to revert
	// a pending change correctly produces a new draft rather than being
	// silently dropped as "matches runtime".
	fresh_provisioning(g_test_root);
	auto& p = Provisioner::instance();

	const int64_t initial = 915000000;
	const int64_t pending = 868000000;

	// Commit a REBOOT change. Setter does NOT fire — runtime still has
	// the initial value while working/disk hold the pending one.
	TEST_ASSERT_TRUE(p.field(CUSTOM_NS_ID, CUSTOM_REBOOT_INT, Value(pending)));
	TEST_ASSERT_TRUE(p.commit(CUSTOM_NS_ID));
	TEST_ASSERT_EQUAL(0, g_reboot_int_setter_count);
	TEST_ASSERT_EQUAL_INT64(initial, g_custom_reboot_runtime);

	// User wants to revert the pending change by submitting the runtime's
	// current value. Dedup against effective() would observe runtime ==
	// initial and drop the draft — defeating the revert. Dedup against
	// working() correctly sees working = pending != initial and stores
	// the draft.
	TEST_ASSERT_TRUE(p.field(CUSTOM_NS_ID, CUSTOM_REBOOT_INT, Value(initial)));
	TEST_ASSERT_EQUAL_INT64(initial,
		p.field(CUSTOM_NS_ID, CUSTOM_REBOOT_INT, Provisioner::Source::Draft).as_int());

	// Commit promotes the revert into working; runtime still lags (no
	// setter call for REBOOT), but the next boot's apply will pick up
	// the reverted-to-initial value from disk.
	TEST_ASSERT_TRUE(p.commit(CUSTOM_NS_ID));
	TEST_ASSERT_EQUAL_INT64(initial,
		p.field(CUSTOM_NS_ID, CUSTOM_REBOOT_INT, Provisioner::Source::Working).as_int());
}

void test_boot_apply_fires_reboot_required_setter(void) {
	// Commit a REBOOT_REQUIRED field. The setter must NOT fire on commit
	// (existing post-boot semantic) but MUST fire once on the next boot
	// when apply_loaded_to_runtime() walks the registry.
	fresh_provisioning(g_test_root);
	auto& p = Provisioner::instance();

	p.field(CUSTOM_NS_ID, CUSTOM_REBOOT_INT, Value((int64_t)868000000));
	TEST_ASSERT_TRUE(p.commit());
	// REBOOT_REQUIRED setter is NOT called by commit().
	TEST_ASSERT_EQUAL(0, g_reboot_int_setter_count);
	TEST_ASSERT_TRUE(p.needs_reboot());

	// Simulate reboot. Reset counters so the boot apply is isolated.
	reset_setter_counters();
	fresh_provisioning(g_test_root);
	auto& p2 = Provisioner::instance();

	// Boot apply fires the REBOOT_REQUIRED setter exactly once.
	TEST_ASSERT_EQUAL(1, g_reboot_int_setter_count);
	// And needs_reboot is cleared on the fresh boot — the reboot already
	// happened, the deferred-apply just executed.
	TEST_ASSERT_FALSE(p2.needs_reboot());
	// Working value still reflects what was on disk.
	TEST_ASSERT_EQUAL_INT64(868000000, p2.field(CUSTOM_NS_ID, CUSTOM_REBOOT_INT).as_int());
}

// ---------------------------------------------------------------------------
// BytesList field type + remote_management_allowed integration
// ---------------------------------------------------------------------------

// Helper: build a 16-byte hash filled with the given byte value.
static Bytes make_hash(uint8_t fill) {
	std::vector<uint8_t> v(16, fill);
	return Bytes(v.data(), v.size());
}

void test_bytes_list_default_is_empty(void) {
	fresh_provisioning(g_test_root);
	auto& p = Provisioner::instance();

	// remote_management_allowed default is an empty list.
	Value v = p.field(Ns::Reticulum::Id, Ns::Reticulum::Field::RemoteManagementAllowed);
	TEST_ASSERT_EQUAL(Provisioning::Type::BytesList, (int)v.type());
	TEST_ASSERT_EQUAL_size_t(0, v.as_bytes_list().size());
}

void test_bytes_list_set_commit_persists(void) {
	fresh_provisioning(g_test_root);
	auto& p = Provisioner::instance();

	std::vector<Bytes> entries = { make_hash(0x01), make_hash(0x02) };
	TEST_ASSERT_TRUE(p.field(Ns::Reticulum::Id,
		Ns::Reticulum::Field::RemoteManagementAllowed, Value(entries)));
	TEST_ASSERT_TRUE(p.commit(Ns::Reticulum::Id));

	// Field is FF_REBOOT_REQUIRED, so the setter must NOT fire on commit —
	// the runtime set stays at whatever reset_runtime_state seeded (empty).
	TEST_ASSERT_EQUAL_size_t(0, RNS::Transport::remote_management_allowed().size());
	TEST_ASSERT_TRUE(p.needs_reboot());

	// Simulated reboot: begin() reloads the file and apply_loaded_to_runtime
	// fires the setter because working != runtime (empty).
	fresh_provisioning(g_test_root);
	const std::set<Bytes>& live2 = RNS::Transport::remote_management_allowed();
	TEST_ASSERT_EQUAL_size_t(2, live2.size());
	TEST_ASSERT_TRUE(live2.count(make_hash(0x01)) == 1);
	TEST_ASSERT_TRUE(live2.count(make_hash(0x02)) == 1);
}

void test_bytes_list_constraint_rejects_wrong_element_size(void) {
	fresh_provisioning(g_test_root);
	auto& p = Provisioner::instance();

	// 8-byte hash violates the declared 16-byte element_size constraint.
	std::vector<uint8_t> short_bytes(8, 0xAB);
	std::vector<Bytes> bad = { Bytes(short_bytes.data(), short_bytes.size()) };
	TEST_ASSERT_FALSE(p.field(Ns::Reticulum::Id,
		Ns::Reticulum::Field::RemoteManagementAllowed, Value(bad)));

	// Draft not populated, runtime untouched.
	TEST_ASSERT_EQUAL_size_t(0, RNS::Transport::remote_management_allowed().size());
}

void test_bytes_list_getter_reflects_direct_setter(void) {
	fresh_provisioning(g_test_root);
	auto& p = Provisioner::instance();

	// Mutate via the Transport direct setter, then read via Provisioning.
	std::set<Bytes> s = { make_hash(0x42) };
	RNS::Transport::remote_management_allowed(s);

	Value v = p.field(Ns::Reticulum::Id, Ns::Reticulum::Field::RemoteManagementAllowed);
	TEST_ASSERT_EQUAL(Provisioning::Type::BytesList, (int)v.type());
	TEST_ASSERT_EQUAL_size_t(1, v.as_bytes_list().size());
	TEST_ASSERT_TRUE(v.as_bytes_list()[0] == make_hash(0x42));

	// Reset runtime so subsequent tests start clean.
	RNS::Transport::remote_management_allowed(std::set<Bytes>{});
}

// ---------------------------------------------------------------------------
// transport_identity (FF_REBOOT_REQUIRED | FF_SECRET) integration
// ---------------------------------------------------------------------------

void test_transport_identity_getter_unset_returns_empty(void) {
	// setUp clears Transport::_identity to NONE. The getter must guard
	// against dereferencing a NONE Identity and return an empty Bytes.
	fresh_provisioning(g_test_root);
	auto& p = Provisioner::instance();

	Value v = p.field(Ns::Reticulum::Id, Ns::Reticulum::Field::TransportIdentity);
	TEST_ASSERT_EQUAL(Provisioning::Type::Bytes, (int)v.type());
	TEST_ASSERT_EQUAL_size_t(0, v.as_bytes().size());
}

void test_transport_identity_commit_reloads_on_reboot(void) {
	fresh_provisioning(g_test_root);
	auto& p = Provisioner::instance();

	// Generate a fresh identity to obtain a known-good 64-byte private key
	// and the hash it should produce when reloaded.
	RNS::Identity src;
	Bytes prv = src.get_private_key();
	Bytes expected_hash = src.hash();
	TEST_ASSERT_EQUAL_size_t(64, prv.size());

	// Set via Provisioning and commit. Field is REBOOT_REQUIRED + SECRET,
	// so the setter must NOT fire on commit — Transport::_identity stays
	// at the NONE state set during reset_runtime_state.
	TEST_ASSERT_TRUE(p.field(Ns::Reticulum::Id,
		Ns::Reticulum::Field::TransportIdentity, Value(prv)));
	TEST_ASSERT_TRUE(p.commit(Ns::Reticulum::Id));
	TEST_ASSERT_FALSE((bool)RNS::Transport::identity());
	TEST_ASSERT_TRUE(p.needs_reboot());

	// Simulate reboot: fresh_provisioning will re-register builtins,
	// reload the file from disk, and apply_loaded_to_runtime will fire
	// the identity setter because working != default(empty).
	fresh_provisioning(g_test_root);

	TEST_ASSERT_TRUE((bool)RNS::Transport::identity());
	TEST_ASSERT_TRUE(RNS::Transport::identity().hash() == expected_hash);
}

void test_transport_identity_excluded_from_get_state(void) {
	// FF_SECRET fields must be omitted from GET_STATE responses.
	fresh_provisioning(g_test_root);
	auto& p = Provisioner::instance();

	// Plant a real key so the field has a non-empty value via the getter.
	RNS::Identity src;
	RNS::Transport::set_identity_prv(src.get_private_key());

	// Wire request: GET_STATE for the Reticulum namespace.
	Bytes req = make_request((uint8_t)Op::GetState, 1, [&](MsgPack::Packer& pk) {
		pk.serialize(MsgPack::map_size_t(1));
		pk.serialize((uint16_t)Key::NamespaceFilter);
		pk.serialize(MsgPack::arr_size_t(1));
		pk.serialize((uint16_t)Ns::Reticulum::Id);
	});
	Bytes resp = p.handle_message(req);

	// Parse: response is [op, seq, payload-map { ns_id -> { field_id -> value } }]
	MsgPack::Unpacker u;
	u.feed(resp.data(), resp.size());
	u.unpackArraySize();
	uint8_t op = 0; u.deserialize(op);
	uint64_t seq = 0; u.deserialize(seq);
	TEST_ASSERT_EQUAL((uint8_t)Op::GetState, op);
	TEST_ASSERT_TRUE(u.isMap());
	const size_t n_ns = u.unpackMapSize();
	TEST_ASSERT_EQUAL_size_t(1, n_ns);

	int64_t ns_id_raw = 0; u.deserialize(ns_id_raw);
	TEST_ASSERT_EQUAL((int64_t)Ns::Reticulum::Id, ns_id_raw);
	TEST_ASSERT_TRUE(u.isMap());
	const size_t n_fields = u.unpackMapSize();

	// Walk the field map; TransportIdentity must NOT be present.
	bool found_secret = false;
	for (size_t i = 0; i < n_fields; ++i) {
		int64_t fid = 0;
		u.deserialize(fid);
		if (fid == Ns::Reticulum::Field::TransportIdentity) found_secret = true;
		// Skip the value — type varies per field.
		if (u.isBool())                              { bool b; u.deserialize(b); }
		else if (u.isUInt() || u.isInt())            { int64_t iv; u.deserialize(iv); }
		else if (u.isFloat32() || u.isFloat64())     { double d; u.deserialize(d); }
		else if (u.isStr())                          { MsgPack::str_t s; u.deserialize(s); }
		else if (u.isBin())                          { MsgPack::bin_t<uint8_t> bin; u.deserialize(bin); }
		else if (u.isArray()) {
			size_t m = u.unpackArraySize();
			for (size_t j = 0; j < m; ++j) {
				if (u.isBin())                       { MsgPack::bin_t<uint8_t> bin; u.deserialize(bin); }
				else if (u.isUInt() || u.isInt())    { int64_t iv; u.deserialize(iv); }
			}
		}
	}
	TEST_ASSERT_FALSE(found_secret);
}

// ---------------------------------------------------------------------------
// Metric (read-only + getter) and command (write-only + setter) patterns
// ---------------------------------------------------------------------------

void test_metric_getter_reflects_runtime(void) {
	fresh_provisioning(g_test_root);
	auto& p = Provisioner::instance();

	// Mutate the simulated runtime; getter must surface live value.
	g_custom_metric_runtime = 0;
	TEST_ASSERT_EQUAL_INT64(0, p.field(CUSTOM_NS_ID, CUSTOM_METRIC).as_int());
	g_custom_metric_runtime = 12345;
	TEST_ASSERT_EQUAL_INT64(12345, p.field(CUSTOM_NS_ID, CUSTOM_METRIC).as_int());
	g_custom_metric_runtime = 99;
	TEST_ASSERT_EQUAL_INT64(99, p.field(CUSTOM_NS_ID, CUSTOM_METRIC).as_int());
}

void test_metric_rejects_set_state(void) {
	fresh_provisioning(g_test_root);
	auto& p = Provisioner::instance();

	// Direct accessor: SET on read-only field returns false.
	TEST_ASSERT_FALSE(p.field(CUSTOM_NS_ID, CUSTOM_METRIC, Value((int64_t)42)));
	// Live value still reflects runtime, not the rejected attempt.
	TEST_ASSERT_EQUAL_INT64(g_custom_metric_runtime,
		p.field(CUSTOM_NS_ID, CUSTOM_METRIC).as_int());
}

void test_metric_not_persisted(void) {
	// Set a non-default runtime value, then look at the on-disk file
	// after a commit of an unrelated field. The metric must not appear.
	fresh_provisioning(g_test_root);
	auto& p = Provisioner::instance();
	g_custom_metric_runtime = 7777;

	// Force a save by committing an unrelated stateful field.
	p.field(CUSTOM_NS_ID, CUSTOM_INT, Value((int64_t)10));
	TEST_ASSERT_TRUE(p.commit());

	// Read the file directly and confirm the metric id is absent.
	Bytes raw;
	size_t n = Utilities::OS::read_file((g_test_root + "/custom.msgpack").c_str(), raw);
	TEST_ASSERT_GREATER_THAN(0, n);
	MsgPack::Unpacker u; u.feed(raw.data(), raw.size());
	TEST_ASSERT_TRUE(u.isMap());
	const size_t entries = u.unpackMapSize();
	bool saw_metric = false;
	for (size_t i = 0; i < entries; ++i) {
		int64_t fid = 0; u.deserialize(fid);
		if (fid == CUSTOM_METRIC) saw_metric = true;
		// skip value
		if (u.isBool())                              { bool b; u.deserialize(b); }
		else if (u.isUInt() || u.isInt())            { int64_t v; u.deserialize(v); }
		else if (u.isFloat32() || u.isFloat64())     { double d; u.deserialize(d); }
		else if (u.isStr())                          { MsgPack::str_t s; u.deserialize(s); }
		else if (u.isBin())                          { MsgPack::bin_t<uint8_t> bin; u.deserialize(bin); }
		else if (u.isArray()) {
			size_t m = u.unpackArraySize();
			for (size_t j = 0; j < m; ++j) {
				if (u.isBin())   { MsgPack::bin_t<uint8_t> b; u.deserialize(b); }
				else             { int64_t iv; u.deserialize(iv); }
			}
		}
	}
	TEST_ASSERT_FALSE(saw_metric);
}

void test_command_fires_setter_on_commit(void) {
	fresh_provisioning(g_test_root);
	auto& p = Provisioner::instance();

	TEST_ASSERT_EQUAL(0, g_command_invocations);

	TEST_ASSERT_TRUE(p.field(CUSTOM_NS_ID, CUSTOM_COMMAND, Value((int64_t)42)));
	TEST_ASSERT_TRUE(p.commit(CUSTOM_NS_ID));

	TEST_ASSERT_EQUAL(1, g_command_invocations);
	TEST_ASSERT_EQUAL_INT64(42, g_last_command_arg);
}

void test_command_can_be_invoked_repeatedly_with_same_arg(void) {
	fresh_provisioning(g_test_root);
	auto& p = Provisioner::instance();

	// First invocation.
	TEST_ASSERT_TRUE(p.field(CUSTOM_NS_ID, CUSTOM_COMMAND, Value((int64_t)7)));
	TEST_ASSERT_TRUE(p.commit(CUSTOM_NS_ID));
	TEST_ASSERT_EQUAL(1, g_command_invocations);

	// Second invocation with the *same* value still triggers — the
	// set-equal-working dedup must be bypassed for write-only fields.
	TEST_ASSERT_TRUE(p.field(CUSTOM_NS_ID, CUSTOM_COMMAND, Value((int64_t)7)));
	TEST_ASSERT_TRUE(p.commit(CUSTOM_NS_ID));
	TEST_ASSERT_EQUAL(2, g_command_invocations);
}

void test_command_read_returns_none(void) {
	fresh_provisioning(g_test_root);
	auto& p = Provisioner::instance();

	// Before any invocation: read returns None (no readable state).
	Value v = p.field(CUSTOM_NS_ID, CUSTOM_COMMAND);
	TEST_ASSERT_EQUAL(Provisioning::Type::None, (int)v.type());

	// After commit: still None — the setter consumed the value; nothing
	// is retained.
	p.field(CUSTOM_NS_ID, CUSTOM_COMMAND, Value((int64_t)99));
	TEST_ASSERT_TRUE(p.commit(CUSTOM_NS_ID));
	v = p.field(CUSTOM_NS_ID, CUSTOM_COMMAND);
	TEST_ASSERT_EQUAL(Provisioning::Type::None, (int)v.type());
}

void test_command_not_persisted_or_replayed(void) {
	fresh_provisioning(g_test_root);
	auto& p = Provisioner::instance();

	// Invoke the command, then simulate a reboot. The setter must NOT
	// fire again on apply_loaded_to_runtime — commands are one-shot.
	TEST_ASSERT_TRUE(p.field(CUSTOM_NS_ID, CUSTOM_COMMAND, Value((int64_t)60)));
	TEST_ASSERT_TRUE(p.commit(CUSTOM_NS_ID));
	TEST_ASSERT_EQUAL(1, g_command_invocations);

	// Simulate reboot: end + re-register + begin reloads from disk.
	// Reset the invocation counter so a stray boot-apply call would
	// be observable.
	reset_setter_counters();
	fresh_provisioning(g_test_root);

	TEST_ASSERT_EQUAL(0, g_command_invocations);
}

void test_command_excluded_from_get_state(void) {
	fresh_provisioning(g_test_root);
	auto& p = Provisioner::instance();

	// Wire GET_STATE for the custom namespace; CUSTOM_COMMAND must not
	// appear in the field map.
	Bytes req = make_request((uint8_t)Op::GetState, 1, [&](MsgPack::Packer& pk) {
		pk.serialize(MsgPack::map_size_t(1));
		pk.serialize((uint16_t)Key::NamespaceFilter);
		pk.serialize(MsgPack::arr_size_t(1));
		pk.serialize((uint16_t)CUSTOM_NS_ID);
	});
	Bytes resp = p.handle_message(req);

	MsgPack::Unpacker u; u.feed(resp.data(), resp.size());
	u.unpackArraySize();
	uint8_t op = 0; u.deserialize(op);
	uint64_t seq = 0; u.deserialize(seq);
	TEST_ASSERT_TRUE(u.isMap());
	const size_t n_ns = u.unpackMapSize();
	TEST_ASSERT_EQUAL_size_t(1, n_ns);
	int64_t ns_id_raw = 0; u.deserialize(ns_id_raw);
	TEST_ASSERT_TRUE(u.isMap());
	const size_t n_fields = u.unpackMapSize();

	bool found_command = false;
	for (size_t i = 0; i < n_fields; ++i) {
		int64_t fid = 0; u.deserialize(fid);
		if (fid == CUSTOM_COMMAND) found_command = true;
		// skip value
		if (u.isBool())                              { bool b; u.deserialize(b); }
		else if (u.isUInt() || u.isInt())            { int64_t v; u.deserialize(v); }
		else if (u.isFloat32() || u.isFloat64())     { double d; u.deserialize(d); }
		else if (u.isStr())                          { MsgPack::str_t s; u.deserialize(s); }
		else if (u.isBin())                          { MsgPack::bin_t<uint8_t> bin; u.deserialize(bin); }
		else if (u.isArray()) {
			size_t m = u.unpackArraySize();
			for (size_t j = 0; j < m; ++j) {
				if (u.isBin())   { MsgPack::bin_t<uint8_t> b; u.deserialize(b); }
				else             { int64_t iv; u.deserialize(iv); }
			}
		}
	}
	TEST_ASSERT_FALSE(found_command);
}

void test_command_constraint_violation_rejected(void) {
	fresh_provisioning(g_test_root);
	auto& p = Provisioner::instance();

	// Constraint declared as 0..3600; 9999 violates.
	TEST_ASSERT_FALSE(p.field(CUSTOM_NS_ID, CUSTOM_COMMAND, Value((int64_t)9999)));
	TEST_ASSERT_EQUAL(0, g_command_invocations);
}

// ---------------------------------------------------------------------------
// command_void — argument-less commands
// ---------------------------------------------------------------------------

void test_command_void_fires_setter_on_commit(void) {
	fresh_provisioning(g_test_root);
	auto& p = Provisioner::instance();

	TEST_ASSERT_EQUAL(0, g_void_command_invocations);

	// SET_STATE with a void marker, then commit — setter fires once.
	TEST_ASSERT_TRUE(p.field(CUSTOM_NS_ID, CUSTOM_VOID_CMD, Value::make_void()));
	TEST_ASSERT_TRUE(p.commit(CUSTOM_NS_ID));
	TEST_ASSERT_EQUAL(1, g_void_command_invocations);
}

void test_command_void_read_returns_none(void) {
	fresh_provisioning(g_test_root);
	auto& p = Provisioner::instance();

	// Write-only: reads return None regardless of prior invocations.
	TEST_ASSERT_EQUAL(Provisioning::Type::None,
		(int)p.field(CUSTOM_NS_ID, CUSTOM_VOID_CMD).type());

	p.field(CUSTOM_NS_ID, CUSTOM_VOID_CMD, Value::make_void());
	TEST_ASSERT_TRUE(p.commit(CUSTOM_NS_ID));
	TEST_ASSERT_EQUAL(Provisioning::Type::None,
		(int)p.field(CUSTOM_NS_ID, CUSTOM_VOID_CMD).type());
}

void test_command_void_wire_set_state_with_nil(void) {
	// End-to-end wire trip: SET_STATE carrying nil as the void field's
	// value, then COMMIT — the setter must fire.
	fresh_provisioning(g_test_root);
	auto& p = Provisioner::instance();

	Bytes set_req = make_request((uint8_t)Op::SetState, 1, [&](MsgPack::Packer& pk) {
		// Payload is the bare {ns_id: {field_id: value}} map.
		pk.serialize(MsgPack::map_size_t(1));
		pk.serialize((uint16_t)CUSTOM_NS_ID);
		pk.serialize(MsgPack::map_size_t(1));
		pk.serialize((uint16_t)CUSTOM_VOID_CMD);
		MsgPack::object::nil_t nil;
		pk.serialize(nil);
	});
	Bytes set_resp = p.handle_message(set_req);
	TEST_ASSERT_GREATER_THAN(0, set_resp.size());

	Bytes commit_req = make_request((uint8_t)Op::Commit, 2, [&](MsgPack::Packer& pk) {
		MsgPack::object::nil_t nil;
		pk.serialize(nil);
	});
	(void)p.handle_message(commit_req);

	TEST_ASSERT_EQUAL(1, g_void_command_invocations);
}

void test_command_void_not_persisted_or_replayed(void) {
	fresh_provisioning(g_test_root);
	auto& p = Provisioner::instance();

	TEST_ASSERT_TRUE(p.field(CUSTOM_NS_ID, CUSTOM_VOID_CMD, Value::make_void()));
	TEST_ASSERT_TRUE(p.commit(CUSTOM_NS_ID));
	TEST_ASSERT_EQUAL(1, g_void_command_invocations);

	// Simulated reboot: counter must stay 0 — write-only commands don't
	// replay on apply_loaded_to_runtime.
	reset_setter_counters();
	fresh_provisioning(g_test_root);
	TEST_ASSERT_EQUAL(0, g_void_command_invocations);
}

// ---------------------------------------------------------------------------
// Hierarchical namespaces (Follow-up #5)
// ---------------------------------------------------------------------------

static constexpr uint16_t HIER_INTERFACES = 9200;
static constexpr uint16_t HIER_LORA       = 9201;
static constexpr uint16_t HIER_UDP        = 9202;

static void register_hierarchy() {
	Provisioner::instance()
		.namespace_("Interfaces", HIER_INTERFACES)
			.field_bool("enabled", 1, FF_LIVE_APPLY, true)
			.namespace_("LoRa", HIER_LORA)
				.field_float("frequency", 1, FF_REBOOT_REQUIRED, 915e6, 100e6, 1e9)
				.end()
			.namespace_("UDP", HIER_UDP)
				.field_string("host", 1, FF_LIVE_APPLY, "0.0.0.0", 64)
				.end()
			.end();
}

void test_hierarchy_nested_builder_registers_all(void) {
	Provisioner::instance().end();
	register_custom_namespace();
	register_hierarchy();
	Provisioner::instance().begin(g_test_root.c_str());

	const Registry& reg = Provisioner::instance().registry();
	const Namespace* interfaces = reg.find(HIER_INTERFACES);
	const Namespace* lora       = reg.find(HIER_LORA);
	const Namespace* udp        = reg.find(HIER_UDP);

	TEST_ASSERT_NOT_NULL(interfaces);
	TEST_ASSERT_NOT_NULL(lora);
	TEST_ASSERT_NOT_NULL(udp);

	TEST_ASSERT_EQUAL(0, interfaces->parent_id());        // root
	TEST_ASSERT_EQUAL(HIER_INTERFACES, lora->parent_id());
	TEST_ASSERT_EQUAL(HIER_INTERFACES, udp->parent_id());

	// Children-of query returns both leaves.
	auto kids = reg.children_of(HIER_INTERFACES);
	TEST_ASSERT_EQUAL_size_t(2, kids.size());
}

void test_hierarchy_root_has_no_parent(void) {
	Provisioner::instance().end();
	register_custom_namespace();
	register_hierarchy();
	Provisioner::instance().begin(g_test_root.c_str());

	const Namespace* reticulum = Provisioner::instance().registry().find(Ns::Reticulum::Id);
	TEST_ASSERT_NOT_NULL(reticulum);
	TEST_ASSERT_EQUAL(0, reticulum->parent_id());
}

void test_hierarchy_storage_filename_is_dotted_path(void) {
	Provisioner::instance().end();
	register_custom_namespace();
	register_hierarchy();
	Provisioner::instance().begin(g_test_root.c_str());

	auto& p = Provisioner::instance();
	TEST_ASSERT_TRUE(p.field(HIER_LORA, 1, Value((double)868e6)));
	TEST_ASSERT_TRUE(p.commit(HIER_LORA));

	// The file for the child namespace should land at the dotted path.
	const std::string lora_path = g_test_root + "/Interfaces.LoRa.msgpack";
	TEST_ASSERT_TRUE(Utilities::OS::file_exists(lora_path.c_str()));
	// And the flat (non-dotted) name must NOT exist as a separate file.
	const std::string flat_path = g_test_root + "/LoRa.msgpack";
	TEST_ASSERT_FALSE(Utilities::OS::file_exists(flat_path.c_str()));
}

void test_hierarchy_schema_emits_parent_id(void) {
	Provisioner::instance().end();
	register_custom_namespace();
	register_hierarchy();
	Provisioner::instance().begin(g_test_root.c_str());

	auto& p = Provisioner::instance();

	Bytes req = make_request((uint8_t)Op::GetSchema, 1, [](MsgPack::Packer& pk) {
		MsgPack::object::nil_t n; pk.serialize(n);
	});
	Bytes resp = p.handle_message(req);

	// Parse envelope, then the schema array.
	MsgPack::Unpacker u; u.feed(resp.data(), resp.size());
	u.unpackArraySize();          // envelope [op, seq, payload]
	uint8_t op; u.deserialize(op);
	uint64_t seq; u.deserialize(seq);
	TEST_ASSERT_TRUE(u.isArray());
	const size_t n_ns = u.unpackArraySize();

	bool found_lora = false;
	for (size_t i = 0; i < n_ns; ++i) {
		TEST_ASSERT_TRUE(u.isArray());
		const size_t ns_arr_size = u.unpackArraySize();
		TEST_ASSERT_EQUAL_size_t(4, ns_arr_size);   // [id, name, parent_id, [fields]]

		int64_t ns_id = 0; u.deserialize(ns_id);
		MsgPack::str_t name; u.deserialize(name);
		int64_t parent = 0; u.deserialize(parent);

		// Field-array shape: read its size then skip every entry.
		const size_t n_fields = u.unpackArraySize();
		for (size_t j = 0; j < n_fields; ++j) {
			const size_t field_map_size = u.unpackMapSize();
			for (size_t k = 0; k < field_map_size; ++k) {
				int64_t key = 0; u.deserialize(key);
				if (u.isBool())                              { bool b; u.deserialize(b); }
				else if (u.isUInt() || u.isInt())            { int64_t v; u.deserialize(v); }
				else if (u.isFloat32() || u.isFloat64())     { double d; u.deserialize(d); }
				else if (u.isStr())                          { MsgPack::str_t s; u.deserialize(s); }
				else if (u.isBin())                          { MsgPack::bin_t<uint8_t> b; u.deserialize(b); }
				else if (u.isNil())                          { MsgPack::object::nil_t z; u.deserialize(z); }
				else if (u.isArray()) {
					size_t a = u.unpackArraySize();
					for (size_t z = 0; z < a; ++z) {
						if (u.isUInt() || u.isInt())   { int64_t v; u.deserialize(v); }
						else if (u.isStr())            { MsgPack::str_t s; u.deserialize(s); }
						else if (u.isBin())            { MsgPack::bin_t<uint8_t> bb; u.deserialize(bb); }
					}
				}
			}
		}

		if (ns_id == HIER_LORA) {
			TEST_ASSERT_EQUAL(HIER_INTERFACES, parent);
			found_lora = true;
		}
	}
	TEST_ASSERT_TRUE(found_lora);
}

void test_hierarchy_get_state_stays_flat(void) {
	Provisioner::instance().end();
	register_custom_namespace();
	register_hierarchy();
	Provisioner::instance().begin(g_test_root.c_str());

	auto& p = Provisioner::instance();
	// Touch the LoRa field via SET+COMMIT so it has a non-default value.
	TEST_ASSERT_TRUE(p.field(HIER_LORA, 1, Value((double)868e6)));
	TEST_ASSERT_TRUE(p.commit(HIER_LORA));

	// GET_STATE for HIER_LORA must return its values keyed by HIER_LORA
	// at the top level — not nested under HIER_INTERFACES.
	Bytes req = make_request((uint8_t)Op::GetState, 1, [&](MsgPack::Packer& pk) {
		pk.serialize(MsgPack::map_size_t(1));
		pk.serialize((uint16_t)Key::NamespaceFilter);
		pk.serialize(MsgPack::arr_size_t(1));
		pk.serialize((uint16_t)HIER_LORA);
	});
	Bytes resp = p.handle_message(req);

	MsgPack::Unpacker u; u.feed(resp.data(), resp.size());
	u.unpackArraySize();
	uint8_t op; u.deserialize(op);
	uint64_t seq; u.deserialize(seq);
	TEST_ASSERT_TRUE(u.isMap());
	const size_t n_ns = u.unpackMapSize();
	TEST_ASSERT_EQUAL_size_t(1, n_ns);
	int64_t top_key = 0; u.deserialize(top_key);
	TEST_ASSERT_EQUAL(HIER_LORA, top_key);   // leaf id at top — NOT parent id
}

void test_hierarchy_unbalanced_scope_is_recovered(void) {
	Provisioner::instance().end();
	register_custom_namespace();
	// Deliberately open a namespace without calling .end(). Provisioner::begin()
	// should warn and clear the scope so the next round of registrations
	// isn't poisoned.
	Provisioner::instance().namespace_("Forgotten", 9300)
		.field_int("x", 1, FF_LIVE_APPLY, 0);
	// (no .end() here)

	Provisioner::instance().begin(g_test_root.c_str());
	TEST_ASSERT_TRUE(Provisioner::instance().registry().find((uint16_t)9300) != nullptr);
	// Scope must be empty after begin() — subsequent registrations would
	// otherwise nest under the orphan. Best-effort check via behavior:
	Provisioner::instance().namespace_("AfterBegin", 9301).field_int("y", 1, FF_LIVE_APPLY, 0).end();
	const Namespace* after = Provisioner::instance().registry().find((uint16_t)9301);
	TEST_ASSERT_NOT_NULL(after);
	TEST_ASSERT_EQUAL(0, after->parent_id());   // would be 9300 if scope had leaked
}

void test_hierarchy_cycle_rejected(void) {
	Provisioner::instance().end();
	register_custom_namespace();
	Provisioner::instance().begin(g_test_root.c_str());

	Registry& reg = Provisioner::instance().registry();
	// Self-cycle: parent_id == own id.
	TEST_ASSERT_NULL(reg.add_namespace(9400, "BadSelfCycle", 9400));
	// Missing parent.
	TEST_ASSERT_NULL(reg.add_namespace(9401, "OrphanChild", 9999));
}

void test_unknown_field_ignored_on_load(void) {
	// Hand-craft a MsgPack file with an extra unknown field and verify it
	// loads cleanly with the unknown id silently dropped.
	fresh_provisioning(g_test_root);
	auto& p = Provisioner::instance();

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
	auto& p2 = Provisioner::instance();

	TEST_ASSERT_EQUAL_INT64(77, p2.field(CUSTOM_NS_ID, CUSTOM_INT).as_int());
}

void test_atomic_write_resilience(void) {
	// Write a normal file, then drop a stray .tmp and ensure load_all
	// removes it and still loads the real file.
	fresh_provisioning(g_test_root);
	auto& p = Provisioner::instance();
	p.field(CUSTOM_NS_ID, CUSTOM_INT, Value((int64_t)21));
	TEST_ASSERT_TRUE(p.commit());

	// Plant a garbage .tmp (mimicking an interrupted save).
	std::string tmp_path = g_test_root + "/custom.msgpack.tmp";
	Bytes garbage("not valid msgpack");
	Utilities::OS::write_file(tmp_path.c_str(), garbage);
	TEST_ASSERT_TRUE(Utilities::OS::file_exists(tmp_path.c_str()));

	fresh_provisioning(g_test_root);
	auto& p2 = Provisioner::instance();

	TEST_ASSERT_EQUAL_INT64(21, p2.field(CUSTOM_NS_ID, CUSTOM_INT).as_int());
	// .tmp was cleaned up on load.
	TEST_ASSERT_FALSE(Utilities::OS::file_exists(tmp_path.c_str()));
}

void test_factory_reset(void) {
	fresh_provisioning(g_test_root);
	auto& p = Provisioner::instance();

	p.field(CUSTOM_NS_ID, CUSTOM_INT, Value((int64_t)42));
	p.commit();
	TEST_ASSERT_EQUAL_INT64(42, p.field(CUSTOM_NS_ID, CUSTOM_INT).as_int());

	TEST_ASSERT_TRUE(p.factory_reset());
	TEST_ASSERT_EQUAL_INT64(5, p.field(CUSTOM_NS_ID, CUSTOM_INT).as_int());
	TEST_ASSERT_FALSE(Utilities::OS::file_exists((g_test_root + "/custom.msgpack").c_str()));

	// Survives reboot.
	fresh_provisioning(g_test_root);
	TEST_ASSERT_EQUAL_INT64(5, Provisioner::instance().field(CUSTOM_NS_ID, CUSTOM_INT).as_int());
}

void test_by_name_accessors(void) {
	fresh_provisioning(g_test_root);
	auto& p = Provisioner::instance();

	TEST_ASSERT_TRUE(p.field("custom", "level", Value((int64_t)17)));
	TEST_ASSERT_EQUAL_INT64(17, p.field("custom", "level", Provisioner::Source::Draft).as_int());

	TEST_ASSERT_TRUE(p.commit("custom"));
	TEST_ASSERT_EQUAL_INT64(17, p.field("custom", "level").as_int());

	// Unknown ns/field returns a None value rather than throwing.
	Value missing = p.field("nope", "nada");
	TEST_ASSERT_EQUAL(Provisioning::Type::None, (int)missing.type());
}

void test_duplicate_field_id_rejected(void) {
	// Register a fresh namespace with a duplicate field id; second add
	// must fail (Namespace::add_field returns false).
	Provisioner::instance().end();
	auto b = Provisioner::instance()
		.namespace_("dup", 9200)
		.field_int("a", 1, FF_LIVE_APPLY, 0, 0, 10)
		.field_int("b", 1, FF_LIVE_APPLY, 0, 0, 10);
	// Pull the underlying namespace to inspect.
	Provisioner::instance().begin(g_test_root.c_str());
	const Namespace* ns = Provisioner::instance().registry().find((uint16_t)9200);
	TEST_ASSERT_NOT_NULL(ns);
	TEST_ASSERT_EQUAL(1u, ns->fields().size());	// duplicate dropped
}

// ---------------------------------------------------------------------------
// Namespace-level on_commit callback
// ---------------------------------------------------------------------------

void test_on_commit_callback_fires_only_when_drafts_present(void) {
	fresh_provisioning(g_test_root);
	auto& p = Provisioner::instance();
	Namespace* ns = p.registry().find(CUSTOM_NS_ID);
	TEST_ASSERT_NOT_NULL(ns);

	int callback_count = 0;
	ns->on_commit([&](Namespace&) { ++callback_count; });

	// No drafts staged → commit should skip the callback entirely.
	TEST_ASSERT_TRUE(p.commit(CUSTOM_NS_ID));
	TEST_ASSERT_EQUAL(0, callback_count);

	// Stage a draft → callback fires exactly once on this commit pass.
	TEST_ASSERT_TRUE(p.field(CUSTOM_NS_ID, CUSTOM_INT, Value((int64_t)42)));
	TEST_ASSERT_TRUE(p.commit(CUSTOM_NS_ID));
	TEST_ASSERT_EQUAL(1, callback_count);

	// And the field's own setter still ran.
	TEST_ASSERT_EQUAL(1, g_live_int_setter_count);
	TEST_ASSERT_EQUAL_INT64(42, g_live_int_setter_value);
}

void test_on_commit_callback_runs_before_field_setters(void) {
	fresh_provisioning(g_test_root);
	auto& p = Provisioner::instance();
	Namespace* ns = p.registry().find(CUSTOM_NS_ID);
	TEST_ASSERT_NOT_NULL(ns);

	int setter_count_observed = -1;
	ns->on_commit([&](Namespace&) {
		// Snapshot the field setter counter at the moment the namespace
		// callback fires — it must still be 0, proving the namespace
		// callback wins the ordering race against commit_one's setter.
		setter_count_observed = g_live_int_setter_count;
	});

	TEST_ASSERT_TRUE(p.field(CUSTOM_NS_ID, CUSTOM_INT, Value((int64_t)11)));
	TEST_ASSERT_TRUE(p.commit(CUSTOM_NS_ID));
	TEST_ASSERT_EQUAL(0, setter_count_observed);
	TEST_ASSERT_EQUAL(1, g_live_int_setter_count);
}

void test_on_commit_callback_can_revert_specific_draft(void) {
	fresh_provisioning(g_test_root);
	auto& p = Provisioner::instance();
	Namespace* ns = p.registry().find(CUSTOM_NS_ID);
	TEST_ASSERT_NOT_NULL(ns);

	ns->on_commit([](Namespace& self) {
		// Veto the level change but allow the label change to proceed.
		self.clear_draft(CUSTOM_INT);
	});

	TEST_ASSERT_TRUE(p.field(CUSTOM_NS_ID, CUSTOM_INT, Value((int64_t)42)));
	TEST_ASSERT_TRUE(p.field(CUSTOM_NS_ID, CUSTOM_STR, Value("ok")));
	TEST_ASSERT_TRUE(p.commit(CUSTOM_NS_ID));

	// Reverted draft: setter never fires, working untouched.
	TEST_ASSERT_EQUAL(0, g_live_int_setter_count);
	TEST_ASSERT_EQUAL_INT64(5, p.field(CUSTOM_NS_ID, CUSTOM_INT).as_int());
	// Unreverted draft: still applied.
	TEST_ASSERT_EQUAL_STRING("ok", p.field(CUSTOM_NS_ID, CUSTOM_STR).as_string().c_str());
}

void test_on_commit_callback_can_revert_entire_namespace(void) {
	fresh_provisioning(g_test_root);
	auto& p = Provisioner::instance();
	Namespace* ns = p.registry().find(CUSTOM_NS_ID);
	TEST_ASSERT_NOT_NULL(ns);

	ns->on_commit([](Namespace& self) { self.clear_draft(); });

	TEST_ASSERT_TRUE(p.field(CUSTOM_NS_ID, CUSTOM_INT, Value((int64_t)42)));
	TEST_ASSERT_TRUE(p.field(CUSTOM_NS_ID, CUSTOM_STR, Value("vetoed")));
	TEST_ASSERT_TRUE(p.commit(CUSTOM_NS_ID));

	TEST_ASSERT_EQUAL(0, g_live_int_setter_count);
	TEST_ASSERT_EQUAL_INT64(5, p.field(CUSTOM_NS_ID, CUSTOM_INT).as_int());
	TEST_ASSERT_EQUAL_STRING("default", p.field(CUSTOM_NS_ID, CUSTOM_STR).as_string().c_str());
}

void test_on_commit_callback_can_amend_draft(void) {
	fresh_provisioning(g_test_root);
	auto& p = Provisioner::instance();
	Namespace* ns = p.registry().find(CUSTOM_NS_ID);
	TEST_ASSERT_NOT_NULL(ns);

	ns->on_commit([](Namespace& self) {
		// Clamp the level draft to 50 — exercises set_draft inside the hook.
		Value v;
		if (self.draft(CUSTOM_INT, v) && v.as_int() > 50) {
			self.set_draft(CUSTOM_INT, Value((int64_t)50));
		}
	});

	TEST_ASSERT_TRUE(p.field(CUSTOM_NS_ID, CUSTOM_INT, Value((int64_t)90)));
	TEST_ASSERT_TRUE(p.commit(CUSTOM_NS_ID));

	TEST_ASSERT_EQUAL(1, g_live_int_setter_count);
	TEST_ASSERT_EQUAL_INT64(50, g_live_int_setter_value);
	TEST_ASSERT_EQUAL_INT64(50, p.field(CUSTOM_NS_ID, CUSTOM_INT).as_int());
}

void test_on_commit_callback_scoped_per_namespace(void) {
	// Two namespaces with their own callbacks. A draft on one must not
	// fire the other's hook.
	fresh_provisioning(g_test_root);
	auto& p = Provisioner::instance();
	Namespace* ns_custom = p.registry().find(CUSTOM_NS_ID);
	TEST_ASSERT_NOT_NULL(ns_custom);
	Namespace* ns_other = p.registry().find(Ns::Transport::Id);
	TEST_ASSERT_NOT_NULL(ns_other);

	int custom_count = 0;
	int other_count = 0;
	ns_custom->on_commit([&](Namespace&) { ++custom_count; });
	ns_other->on_commit([&](Namespace&) { ++other_count; });

	TEST_ASSERT_TRUE(p.field(CUSTOM_NS_ID, CUSTOM_INT, Value((int64_t)42)));
	// Global commit walks every namespace; only the one with a draft
	// should invoke its hook.
	TEST_ASSERT_TRUE(p.commit());
	TEST_ASSERT_EQUAL(1, custom_count);
	TEST_ASSERT_EQUAL(0, other_count);
}

// ---------------------------------------------------------------------------
// Wire round-trip tests — exercise handle_message()
// ---------------------------------------------------------------------------

void test_wire_get_info(void) {
	fresh_provisioning(g_test_root);
	auto& p = Provisioner::instance();

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
	auto& p = Provisioner::instance();

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
		p.field(CUSTOM_NS_ID, CUSTOM_INT, Provisioner::Source::Draft).as_int());

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
	auto& p = Provisioner::instance();

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
	auto& p = Provisioner::instance();

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
	auto& p = Provisioner::instance();
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

void test_wire_reboot(void) {
	fresh_provisioning(g_test_root);
	auto& p = Provisioner::instance();

	int callback_count = 0;
	p.on_reboot([&callback_count]() { ++callback_count; });

	Bytes req = make_request((uint8_t)Op::Reboot, 3, [](MsgPack::Packer& pk) {
		MsgPack::object::nil_t n;
		pk.serialize(n);
	});
	Bytes resp = p.handle_message(req);
	TEST_ASSERT_GREATER_THAN(0, resp.size());
	TEST_ASSERT_EQUAL(1, callback_count);

	// Response envelope echoes the Reboot op id and seq; no payload map.
	MsgPack::Unpacker u;
	u.feed(resp.data(), resp.size());
	u.unpackArraySize();
	uint8_t op = 0; u.deserialize(op);
	uint64_t seq = 0; u.deserialize(seq);
	TEST_ASSERT_EQUAL((uint8_t)Op::Reboot, op);
	TEST_ASSERT_EQUAL(3, seq);
}

void test_wire_reboot_no_callback_still_acks(void) {
	fresh_provisioning(g_test_root);
	auto& p = Provisioner::instance();
	// Intentionally do NOT register on_reboot.

	Bytes req = make_request((uint8_t)Op::Reboot, 1, [](MsgPack::Packer& pk) {
		MsgPack::object::nil_t n;
		pk.serialize(n);
	});
	Bytes resp = p.handle_message(req);

	MsgPack::Unpacker u;
	u.feed(resp.data(), resp.size());
	u.unpackArraySize();
	uint8_t op = 0; u.deserialize(op);
	TEST_ASSERT_EQUAL((uint8_t)Op::Reboot, op);
}

// Captures field state at the moment the callback fires, so we can prove
// the internal reset has already restored defaults by then.
static int g_factory_reset_cb_count = 0;
static int64_t g_factory_reset_cb_observed_int = -1;

void test_factory_reset_callback_fires_after_internal_reset(void) {
	fresh_provisioning(g_test_root);
	auto& p = Provisioner::instance();

	g_factory_reset_cb_count = 0;
	g_factory_reset_cb_observed_int = -1;
	p.on_factory_reset([]() {
		++g_factory_reset_cb_count;
		g_factory_reset_cb_observed_int =
			Provisioner::instance().field(CUSTOM_NS_ID, CUSTOM_INT).as_int();
	});

	// Mutate so default vs current is visible.
	p.field(CUSTOM_NS_ID, CUSTOM_INT, Value((int64_t)42));
	p.commit();
	TEST_ASSERT_EQUAL_INT64(42, p.field(CUSTOM_NS_ID, CUSTOM_INT).as_int());

	TEST_ASSERT_TRUE(p.factory_reset());
	TEST_ASSERT_EQUAL(1, g_factory_reset_cb_count);
	// Callback ran AFTER the internal reset, so it observed the default (5).
	TEST_ASSERT_EQUAL_INT64(5, g_factory_reset_cb_observed_int);

	// Wire path also fires it.
	p.field(CUSTOM_NS_ID, CUSTOM_INT, Value((int64_t)99));
	p.commit();
	Bytes req = make_request((uint8_t)Op::FactoryReset, 2, [](MsgPack::Packer& pk) {
		MsgPack::object::nil_t n;
		pk.serialize(n);
	});
	p.handle_message(req);
	TEST_ASSERT_EQUAL(2, g_factory_reset_cb_count);
}

void test_wire_unknown_op_error(void) {
	fresh_provisioning(g_test_root);
	auto& p = Provisioner::instance();

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
	RUN_TEST(test_reboot_required_callback_fires_once);
	RUN_TEST(test_getter_reflects_direct_setter);
	RUN_TEST(test_set_draft_dedups_against_live_runtime);
	RUN_TEST(test_reboot_required_dedups_against_working);
	RUN_TEST(test_boot_apply_fires_reboot_required_setter);
	RUN_TEST(test_bytes_list_default_is_empty);
	RUN_TEST(test_bytes_list_set_commit_persists);
	RUN_TEST(test_bytes_list_constraint_rejects_wrong_element_size);
	RUN_TEST(test_bytes_list_getter_reflects_direct_setter);
	RUN_TEST(test_transport_identity_getter_unset_returns_empty);
	RUN_TEST(test_transport_identity_commit_reloads_on_reboot);
	RUN_TEST(test_transport_identity_excluded_from_get_state);
	RUN_TEST(test_metric_getter_reflects_runtime);
	RUN_TEST(test_metric_rejects_set_state);
	RUN_TEST(test_metric_not_persisted);
	RUN_TEST(test_command_fires_setter_on_commit);
	RUN_TEST(test_command_can_be_invoked_repeatedly_with_same_arg);
	RUN_TEST(test_command_read_returns_none);
	RUN_TEST(test_command_not_persisted_or_replayed);
	RUN_TEST(test_command_excluded_from_get_state);
	RUN_TEST(test_command_constraint_violation_rejected);
	RUN_TEST(test_command_void_fires_setter_on_commit);
	RUN_TEST(test_command_void_read_returns_none);
	RUN_TEST(test_command_void_wire_set_state_with_nil);
	RUN_TEST(test_command_void_not_persisted_or_replayed);
	RUN_TEST(test_hierarchy_nested_builder_registers_all);
	RUN_TEST(test_hierarchy_root_has_no_parent);
	RUN_TEST(test_hierarchy_storage_filename_is_dotted_path);
	RUN_TEST(test_hierarchy_schema_emits_parent_id);
	RUN_TEST(test_hierarchy_get_state_stays_flat);
	RUN_TEST(test_hierarchy_unbalanced_scope_is_recovered);
	RUN_TEST(test_hierarchy_cycle_rejected);
	RUN_TEST(test_unknown_field_ignored_on_load);
	RUN_TEST(test_atomic_write_resilience);
	RUN_TEST(test_factory_reset);
	RUN_TEST(test_by_name_accessors);
	RUN_TEST(test_duplicate_field_id_rejected);
	RUN_TEST(test_on_commit_callback_fires_only_when_drafts_present);
	RUN_TEST(test_on_commit_callback_runs_before_field_setters);
	RUN_TEST(test_on_commit_callback_can_revert_specific_draft);
	RUN_TEST(test_on_commit_callback_can_revert_entire_namespace);
	RUN_TEST(test_on_commit_callback_can_amend_draft);
	RUN_TEST(test_on_commit_callback_scoped_per_namespace);
	RUN_TEST(test_wire_get_info);
	RUN_TEST(test_wire_set_state_then_commit);
	RUN_TEST(test_wire_set_state_constraint_error);
	RUN_TEST(test_wire_get_capabilities);
	RUN_TEST(test_wire_factory_reset);
	RUN_TEST(test_wire_reboot);
	RUN_TEST(test_wire_reboot_no_callback_still_acks);
	RUN_TEST(test_factory_reset_callback_fires_after_internal_reset);
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
