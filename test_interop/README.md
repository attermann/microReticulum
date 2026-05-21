# Interop tests against the Python Reticulum reference

This directory holds tests that validate the C++ port's behaviour against the
Python Reticulum reference implementation. Two flavours, in increasing scope:

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

## 2. Full Resource transfer over UDP (not yet implemented)

Goal: drive an actual `RNS.Resource` transfer end-to-end between a Python
peer and the C++ port over a UDP loopback interface and verify byte-exact
payload reception.

Sketch:

- `python_resource_receiver.py` — Python script that runs an `RNS.Reticulum`
  instance on a known UDP port, with a known `Identity` (fixed private key
  for reproducibility) and a `Destination` that accepts links with
  `ACCEPT_ALL`. Waits for a `Resource`, verifies its payload, and exits
  with a status code.
- A C++ test binary or extended example that runs an `RNS::Reticulum`
  instance, opens a UDP interface to the Python peer, receives the
  announce, builds a `Link`, sends a `Resource` with a known payload, and
  exits on proof arrival or timeout.
- `run.sh` — driver script that launches both processes, collects their
  stdout, and exits with success iff both report SUCCESS.

Architectural caveats:

- microReticulum currently uses a global static `RNS::Transport`. A
  two-instance C++ loopback within the same process is not currently
  possible, which is why this test is structured as two OS processes
  communicating over UDP.
- The Python side must use a UDP interface configuration that mirrors the
  C++ `UDPInterface` (port 4242 by default, broadcast or
  point-to-point loopback). The Python `UDPInterface` config goes in
  Python's `~/.reticulum/config` (or a dedicated `--config` dir).

This work is deferred from sub-phase 1.7 to a follow-up because building
a robust two-process driver and debugging the live-network interop has a
significant time cost. The wire-format cross-validation in #1 is the most
error-prone single piece and is covered now.
