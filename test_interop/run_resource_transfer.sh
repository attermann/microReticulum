#!/usr/bin/env bash
# Driver: launches the Python receiver and the C++ sender concurrently,
# captures their outputs, and reports success iff both succeed.
#
# Prereqs:
#   - Python RNS available on PYTHONPATH (defaults to the local fork at
#     /Users/chad/python/Reticulum-latest)
#   - The C++ sender binary already built:
#       cd examples/resource_interop_sender && pio run -e native17
#
# Usage:
#   bash test_interop/run_resource_transfer.sh
#
# Exit codes:
#   0  both ends report SUCCESS
#   1  failure (mismatch / receiver complaint / sender non-zero)
#   2  timeout

set -u

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PY_RNS="${PY_RNS:-/Users/chad/python/Reticulum-latest}"
TIMEOUT_S="${TIMEOUT_S:-90}"

RECV="$REPO_ROOT/test_interop/python_resource_receiver.py"
SENDER="$REPO_ROOT/examples/resource_interop_sender/.pio/build/native17/program"

if [[ ! -x "$SENDER" ]]; then
  echo "Sender binary not found at $SENDER. Build with:" >&2
  echo "  cd examples/resource_interop_sender && pio run -e native17" >&2
  exit 1
fi

TMPDIR="$(mktemp -d -t microreticulum_interop.XXXXXX)"
trap 'rm -rf "$TMPDIR"; if [[ -n "${RECV_PID:-}" ]] && kill -0 "$RECV_PID" 2>/dev/null; then kill "$RECV_PID" 2>/dev/null || true; fi' EXIT

RECV_LOG="$TMPDIR/receiver.log"
SENDER_LOG="$TMPDIR/sender.log"

echo "[driver] launching Python receiver"
PYTHONPATH="$PY_RNS" python3 "$RECV" --timeout "$TIMEOUT_S" \
    >"$RECV_LOG" 2>&1 &
RECV_PID=$!

# Wait a couple of seconds so the Python side binds its UDP socket and
# emits its first announce before the sender begins listening.
sleep 2

echo "[driver] launching C++ sender"
"$SENDER" >"$SENDER_LOG" 2>&1 &
SENDER_PID=$!

# Wait for either side to finish.
WAIT_DEADLINE=$(( $(date +%s) + TIMEOUT_S + 10 ))
while true; do
  if ! kill -0 "$RECV_PID" 2>/dev/null && ! kill -0 "$SENDER_PID" 2>/dev/null; then
    break
  fi
  if (( $(date +%s) >= WAIT_DEADLINE )); then
    echo "[driver] watchdog timeout"
    break
  fi
  sleep 0.5
done

# Reap whatever's left
kill "$SENDER_PID" 2>/dev/null || true
kill "$RECV_PID"   2>/dev/null || true
wait "$SENDER_PID" 2>/dev/null
SENDER_RC=$?
wait "$RECV_PID"   2>/dev/null
RECV_RC=$?

echo "--- python receiver log ---"
cat "$RECV_LOG"
echo "--- c++ sender log ---"
cat "$SENDER_LOG"
echo "--- summary ---"
echo "[driver] receiver exit $RECV_RC, sender exit $SENDER_RC"

if grep -q "SUCCESS" "$RECV_LOG" && [[ $SENDER_RC -eq 0 ]]; then
  echo "[driver] PASS"
  exit 0
fi
echo "[driver] FAIL"
exit 1
