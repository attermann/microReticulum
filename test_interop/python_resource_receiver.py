#!/usr/bin/env python3
"""
Starter Python receiver for interop-testing Resource transfers from the
C++ microReticulum port.

What it does:
- Initialises a Python `RNS.Reticulum` instance with a per-run config dir.
- Writes a config that declares a UDP interface on a known port.
- Loads (or creates) an `Identity` from a fixed private-key file so the
  destination hash is reproducible across runs.
- Creates a Destination matching the agreed app/aspect.
- Sets an ACCEPT_ALL resource strategy and a concluded callback that prints
  the received payload's length and sha-256 to stdout, then exits.

What's NOT yet wired:
- The C++ side. See README.md for the planned shape of `cpp_resource_sender`.
- A driver script that launches both ends and reconciles outputs.
- An automated payload-comparison check against a known input.

Usage:
    PYTHONPATH=/Users/chad/python/Reticulum-latest \
        python3 python_resource_receiver.py [--port 4242] [--timeout 60]

Prints SUCCESS / FAILURE / TIMEOUT on stdout. Exit codes: 0 success,
1 payload mismatch, 2 timeout, 3 setup error.
"""

import argparse
import hashlib
import os
import sys
import tempfile
import time

try:
    import RNS
except ImportError as e:
    print(f"[python] failed to import RNS: {e}", file=sys.stderr)
    sys.exit(3)

APP_NAME = "microreticulum_interop"
ASPECT   = "resource"
EXPECTED_HASH_HEX = None  # set via --expect-sha256 if you want byte-exact validation

# Module-level state held in a dict so callbacks can mutate it.
state = {"received": False, "payload_bytes": None, "payload_hash": None, "link": None}


def on_resource_concluded(resource):
    if resource.status == RNS.Resource.COMPLETE:
        # Resource.data may be a file-handle or bytes depending on size.
        if hasattr(resource.data, "read"):
            try:
                resource.data.seek(0)
            except Exception:
                pass
            data = resource.data.read()
        else:
            data = bytes(resource.data) if resource.data is not None else b""
        state["payload_bytes"] = data
        state["payload_hash"]  = hashlib.sha256(data).hexdigest()
        state["received"]      = True
        print(f"[python] resource complete: {len(data)} bytes, sha256 {state['payload_hash']}",
              flush=True)
    else:
        print(f"[python] resource failed with status {resource.status}", flush=True)
        state["received"] = True  # break the wait loop


def on_link_established(link):
    print(f"[python] link established: {link.link_id.hex()}", flush=True)
    link.set_resource_strategy(RNS.Link.ACCEPT_ALL)
    link.set_resource_concluded_callback(on_resource_concluded)
    state["link"] = link


def write_config(config_dir: str, port: int) -> None:
    config_path = os.path.join(config_dir, "config")
    cfg = f"""
[reticulum]
  enable_transport = No
  share_instance = No
  shared_instance_port = 37428
  instance_control_port = 37429
  panic_on_interface_error = No

[logging]
  loglevel = 4

[interfaces]

  [[UDPInterop]]
    type = UDPInterface
    interface_enabled = True
    listen_ip = 127.0.0.1
    listen_port = {port}
    forward_ip = 127.0.0.1
    forward_port = {port + 1}
"""
    with open(config_path, "w") as f:
        f.write(cfg)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=4242,
                    help="UDP port the Python receiver listens on")
    ap.add_argument("--timeout", type=float, default=60.0,
                    help="seconds to wait for a resource before giving up")
    ap.add_argument("--identity-file", default=None,
                    help="path to a saved RNS Identity; created if missing")
    ap.add_argument("--expect-sha256", default=None,
                    help="hex sha256 of expected payload; compared on receive")
    args = ap.parse_args()

    config_dir = tempfile.mkdtemp(prefix="rns_interop_recv_")
    write_config(config_dir, args.port)
    print(f"[python] config dir: {config_dir}", flush=True)

    try:
        reticulum = RNS.Reticulum(config_dir)
    except Exception as e:
        print(f"[python] Reticulum init failed: {e}", file=sys.stderr)
        sys.exit(3)

    # Identity (fixed if --identity-file was provided and exists)
    if args.identity_file and os.path.isfile(args.identity_file):
        identity = RNS.Identity.from_file(args.identity_file)
        if identity is None:
            print("[python] failed to load identity", file=sys.stderr)
            sys.exit(3)
    else:
        identity = RNS.Identity()
        if args.identity_file:
            identity.to_file(args.identity_file)

    destination = RNS.Destination(
        identity,
        RNS.Destination.IN,
        RNS.Destination.SINGLE,
        APP_NAME,
        ASPECT,
    )
    destination.set_link_established_callback(on_link_established)
    destination.set_proof_strategy(RNS.Destination.PROVE_ALL)

    print(f"[python] destination hash: {destination.hash.hex()}", flush=True)
    destination.announce()

    start = time.time()
    while time.time() - start < args.timeout:
        if state["received"]:
            payload = state["payload_bytes"]
            if payload is None:
                print("[python] FAILURE — resource did not deliver bytes",
                      flush=True)
                sys.exit(1)
            if args.expect_sha256 and state["payload_hash"] != args.expect_sha256.lower():
                print(f"[python] FAILURE — sha256 mismatch "
                      f"(got {state['payload_hash']}, expected {args.expect_sha256})",
                      flush=True)
                sys.exit(1)
            print("[python] SUCCESS", flush=True)
            sys.exit(0)
        time.sleep(0.25)

    print("[python] TIMEOUT", flush=True)
    sys.exit(2)


if __name__ == "__main__":
    main()
