# WIP

### Cryptography

* Fixed X25519 seeded-key derivation to clamp the scalar per RFC 7748
  (`X25519PrivateKey::from_private_bytes`): the raw Curve25519 ladder left
  clamping to the caller, so identities derived from an unclamped seed
  (e.g. `SHA-256(name)` used directly as the private scalar) produced public
  keys and Diffie-Hellman secrets that disagreed with the Python reference
  implementation, which clamps inside X25519. Clamping is idempotent, so
  generated and file-loaded keys are unaffected.

### Tests

* Added a `test_crypto` interop vector pinning seeded X25519 public-key
  derivation, Diffie-Hellman, and `Identity::load_private_key` agreement
  with the Python reference.

# microReticulum v0.5.1

## Changed

### Transport

* Changed link proof timeouts to schedule a neighbor probe (divergence from the Python reference).
* Changed link proof timeouts on a path already marked UNRESPONSIVE to force replacement of the path (divergence from the Python reference).
* Changed announce handling to replace UNRESPONSIVE paths when a new announce is received (divergence from the Python reference).
* Disabled path lookup for neighbor probes, since neighbors are always adjacent.
* Refactored neighbor probe handling and invalidation.

### Link

* Added a per-hop grace period to the link establishment timeout.

### Identity

* Skipped known destination caching when the destination is already cached.

### Persistence

* Refactored `packet_hashlist` persistence.

### Packet / Link Metrics

* Adjusted packet RSSI, SNR and Q handling.

## Fixed

### Transport

* Fixed announce processing so that skipping an announce no longer breaks out of processing early.
* Fixed the `receiving_interface` of cached announce packets not being restored after reading them from cache.

### Persistence

* Fixed loading of corrupted or incorrect persisted time offsets by limiting the accepted value to a reasonable range.

### Build

* Fixed CMake handling of missing GitHub tags.

## General

* Added `CONTRIBUTING.md` with contributor information and linked it from the README.
* Updated the PR policy and added a pull request template.
* Applied minor optimizations and logging changes in Identity and Transport.

## Contributors

* Chad Attermann (@attermann)
* Nils-Lucas Schönfeld (@nilu96)
* @ronandi

# microReticulum v0.5.0

## Added

### Transport

* Added passive neighbor-liveness detection with targeted probe confirmation.
* Added neighbor statistics tracking and probe metrics.
* Added configurable neighbor probing parameters and runtime controls.
* Added path responsiveness states:
    * UNKNOWN
    * RESPONSIVE
    * UNRESPONSIVE
* Added path responsiveness metrics and logging.
* Added network identity accessor methods and network destination registration.
* Added transport-level traffic counters and bandwidth tracking.
* Added per-interface RX/TX rate tracking.
* Added interface prioritization by bitrate.
* Added queued and throttled discovery path requests.
* Added interface lifecycle hooks:
    * detach()
    * sent_announce()
    * sent_path_request()
* Added interface shutdown handling through detach_interfaces().
* Added announce queue dropping support.

### Provisioning / Remote Management

* Added remote provisioning support through /provision.
* Added namespace commit callbacks.
* Added first-class remote reboot support.
* Expanded built-in provisioning namespaces and metrics.
* Added schema hashing for provisioning schema caching.
* Added per-namespace schema hashes and repurposed `GET_CAPABILITIES` to return a namespace hierarchy map, letting clients fetch schema lazily per namespace.
* Added `NamespaceFilter` support to `GET_SCHEMA` so clients can fetch only the namespaces they don't already have cached.
* Added `Draft` flag to `GET_STATE`: when `true`, the response includes drafts alongside working values in a single round-trip. Response body is now `{Values, Drafts?, Hash}`.
* Added `PriorHash` cache short-circuit to `GET_STATE`: clients echo a previously-received `Hash`, and the server responds with `{Unchanged: true}` when the state would be byte-identical.
* Added `IncludeState` flag to `SET_STATE` and `COMMIT`: when `true`, the response includes post-op `PostOpValues` / `PostOpDrafts` / `PostOpHash`, eliminating the follow-up `GET_STATE` round-trip after a save or commit.
* Added provisioning payload compression support.
* Embedded Heatshrink compression implementation.

### Storage / Persistence

* Added IdentityEntry persistence support.
* Added MsgPack serialization for DestinationEntry.
* Added microStore-backed known destination storage.
* Added microStore-backed packet hashlist storage.

### Security / Filtering

* Added blackhole identity management.
* Added blackhole persistence support.
* Added blackhole list publication support.
* Added blackhole path filtering and automatic expiration handling.

### Packet / Link Metrics

* Added packet RSSI support.
* Added packet SNR support.
* Added packet quality (Q) support.
* Added remote reporting of signal metrics.

### APIs

* Added std::function callback support to PacketReceipt.
* Added RNS_LOG_LEVEL compile-time configuration.

## Changed

### Transport

* Filtered UNRESPONSIVE paths from path responses.
* Added replay and out-of-order announce rejection.
* Updated discovery path request behavior to use queued throttling.
* Updated routing behavior for roaming interfaces.
* Updated link-request MTU handling.
* Updated interface routing behavior to match Python reference behavior.
* Updated interface storage from hash map to bitrate-prioritized vector.
* Increased default neighbor probe timers.

### Persistence

* Changed DestinationEntry persistence format from custom binary encoding to MsgPack.
* Moved known destinations to microStore.
* Moved packet hashlist persistence to microStore.

### Memory

* Moved Bytes allocations to container allocator memory pools.
* Updated memory allocation behavior for embedded platforms.

### Logging

* Refactored logging configuration.
* Added compile-time log level controls.
* Expanded transport and probe observability logging.

## Fixed

### Transport

* Fixed Transport::expire_path() behavior.
* Fixed path request gate timeout handling.
* Fixed packet deduplication behavior during interface mismatch scenarios.
* Fixed path culling behavior after microStore migration.
* Fixed timeout callback dispatch in PacketReceipt.

### Identity

* Fixed crash conditions when identities were missing from known destinations.
* Fixed identity recall from cached announces.
* Added gating for updated identity recall behavior.

### Provisioning

* Fixed draft/working/effective configuration value handling.
* Fixed provisioning storage path handling across native and embedded platforms.

## General

* Fixed persistence test updates after schema changes.
* Added preprocessor guards around divergence implementations.
* Optimized path-state culling performance.
* Reduced compiler warnings.

## Deprecated

* Deprecated legacy function-pointer callback handlers in PacketReceipt.
* Existing callback APIs remain supported for compatibility.

## Migration Notes

* Persisted path-table data created by previous releases may not be reusable after upgrade and may be rebuilt automatically from network announces.
* Applications using internal persistence formats should verify compatibility.
* **Provisioning wire format changed.** Existing v0.4-era clients will not interoperate with a v0.5 server:
    * `GET_CAPABILITIES` now returns an array of namespace map entries (`{NsId, NsName, NsParent, NsFieldCount, NsSchemaHash}`), replacing the previous bare array of ids.
    * `GET_STATE` responses are wrapped in `{Values, Drafts?, Hash}` (previously the response body was the `{ns_id: {fid: value}}` map directly). The old `Pending` request flag is replaced by `Draft`, with new "include drafts alongside working" semantics.
    * `SET_STATE` requests use a new envelope `{State: {...}, IncludeState?, ReqCompress?}` (previously the top-level map was the state map itself).
    * `COMMIT` requests use a map envelope `{NamespaceFilter?, IncludeState?, ReqCompress?}` instead of a bare array of ns ids.
    * The `Pending` request key and `Applied`-slot-3 collision in the old commit response are gone. See [`docs/provisioning_client_guide.md`](docs/provisioning_client_guide.md) for the current wire format.
* Existing applications using function-pointer callback handlers do not require immediate changes but should migrate toward std::function callbacks.

## Contributors

* Chad Attermann (@attermann)
* Nils-Lucas Schönfeld (@nilu96)
