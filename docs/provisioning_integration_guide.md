# microReticulum Provisioning — Integration Guide

This guide is for **firmware and application developers** building on top of the microReticulum library who want to expose configuration, instrumentation, and command surfaces to outside clients through the Provisioning subsystem.

It covers the C++ API: build flags, registration patterns, field lifecycle, persistence, hierarchical organization, hooking into a transport, and the pitfalls that come with `static`-backed state on a singleton.

For the **wire protocol** (what clients send and receive), see [`provisioning_client_guide.md`](provisioning_client_guide.md).

## Table of contents

1. [What Provisioning gives you](#what-provisioning-gives-you)
2. [Build flags](#build-flags)
3. [Lifecycle and bootstrap](#lifecycle-and-bootstrap)
4. [Registering namespaces and fields](#registering-namespaces-and-fields)
5. [Field types reference](#field-types-reference)
6. [Field flags and the lifecycle they imply](#field-flags-and-the-lifecycle-they-imply)
7. [Setter and getter contracts](#setter-and-getter-contracts)
8. [Namespace-level commit hook](#namespace-level-commit-hook)
9. [Working, draft, and commit semantics](#working-draft-and-commit-semantics)
10. [Persistence on flash](#persistence-on-flash)
11. [Hierarchical namespaces (nested builder)](#hierarchical-namespaces-nested-builder)
12. [Reboot handling](#reboot-handling)
13. [Direct in-process accessors](#direct-in-process-accessors)
14. [Hooking up a transport](#hooking-up-a-transport)
15. [Patterns: metrics and commands](#patterns-metrics-and-commands)
16. [Linker exclusion when Provisioning isn't used](#linker-exclusion-when-provisioning-isnt-used)
17. [Field & namespace id stability](#field--namespace-id-stability)
18. [Testing patterns](#testing-patterns)
19. [Common pitfalls](#common-pitfalls)
20. [API reference summary](#api-reference-summary)

---

## What Provisioning gives you

The Provisioning subsystem (`src/Provisioning/`) provides a schema-driven configuration layer with these capabilities:

- **Typed fields** with declarative constraints, organized into named **namespaces** (optionally hierarchical).
- A **working/draft/commit** workflow so external tools can stage edits, validate them, and apply atomically.
- **Atomic per-namespace persistence** to a flash filesystem (when `RNS_USE_FS` is enabled), with reload at boot and graceful recovery from interrupted writes.
- A **MsgPack wire codec** with `Bytes Provisioner::handle_message(const Bytes&)` for any transport you choose (KISS, BLE, Web Serial, raw socket, Reticulum link).
- **In-process accessors** by namespace/field id or by name so app code can read configured values without going through the wire.
- **Read-only metric fields** (counters/sensors) and **write-only command fields** (one-shot actions) on top of the same schema model.
- A **linker-level opt-out** so you pay zero binary cost if your firmware doesn't need Provisioning.

The subsystem deliberately doesn't handle:

- Framing or transport (KISS, BLE GATT, Web Serial) — that's the application's job.
- Authentication or encryption — wrap your transport.
- Identity key generation flows (use `RNS::Identity` directly).

## Build flags

Both flags default to **ON** in CMake and PlatformIO. You can disable them per-environment to opt out.

| Flag | Default | Effect |
|---|---|---|
| `RNS_USE_PROVISIONING` | ON | `Reticulum::start()` calls `Provisioning::Provisioner::instance().begin()`. With this off, **zero Provisioning symbols are linked into the final binary** — apps can use only the existing fluent setters as before. |
| `RNS_USE_FS`           | ON | Provisioning persists committed values to per-namespace files on flash. With this off, Provisioning still runs but is RAM-only: edits and commits work, but everything is lost on reboot. |

Combination matrix:

| `RNS_USE_PROVISIONING` | `RNS_USE_FS` | Behaviour |
|---|---|---|
| OFF | (either) | Library compiled but **never linked** into the firmware. Apps use existing setters directly. |
| ON  | OFF      | Provisioning starts at boot, accepts wire commands, but in-RAM only. Reboot loses commits. |
| ON  | ON       | Full system: drafts, commits, atomic flash writes, reload-and-apply at next boot. |

To opt out for a specific PlatformIO env, override `build_flags` and drop the flag, or pass `-DRNS_USE_PROVISIONING=OFF`. For CMake, pass `-DRNS_USE_PROVISIONING=OFF` at configure time.

## Lifecycle and bootstrap

`Reticulum::start()` automatically starts Provisioning when `RNS_USE_PROVISIONING` is defined:

```cpp
void Reticulum::start() {
    // ... time setup ...
#ifdef RNS_USE_PROVISIONING
    INFO("Starting Provisioning...");
    Provisioning::Provisioner::instance().begin();
#endif
    INFO("Starting Transport...");
    Transport::start(*this);
    // ...
}
```

Order is significant: Provisioning runs **before** `Transport::start()` so any committed values that affect Transport (path table sizes, identity, etc.) are already in the runtime statics by the time Transport's own initialization reads them.

Inside `Provisioner::begin()`:

1. **Registers built-in namespaces** (`Reticulum`, `Transport`) with their fields, setters, and getters.
2. (`RNS_USE_FS` only) **Mounts storage** under `<root>/config/` and walks every registered namespace, loading any persisted `.msgpack` file into the namespace's working map.
3. (`RNS_USE_FS` only) **Applies the loaded values to the runtime** by firing each field's setter for any value that differs from the field's declared default. This is what makes persistence actually take effect — committed values from prior boots are re-applied via the setter callbacks.

App code that wants to register its own namespaces should do so **before** `Reticulum::start()` runs. Register order:

```cpp
void setup() {
    // 1. Initialize filesystem first (Provisioning needs it for persistence).
    OS::register_filesystem(my_fs);

    // 2. Register any app-defined namespaces.
    register_my_app_namespaces();   // see "Registering namespaces" below

    // 3. Start Reticulum — Provisioning::begin() runs inside this.
    reticulum.start();

    // 4. Register a reboot-required callback if you want to react when a
    //    committed field flips needs_reboot from false -> true. Typical
    //    integrations leave this unset and let the user trigger the reboot.
    Provisioning::Provisioner::instance().on_reboot_required([]{ display_reboot_badge(); });

    // 5. Register an active reboot callback if the firmware should reboot
    //    when the client sends the explicit REBOOT wire op (op = 9).
    Provisioning::Provisioner::instance().on_reboot([]{ schedule_reboot(); });

    // 6. Optional: extend FACTORY_RESET with app-side cleanup. Fires after
    //    Provisioning's internal reset has completed.
    Provisioning::Provisioner::instance().on_factory_reset([]{ wipe_app_state(); });
}
```

If app code calls `Provisioner::instance().begin()` itself, the auto-start in `Reticulum::start()` is a no-op (begin is idempotent).

`Provisioner::end()` resets the entire singleton: clears the registry, drops storage, clears reboot state, clears the registration scope stack, and drops the three app-registered callbacks (`on_reboot_required`, `on_reboot`, `on_factory_reset`). Used in tests and on shutdown. Not typically called by production firmware.

## Registering namespaces and fields

All registration goes through the fluent `NamespaceBuilder` returned by `Provisioner::namespace_()` or `Provisioner::register_namespace()` (the same call under two names — `register_namespace` reads better from inside macros).

The minimum example — a single namespace with a couple of fields:

```cpp
#include <Provisioning/Provisioning.h>
using namespace RNS::Provisioning;

constexpr uint16_t NS_MYAPP    = 200;   // app range, see id stability section
constexpr uint16_t F_NAME      = 1;
constexpr uint16_t F_BRIGHTNESS = 2;

void register_my_app_namespaces() {
    Provisioner::instance()
        .namespace_("MyApp", NS_MYAPP)
            .field_string("device_name", F_NAME, FF_LIVE_APPLY, "device-001", 32,
                [](const Value& v) { set_device_name(v.as_string()); return true; },
                []() { return std::string(get_device_name()); })
            .field_int("brightness", F_BRIGHTNESS, FF_LIVE_APPLY, 50, 0, 100,
                [](const Value& v) { set_brightness(v.as_int()); return true; },
                []() { return (int64_t)get_brightness(); })
            .end();
}
```

Notes:

- The name `"MyApp"` is what shows up in `GET_SCHEMA`. Doesn't have to be unique globally — it just needs to be human-recognizable for the UI.
- The id `200` is the namespace's stable identifier. Once shipped, never change it. Pick from the appropriate range — see [Field & namespace id stability](#field--namespace-id-stability).
- The setter callback receives a `Value` whose declared type matches `field_int` / `field_string` / etc. — call the matching `as_*` accessor to extract.
- The getter callback returns the live runtime value. Provisioning reads always go through the getter when present, so direct mutations to your runtime statics don't cause drift.
- `.end()` is required to close the namespace scope (otherwise the next registration nests under this one — used intentionally for [hierarchical namespaces](#hierarchical-namespaces-nested-builder)).

## Field types reference

Every typed field has a dedicated builder method on `NamespaceBuilder`. All take the field name (string), field id (uint16), flag bitfield, default value, optional type-specific constraint, optional setter, and optional typed getter.

| Builder | C++ runtime type | Use for |
|---|---|---|
| `field_bool(name, id, flags, default, [setter, getter])`                          | `bool`                  | On/off toggles. |
| `field_int(name, id, flags, default, [min, max], [setter, getter])`               | `int64_t`               | Integers with optional range constraint. The range overload takes `imin, imax` as `int64`. |
| `field_float(name, id, flags, default, [fmin, fmax], [setter, getter])`           | `double`                | Floats with optional range. |
| `field_string(name, id, flags, default, max_len=0, [setter, getter])`             | `std::string`           | UTF-8 strings. `max_len` of 0 means no limit. |
| `field_bytes(name, id, flags, default, max_len=0, [setter, getter])`              | `RNS::Bytes`            | Opaque binary blob (identity hash, signature, opaque token). |
| `field_bytes_list(name, id, flags, default, element_size=0, max_count=0, [setter, getter])` | `std::vector<RNS::Bytes>` | Lists/sets of binary blobs (e.g. allowed-peer hashes). `element_size>0` forces each entry to exactly that many bytes. |
| `field_enum(name, id, flags, default, values, labels, [setter, getter])`          | `int64_t`               | Pick-from-list. `values` is the integer codes, `labels` the human strings (parallel arrays). |

Typed getters take native C++ types so you can pass plain lambdas without wrapping in `Value`:

```cpp
.field_bool("enabled", 1, FF_LIVE_APPLY, true,
    [](const Value& v) { MyClass::enabled(v.as_bool()); return true; },
    []() { return MyClass::enabled(); })     // bool, not Value
```

Two ergonomic shortcuts for common patterns:

```cpp
.metric_int("packets_received", id,
    []() { return (int64_t)Transport::packets_received(); })   // FF_READ_ONLY + getter

.command_int("reboot_in_seconds", id, 0, 3600,
    [](const Value& v) { schedule_reboot(v.as_int()); return true; })   // FF_WRITE_ONLY + setter
```

See [Patterns: metrics and commands](#patterns-metrics-and-commands).

## Field flags and the lifecycle they imply

Flags are an 8-bit bitfield on each field. The combination determines exactly what happens during `SET_STATE`, `COMMIT`, `apply_loaded_to_runtime` (boot), `factory_reset`, and `GET_STATE`. Reference table:

| Flag                  | Bit  | Drafts accepted | Persisted | Setter on commit | Setter on boot apply | Visible in GET_STATE |
|---|---|---|---|---|---|---|
| (none, no flags set)  | —    | yes | yes | — | — | yes |
| `FF_LIVE_APPLY`       | 0x01 | yes | yes | **yes** | **yes** (if changed from default) | yes |
| `FF_REBOOT_REQUIRED`  | 0x02 | yes | yes | no  | **yes** (at next boot) | yes |
| `FF_READ_ONLY`        | 0x04 | rejected | no | — | — | yes (via getter) |
| `FF_SECRET`           | 0x08 | yes (when combined with apply flag) | yes (when combined with apply flag) | depends on apply flag | depends | **no** |
| `FF_WRITE_ONLY`       | 0x10 | yes (dedup-bypassed) | **no** | **yes** | **no** | **no** |

Notes on individual flags:

- `FF_LIVE_APPLY` and `FF_REBOOT_REQUIRED` are mutually exclusive in intent — pick one. If neither is set on a stateful field, commit promotes the value into working and persists, but the setter never fires. That's only useful for tracking state with no runtime side-effect.
- `FF_READ_ONLY` is the **metric** flag. It rejects all draft writes, isn't persisted, and isn't applied at boot. Pair it with a getter for live telemetry. Without a getter it always returns the default, which is rarely what you want.
- `FF_SECRET` combines with `FF_LIVE_APPLY` or `FF_REBOOT_REQUIRED` to give settable-but-not-readable. Persisted and applied like the underlying flag; just hidden from `GET_STATE` responses.
- `FF_WRITE_ONLY` is the **command** flag. Setter fires on commit, value is consumed (not stored), nothing persists, nothing replays at boot. The set-equal-working dedup is bypassed so the same argument can re-trigger the command multiple times.

## Setter and getter contracts

A field's **setter** is `std::function<bool(const Value&)>`:

```cpp
SetterFn = std::function<bool(const Value&)>;
```

- Return `true` on success, `false` to signal the apply failed.
- Failure during `COMMIT` is reported through the engine but doesn't roll back the working map (the value is already promoted by the time the setter fires).
- A `false` return propagates up to clients as `CommitOutcome::SetterFailed` in the in-process API. The wire `COMMIT` response still includes it in the `applied` count — the setter contract is "best-effort apply".
- Setters should be **fast and non-blocking**. They run on the caller's thread inside the wire handler. Don't sleep, don't do network I/O. Long actions should record intent and let your main loop pick it up.

A field's **getter** is `std::function<Value()>`:

```cpp
GetterFn = std::function<Value()>;
```

The builder accepts **typed** getters and wraps them internally:

```cpp
.field_int("foo", id, FF_LIVE_APPLY, 0, 0, 100, setter,
    []() { return (int64_t)current_value(); })   // typed getter, wrapped to GetterFn
```

When a field has a getter:

- `Provisioner::field(ns_id, field_id)` reads return the getter's value, not the working-map cache. This is what eliminates drift when something mutates the runtime outside Provisioning.
- `Codec::pack_namespace_working` (save path) writes the getter's value for `FF_LIVE_APPLY` fields, so the on-disk state always reflects current runtime. For `FF_REBOOT_REQUIRED` fields the save path uses the working map instead (since the runtime hasn't caught up yet — that's the whole point of the flag).
- Read-only / write-only fields use their getter for reads where applicable; write-only reads always return `Value::None`.

Getters should be **cheap** — they may be called frequently during `GET_STATE` polls. If a getter is expensive, cache the value in your runtime static and return that.

## Namespace-level commit hook

In addition to per-field setters and getters, each namespace can register an optional **commit hook** that fires once per `COMMIT` pass — before any field setter runs — when the namespace has at least one pending draft. The hook is the natural place to validate combinations of related fields, clamp values across multiple drafts, or veto an entire change set that's inconsistent.

```cpp
Provisioner::instance()
    .namespace_("radio", 100)
        .field_int  ("sf",        1, FF_REBOOT_REQUIRED, 8, 7, 12)
        .field_float("bandwidth", 2, FF_REBOOT_REQUIRED, 125e3, 7.8e3, 500e3)
        .on_commit([](Namespace& ns) {
            // Cross-field validation: SF12 + BW7.8k is allowed but SF12 + BW500k
            // saturates the chip. Revert just the bandwidth draft if the
            // combination is illegal, leaving the SF change to proceed.
            Value sf, bw;
            const bool has_sf = ns.draft(1, sf);
            const bool has_bw = ns.draft(2, bw);
            const int64_t eff_sf = has_sf ? sf.as_int() : ns.effective(1).as_int();
            const double  eff_bw = has_bw ? bw.as_float() : ns.effective(2).as_float();
            if (eff_sf >= 11 && eff_bw > 250e3) {
                ns.clear_draft(2);   // bandwidth change refused
            }
        })
    .end();
```

**Contract:**

- Fires once per namespace per `Provisioner::commit()` / wire `COMMIT` pass.
- Fires only if the namespace has at least one pending draft entry. A commit pass over a namespace with no drafts skips the hook entirely.
- Fires **before** any of that namespace's field setters run via `commit_one`. This is the key ordering guarantee — the hook sees the drafts before they are promoted into the working map and before any `FF_LIVE_APPLY` side-effects happen on the runtime.
- The hook receives a mutable `Namespace&` so it can:
  - **Inspect** drafts with `ns.has_draft(fid)` / `ns.draft(fid, out)` and current values with `ns.effective(fid)`.
  - **Revert** a single draft with `ns.clear_draft(fid)` — that field will not commit, its setter will not fire, and the on-disk file will not change for that entry.
  - **Revert everything** with `ns.clear_draft()` — turns the whole commit into a no-op for this namespace.
  - **Amend** a draft with `ns.set_draft(fid, v)` — applies the field's validator first. The amended value is what reaches the setter.
- After the hook returns, the engine **re-collects** the draft id list before iterating `commit_one`, so additions and reverts inside the hook are honoured on this same commit pass.
- Exceptions thrown by the hook are caught and logged via `ERRORF`; the commit continues with whatever state the hook left in the draft map. Don't rely on `throw` to abort — call `clear_draft()` instead.

**When to use the hook vs a per-field setter:**

- Use a **setter** when the constraint applies to one field in isolation and the apply action is straightforward.
- Use the **hook** when validation depends on multiple drafts seen together, when you need to enforce ordering between fields, or when "accept all-or-nothing" semantics matter (commit either every change or none).

The hook lives on the `Namespace` itself — `Namespace::on_commit(cb)` — so it can also be registered or replaced after the namespace was built, e.g. when wiring up callbacks that depend on objects constructed later in the boot sequence.

## Working, draft, and commit semantics

Each namespace internally keeps two maps keyed by field id:

- **Working** — the currently effective value. Initialised to the field's declared default. Overlaid with disk contents at boot.
- **Draft** — pending edits that haven't been applied yet. Empty by default.

For fields with getters, the working map is essentially a cache — the getter is consulted on read. For fields without getters, the working map is the only state.

| Operation | Effect |
|---|---|
| `Provisioner::field(ns, f, Source::Working)`   | Read: getter if present, else working map. |
| `Provisioner::field(ns, f, Source::Draft)`     | Returns the draft value if any, else `Value::None`. |
| `Provisioner::field(ns, f, Source::Effective)` | Draft if present, else getter/working. |
| `Provisioner::field(ns, f, value)` (set)       | Validates type and constraint, writes to draft. Rejects `FF_READ_ONLY`. |
| `Provisioner::commit(ns)` / `Provisioner::commit()` | Promotes drafts → working, fires setters per flag rules, persists to flash. |
| `Provisioner::discard(ns)` / `Provisioner::discard()` | Drops drafts; working unchanged. |
| `Provisioner::factory_reset()`                  | Resets all working to declared defaults, fires setters with defaults, removes all namespace files. |

The same workflow is driven by the wire ops `SET_STATE`, `COMMIT`, `DISCARD`, `FACTORY_RESET`.

## Persistence on flash

When `RNS_USE_FS` is enabled, every namespace gets its own file under the storage root (default `<root>/config/`).

- **Filename**: walks the namespace's parent chain to construct a dotted path. `Reticulum.msgpack`, `Interfaces.LoRa.msgpack`, `Interfaces.UDP.msgpack`. Flat (no parent) namespaces produce filenames identical to before hierarchy support was added — fully backward compatible.
- **Format**: a single MsgPack map keyed by field id (uint), with the typed value as the entry.
- **Atomic writes**: `Storage` writes the new contents to `<name>.tmp` first, then `rename()`s it over the live file. On power-loss mid-write the `.tmp` is orphaned and the previous file is intact; `.tmp` files are removed on next load.
- **Unknown fields ignored**: loading a file with field ids the firmware no longer knows about silently drops the unknown entries (forward-compatible drops).
- **Read-only and write-only fields are skipped**: they have no value worth persisting. Read-only metrics change every second; write-only commands are one-shot.
- **Secret fields are persisted**: e.g. the device private key. Hidden from the wire, kept on disk.

The Provisioning subsystem does not perform encryption-at-rest. If you store secret material, ensure your flash is physically secure and / or implement an outer encryption layer.

## Hierarchical namespaces (nested builder)

A parent namespace can contain child namespaces. Children are full namespaces — they have their own ids, their own fields, their own persistence file — but their `parent_id` lets clients render them in a tree.

```cpp
constexpr uint16_t NS_INTERFACES = 100;
constexpr uint16_t NS_LORA       = 101;
constexpr uint16_t NS_UDP        = 102;

Provisioner::instance()
    .namespace_("Interfaces", NS_INTERFACES)
        .field_bool("enabled", 1, FF_LIVE_APPLY, true, ...)    // parents can have own fields
        .namespace_("LoRa", NS_LORA)
            .field_float("frequency", 1, FF_REBOOT_REQUIRED, 915e6, 100e6, 1e9, ...)
            .field_int("sf",          2, FF_REBOOT_REQUIRED, 8, 7, 12, ...)
            .end()
        .namespace_("UDP", NS_UDP)
            .field_string("host", 1, FF_LIVE_APPLY, "0.0.0.0", 64, ...)
            .field_int("port",    2, FF_LIVE_APPLY, 4242, 1, 65535, ...)
            .end()
        .end();
```

Mechanics:

- Each `.namespace_()` call pushes onto a per-`Provisioner` scope stack and uses the current scope top as the new namespace's parent.
- Each `.end()` pops one level. Forgetting `.end()` is caught at `Provisioner::begin()` — the scope stack must be empty by the time `begin()` runs (or it gets cleared with a warning).
- The Registry remains **flat** internally — every namespace is a top-level entry with an optional `parent_id`. Lookups stay O(1) by id.
- On disk: `Interfaces.msgpack`, `Interfaces.LoRa.msgpack`, `Interfaces.UDP.msgpack`. Three files at the top level of the storage directory.
- On the wire: `GET_STATE` keeps each namespace at the top level keyed by its own id. The tree shape is conveyed only through the schema's `parent_id`. Clients reconstruct the tree client-side.

Cycle detection runs at registration time — declaring a parent that walks up to the new namespace itself is refused.

Field ids stay **per-namespace**, so `LoRa.frequency` (field 1) and `UDP.host` (field 1) coexist without conflict. The `(ns_id, field_id)` tuple uniquely identifies any field across the whole registry.

## Reboot handling

`FF_REBOOT_REQUIRED` fields persist on commit but defer their setter until the next boot. The lifecycle:

1. Client `SET_STATE` writes the field's draft.
2. Client `COMMIT` promotes draft → working and persists to flash. The setter is **not** called. `needs_reboot()` flips to `true`.
3. App / firmware detects `needs_reboot()` (or receives the `on_reboot_required` callback) and triggers a reboot via its platform's mechanism (`ESP.restart()`, `NVIC_SystemReset()`, host process exit, etc.).
4. On next boot, `Provisioner::begin()` reads the file from flash, populates working, then `apply_loaded_to_runtime()` fires the setter for any value that differs from the field's declared default. The runtime now reflects the committed value.

There are two reboot-related callbacks, kept distinct on purpose:

| Callback | Triggered by | Intended action |
|---|---|---|
| `on_reboot_required(cb)` | *Passive* — fires when a commit flips `needs_reboot()` false → true. | Surface a "reboot needed" UI badge / log event. Most integrations leave this unset and let the user decide when to reboot. |
| `on_reboot(cb)` | *Active* — fires when the client sent the explicit `REBOOT` wire op (op 9). | Actually reboot the device (usually scheduled so the wire ack can be sent first). |

```cpp
// Passive: only react to "reboot is needed".
Provisioner::instance().on_reboot_required([] {
    INFO("Provisioning committed a reboot-required change.");
});

// Active: the client explicitly asked us to reboot.
Provisioner::instance().on_reboot([] {
    schedule_reboot();   // ESP.restart(), NVIC_SystemReset(), exit(), ...
});
```

`on_reboot_required` fires once, on the `false → true` transition of `needs_reboot()`. Subsequent commits of more `REBOOT_REQUIRED` fields don't re-fire (you're already pending a reboot). `on_reboot` fires every time the `REBOOT` wire op is dispatched.

The `REBOOT` wire op always returns a successful ack envelope; if no `on_reboot` callback is registered, microReticulum performs no reboot itself and the op is a successful no-op. This lets clients ack-poll without depending on the device actually rebooting.

`Provisioner::end()` clears both callbacks (and `on_factory_reset`), so re-registering after a test teardown is required if you want to keep receiving notifications.

If your firmware can't reboot itself (e.g. it's a Linux process), you can leave both callbacks unset. Clients see `needs_reboot=true` in `COMMIT` responses and `GET_INFO` and can prompt the user.

## Factory-reset extension hook

`FACTORY_RESET` (op 8) resets every working value to its declared default, fires field setters for `FF_LIVE_APPLY` fields, deletes the persisted namespace files under `<storage_root>/config/`, and clears `needs_reboot`.

If the app needs to extend the operation — wiping storage outside Provisioning's root, dropping in-memory app state, etc. — register `on_factory_reset(cb)`. The callback fires **after** the internal reset has completed, so the namespaces it reads back will already be at defaults.

```cpp
Provisioner::instance().on_factory_reset([] {
    INFO("Factory reset complete; extending with app cleanup.");
    wipe_app_state();
});
```

The wire response is unchanged — clients don't see any extension. The callback is cleared by `Provisioner::end()`.

## Direct in-process accessors

App code that wants to read or write Provisioning state without going through MsgPack can use the direct accessors on `Provisioner`. All edits land in the draft layer; only `commit()` promotes them.

By id (preferred — uses constexpr ids from `Ids.h`):

```cpp
namespace R = RNS::Provisioning::Ns::Reticulum;
auto& mgr = Provisioning::Provisioner::instance();

// Read working / live value
bool live = mgr.field(R::Id, R::Field::TransportEnabled).as_bool();

// Edit draft
mgr.field(R::Id, R::Field::PersistInterval, Value{(int64_t)600});

// Apply
mgr.commit(R::Id);

// Or back out
mgr.discard(R::Id);
```

By name (slower — hash lookups; good for one-shot wiring or REPL-style debug):

```cpp
mgr.field("Reticulum", "transport_enabled", Value{false});
mgr.commit("Reticulum");
```

For metrics:

```cpp
Value v = mgr.field("Interfaces.LoRa", "frequency");   // NOTE: by-name uses just the leaf
                                                       // namespace name, not the dotted path
```

Important: namespace name lookup is by the **registered name only** (e.g. `"LoRa"`), not by the dotted path. The dotted path is a storage/UI convention, not an identifier.

`needs_reboot()` and `draft_has_reboot()` are also available for in-process inspection. The optional `Provisioner::factory_reset()` does the same job as the wire op.

## Hooking up a transport

Wiring Provisioning to your wire transport is one line per direction. The library doesn't care what your framing looks like — it gives you bytes-in/bytes-out:

```cpp
RNS::Bytes Provisioning::Provisioner::handle_message(const RNS::Bytes& request);
```

A typical KISS-over-USB-serial handler:

```cpp
constexpr uint8_t KISS_CMD_PROVISIONING = 0x0F;   // reserved KISS command byte

void on_kiss_frame(uint8_t cmd, const RNS::Bytes& payload) {
    if (cmd == KISS_CMD_PROVISIONING) {
        RNS::Bytes response = Provisioning::Provisioner::instance().handle_message(payload);
        kiss_send(KISS_CMD_PROVISIONING, response);
    }
    // ... other KISS commands ...
}
```

Equivalent over BLE (Nordic UART Service):

```cpp
void on_nus_rx(const uint8_t* data, size_t len) {
    RNS::Bytes request(data, len);
    RNS::Bytes response = Provisioning::Provisioner::instance().handle_message(request);
    nus_send(response.data(), response.size());
}
```

The Provisioning engine is **synchronous and reentrant-unsafe**. Serialise calls — don't invoke `handle_message` concurrently from interrupts and the main loop. On typical microcontroller event loops this is a non-issue (single-threaded by default).

## Patterns: metrics and commands

### Metrics — read-only with live getter

Use `metric_*` for instrumentation that the device pushes to clients on demand. The getter is consulted every time a client reads.

```cpp
Provisioner::instance().namespace_("Stats", NS_STATS)
    .metric_int("packets_received",  1, []() { return (int64_t)Transport::packets_received(); })
    .metric_int("packets_sent",      2, []() { return (int64_t)Transport::packets_sent(); })
    .metric_float("rssi_dbm",        3, []() { return radio.last_rssi(); })
    .metric_string("fw_version",     4, []() { return std::string("v1.2.3"); })
    .end();
```

`metric_*` is sugar for `field_*(name, id, FF_READ_ONLY, default, setter=nullptr, getter)`. The default is unused at runtime (the getter always wins) but must be provided to the underlying builder; `metric_*` fills it with the type's zero value.

### Commands — one-shot write-only actions

Use `command_*` for "do something now" gestures: reboot, format flash, rotate identity, send test packet. The setter receives the argument the client supplied and runs the action. Nothing is persisted; nothing replays at boot; the field doesn't appear in `GET_STATE`.

```cpp
Provisioner::instance().namespace_("Actions", NS_ACTIONS)
    .command_bool("ping", 1,
        [](const Value&) { send_test_packet(); return true; })
    .command_int("reboot_in_seconds", 2, 0, 3600,
        [](const Value& v) { schedule_reboot(v.as_int()); return true; })
    .command_string("log_line", 3, 256,
        [](const Value& v) { INFOF("client-log: %s", v.as_string().c_str()); return true; })
    .command_bytes("inject_packet", 4, 512,
        [](const Value& v) { transport_inject(v.as_bytes()); return true; })
    .end();
```

The same argument value can trigger the command multiple times — the engine bypasses the dedup-on-equal-working that normal fields use.

### Secret settable fields (keys, passwords)

Use `FF_SECRET` combined with `FF_LIVE_APPLY` or `FF_REBOOT_REQUIRED`:

```cpp
.field_bytes("transport_identity", id,
    (uint8_t)(FF_REBOOT_REQUIRED | FF_SECRET), Bytes(), 64,
    [](const Value& v) {
        if (v.as_bytes().size() == 0) return true;          // empty = no-op
        Transport::identity_from_prv(v.as_bytes());
        return true;
    },
    []() {
        if (!Transport::identity()) return Bytes();         // not yet set
        return Transport::identity_to_prv();
    })
```

The field is settable from clients (UI presents a write-only password input) and persisted to flash, but never returned in `GET_STATE`.

## Linker exclusion when Provisioning isn't used

With `-DRNS_USE_PROVISIONING=OFF`, the library compiles the Provisioning sources into `libmicroReticulum.a` but **no Provisioning object files get linked into the final binary**. The mechanism:

- Static-archive linkage pulls in only `.o` files that resolve undefined references in the executable.
- `Reticulum.cpp`'s `Provisioner::instance().begin()` call is the **only** reference to a Provisioning symbol from outside `src/Provisioning/`.
- That reference is gated by `#ifdef RNS_USE_PROVISIONING`. With the flag off, the call (and its `#include`) disappear at preprocessing time.
- With zero outside references, zero Provisioning `.o` files are pulled in. The Meyers-style singleton inside `Provisioner::instance()` is a function-local `static` — it never constructs because the function is never linked.

To preserve this property as you add code:

1. Do **not** reference `RNS::Provisioning::*` symbols from any file outside `src/Provisioning/` unless guarded by `#ifdef RNS_USE_PROVISIONING`.
2. Do **not** add file-scope static variables in `src/Provisioning/*.cpp` whose initialisers reach into core code — they'd force-pull the TU. (Function-local statics are fine — they only construct on first call.)
3. Tests under `test/test_provisioning/` always link Provisioning by design; this is fine, they're separate executables.

Verify with `nm`:

```
$ cmake -B build-noprov -DRNS_USE_PROVISIONING=OFF -DRNS_BUILD_TESTS=OFF -DRNS_BUILD_INTEROP=OFF
$ cmake --build build-noprov --target udp_announce
$ nm build-noprov/examples/udp_announce | grep -c '_ZN.*12Provisioning'
0
```

## Field & namespace id stability

A schema-driven system breaks silently if ids are ever reused. The library follows strict append-only rules — apps should do the same.

**Rules:**

1. **Never re-use a namespace or field id**, even after the namespace/field is removed.
2. **Never re-assign a meaning.** If a field's semantics change incompatibly (units, type, range), allocate a new id and retire the old one.
3. **Renames are free.** The human-readable name is informational; the id is the identifier.
4. **Removal**: drop the registration but leave the id reserved in a central comment so no one else picks it.
5. **Type widening within `INT`** (raising max) is OK; **type changes** (`INT` → `STRING`) require a new id.

**Recommended id ranges:**

| Range | Owner |
|---|---|
| Namespace id `0`         | Reserved (engine). Don't use. |
| Namespace ids `1-99`     | Core library (Reticulum=1, Transport=2; id 3 permanently reserved). |
| Namespace ids `100-199`  | Official microReticulum companion apps. |
| Namespace ids `200-65535`| Third-party / vendor / per-installation. |
| Field id `0`             | Reserved (schema version) within every namespace. |
| Field ids `1-65535`      | Namespace-owned; allocate append-only. |

**Mechanism — single source of truth header:**

Stash your ids in a per-app or per-namespace header so they're greppable and the "what's the next free id" question is one line away:

```cpp
// my_app/IdsForProvisioning.h
namespace MyApp::Ns {
    constexpr uint16_t Id = 200;
    namespace Field {
        constexpr uint16_t SchemaVersion = 0;       // reserved
        constexpr uint16_t DeviceName    = 1;
        constexpr uint16_t Brightness    = 2;
        // constexpr uint16_t _Reserved3 = 3;        // (was Heading, removed v0.4)
        constexpr uint16_t Volume        = 4;
        // next-id: 5
    }
}
```

The `// next-id: N` comment is your contract for the next contributor. Reserved-but-removed slots stay commented out.

## Testing patterns

The library's own tests live in `test/test_provisioning/test_provisioning.cpp`. Key patterns you can borrow:

**Isolated storage per test:**

```cpp
static std::string make_test_root() {
    char buf[256];
    snprintf(buf, sizeof(buf), "/tmp/myapp_test_%d_%d", getpid(), ++counter);
    mkdir(buf, 0755);
    return std::string(buf);
}
```

**Full reset between tests:**

```cpp
void setUp(void) {
    g_test_root = make_test_root();
    reset_runtime_statics();   // your simulated runtime state
}

void tearDown(void) {
    Provisioning::Provisioner::instance().end();   // clears registry, scope, callback
    rm_rf(g_test_root);
}
```

**Re-register before begin:**

```cpp
static void fresh(const std::string& root) {
    Provisioner::instance().end();
    register_my_app_namespaces();   // must run before begin()
    Provisioner::instance().begin(root.c_str());
}
```

**Round-trip a wire request:**

```cpp
template <typename F>
static Bytes make_request(uint8_t op, uint64_t seq, F&& pack_payload) {
    MsgPack::Packer p;
    p.serialize(MsgPack::arr_size_t(3));
    p.serialize(op);
    p.serialize(seq);
    pack_payload(p);
    return Bytes(p.data(), p.size());
}

Bytes resp = Provisioner::instance().handle_message(make_request(...));
```

**Cross-test state leak**: the `Provisioner` is a process-global singleton. Tests that register a callback, mutate a global static (e.g. `Transport::_remote_management_allowed`), or commit fields must reset that state in `setUp` / `tearDown`. The library's tests reset every Transport static that Provisioning fields touch — do the same for yours.

## Common pitfalls

- **Forgetting `.end()`** — leaves the scope stack non-empty and silently nests the next registration under the previous namespace. The library warns and clears at `begin()`, but you'll likely register things under the wrong parent. Match every `.namespace_()` with an `.end()`.
- **Registering after `Reticulum::start()`** — built-ins are already registered, your namespace won't have its disk file loaded into working, and `apply_loaded_to_runtime()` has already run. Register before `start()`.
- **Direct mutation of statics after begin()** — without a getter, the working map drifts. Always wire both a setter and a getter when the value is also mutable via direct code paths.
- **Setting `default_value` that doesn't match the field's `Type`** — silent acceptance, weird `GET_STATE` output. The builder type and the `Value` constructor types must match.
- **Heavy work in setters or getters** — these run on the wire handler's thread. Defer to your main loop via flags/queues.
- **Persistence churn from metrics** — never persist counter-like fields. Use `metric_*` (which sets `FF_READ_ONLY`) or `FF_WRITE_ONLY`. Both skip the save path.
- **Re-using a removed id** — breaks any field already on flash with that id. If you're not sure whether an id was ever shipped, allocate a new one.
- **Multiple `Provisioner::instance()` calls in tests assuming fresh state** — the singleton persists across tests in the same process. Always `end()` in `tearDown`, and reset any non-Provisioning globals that your setters write into.
- **Mixing direct setters with REBOOT_REQUIRED fields** — direct setter changes are LIVE; committing the same field via Provisioning is REBOOT. Both can apply, but they don't synchronise via the working map automatically (the working map gets the committed value; the runtime reflects whichever happened last). Pick one path per field.

## API reference summary

### `Provisioner` (singleton)

```cpp
class Provisioner {
public:
    static Provisioner& instance();

    void begin(const char* storage_root = nullptr);
    void end();
    bool started() const;

    NamespaceBuilder namespace_(const char* name, uint16_t id);
    NamespaceBuilder register_namespace(const char* name, uint16_t id);

    Bytes handle_message(const Bytes& request);

    enum class Source { Working, Draft, Effective };
    Value field(uint16_t ns_id, uint16_t field_id, Source = Source::Working) const;
    bool  field(uint16_t ns_id, uint16_t field_id, const Value& v);
    bool  commit(uint16_t ns_id = 0);   // 0 = all
    bool  discard(uint16_t ns_id = 0);

    Value field(const char* ns_name, const char* field_name, Source = Source::Working) const;
    bool  field(const char* ns_name, const char* field_name, const Value& v);
    bool  commit(const char* ns_name);
    bool  discard(const char* ns_name);

    bool  factory_reset();

    bool needs_reboot() const;
    bool draft_has_reboot() const;

    using RebootRequiredCallback = std::function<void()>;
    using RebootCallback         = std::function<void()>;
    using FactoryResetCallback   = std::function<void()>;
    void on_reboot_required(RebootRequiredCallback cb);
    void on_reboot(RebootCallback cb);
    void on_factory_reset(FactoryResetCallback cb);

    Registry& registry();
    Storage* storage();

    static constexpr uint16_t SCHEMA_VERSION = 1;
};
```

### `NamespaceBuilder` (fluent)

```cpp
class NamespaceBuilder {
public:
    NamespaceBuilder& namespace_(const char* name, uint16_t id);   // push child
    NamespaceBuilder& end();                                       // pop scope

    NamespaceBuilder& field_bool(...);
    NamespaceBuilder& field_int(...);                              // both range & no-range overloads
    NamespaceBuilder& field_float(...);                            // both overloads
    NamespaceBuilder& field_string(...);
    NamespaceBuilder& field_bytes(...);
    NamespaceBuilder& field_bytes_list(...);
    NamespaceBuilder& field_enum(...);

    NamespaceBuilder& metric_bool(name, id, getter);
    NamespaceBuilder& metric_int(name, id, getter);
    NamespaceBuilder& metric_float(name, id, getter);
    NamespaceBuilder& metric_string(name, id, getter);
    NamespaceBuilder& metric_bytes(name, id, getter);

    NamespaceBuilder& command_bool(name, id, setter);
    NamespaceBuilder& command_int(name, id, imin, imax, setter);
    NamespaceBuilder& command_float(name, id, fmin, fmax, setter);
    NamespaceBuilder& command_string(name, id, max_len, setter);
    NamespaceBuilder& command_bytes(name, id, max_len, setter);

    // Per-namespace pre-commit hook — see "Namespace-level commit hook" above.
    NamespaceBuilder& on_commit(CommitCallback cb);
};

using CommitCallback = std::function<void(Namespace&)>;
```

### `Value` (tagged variant)

```cpp
enum class Type : uint8_t { None=0, Bool=1, Int=2, Float=3, String=4, Bytes=5, Enum=6, BytesList=7 };

class Value {
public:
    Value();                                       // None
    Value(bool); Value(int); Value(int64_t); Value(unsigned long long);
    Value(double); Value(float);
    Value(const char*); Value(const std::string&);
    Value(const Bytes&);
    Value(std::vector<Bytes>);
    static Value make_enum(int64_t);
    static Value from_int_as(Type, int64_t);

    Type type() const;
    bool is_none() const;

    bool                       as_bool() const;
    int64_t                    as_int() const;
    double                     as_float() const;
    const std::string&         as_string() const;
    const Bytes&               as_bytes() const;
    const std::vector<Bytes>&  as_bytes_list() const;
};
```

### `Field` flags

```cpp
enum FieldFlags : uint8_t {
    FF_NONE            = 0,
    FF_LIVE_APPLY      = 1 << 0,   // 0x01
    FF_REBOOT_REQUIRED = 1 << 1,   // 0x02
    FF_READ_ONLY       = 1 << 2,   // 0x04
    FF_SECRET          = 1 << 3,   // 0x08
    FF_WRITE_ONLY      = 1 << 4,   // 0x10
};
```

---

For the **wire protocol** that clients implement against this engine, see [`provisioning_client_guide.md`](provisioning_client_guide.md).
