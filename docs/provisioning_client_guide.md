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
   - [`REBOOT`](#reboot-op--9)
   - [`ERROR` response](#error-response-op--101)
6. [PriorHash cache short-circuit](#priorhash-cache-short-circuit)
7. [IncludeState round-trip elimination](#includestate-round-trip-elimination)
8. [Response compression](#response-compression)
9. [Field types](#field-types)
10. [Field flags](#field-flags)
11. [Field schema entry](#field-schema-entry)
12. [Hierarchical namespaces](#hierarchical-namespaces)
13. [Error code reference](#error-code-reference)
14. [Built-in namespaces and fields](#built-in-namespaces-and-fields)
15. [Recommended client lifecycle](#recommended-client-lifecycle)
16. [Wire format quick reference](#wire-format-quick-reference)

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
| `REBOOT`            | 9   | request → response |
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

`SchemaVersion` describes the `GET_SCHEMA` response layout only. The request/response shapes of the other ops (`GET_STATE`, `SET_STATE`, `COMMIT`, `GET_CAPABILITIES`) are documented per-op below.

## Operation reference

### `GET_INFO` (op = 2)

Probe device version and reboot state. Always safe to call before anything else.

**Request payload**: `nil`, or an options map with `ReqCompress` (see [Response compression](#response-compression)).

**Response payload**: a map.

| Key | Value type | Meaning |
|---|---|---|
| 1 (`FirmwareVersion`) | `str` | Free-form firmware identifier (currently always `"microReticulum"` from the library; firmware can override). |
| 2 (`SchemaVersion`)   | `uint16` | See [Schema versioning](#schema-versioning). |
| 3 (`NeedsRebootInfo`) | `bool` | `true` if the device has committed any `FF_REBOOT_REQUIRED` field since boot. Sticky until reboot or factory reset. |
| 4 (`SchemaHash`)      | `uint32` | CRC32 over the serialized `GET_SCHEMA` response bytes. Clients cache the schema keyed by this hash so subsequent connections skip the full fetch when the device's schema hasn't changed. |

### `GET_CAPABILITIES` (op = 3)

Fetch the namespace hierarchy map — one entry per namespace, with just enough metadata for the client to render the top-level navigation and decide which schemas need to be fetched. Combined with `GET_SCHEMA`'s namespace-filter support, this powers lazy per-namespace schema loading.

**Request payload**: `nil`, or an options map with `ReqCompress`.

**Response payload**: a MsgPack array of per-namespace map entries.

Each entry is a map:

| Key | Value type | Meaning |
|---|---|---|
| 1 (`NsId`)          | `uint16` | Canonical namespace id. |
| 2 (`NsName`)        | `str`    | Human-readable name (may be empty). |
| 4 (`NsParent`)      | `uint16` | Parent namespace id, or `0` for root. |
| 5 (`NsFieldCount`)  | `uint64` | Number of fields the namespace exposes. Handy for UI placeholders. |
| 6 (`NsSchemaHash`)  | `uint32` | CRC32 over just this namespace's `GET_SCHEMA` entry. Lets clients cache each namespace's schema independently. |

Namespaces appear in registration order (same order as they'd appear in `GET_SCHEMA`).

Example:

```
[
  { 1:1,   2:"Reticulum",  4:0,   5:9,  6:0xa3f2c8d1 },
  { 1:2,   2:"Transport",  4:0,   5:5,  6:0x1e5b90c4 },
  { 1:100, 2:"Interfaces", 4:0,   5:0,  6:0x00000000 },
  { 1:101, 2:"LoRa",       4:100, 5:12, 6:0x7f2a1e63 }
]
```

### `GET_SCHEMA` (op = 1)

Fetch the full field schema. Clients typically call this once per session (or per changed namespace, when combined with `GET_CAPABILITIES`) to build a UI.

**Request payload**: `nil` for a full-schema fetch, or an options map:

| Key | Value type | Meaning |
|---|---|---|
| 1 (`NamespaceFilter`) | array of `uint16` | Only return schema entries for these namespace ids. If absent, returns all namespaces. |
| 100 (`ReqCompress`)   | `bool` | See [Response compression](#response-compression). |

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

**Request payload**: `nil` for "all namespaces, working only," or an options map:

| Key | Value type | Meaning |
|---|---|---|
| 1 (`NamespaceFilter`) | array of `uint16` | Only return state for these namespace ids. If absent, returns all namespaces. |
| 2 (`Draft`)           | `bool` | If `true`, the server includes drafts alongside working in the same response. Draft entries are sparse — only namespaces/fields that actually hold drafts appear. Default `false` (working only). |
| 4 (`PriorHash`)       | `uint32` | Optional CRC32 the client received from a previous `GET_STATE` response for the same scope. Enables the server-side cache short-circuit; see [PriorHash cache short-circuit](#priorhash-cache-short-circuit). |
| 100 (`ReqCompress`)   | `bool` | See [Response compression](#response-compression). |

**Response payload**: a map. On a normal (cache-miss) response:

| Key | Value type | Meaning |
|---|---|---|
| 1 (`Values`)  | map | `{ ns_id: { field_id: value, ... }, ... }` — the working state. |
| 2 (`Drafts`)  | map (optional) | Sparse `{ ns_id: { field_id: draft_value, ... }, ... }`. Only present when the request set `Draft: true` **and** at least one in-scope namespace has drafts. |
| 3 (`Hash`)    | `uint32` | CRC32 over the content that would be returned for this exact request/scope (Values + Drafts, excluding the Hash entry itself). Client can echo this back as `PriorHash` on the next request. |

On a cache hit (client's `PriorHash` matched):

| Key | Value type | Meaning |
|---|---|---|
| 4 (`Unchanged`) | `bool` | Always `true`. No `Values`/`Drafts`/`Hash` accompany this response — the client should reuse the cached body it received the last time it saw this Hash. |

Response state is **flat by namespace id** even when namespaces are hierarchical — see [Hierarchical namespaces](#hierarchical-namespaces). Each namespace's fields appear under its own id; the parent doesn't contain the children's fields.

Namespaces that have only secret/write-only/none-valued fields are omitted from `Values`.

Example (working + drafts response):

```
{
  1: {                                    # Values
    1: { 1: true, 6: 300 },               #   Reticulum
    101: { 1: 915000000.0 }               #   LoRa (child ns)
  },
  2: {                                    # Drafts (sparse)
    1: { 6: 600 }                         #   only Reticulum.persist_interval has a draft
  },
  3: 0x1e5b90c4                           # Hash
}
```

Example (cache hit):

```
{ 4: true }
```

### `SET_STATE` (op = 5)

Stage one or more field changes into the device's **draft layer**. Drafts are RAM-only — they don't take effect on the runtime, don't get persisted, and disappear on reboot unless [committed](#commit-op--6).

**Request payload**: a request envelope.

| Key | Value type | Meaning |
|---|---|---|
| 3 (`State`)         | map | Required. `{ ns_id: { field_id: value, ... }, ... }` — the fields to stage. |
| 5 (`IncludeState`)  | `bool` | Optional. If `true`, the response includes `PostOpValues` + `PostOpDrafts` + `PostOpHash` for the namespaces touched by the request. See [IncludeState round-trip elimination](#includestate-round-trip-elimination). |
| 100 (`ReqCompress`) | `bool` | See [Response compression](#response-compression). |

Example request payload:

```
{
  3: {                                    # State
    1: { 1: false },                      #   Reticulum.transport_enabled = false
    101: { 1: 868000000.0 }               #   LoRa.frequency = 868 MHz
  },
  5: true                                 # IncludeState — return updated values in the response
}
```

Every value is validated against the field's declared [type](#field-types) and [constraint](#field-schema-entry). Failures don't abort the whole request — the per-field error is reported and the rest of the request proceeds.

**Response payload**: a map.

| Key | Value type | Meaning |
|---|---|---|
| 1 (`Applied`)         | `uint64` | Count of fields successfully staged into draft. |
| 2 (`DraftHasReboot`)  | `bool`   | `true` if the current draft (across all namespaces) touches any `FF_REBOOT_REQUIRED` field. UI affordance: show "you'll need to reboot to apply these changes". |
| 3 (`FieldErrors`)     | array (optional, only present on partial failure) | One entry per rejected field. |
| 4 (`PostOpValues`)    | map (optional, only when `IncludeState: true`) | `{ ns_id: { field_id: value, ... }, ... }` — the working state for the touched namespaces. Working is unchanged by `SET_STATE`; the map is returned so the client can refresh its cache in one round-trip. |
| 5 (`PostOpDrafts`)    | map (optional, only when `IncludeState: true` **and** at least one touched namespace has drafts) | Sparse `{ ns_id: { field_id: draft_value, ... }, ... }`. Reflects `set_draft` dedup logic (a value that matches the current working is not staged as a draft). |
| 6 (`PostOpHash`)      | `uint32` (only when `IncludeState: true`) | CRC32 over the PostOpValues + PostOpDrafts content. Byte-equivalent to what `GET_STATE` with the same scope + `Draft: true` would produce, so the client can prime the `PriorHash` cache. |

Each `FieldErrors` entry is a 3-element array:

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

**Request payload**: `nil` for "commit all", or an options map:

| Key | Value type | Meaning |
|---|---|---|
| 1 (`NamespaceFilter`) | array of `uint16` | Only commit drafts for these namespaces. |
| 5 (`IncludeState`)    | `bool` | Optional. If `true`, the response includes `PostOpValues` + `PostOpHash` for the committed namespaces. See [IncludeState round-trip elimination](#includestate-round-trip-elimination). |
| 100 (`ReqCompress`)   | `bool` | See [Response compression](#response-compression). |

Example request payload (commit two namespaces, return the new state):

```
{
  1: [1, 101],                            # NamespaceFilter — commit Reticulum + LoRa
  5: true                                 # IncludeState
}
```

**Response payload**: a map.

| Key | Value type | Meaning |
|---|---|---|
| 1 (`Applied`)      | `uint64` | Count of fields whose drafts were promoted. |
| 2 (`NeedsReboot`)  | `bool`   | `true` if any committed field has `FF_REBOOT_REQUIRED` set. Sticky from the moment of commit until a real device reboot. Distinct from `DraftHasReboot` in `SET_STATE` responses (which tracks pending drafts). |
| 4 (`PostOpValues`) | map (optional, only when `IncludeState: true`) | `{ ns_id: { field_id: value, ... }, ... }` — the post-commit working state for the committed namespaces. Drafts are gone (they were promoted or discarded), so there is no `PostOpDrafts`. |
| 6 (`PostOpHash`)   | `uint32` (only when `IncludeState: true`) | CRC32 over `PostOpValues`. Client can prime the `GET_STATE` PriorHash cache with this hash for the same scope + `Draft: false`. |

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

**Request payload**: `nil`, or an options map with `ReqCompress`.

**Response payload**: a map.

| Key | Value type | Meaning |
|---|---|---|
| 2 (`NeedsReboot`) | `bool` | Always `false` after a successful factory reset. |

Integrations may register an `on_factory_reset` callback to extend the operation with app-side cleanup (storage outside Provisioning's root, in-memory app state, etc.). The callback fires after the internal reset has completed; the wire contract is unchanged.

### `REBOOT` (op = 9)

Ask the firmware to reboot. microReticulum itself performs no reboot — it dispatches to an `on_reboot` callback registered by the integration. If the integration registered a callback the device is expected to actually reboot (usually scheduled, so the response can be sent first); if no callback is registered the op is a successful no-op.

**Request payload**: `nil`, or an options map with `ReqCompress`.

**Response payload**: none. The response envelope is a successful ack `[op=9, seq]` with no payload element. Clients should not depend on receiving the response, since the device may be mid-reboot by the time it would have been sent.

### `ERROR` response (op = 101)

Returned in place of the normal response when the engine cannot service the request at the envelope/transport layer (malformed envelope, unknown op, Provisioning not started). Per-field errors during `SET_STATE` come back in the normal response's `FieldErrors` array, not as a separate `ERROR` op.

**Response payload**: a map.

| Key | Value type | Meaning |
|---|---|---|
| 1 (`ErrorCodeKey`) | `uint16` | See [Error code reference](#error-code-reference). |
| 2 (`ErrorMessage`) | `str` (optional) | Free-form diagnostic. Don't show verbatim; map by code. |

## PriorHash cache short-circuit

`GET_STATE` responses always carry a `Hash` (CRC32 over the response body content, excluding the Hash entry itself). Clients can echo that hash back on the next `GET_STATE` with the **same scope** as `PriorHash`. When it matches the server's computed hash for the current state, the server replies with a minimal `{ Unchanged: true }` body instead of re-sending the full Values/Drafts payload.

**Cache key.** The hash is a pure function of the content the server would emit for a given `(NamespaceFilter, Draft)` combination. The client's cache key must therefore include both. A hash returned for `Draft: false, filter=[1,2]` is not comparable to a hash returned for `Draft: true, filter=[1,2]` or for `filter=[1]`.

**When the cache hits.** For static-config namespaces the hash rarely changes between polls — only after `SET_STATE`, `COMMIT`, `DISCARD`, or `FACTORY_RESET`. Metric-heavy namespaces (fields with `FF_READ_ONLY` and a live getter, e.g. current RSSI) drift continuously and almost always miss — the response comes back with fresh Values as usual, so there's no cost besides the client's wasted PriorHash.

**Invalidation.** After any `SET_STATE`/`COMMIT`/`DISCARD`/`FACTORY_RESET`/`REBOOT`, discard cached hashes. `SET_STATE` and `COMMIT` with `IncludeState: true` return a fresh `PostOpHash` the client can use to re-prime the cache without a follow-up `GET_STATE`.

## IncludeState round-trip elimination

`SET_STATE` and `COMMIT` optionally return the post-op state (working, and for `SET_STATE` also drafts) in the same response body. This saves a follow-up `GET_STATE` round-trip after a save or commit — a big win over LoRa where every request/response transaction carries fixed encryption and framing overhead in addition to the payload.

- Set `IncludeState: true` in the request envelope to opt in.
- The response gains `PostOpValues` (working state for touched/committed namespaces), and for `SET_STATE`, `PostOpDrafts` (sparse — only namespaces/fields that hold drafts).
- The response also gains `PostOpHash` — a CRC32 over the PostOpValues + PostOpDrafts content, byte-equivalent to what a follow-up `GET_STATE` with the same scope would produce. Prime your `PriorHash` cache with it.
- **Scope of the returned state:** `SET_STATE` returns state for the namespaces referenced by the request's `State` map. `COMMIT` returns state for namespaces in the request's `NamespaceFilter` (if provided), otherwise for namespaces that actually had drafts at commit time.

Older firmware that predates this optimization ignores `IncludeState` and simply doesn't include the PostOp keys in the response; clients should treat their absence as a fallback signal to fetch state separately.

## Response compression

For bulky responses (`GET_SCHEMA`, `GET_STATE` with many fields, `GET_CAPABILITIES`), the client can request that the server compress the response using **heatshrink** (LZSS-family, window=9 bits, lookahead=4 bits — matching the firmware's build flags).

- Set `ReqCompress: true` (key 100) in the request options map.
- The server compresses only if the compressed body + wrapper is smaller than the raw payload. Otherwise it emits the uncompressed response as usual, so clients must handle both shapes.
- **Compressed responses** carry a single-key map body:

  ```
  { 101 (CompressedPayload): <bin of heatshrink-encoded MsgPack> }
  ```

  Decode by heatshrink-decompressing the `bin` payload and then re-decoding it as MsgPack — the decoded bytes are the exact response payload the server would have emitted uncompressed.

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
| 8 | `VOID`      | `nil`                  | `None`  | `null`                | Argument-less command marker (write-only fields whose setter takes no value). |

`None` / `nil` values are **not** valid field values on the wire for typed fields — a field is either present with a typed value or absent from a state response. `VOID` is a distinct type used specifically for argument-less commands (`command_void`).

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
| `FF_WRITE_ONLY`                          | Action button. If the field has an integer/float/string argument, present an input alongside. For `VOID` fields (no argument), just the button. |

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

`GET_STATE` responses remain **flat** — each namespace's values appear under its own id inside `Values` at the top level. A parent namespace can carry its own fields independently of its children.

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
2. **`GET_INFO`** — confirm `SchemaVersion` is one you support and read `needs_reboot` and `SchemaHash`.
3. **`GET_CAPABILITIES`** — fetch the namespace hierarchy with per-namespace schema hashes. Cheap; renders the top-level navigation.
4. **`GET_SCHEMA`** with `NamespaceFilter` — for each namespace whose `NsSchemaHash` isn't already in your localStorage/session cache, fetch just those. Cache per-namespace by `(ns_id, schemaHash)`.
5. **`GET_STATE`** — fetch current values (with `Draft: true` if the UI needs to show pending changes). Cache the returned `Hash` per `(scope, Draft)` combination.
6. On subsequent polls of the same scope: send the cached `Hash` back as `PriorHash`. On cache hit the response is a tiny `{ Unchanged: true }` — reuse your cached body.
7. As the user edits fields, send **`SET_STATE`** with `IncludeState: true` and the `State` map. Parse `PostOpValues` + `PostOpDrafts` from the response to refresh the panel without a follow-up `GET_STATE`. Inspect `DraftHasReboot` and `FieldErrors`.
8. When the user clicks "Save" / "Apply", send **`COMMIT`** with `IncludeState: true`. Parse `PostOpValues` to refresh the working view; inspect `NeedsReboot`.
9. If `NeedsReboot` is now `true`, prompt the user to reboot. Send **`REBOOT`** (op 9) if they agree. The library doesn't reboot the device itself — that's the firmware/app's responsibility.
10. For commands (`FF_WRITE_ONLY`), the sequence is `SET_STATE` (argument, or `nil` for `VOID` commands) → `COMMIT` (triggers the setter). The next `GET_STATE` will not include the command field.
11. For metrics (`FF_READ_ONLY`), poll `GET_STATE` at whatever interval your UI needs. `PriorHash` won't help here — live-metric hashes drift on every read.

## Wire format quick reference

```
Envelope (always):       [op_id, seq, payload]

GET_INFO request:        [2, seq, nil | {100: compress?}]
GET_INFO response:       [2, seq, {1: "fw_version", 2: schema_ver, 3: needs_reboot, 4: schema_hash}]

GET_CAPABILITIES req:    [3, seq, nil | {100: compress?}]
GET_CAPABILITIES resp:   [3, seq, [
                            {1: ns_id, 2: ns_name, 4: parent_id, 5: field_count, 6: schema_hash},
                            ...
                         ]]

GET_SCHEMA request:      [1, seq, nil | {1: [ns_ids?], 100: compress?}]
GET_SCHEMA response:     [1, seq, [
                            [ns_id, ns_name, parent_id, [field_map, field_map, ...]],
                            ...
                         ]]

GET_STATE request:       [4, seq, nil | {1: [ns_ids?], 2: draft?, 4: prior_hash?, 100: compress?}]
GET_STATE response (miss):
                         [4, seq, {1: {ns_id: {fid: value, ...}, ...},   # Values
                                   2: {ns_id: {fid: value, ...}, ...}?,  # Drafts (optional)
                                   3: hash}]                             # Hash
GET_STATE response (hit):
                         [4, seq, {4: true}]                             # Unchanged

SET_STATE request:       [5, seq, {3: {ns_id: {fid: value, ...}, ...},   # State (required)
                                   5: include_state?,
                                   100: compress?}]
SET_STATE response:      [5, seq, {1: applied,
                                   2: draft_has_reboot,
                                   3: [[ns,f,code], ...]?,               # FieldErrors (optional)
                                   4: {ns_id: {fid: value, ...}, ...}?,  # PostOpValues (optional)
                                   5: {ns_id: {fid: value, ...}, ...}?,  # PostOpDrafts (optional)
                                   6: hash?}]                            # PostOpHash (optional)

COMMIT request:          [6, seq, nil | {1: [ns_ids?], 5: include_state?, 100: compress?}]
COMMIT response:         [6, seq, {1: applied,
                                   2: needs_reboot,
                                   4: {ns_id: {fid: value, ...}, ...}?,  # PostOpValues (optional)
                                   6: hash?}]                            # PostOpHash (optional)

DISCARD request:         [7, seq, [ns_id, ns_id, ...]?]
DISCARD response:        [7, seq, {1: applied}]

FACTORY_RESET request:   [8, seq, nil | {100: compress?}]
FACTORY_RESET response:  [8, seq, {2: needs_reboot_false}]

REBOOT request:          [9, seq, nil | {100: compress?}]
REBOOT response:         [9, seq]                                        # no payload — device may be mid-reboot

ERROR response:          [101, seq, {1: error_code, 2: "message"?}]

COMPRESSED response:     the payload is replaced by a single-entry map
                         {101 (CompressedPayload): <bin>} whose value
                         is a heatshrink-encoded MsgPack blob that
                         decompresses to the raw payload above.
```

---

For the firmware side (registering namespaces, wiring setters/getters, integrating into an app), see `provisioning_integration_guide.md` in this directory.
