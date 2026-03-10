#!/bin/bash
# test_part1_chroot.sh - Test Part 1: chroot mode
#
# Prerequisites:
#   export ROOTFS=/path/to/rootfs
#   export MINICTL=./minictl

set -e

: "${ROOTFS:?Please set ROOTFS to your root filesystem path}"
: "${MINICTL:=./minictl}"

echo "=== Testing Part 1: chroot mode ==="
echo "ROOTFS: $ROOTFS"
echo "MINICTL: $MINICTL"
echo ""

# Test 1: Basic execution
echo "Test 1: Basic execution"
echo "Command: $MINICTL chroot $ROOTFS /bin/sh -c 'echo hello'"
OUTPUT=$("$MINICTL" chroot "$ROOTFS" /bin/sh -c 'echo hello')
if [ "$OUTPUT" = "hello" ]; then
    echo "PASS: Output is 'hello'"
else
    echo "FAIL: Expected 'hello', got '$OUTPUT'"
    exit 1
fi
echo ""

# Test 2: pwd should be /
echo "Test 2: Current directory should be /"
echo "Command: $MINICTL chroot $ROOTFS /bin/sh -c 'pwd'"
OUTPUT=$("$MINICTL" chroot "$ROOTFS" /bin/sh -c 'pwd')
if [ "$OUTPUT" = "/" ]; then
    echo "PASS: pwd is '/'"
else
    echo "FAIL: Expected '/', got '$OUTPUT'"
    exit 1
fi
echo ""

# Test 3: Can see rootfs contents
echo "Test 3: Can list rootfs contents"
echo "Command: $MINICTL chroot $ROOTFS /bin/ls /"
OUTPUT=$("$MINICTL" chroot "$ROOTFS" /bin/ls /)
if echo "$OUTPUT" | grep -q "bin"; then
    echo "PASS: Can see /bin in rootfs"
else
    echo "FAIL: Cannot see rootfs contents"
    exit 1
fi
echo "Contents of /:"
echo "$OUTPUT"
echo ""

# Test 4: Hostname NOT isolated (chroot doesn't isolate hostname)
echo "Test 4: Hostname should NOT be isolated (same as host)"
HOST_HOSTNAME=$(hostname)
echo "Host hostname: $HOST_HOSTNAME"
if command -v hostname &> /dev/null && [ -x "$ROOTFS/bin/hostname" ]; then
    CHROOT_HOSTNAME=$("$MINICTL" chroot "$ROOTFS" /bin/hostname)
    echo "Chroot hostname: $CHROOT_HOSTNAME"
    if [ "$HOST_HOSTNAME" = "$CHROOT_HOSTNAME" ]; then
        echo "PASS: Hostname is the same (chroot doesn't isolate hostname)"
    else
        echo "WARN: Hostnames differ - unexpected for chroot"
    fi
else
    echo "SKIP: hostname command not available"
fi
echo ""

# Test 5: Exit code propagation
echo "Test 5: Exit code propagation"
echo "Command: $MINICTL chroot $ROOTFS /bin/sh -c 'exit 42'"
set +e
"$MINICTL" chroot "$ROOTFS" /bin/sh -c 'exit 42'
EXIT_CODE=$?
set -e
if [ "$EXIT_CODE" = "42" ]; then
    echo "PASS: Exit code 42 propagated correctly"
else
    echo "FAIL: Expected exit code 42, got $EXIT_CODE"
fi
echo ""

echo "=== Part 1 tests complete ==="
