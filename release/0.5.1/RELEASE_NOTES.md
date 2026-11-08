# microReticulum 0.5.1

## Highlights

* Faster recovery from dead paths: link proof timeouts now trigger a neighbor probe, and paths already marked UNRESPONSIVE are replaced, either immediately on a further timeout or when a new announce arrives.
* Link establishment timeouts now include a per-hop grace period.
* Fixed announce processing ending early when an announce was skipped.
* A corrupted or incorrect persisted time offset is no longer loaded from storage.

## Improvements

* When a link proof times out, transport now schedules a neighbor probe. If the path is already marked UNRESPONSIVE, transport forces the path to be replaced. This is an intentional divergence from the Python reference.
* A new announce for a destination now replaces an UNRESPONSIVE path in the path table. This is also a divergence from the Python reference.
* Neighbor probes no longer perform a path lookup, because neighbors are always adjacent. Neighbor probe handling and invalidation were also reworked.
* Link establishment timeouts include a per-hop grace period, which gives multi-hop links more time to establish.
* A known destination that is already cached is no longer cached again.
* Packet RSSI, SNR and Q handling was adjusted.
* Minor optimizations and logging changes were made in Identity and Transport.

## Fixes

* Announce processing now runs to completion when an announce is skipped, instead of breaking out early.
* Announce packets read from the cache have their `receiving_interface` restored.
* The persisted time offset is limited to a reasonable range, so a corrupted or incorrect value read from file is no longer applied.

## Build and Test

* Fixed CMake handling of missing GitHub tags.
* Added `CONTRIBUTING.md`, an updated PR policy and a pull request template for contributors.

## Contributors

Thanks to @attermann, @nilu96 and @ronandi for their contributions to this release.
