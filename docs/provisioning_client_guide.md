# microReticulum Provisioning — Client Implementation Guide

This guide is for developers building **clients** that talk to a microReticulum device's Provisioning subsystem over a wire transport. It documents every byte of the MsgPack protocol — envelope format, operation IDs, payload shapes, field metadata, flag semantics, and error codes — so you can implement a browser UI, a Python tool, or a desktop app that interrogates and configures a Provisioning-enabled microReticulum stack.

> The library only handles the engine (schema, draft/commit, persistence) and the wire codec. The transport that carries MsgPack frames in and out — KISS over USB serial, Nordic UART over BLE, Web Serial, raw TCP, a Reticulum link — is the application/firmware's responsibility. This guide assumes you have a framed byte stream and focuses on what's inside each frame.

## Table of contents

1. [Transport assumptions](#transport-assumptions)
2. [Request/response envelope](#requestresponse-envelope)
3. [Operation IDs](#operation-ids)
4. [Schema versioning](#schema-versioning)
5. [Operation reference](#operation-reference)
   - [`GET_INFO`](#get_info-op--2)
   - [`GET_CAPABILITIES`](#get_capabilities-op--3)
   - [`GET_SCHEMA`](#get_schema-op--1)
   - [`GET_STATE`](#get_state-op--4)
   - [`SET_STATE`](#set_state-op--5)
   - [`COMMIT`](#commit-op--6)
   - [`DISCARD`](#discard-op--7)
   - [`FACTORY_RESET`](#factory_reset-op--8)
   - [`ERROR` response](#error-response-op--101)
6. [Field types](#field-types)
7. [Field flags](#field-flags)
8. [Field schema entry](#field-schema-entry)
9. [Hierarchical namespaces](#hierarchical-namespaces)
10. [Error code reference](#error-code-reference)
11. [Built-in namespaces and fields](#built-in-namespaces-and-fields)
12. [Recommended client lifecycle](#recommended-client-lifecycle)
13. [Wire wire-format quick reference](#wire-format-quick-reference)

---

## Transport assumptions

- Every request and response is a single MsgPack-encoded `Bytes` blob. The library does not chunk or stream — clients receive a complete response after each request.
- The library is **synchronous and reentrant-unsafe** on the firmware side. Clients should serialise their requests (no parallel in-flight ops to the same device).
- The wire is **little-endian-agnostic**: MsgPack defines all multi-byte encodings on the wire, so endianness on either side is not a concern.
- There is no built-in authentication, encryption, or session concept on this layer. If you need those, layer them in the transport that wraps the Provisioning frames.

## Request/response envelope

Every request and every response is a **MsgPack array of exactly 3 elements**:

```
[ op_id, seq, payload ]
```

| Position | Type | Notes |
|---|---|---|
| 0 | `uint8`  | Operation ID. See [Operation IDs](#operation-ids). |
| 1 | `uint64` | Caller-supplied sequence number, echoed back in the response. Use 0 if you don't need to match requests/responses. |
| 2 | varies   | Op-specific payload, or `nil` if the op takes no input / produces no body. |

The response's `op_id` is identical to the request's, **except** when the engine returns an error — in which case `op_id` is `ERROR` (101) and the original op id can be recovered from the `seq` you sent.

If the device receives a request before `Provisioner::begin()` has run (the firmware hasn't started Provisioning), it responds with an `ERROR` carrying `NotInitialized` (9).

## Operation IDs

| Op | ID | Direction |
|---|---|---|
| `GET_SCHEMA`        | 1   | request → response |
| `GET_INFO`          | 2   | request → response |
| `GET_CAPABILITIES`  | 3   | request → response |
| `GET_STATE`         | 4   | request → response |
| `SET_STATE`         | 5   | request → response |
| `COMMIT`            | 6   | request → response |
| `DISCARD`           | 7   | request → response |
| `FACTORY_RESET`     | 8   | request → response |
| `ACK`               | 100 | (reserved) |
| `ERROR`             | 101 | response only |

Unknown ops return an `ERROR` with `UnknownOp` (2).

## Schema versioning

The schema version is reported by `GET_INFO` (key `SchemaVersion`, see below). Current version is **2**.

| Version | Changes |
|---|---|
| 1 | Initial. Each schema namespace entry was a 3-element array `[id, name, [fields]]`. |
| 2 | Adds optional `parent_id` per namespace. Entries are now 4-element arrays `[id, name, parent_id, [fields]]`. `parent_id == 0` means root. |

Clients should branch on `SchemaVersion` from `GET_INFO`. A v1-only client that reads only the first three array elements of a v2 response will still get id/name/fields and can render a flat list (just without the hierarchy).

## Operation reference

### `GET_INFO` (op = 2)

Probe device version and reboot state. Always safe to call before anything else.

**Request payload**: `nil`.

**Response payload**: a map.

| Key | Value type | Meaning |
|---|---|---|
| 1 (`FirmwareVersion`) | `str` | Free-form firmware identifier (currently always `"microReticulum"` from the library; firmware can override). |
| 2 (`SchemaVersion`)   | `uint16` | See [Schema versioning](#schema-versioning). |
| 3 (`NeedsRebootInfo`) | `bool` | `true` if the device has committed any `FF_REBOOT_REQUIRED` field since boot. Sticky until reboot or factory reset. |

### `GET_CAPABILITIES` (op = 3)

List the namespace ids that the device exposes. Use this to detect optional/extended namespaces without fetching the full schema.

**Request payload**: `nil`.

**Response payload**: an array of `uint16` namespace ids, in registration order.

```
[ 1, 2, 100, 101, 102 ]
```

### `GET_SCHEMA` (op = 1)

Fetch the full field schema. Clients call this once per session (or whenever the schema version changes) to build a UI.

**Request payload**: `nil`. (Future versions may add a namespace filter.)

**Response payload**: a MsgPack array of **namespace entries**. Each entry is a 4-element array:

```
[ ns_id (uint16), ns_name (str), parent_id (uint16), fields (array of maps) ]
```

- `ns_id` is the canonical identifier. Address namespaces by id in all subsequent ops, never by name.
- `ns_name` is human-readable. May be empty. Not unique globally.
- `parent_id` is the parent namespace's id, or `0` for root namespaces. See [Hierarchical namespaces](#hierarchical-namespaces).
- `fields` is an array of [field schema entries](#field-schema-entry).

Example (truncated):

```
[
  [ 1, "Reticulum", 0, [ ...field maps... ] ],
  [ 2, "Transport", 0, [ ...field maps... ] ],
  [ 100, "Interfaces", 0, [ ...parent's own fields... ] ],
  [ 101, "LoRa", 100, [ ...field maps... ] ],
  [ 102, "UDP",  100, [ ...field maps... ] ]
]
```

### `GET_STATE` (op = 4)

Read current values. Excludes [`FF_SECRET`](#field-flags) and [`FF_WRITE_ONLY`](#field-flags) fields.

**Request payload**: optional map. If present:

| Key | Value type | Meaning |
|---|---|---|
| 1 (`NamespaceFilter`) | array of `uint16` | Only return state for these namespace ids. If absent, returns all namespaces. |
| 2 (`Pending`)         | `bool` | If `true`, returned values overlay any uncommitted draft on top of working state. Default `false` (committed state only). |

If the payload is absent or `nil`, the response covers every namespace, with no draft overlay.

**Response payload**: a MsgPack map keyed by `uint16` namespace id, each value being a map keyed by `uint16` field id, each value being the field's typed value.

```
{
  1: {                       # Reticulum namespace
    1: true,                 # transport_enabled
    6: 300,                  # persist_interval
    7: 900                   # clean_interval
  },
  101: {                     # LoRa namespace
    1: 915000000.0           # frequency
  }
}
```

State is **flat by namespace id** even when namespaces are hierarchical — see [Hierarchical namespaces](#hierarchical-namespaces). Each namespace's fields appear under its own id; the parent doesn't contain the children's fields in the response.

Namespaces that have only secret/write-only/none-valued fields are omitted from the response.

### `SET_STATE` (op = 5)

Stage one or more field changes into the device's **draft layer**. Drafts are RAM-only — they don't take effect on the runtime, don't get persisted, and disappear on reboot unless [committed](#commit-op--6).

**Request payload**: a map keyed by `uint16` namespace id, each value a map keyed by `uint16` field id, each value the field's typed value.

```
{
  1: {                       # Reticulum
    1: false                 # transport_enabled = false (draft)
  },
  101: {                     # LoRa
    1: 868000000.0           # frequency draft
  }
}
```

Every value is validated against the field's declared [type](#field-types) and [constraint](#field-schema-entry). Failures don't abort the whole request — the per-field error is reported and the rest of the request proceeds.

**Response payload**: a map.

| Key | Value type | Meaning |
|---|---|---|
| 1 (`Applied`)        | `uint64` | Count of fields successfully staged into draft. |
| 2 (`DraftHasReboot`) | `bool`   | `true` if the current draft (across all namespaces) touches any `FF_REBOOT_REQUIRED` field. UI affordance: show "you'll need to reboot to apply these changes". |
| 3 (`FieldErrors`)    | array (optional, only present on partial failure) | One entry per rejected field. |

Each `FieldErrors` entry is itself a 3-element array:

```
[ ns_id (uint16), field_id (uint16), error_code (uint16) ]
```

See [Error code reference](#error-code-reference) for the full list of codes that can appear here.

### `COMMIT` (op = 6)

Promote the draft layer to the working layer:

- `FF_LIVE_APPLY` fields: setter fires immediately in the firmware, runtime updates.
- `FF_REBOOT_REQUIRED` fields: value is persisted to flash but the setter is **not** called until the next boot.
- `FF_WRITE_ONLY` commands: setter fires immediately; the value is consumed and not retained.
- All non-`FF_READ_ONLY`, non-`FF_WRITE_ONLY` fields' working values are persisted to per-namespace files on flash (if `RNS_USE_FS` is enabled in the firmware).

**Request payload**: optional array of namespace ids to commit. If absent, all namespaces with drafts are committed.

```
[ 1, 101 ]    # commit Reticulum and LoRa only
```

**Response payload**: a map.

| Key | Value type | Meaning |
|---|---|---|
| 1 (`Applied`)     | `uint64` | Count of fields whose drafts were promoted. |
| 2 (`NeedsReboot`) | `bool`   | `true` if any committed field has `FF_REBOOT_REQUIRED` set. Sticky from the moment of commit until a real device reboot. Distinct from `DraftHasReboot` in `SET_STATE` responses (which tracks pending drafts). |

After a successful commit, the draft layer is cleared for the affected namespaces.

### `DISCARD` (op = 7)

Throw away the current draft without persisting or applying.

**Request payload**: optional array of namespace ids to clear. If absent, clears drafts in every namespace.

**Response payload**: a map.

| Key | Value type | Meaning |
|---|---|---|
| 1 (`Applied`) | `uint64` | Count of draft entries cleared. |

### `FACTORY_RESET` (op = 8)

Delete every persisted namespace file from flash and reset all working values to their declared defaults. Setters are fired with default values so the runtime reverts immediately for `FF_LIVE_APPLY` fields. `needs_reboot` is cleared.

**Request payload**: `nil`.

**Response payload**: a map.

| Key | Value type | Meaning |
|---|---|---|
| 2 (`NeedsReboot`) | `bool` | Always `false` after a successful factory reset. |

Integrations may register an `on_factory_reset` callback to extend the operation with app-side cleanup (storage outside Provisioning's root, in-memory app state, etc.). The callback fires after the internal reset has completed; the wire contract is unchanged.

### `REBOOT` (op = 9)

Ask the firmware to reboot. microReticulum itself performs no reboot — it dispatches to an `on_reboot` callback registered by the integration. If the integration registered a callback the device is expected to actually reboot (usually scheduled, so the response can be sent first); if no callback is registered the op is a successful no-op.

**Request payload**: `nil`.

**Response payload**: none. The response envelope is a successful ack `[op=9, seq]` with no payload element. Clients should not depend on receiving the response, since the device may be mid-reboot by the time it would have been sent.

### `ERROR` response (op = 101)

Returned in place of the normal response when the engine cannot service the request at the envelope/transport layer (malformed envelope, unknown op, Provisioning not started). Per-field errors during `SET_STATE` come back in the normal response's `FieldErrors` array, not as a separate `ERROR` op.

**Response payload**: a map.

| Key | Value type | Meaning |
|---|---|---|
| 1 (`ErrorCodeKey`) | `uint16` | See [Error code reference](#error-code-reference). |
| 2 (`ErrorMessage`) | `str` (optional) | Free-form diagnostic. Don't show verbatim; map by code. |

## Field types

Wire types are intentionally a tight subset that maps cleanly to Python, JavaScript, and embedded C++.

| Type id | Name        | MsgPack on wire        | Python  | JavaScript            | Notes |
|---|---|---|---|---|---|
| 1 | `BOOL`      | `bool`                 | `bool`  | `Boolean`             | |
| 2 | `INT`       | positive/negative int  | `int`   | `Number` or `BigInt`  | Range constrained per-field. JS clients should warn when `min`/`max` exceed `Number.MAX_SAFE_INTEGER` (2⁵³). |
| 3 | `FLOAT`     | `float64`              | `float` | `Number`              | Always float64 on the wire. |
| 4 | `STRING`    | `str` (UTF-8)          | `str`   | `String`              | |
| 5 | `BYTES`     | `bin`                  | `bytes` | `Uint8Array`          | Opaque binary blob. Render as hex by default. |
| 6 | `ENUM`      | positive int on wire   | `int`   | `Number`              | Schema carries the value list and labels; render as a dropdown. |
| 7 | `BYTES_LIST`| array of `bin`         | `list[bytes]` | `Array<Uint8Array>` | Variable-length list of binary blobs, often a set of fixed-size hashes. |

`None` / `nil` values are **not** valid field values — a field is either present with a typed value or absent from a state response.

## Field flags

Flags are an 8-bit bitfield on each schema field. Multiple flags can be combined.

| Bit  | Constant            | Hex   | Semantics for clients |
|---|---|---|---|
| `1<<0` | `FF_LIVE_APPLY`     | 0x01 | Field's setter fires immediately on `COMMIT`. UI: "save" applies right away. |
| `1<<1` | `FF_REBOOT_REQUIRED`| 0x02 | Persisted on `COMMIT`, but the firmware's setter only runs at next boot. UI: show a "reboot required" badge after commit; offer to reboot. |
| `1<<2` | `FF_READ_ONLY`      | 0x04 | Field cannot be set. UI: render display-only. Often used for **metrics / instrumentation** — pair with frequent `GET_STATE` polls to surface live counters. |
| `1<<3` | `FF_SECRET`         | 0x08 | Excluded from `GET_STATE` responses but still settable via `SET_STATE`. UI: render as a write-only secret input (e.g. private key). Persisted on flash. |
| `1<<4` | `FF_WRITE_ONLY`     | 0x10 | One-shot **command** field. `SET_STATE`+`COMMIT` triggers the firmware action once with the provided argument. Not persisted, not exposed in `GET_STATE`, not replayed on reboot. UI: render as an action button (with an optional argument input). |

UI rendering recipes:

| Flag combination | UI pattern |
|---|---|
| `FF_LIVE_APPLY`                          | Editable input with a "save" button (or auto-save). |
| `FF_REBOOT_REQUIRED`                     | Editable input with "save & reboot" affordance. Show a yellow indicator near the field. |
| `FF_READ_ONLY`                           | Read-only display (text, gauge, status badge). Refresh via periodic `GET_STATE`. |
| `FF_LIVE_APPLY | FF_SECRET`              | Write-only password-style input. Never echoes the current value. |
| `FF_REBOOT_REQUIRED | FF_SECRET`         | Same as above, but accompanied by a "reboot required" warning after commit. |
| `FF_WRITE_ONLY`                          | Action button. If the field has an integer/float/string argument, present an input alongside. |

## Field schema entry

Each entry in the `fields` array of a schema response is a map. Required keys are always present; optional keys appear only when relevant to the field's type.

| Key  | Constant         | Value type | Always present | Notes |
|---|---|---|---|---|
| 1    | `FieldId`         | `uint16`    | yes | Stable across firmware releases. Always address fields by id. |
| 2    | `FieldName`       | `str`       | yes | Human label. May be renamed across releases — don't key on this. |
| 3    | `FieldType`       | `uint8`     | yes | See [Field types](#field-types). |
| 4    | `FieldFlags`      | `uint8`     | yes | See [Field flags](#field-flags). |
| 12   | `FieldDefault`    | typed       | yes | Encoded as the field's type. For `Type::None` (only used by the engine internally) this is `nil`. |
| 5    | `FieldMinI`       | `int64`     | only for `INT` with range | |
| 6    | `FieldMaxI`       | `int64`     | only for `INT` with range | |
| 7    | `FieldMinF`       | `float64`   | only for `FLOAT` with range | |
| 8    | `FieldMaxF`       | `float64`   | only for `FLOAT` with range | |
| 9    | `FieldMaxLen`     | `uint64`    | only for `STRING`/`BYTES` with a limit (0 = no limit, key omitted) | Max byte length. |
| 10   | `FieldEnumValues` | array of `int64` | `ENUM` fields with declared values | Allowed integer values, in declaration order. |
| 11   | `FieldEnumLabels` | array of `str`   | `ENUM` fields with declared labels | Parallel to `FieldEnumValues`; render as dropdown text. |
| 13   | `FieldElementSize`| `uint64`    | `BYTES_LIST` fields with a fixed per-entry size | When present, every entry in the list must be exactly this many bytes. |
| 14   | `FieldMaxCount`   | `uint64`    | `BYTES_LIST` fields with a count cap | Maximum number of entries. |

Clients should treat unknown keys as forward-compat extensions and ignore them.

Example field map (frequency, `FLOAT`, `FF_REBOOT_REQUIRED`, range 100MHz–1GHz, default 915MHz):

```
{
  1:  1,                 # FieldId
  2:  "frequency",       # FieldName
  3:  3,                 # FieldType (FLOAT)
  4:  2,                 # FieldFlags (FF_REBOOT_REQUIRED)
  12: 9.15e8,            # FieldDefault
  7:  1.0e8,             # FieldMinF
  8:  1.0e9              # FieldMaxF
}
```

## Hierarchical namespaces

Schema entries carry a `parent_id` element (position 2 of the 4-tuple). To render a tree:

1. Walk the schema array once, indexing namespaces by id.
2. For each entry where `parent_id != 0`, append the entry as a child of its parent.
3. Roots are entries where `parent_id == 0`.

Example tree:

```
schema = [
  [1,   "Reticulum",  0,   [...]],
  [2,   "Transport",  0,   [...]],
  [100, "Interfaces", 0,   [...]],
  [101, "LoRa",       100, [...]],
  [102, "UDP",        100, [...]],
]

# renders as:
# Reticulum
# Transport
# Interfaces
#   ├─ LoRa
#   └─ UDP
```

`GET_STATE` responses remain **flat** — each namespace's values appear under its own id at the top level. A parent namespace can carry its own fields independently of its children.

Parent ids are stable across firmware releases per the same rules as namespace ids. Once a namespace is published, its parent doesn't change.

## Error code reference

| Code | Name                | When |
|---|---|---|
| 0   | `Ok`                  | Normal operation; reserved value, not actually emitted. |
| 1   | `MalformedRequest`    | Envelope or payload structure invalid (wrong types, missing required keys). |
| 2   | `UnknownOp`           | The `op_id` in the request is not recognised. |
| 3   | `UnknownNamespace`    | `SET_STATE` referenced a namespace id not in the registry. |
| 4   | `UnknownField`        | `SET_STATE` referenced a field id not present in the named namespace. |
| 5   | `InvalidValue`        | The supplied MsgPack value couldn't be decoded into the declared field type (e.g. you sent a string for an `INT` field). |
| 6   | `ConstraintViolation` | Value type-decoded fine but violates the field's range/length/enum/element-size constraint. |
| 7   | `ReadOnly`            | `SET_STATE` targeted an `FF_READ_ONLY` field. |
| 8   | `StorageError`        | Filesystem error during a `COMMIT` (rare — disk full, FS not mounted, rename failed). |
| 9   | `NotInitialized`      | Request arrived before `Provisioner::begin()` ran on the device. Wait and retry. |
| 99  | `Internal`            | Engine bug — please report. |

Codes 1, 2, 9, 99 are returned as full `ERROR` responses. Codes 3–8 also appear inside `SET_STATE` responses as per-field errors.

## Built-in namespaces and fields

The library ships these namespaces by default (when `RNS_USE_PROVISIONING` is enabled in firmware). Apps may register additional namespaces on top.

### `Reticulum` (ns id 1)

Core stack toggles. Wire name `"Reticulum"`.

| Field id | Name | Type | Flags | Notes |
|---|---|---|---|---|
| 1 | `transport_enabled`         | `BOOL`       | `FF_LIVE_APPLY`                           | Enable transport routing for other peers. |
| 2 | `link_mtu_discovery`        | `BOOL`       | `FF_REBOOT_REQUIRED`                      | |
| 3 | `remote_management_enabled` | `BOOL`       | `FF_LIVE_APPLY`                           | Accept remote-management requests. |
| 4 | `probe_destination_enabled` | `BOOL`       | `FF_LIVE_APPLY`                           | |
| 5 | `use_implicit_proof`        | (reserved)   | —                                         | Currently not exposed; id reserved. |
| 6 | `persist_interval`          | `INT` 30..86400 | `FF_LIVE_APPLY`                        | Seconds. |
| 7 | `clean_interval`            | `INT` 60..86400 | `FF_LIVE_APPLY`                        | Seconds. |
| 8 | `remote_management_allowed` | `BYTES_LIST` (element_size=16, max_count=32) | `FF_LIVE_APPLY`             | Allowed destination hashes (16 bytes each). |
| 9 | `transport_identity`        | `BYTES` (max_len=64) | `FF_REBOOT_REQUIRED | FF_SECRET`     | Private key bytes. Not returned in `GET_STATE`. |

### `Transport` (ns id 2)

Routing-table sizing. Wire name `"Transport"`.

| Field id | Name | Type | Flags | Notes |
|---|---|---|---|---|
| 1 | `path_table_maxsize`      | `INT` 1..65535 | `FF_LIVE_APPLY` | |
| 2 | `announce_table_maxsize`  | `INT` 1..65535 | `FF_LIVE_APPLY` | |
| 3 | `hashlist_maxsize`        | `INT` 1..65535 | `FF_LIVE_APPLY` | |
| 4 | `max_pr_tags`             | `INT` 1..65535 | `FF_LIVE_APPLY` | |
| 5 | `path_table_maxpersist`   | `INT` 1..65535 | `FF_LIVE_APPLY` | |

Reserved ids:

- Namespace id `3` was used for an "identity" prototype and is permanently reserved (never re-use).
- Field id `0` is reserved for "schema version" metadata in every namespace.

## Recommended client lifecycle

A typical session for a configuration UI:

1. **Connect** to the device via your transport. Verify the link.
2. **`GET_INFO`** — confirm `SchemaVersion` is one you support and read `needs_reboot`.
3. **`GET_CAPABILITIES`** (optional) — quick sanity check that expected namespaces exist before fetching the full schema.
4. **`GET_SCHEMA`** — fetch once per session; cache by `SchemaVersion`. Build the namespace tree (parent_id) and field metadata.
5. **`GET_STATE`** — fetch current values. Render the UI.
6. As the user edits fields, send **`SET_STATE`** for each change (or batch them). Inspect `DraftHasReboot` and `FieldErrors` in each response.
7. When the user clicks "Save" / "Apply", send **`COMMIT`**. Inspect `NeedsReboot`.
8. If `NeedsReboot` is now `true`, prompt the user to reboot. The library doesn't reboot the device itself — that's the firmware/app's responsibility.
9. For commands (`FF_WRITE_ONLY`), the sequence is `SET_STATE` (argument) → `COMMIT` (triggers the setter). The next `GET_STATE` will not include the command field.
10. For metrics (`FF_READ_ONLY`), poll `GET_STATE` at whatever interval your UI needs.

## Wire-format quick reference

```
Envelope (always):       [op_id, seq, payload]

GET_INFO request:        [2, seq, nil]
GET_INFO response:       [2, seq, {1: "fw_version", 2: schema_ver, 3: needs_reboot}]

GET_CAPABILITIES req:    [3, seq, nil]
GET_CAPABILITIES resp:   [3, seq, [ns_id, ns_id, ...]]

GET_SCHEMA request:      [1, seq, nil]
GET_SCHEMA response:     [1, seq, [
                            [ns_id, ns_name, parent_id, [field_map, field_map, ...]],
                            ...
                         ]]

GET_STATE request:       [4, seq, {1: [ns_ids?], 2: pending_bool?}]
                         (payload optional; nil OK)
GET_STATE response:      [4, seq, {ns_id: {field_id: value, ...}, ...}]

SET_STATE request:       [5, seq, {ns_id: {field_id: value, ...}, ...}]
SET_STATE response:      [5, seq, {1: applied, 2: draft_has_reboot, 3: [[ns,f,code], ...]?}]

COMMIT request:          [6, seq, [ns_id, ns_id, ...]?]
                         (array optional; nil = commit all)
COMMIT response:         [6, seq, {1: applied, 2: needs_reboot}]

DISCARD request:         [7, seq, [ns_id, ns_id, ...]?]
DISCARD response:        [7, seq, {1: applied}]

FACTORY_RESET request:   [8, seq, nil]
FACTORY_RESET response:  [8, seq, {2: needs_reboot_false}]

ERROR response:          [101, seq, {1: error_code, 2: "message"?}]
```

---

For the firmware side (registering namespaces, wiring setters/getters, integrating into an app), see `provisioning_integration_guide.md` in this directory.
