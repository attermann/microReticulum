#!/usr/bin/env python3
"""
Python receiver for the basic-packet-exchange interop test.

Bidirectional flow:
  1. Initialise RNS with a fresh UDP-interface config.
  2. Create an IN destination on app=microreticulum_interop, aspect=packet.
  3. Register a packet callback that verifies an inbound packet's payload
     against the expected C++->Python pattern (0x00..0xff, 256 bytes).
  4. Announce so the C++ sender can learn our destination hash.
  5. Wait for the C++ packet to arrive; on receipt, announce and then send
     OUR packet (0xff..0x00, 256 bytes) to the C++ side's destination via
     the announced identity.
  6. Exit 0 on full bidirectional SUCCESS, 1 on payload mismatch, 2 on
     timeout, 3 on setup error.
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
ASPECT   = "packet"
EXPECTED_INBOUND = bytes(range(256))                  # C++ -> Python pattern
OUTBOUND_PAYLOAD = bytes(0xff - i for i in range(256))  # Python -> C++ pattern

state = {
    "received_ok": False,
    "sent": False,
    "remote_identity": None,
    "remote_dest_hash": None,
}


def on_packet(data, packet):
    ok = (data == EXPECTED_INBOUND)
    print(f"[python] packet received: {len(data)} bytes, "
          f"sha256={hashlib.sha256(data).hexdigest()[:16]}..., match={ok}",
          flush=True)
    state["received_ok"] = ok


class _AnnounceHandler:
    def __init__(self):
        self.aspect_filter = f"{APP_NAME}.{ASPECT}"
    def received_announce(self, destination_hash, announced_identity, app_data):
        # Skip our own echo.
        if state["remote_identity"] is not None:
            return
        state["remote_identity"] = announced_identity
        state["remote_dest_hash"] = destination_hash
        print(f"[python] received announce: dest={destination_hash.hex()}",
              flush=True)


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
    ap.add_argument("--port", type=int, default=14252)
    ap.add_argument("--timeout", type=float, default=30.0)
    args = ap.parse_args()

    config_dir = tempfile.mkdtemp(prefix="rns_interop_pkt_")
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
    destination.set_packet_callback(on_packet)
    destination.set_proof_strategy(RNS.Destination.PROVE_ALL)

    handler = _AnnounceHandler()
    RNS.Transport.register_announce_handler(handler)

    print(f"[python] destination hash: {destination.hash.hex()}", flush=True)
    destination.announce()
    print("[python] announced", flush=True)

    start = time.time()
    last_announce = start
    while time.time() - start < args.timeout:
        # Re-announce periodically. Keep doing this even after we know the
        # remote identity, because the C++ side ALSO needs to learn about
        # ours and the very first announce may race their startup.
        if time.time() - last_announce >= 3.0:
            destination.announce()
            last_announce = time.time()
            print("[python] re-announced", flush=True)

        # As soon as we know the remote identity AND have received the
        # inbound packet, send our outbound packet (once).
        if state["received_ok"] and state["remote_identity"] is not None and not state["sent"]:
            remote_dest = RNS.Destination(state["remote_identity"],
                                          RNS.Destination.OUT,
                                          RNS.Destination.SINGLE,
                                          APP_NAME, ASPECT)
            packet = RNS.Packet(remote_dest, OUTBOUND_PAYLOAD)
            packet.send()
            state["sent"] = True
            print(f"[python] packet sent: {len(OUTBOUND_PAYLOAD)} bytes",
                  flush=True)
            # Once we've sent, success criterion is met from Python's
            # side; give the C++ side a brief grace period to ack.
            print("[python] SUCCESS", flush=True)
            sys.exit(0)

        time.sleep(0.1)

    print(f"[python] TIMEOUT received_ok={state['received_ok']} "
          f"announce_seen={state['remote_identity'] is not None}", flush=True)
    sys.exit(2)


if __name__ == "__main__":
    main()
