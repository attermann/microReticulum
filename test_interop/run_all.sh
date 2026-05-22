#!/usr/bin/env bash
# Master interop driver: builds each C++ test app under test_interop/,
# then runs each test's driver script sequentially. Reports aggregated
# PASS/FAIL. Exit 0 iff all tests PASS.
#
# Per-test logs preserved under /tmp/microreticulum_interop_<timestamp>/<name>.log
# so a failed run can be diagnosed after the fact.
#
# Usage:
#   bash test_interop/run_all.sh

set -u

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TS="$(date +%Y%m%d_%H%M%S)"
LOG_DIR="/tmp/microreticulum_interop_${TS}"
mkdir -p "$LOG_DIR"
echo "[run_all] logs: $LOG_DIR"

# Each row: name|app_subdir|driver_script
# An empty app_subdir means the driver doesn't need a C++ app build.
TESTS=(
  "resource|test_interop/resource_interop_sender|test_interop/run_resource_transfer.sh"
  "packet|test_interop/packet_interop_sender|test_interop/run_packet_exchange.sh"
  "link|test_interop/link_interop_sender|test_interop/run_link_lifecycle.sh"
  "request|test_interop/request_interop_sender|test_interop/run_request_response.sh"
)

declare -a RESULTS=()
OVERALL_RC=0

for row in "${TESTS[@]}"; do
  IFS='|' read -r NAME APP_SUBDIR DRIVER <<<"$row"

  if [[ ! -f "$REPO_ROOT/$DRIVER" ]]; then
    echo "[run_all] $NAME: SKIP (driver missing: $DRIVER)"
    RESULTS+=("$NAME: SKIP")
    continue
  fi

  if [[ -n "$APP_SUBDIR" ]]; then
    if [[ ! -d "$REPO_ROOT/$APP_SUBDIR" ]]; then
      echo "[run_all] $NAME: SKIP (test app missing: $APP_SUBDIR)"
      RESULTS+=("$NAME: SKIP")
      continue
    fi
    echo "[run_all] $NAME: building $APP_SUBDIR"
    if ! ( cd "$REPO_ROOT/$APP_SUBDIR" && pio run -e native17 ) \
        >"$LOG_DIR/${NAME}.build.log" 2>&1; then
      echo "[run_all] $NAME: FAIL (build failed; see $LOG_DIR/${NAME}.build.log)"
      RESULTS+=("$NAME: FAIL")
      OVERALL_RC=1
      continue
    fi
  fi

  echo "[run_all] $NAME: running driver"
  if bash "$REPO_ROOT/$DRIVER" >"$LOG_DIR/${NAME}.log" 2>&1; then
    echo "[run_all] $NAME: PASS"
    RESULTS+=("$NAME: PASS")
  else
    echo "[run_all] $NAME: FAIL (see $LOG_DIR/${NAME}.log)"
    RESULTS+=("$NAME: FAIL")
    OVERALL_RC=1
  fi
done

echo
echo "=================== interop summary ==================="
for r in "${RESULTS[@]}"; do
  echo "  $r"
done
echo "logs: $LOG_DIR"
exit $OVERALL_RC
