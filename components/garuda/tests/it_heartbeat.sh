#!/bin/sh
# 9xOS Garuda - integration tests: real daemon, real socket, real client.
#   1. A client that hangs must be detected and garudad must exit with status 2.
#   2. A client that unregisters cleanly must not cause a fault.
set -eu

GARUDAD="$1"
DEMO="$2"
TMP="$(mktemp -d)"
SOCK="$TMP/garuda.sock"
trap 'kill "$DPID" 2>/dev/null || true; rm -rf "$TMP"' EXIT

wait_for_socket() {
    i=0
    while [ ! -S "$SOCK" ]; do
        i=$((i + 1))
        [ "$i" -le 50 ] || { echo "FAIL: socket never appeared"; exit 1; }
        sleep 0.1
    done
}

# --- Test 1: heartbeat loss -> fault -------------------------------------
"$GARUDAD" -s "$SOCK" -x -F 2>"$TMP/log1" &
DPID=$!
wait_for_socket
"$DEMO" -s "$SOCK" -n hang-test -p 100 -c 5 -H 1500 &
CPID=$!

status=0
i=0
while kill -0 "$DPID" 2>/dev/null; do
    i=$((i + 1))
    if [ "$i" -gt 50 ]; then
        echo "FAIL: garudad did not detect the hang within 5 s"
        cat "$TMP/log1"
        exit 1
    fi
    sleep 0.1
done
wait "$DPID" || status=$?
wait "$CPID" 2>/dev/null || true
if [ "$status" -ne 2 ]; then
    echo "FAIL: expected exit status 2 on fault, got $status"
    cat "$TMP/log1"
    exit 1
fi
grep -q "FAULT client=hang-test" "$TMP/log1" || { echo "FAIL: no FAULT log line"; cat "$TMP/log1"; exit 1; }
echo "PASS: heartbeat loss detected"

# --- Test 2: clean unregister -> no fault ---------------------------------
"$GARUDAD" -s "$SOCK" -x -F 2>"$TMP/log2" &
DPID=$!
wait_for_socket
"$DEMO" -s "$SOCK" -n clean-test -p 100 -c 5 -u
sleep 0.5
if ! kill -0 "$DPID" 2>/dev/null; then
    echo "FAIL: garudad exited after a clean unregister"
    cat "$TMP/log2"
    exit 1
fi
kill -TERM "$DPID"
status=0
wait "$DPID" || status=$?
[ "$status" -eq 0 ] || { echo "FAIL: clean shutdown returned $status"; cat "$TMP/log2"; exit 1; }
grep -q "unregistered client=clean-test" "$TMP/log2" || { echo "FAIL: no unregister log line"; cat "$TMP/log2"; exit 1; }
echo "PASS: clean unregister, no fault"
