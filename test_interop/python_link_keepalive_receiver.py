#!/usr/bin/env python3
"""
Python receiver for the link keepalive interop test.

The C++ side opens a link and leaves it idle.
  Hold   — for HOLD seconds Python behaves normally. On a local link the
           keepalive interval is 5 s and the stale time 10 s, so the link is
           still ACTIVE at the end only if the C++ initiator sent keepalives
           and Python answered them.
  Silent — Python then drops everything on the link except LINKCLOSE, and
           stops its own watchdog from closing the link. The C++ side must go
           stale, close the link and send LINKCLOSE, which Python sees as
           INITIATOR_CLOSED.

Exit codes:
  0 — link held with keepalives answered, then closed by the initiator
  1 — link closed early, too few keepalives, or wrong teardown_reason
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
ASPECT   = "keepalive"

HOLD           = 22.0  # longer than the C++ side's HOLD_S
MIN_KEEPALIVES = 3     # answered during HOLD, at one per 5 s
CLOSE_WITHIN   = 25.0  # stale time + grace + margin

state = {
    "link": None,
    "established_at": 0.0,
    "keepalives": 0,
    "last_keepalive": 0.0,
    "silent_at": 0.0,
    "closed_reason": None,
    "closed_at": 0.0,
}


def reason_name(r):
    return {
        RNS.Link.TIMEOUT:            "TIMEOUT",
        RNS.Link.INITIATOR_CLOSED:   "INITIATOR_CLOSED",
        RNS.Link.DESTINATION_CLOSED: "DESTINATION_CLOSED",
    }.get(r, f"OTHER({r})")


def on_link_established(link):
    if state["link"] is not None:
        return
    link.set_link_closed_callback(on_link_closed)
    state["link"] = link
    state["established_at"] = time.time()
    print(f"[python] link established: {link.link_id.hex()}", flush=True)


def on_link_closed(link):
    state["closed_reason"] = link.teardown_reason
    state["closed_at"] = time.time()
    print(f"[python] link closed after {state['closed_at'] - state['established_at']:.1f} s: "
          f"teardown_reason={reason_name(link.teardown_reason)}", flush=True)


def go_silent(link):
    # Drop everything but LINKCLOSE, so the initiator hears nothing back.
    original_receive = link.receive
    def receive(packet):
        if packet.context == RNS.Packet.LINKCLOSE:
            original_receive(packet)
    link.receive = receive
    # Keep this side's watchdog from closing the link first.
    link.stale_time = 3600


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


def finish(rc, message):
    print(f"[python] {message}", flush=True)
    sys.exit(rc)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=14258)
    ap.add_argument("--timeout", type=float, default=75.0)
    args = ap.parse_args()

    config_dir = tempfile.mkdtemp(prefix="rns_interop_keepalive_")
    os.makedirs(os.path.join(config_dir, "storage", "resources"), exist_ok=True)
    write_config(config_dir, args.port)
    print(f"[python] config dir: {config_dir}", flush=True)

    try:
        RNS.Reticulum(config_dir)
    except Exception as e:
        print(f"[python] Reticulum init failed: {e}", file=sys.stderr)
        sys.exit(3)

    identity = RNS.Identity()
    destination = RNS.Destination(identity, RNS.Destination.IN,
                                  RNS.Destination.SINGLE, APP_NAME, ASPECT)
    destination.set_link_established_callback(on_link_established)

    print(f"[python] destination hash: {destination.hash.hex()}", flush=True)
    destination.announce()

    start = time.time()
    last_announce = start
    while time.time() - start < args.timeout:
        link = state["link"]
        now = time.time()

        if link is None:
            if now - last_announce >= 3.0:
                destination.announce()
                last_announce = now
            time.sleep(0.1)
            continue

        # A responder sets last_keepalive when it answers a keepalive.
        if link.last_keepalive != state["last_keepalive"] and not state["silent_at"]:
            state["last_keepalive"] = link.last_keepalive
            state["keepalives"] += 1
            print(f"[python] answered keepalive {state['keepalives']} at "
                  f"{now - state['established_at']:.1f} s", flush=True)

        if not state["silent_at"]:
            if state["closed_reason"] is not None:
                finish(1, f"FAILURE link closed during hold: "
                          f"{reason_name(state['closed_reason'])}")
            if now - state["established_at"] >= HOLD:
                if link.status != RNS.Link.ACTIVE or state["keepalives"] < MIN_KEEPALIVES:
                    finish(1, f"FAILURE after hold: status={link.status} "
                              f"keepalives={state['keepalives']}")
                print(f"[python] held {HOLD:.0f} s with {state['keepalives']} keepalives "
                      f"answered; going silent", flush=True)
                go_silent(link)
                state["silent_at"] = now
        else:
            if state["closed_reason"] is not None:
                after = state["closed_at"] - state["silent_at"]
                if state["closed_reason"] == RNS.Link.INITIATOR_CLOSED and after <= CLOSE_WITHIN:
                    finish(0, f"SUCCESS initiator closed the link {after:.1f} s after "
                              f"Python went silent")
                finish(1, f"FAILURE closed {after:.1f} s after going silent: "
                          f"{reason_name(state['closed_reason'])}")
            if now - state["silent_at"] > CLOSE_WITHIN:
                finish(1, f"FAILURE link still open {CLOSE_WITHIN:.0f} s after going silent")

        time.sleep(0.1)

    finish(2, f"TIMEOUT established={state['link'] is not None} "
              f"keepalives={state['keepalives']}")


if __name__ == "__main__":
    main()
