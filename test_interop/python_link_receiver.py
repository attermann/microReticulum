#!/usr/bin/env python3
"""
Python receiver for the link lifecycle interop test.

Two consecutive Link establishments are expected:
  Pass 1 — C++ initiates teardown. Python observes teardown_reason =
           DESTINATION_CLOSED (the *other* side closed).
  Pass 2 — Python initiates teardown 1 second after established.
           Python observes teardown_reason = INITIATOR_CLOSED.

Exit codes:
  0 — both passes saw established+closed callbacks with the expected reasons
  1 — wrong teardown_reason or callback never fired
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
ASPECT   = "link"

state = {
    "pass": 1,
    "pass1_established": False,
    "pass1_closed_reason": None,
    "pass2_established": False,
    "pass2_closed_reason": None,
    "current_link": None,
    "pass2_established_at": 0.0,
}


def reason_name(r):
    return {
        RNS.Link.TIMEOUT:            "TIMEOUT",
        RNS.Link.INITIATOR_CLOSED:   "INITIATOR_CLOSED",
        RNS.Link.DESTINATION_CLOSED: "DESTINATION_CLOSED",
    }.get(r, f"OTHER({r})")


def on_link_established(link):
    if state["pass"] == 1:
        print(f"[python] pass1 link established: {link.link_id.hex()}",
              flush=True)
        state["pass1_established"] = True
    elif state["pass"] == 2:
        print(f"[python] pass2 link established: {link.link_id.hex()}",
              flush=True)
        state["pass2_established"] = True
        state["pass2_established_at"] = time.time()
    state["current_link"] = link


def on_link_closed(link):
    r = link.teardown_reason
    if state["pass"] == 1:
        print(f"[python] pass1 link closed: teardown_reason={reason_name(r)}",
              flush=True)
        state["pass1_closed_reason"] = r
        # Roll the phase counter forward; next link establishment is pass 2.
        state["pass"] = 2
        state["current_link"] = None
    elif state["pass"] == 2:
        print(f"[python] pass2 link closed: teardown_reason={reason_name(r)}",
              flush=True)
        state["pass2_closed_reason"] = r


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
    ap.add_argument("--port", type=int, default=14254)
    ap.add_argument("--timeout", type=float, default=45.0)
    args = ap.parse_args()

    config_dir = tempfile.mkdtemp(prefix="rns_interop_link_")
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
    # Wire the link-closed callback by installing it on the link as it's
    # established (Python doesn't expose a destination-level closed hook).
    original_estab = on_link_established
    def estab_and_wire_close(link):
        link.set_link_closed_callback(on_link_closed)
        original_estab(link)
    destination.set_link_established_callback(estab_and_wire_close)

    print(f"[python] destination hash: {destination.hash.hex()}", flush=True)
    destination.announce()
    print("[python] announced", flush=True)

    start = time.time()
    last_announce = start
    while time.time() - start < args.timeout:
        # Re-announce periodically until pass 1 starts (link established).
        if time.time() - last_announce >= 3.0 and not state["pass1_established"]:
            destination.announce()
            last_announce = time.time()
            print("[python] re-announced", flush=True)

        # Pass 2: 1 second after we observe establishment, Python tears down.
        if (state["pass"] == 2
                and state["pass2_established"]
                and state["current_link"] is not None
                and state["pass2_closed_reason"] is None
                and time.time() - state["pass2_established_at"] >= 1.0):
            print("[python] pass2 calling link.teardown()", flush=True)
            state["current_link"].teardown()

        # Completion check.
        # teardown_reason on a Link indicates which *role* closed it
        # (the side that called Link() — i.e., the initiator — vs. the
        # side that accepted the link — i.e., the destination), NOT
        # whether the local or remote side initiated the close. Both
        # sides observe the same teardown_reason value.
        #
        # Pass 1: C++ (initiator) tears down  -> both see INITIATOR_CLOSED
        # Pass 2: Python (destination) tears down -> both see DESTINATION_CLOSED
        if (state["pass1_established"] and state["pass1_closed_reason"] is not None
                and state["pass2_established"] and state["pass2_closed_reason"] is not None):
            ok_p1 = (state["pass1_closed_reason"] == RNS.Link.INITIATOR_CLOSED)
            ok_p2 = (state["pass2_closed_reason"] == RNS.Link.DESTINATION_CLOSED)
            if ok_p1 and ok_p2:
                print("[python] SUCCESS", flush=True)
                sys.exit(0)
            else:
                print(f"[python] FAILURE p1_reason={reason_name(state['pass1_closed_reason'])} "
                      f"p2_reason={reason_name(state['pass2_closed_reason'])}", flush=True)
                sys.exit(1)

        time.sleep(0.1)

    print(f"[python] TIMEOUT p1est={state['pass1_established']} "
          f"p1cls={state['pass1_closed_reason']} "
          f"p2est={state['pass2_established']} "
          f"p2cls={state['pass2_closed_reason']}", flush=True)
    sys.exit(2)


if __name__ == "__main__":
    main()
