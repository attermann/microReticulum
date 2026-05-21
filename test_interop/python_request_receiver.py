#!/usr/bin/env python3
"""
Python receiver for the link request/response interop test.

Registers an echo handler on path "/echo" that returns the request bytes
verbatim. The C++ side initiates a Link, sends a 200-byte request, and
expects the same 200 bytes back as the response.

Exit codes:
  0 — handler was invoked with the expected payload and echoed back
  1 — payload mismatch on inbound request
  2 — timeout
  3 — setup error
"""

import argparse
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
ASPECT   = "request"
EXPECTED_PAYLOAD = bytes(i & 0xff for i in range(200))

state = {
    "request_seen":  False,
    "request_ok":    False,
    "link_closed":   False,
}


def echo_handler(path, data, request_id, link_id, remote_identity, requested_at):
    ok = (data == EXPECTED_PAYLOAD)
    print(f"[python] handler called: path={path!r} data_len={len(data)} match={ok}",
          flush=True)
    state["request_seen"] = True
    state["request_ok"]   = ok
    # Echo the bytes back regardless — easier to debug a mismatch on the
    # client side if both ends see the wrong content.
    return data


def on_link_established(link):
    print(f"[python] link established: {link.link_id.hex()}", flush=True)
    link.set_link_closed_callback(on_link_closed)


def on_link_closed(link):
    print(f"[python] link closed", flush=True)
    state["link_closed"] = True


def write_config(config_dir: str, port: int) -> None:
    forward_port = port + 1
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
    forward_port = {forward_port}
"""
    with open(os.path.join(config_dir, "config"), "w") as f:
        f.write(cfg)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=14256)
    ap.add_argument("--timeout", type=float, default=30.0)
    args = ap.parse_args()

    config_dir = tempfile.mkdtemp(prefix="rns_interop_req_")
    os.makedirs(os.path.join(config_dir, "storage", "resources"), exist_ok=True)
    write_config(config_dir, args.port)
    print(f"[python] config dir: {config_dir}", flush=True)

    try:
        reticulum = RNS.Reticulum(config_dir)
    except Exception as e:
        print(f"[python] Reticulum init failed: {e}", file=sys.stderr)
        sys.exit(3)

    identity = RNS.Identity()
    destination = RNS.Destination(identity, RNS.Destination.IN,
                                  RNS.Destination.SINGLE, APP_NAME, ASPECT)
    destination.set_link_established_callback(on_link_established)
    destination.set_proof_strategy(RNS.Destination.PROVE_ALL)
    destination.register_request_handler("/echo", echo_handler,
                                         allow=RNS.Destination.ALLOW_ALL)

    print(f"[python] destination hash: {destination.hash.hex()}", flush=True)
    destination.announce()
    print("[python] announced", flush=True)

    start = time.time()
    last_announce = start
    while time.time() - start < args.timeout:
        if time.time() - last_announce >= 3.0 and not state["request_seen"]:
            destination.announce()
            last_announce = time.time()
            print("[python] re-announced", flush=True)

        if state["request_seen"]:
            if state["request_ok"]:
                # Give the response a moment to land on the wire before
                # exiting (the handler return value is sent asynchronously
                # by RNS's link machinery).
                time.sleep(0.5)
                print("[python] SUCCESS", flush=True)
                sys.exit(0)
            else:
                print("[python] FAILURE — payload mismatch on inbound",
                      flush=True)
                sys.exit(1)

        time.sleep(0.1)

    print(f"[python] TIMEOUT request_seen={state['request_seen']}",
          flush=True)
    sys.exit(2)


if __name__ == "__main__":
    main()
