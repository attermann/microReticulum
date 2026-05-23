# Interop tests against the Python Reticulum reference

This directory holds tests that validate the C++ port's behaviour against the
Python Reticulum reference implementation. Two flavours, in increasing scope:

## Targets

Four C++ sender executables, one per interop scenario. Each is a CMake
target of the same name and is configured at compile time with its own
UDP port pair so the scenarios can coexist on a single host:

| CMake target              | UDP local | UDP remote | Scenario                                                |
|---------------------------|-----------|------------|---------------------------------------------------------|
| `packet_interop_sender`   | 14253     | 14252      | DATA packets to a Destination, bidirectional            |
| `link_interop_sender`     | 14255     | 14254      | Link establish + two-pass teardown                      |
| `request_interop_sender`  | 14257     | 14256      | `Link::request("/echo", ...)` single-packet RPC         |
| `resource_interop_sender` | 14243     | 14242      | Full Resource transfer over a Link (1024-byte payload)  |

## CMake

The four C++ interop senders are built by the top-level CMake project.
Configure and build from the repository root:

Configure:
```
cmake -S . -B build
```

Build all interop senders:
```
cmake --build build -j --target packet_interop_sender link_interop_sender request_interop_sender resource_interop_sender
```

Build a single sender:
```
cmake --build build -j --target packet_interop_sender
```

After building, point the existing driver scripts at the binaries in
`build/test_interop/` (the scripts default to the PlatformIO output path —
adjust `PIO_BUILD_DIR` or the binary path inside the script as needed):
```
bash test_interop/run_all.sh
bash test_interop/run_packet_exchange.sh
```

Skip building interop senders entirely when configuring the parent project:
```
cmake -S . -B build -DRNS_BUILD_INTEROP=OFF
```

## Top-level driver

`test_interop/run_all.sh` builds each C++ test app and runs all four
interop scenarios sequentially, reporting per-test PASS/FAIL and an
aggregated exit status. Run from the repo root:

```
bash test_interop/run_all.sh
```

Each scenario also has its own driver script if you only want to run one:

| Test         | Driver script                            | What it covers                                                  |
|--------------|------------------------------------------|------------------------------------------------------------------|
| resource     | `test_interop/run_resource_transfer.sh`  | Full Resource transfer over a Link (1024-byte payload)           |
| packet       | `test_interop/run_packet_exchange.sh`    | DATA packets to a Destination, bidirectional, 256-byte patterns  |
| link         | `test_interop/run_link_lifecycle.sh`     | Link establish + two-pass teardown (initiator and destination)   |
| request      | `test_interop/run_request_response.sh`   | `Link::request("/echo", ...)` single-packet RPC (200-byte payload) |

All four currently PASS against Python RNS 1.2.9 at `/Users/chad/python/Reticulum-latest`.

## 1. Wire-format cross-validation (implemented)

The C++ unit test `test/test_resource_advertisement` includes a
`PYTHON_GOLDEN_HEX` byte sequence captured from Python 1.2.9
`RNS.Resource.ResourceAdvertisement.pack()` for a known set of input fields.
The test:

- Unpacks Python's bytes via C++ `ResourceAdvertisement::unpack` and asserts
  every field round-trips correctly.
- Packs the same input fields via C++ `ResourceAdvertisement::pack`, unpacks
  via C++ again, and compares byte-for-byte against the Python golden. If
  the bytes match exactly we have byte-equal wire interop; if they only
  match by field-equivalence we have spec-compliant but not byte-equal
  interop (msgpack permits multiple valid integer encodings of the same
  value).

`encode_advertisement.py` regenerates the golden bytes from Python so the
constants in the C++ test can be updated whenever the Python reference's
wire format changes.

## 2. Full Resource transfer over UDP — C++ initiator -> Python receiver

A driver that:
1. Spawns `python_resource_receiver.py` on UDP port 14242 (avoids the
   common RNS desktop-app conflict on 4242).
2. Spawns the C++ sender binary built from
   `test_interop/resource_interop_sender/` on UDP port 14243, configured to
   send to 127.0.0.1:14242.
3. Watches both stdouts and exits 0 iff both report SUCCESS.

Wire path on each side:

- Python: listens on `127.0.0.1:14242`, sends to `127.0.0.1:14243`.
- C++:    listens on `127.0.0.1:14243`, sends to `127.0.0.1:14242`.

Point-to-point loopback is used instead of `255.255.255.255` broadcast
because macOS doesn't reliably loop limited-broadcast traffic between two
processes on the same host. The example's `platformio.ini` sets
`DEFAULT_UDP_LOCAL_HOST`, `DEFAULT_UDP_REMOTE_HOST`,
`DEFAULT_UDP_LOCAL_PORT` and `DEFAULT_UDP_REMOTE_PORT` build flags;
`examples/common/udp_interface/UDPInterface.h` has been extended with
matching `#ifndef` guards so these are overridable per-example without
touching the library code.

Architectural caveats:

- microReticulum uses a global static `RNS::Transport`. A two-instance
  C++ loopback inside the same process is not possible — this is why the
  test is structured as two OS processes communicating over UDP.

### Running

```
# Build the C++ sender once
cd test_interop/resource_interop_sender
pio run -e native17
cd ../..

# Run the interop driver
bash test_interop/run_resource_transfer.sh
```

Expected last line: `[driver] PASS` with both processes exiting 0.

### Bugs found and fixed during live interop

The interop test exposed several pre-existing bugs as well as ones in the
new Resource implementation:

- **`Link::ready_for_new_resource()`** — boolean returned the inverse of
  what it should (true when an outgoing resource already existed,
  blocking new ones). Fixed in `src/Link.cpp`.
- **`Link::link_closed()`** — iterated `_incoming_resources` /
  `_outgoing_resources` while `Resource::cancel()` erases from those same
  sets, invalidating the iterator and crashing. Now snapshots first.
- **`Link::receive()` PROOF / RESOURCE_PRF dispatch** — same iterator
  invalidation pattern when `validate_proof()` calls
  `resource_concluded()` which erases. Now snapshots first.
- **`Resource` hash & expected-proof input order** — Python computes
  `full_hash(data || random_hash)` and `full_hash(data || hash)`; the C++
  port was computing `full_hash(random_hash || data || ...)`. Receiver
  computed the right thing locally but the advertised hash didn't match,
  failing the integrity check. Fixed in `src/Resource.cpp`.
- **`UDPInterface.h` macros** — `DEFAULT_UDP_PORT` etc. lacked `#ifndef`
  guards, so build-flag overrides were silently ignored.

Additional pre-existing bugs uncovered by the broader interop suite
(`test_interop/run_all.sh`):

- **`RequestReceiptData::_link`** declared as bare `Link _link;` instead
  of `Link _link = {Type::NONE};`. The default Link constructor with no
  destination and no owner generates `_prv` but leaves `_sig_prv` null,
  then dereferences `_sig_prv->public_key()` → SIGSEGV. Every call to
  `Link::request()` constructs a `RequestReceipt` containing a
  `RequestReceiptData`, so this crashed the RPC path entirely. Fixed in
  `src/LinkData.h`.
- **Embedded `Link` members in `RequestReceiptData`** required by the
  fix above are now self-consistent across all four interop scenarios.
