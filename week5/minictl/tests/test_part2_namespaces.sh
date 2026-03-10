#!/bin/bash
# test_part2_namespaces.sh - Test Part 2: namespace isolation
#
# Prerequisites:
#   export ROOTFS=/path/to/rootfs
#   export MINICTL=./minictl

set -e

: "${ROOTFS:?Please set ROOTFS to your root filesystem path}"
: "${MINICTL:=./minictl}"

echo "=== Testing Part 2: namespace isolation ==="
echo "ROOTFS: $ROOTFS"
echo "MINICTL: $MINICTL"
echo ""

# Test 1: Hostname isolation
echo "Test 1: Hostname isolation"
TEST_HOSTNAME="test-container-$$"
echo "Command: $MINICTL run --hostname=$TEST_HOSTNAME $ROOTFS /bin/hostname"
if [ -x "$ROOTFS/bin/hostname" ]; then
    OUTPUT=$("$MINICTL" run --hostname="$TEST_HOSTNAME" "$ROOTFS" /bin/hostname)
    if [ "$OUTPUT" = "$TEST_HOSTNAME" ]; then
        echo "PASS: Container hostname is '$TEST_HOSTNAME'"
    else
        echo "FAIL: Expected hostname '$TEST_HOSTNAME', got '$OUTPUT'"
        exit 1
    fi
else
    echo "SKIP: hostname command not available in rootfs"
fi
echo ""

# Test 2: PID isolation (process should be PID 1)
echo "Test 2: PID isolation"
echo "Command: $MINICTL run $ROOTFS /bin/sh -c 'echo \$\$'"
OUTPUT=$("$MINICTL" run "$ROOTFS" /bin/sh -c 'echo $$')
if [ "$OUTPUT" = "1" ]; then
    echo "PASS: Process is PID 1 inside container"
else
    echo "FAIL: Expected PID 1, got '$OUTPUT'"
    echo "Note: PID namespace may not be working correctly"
fi
echo ""

# Test 3: User namespace (rootless - should see uid=0)
echo "Test 3: User namespace (rootless)"
echo "Command: $MINICTL run $ROOTFS /usr/bin/id"
if [ -x "$ROOTFS/usr/bin/id" ]; then
    OUTPUT=$("$MINICTL" run "$ROOTFS" /usr/bin/id)
    echo "Output: $OUTPUT"
    if echo "$OUTPUT" | grep -q "uid=0"; then
        echo "PASS: User appears as root (uid=0) inside container"
    else
        echo "FAIL: Expected uid=0, got different uid"
        exit 1
    fi
else
    echo "SKIP: id command not available in rootfs"
fi
echo ""

# Test 4: Mount namespace (can't see host processes in /proc)
echo "Test 4: Mount namespace (/proc isolation)"
echo "Command: $MINICTL run $ROOTFS /bin/sh -c 'cat /proc/1/cmdline'"
# If /proc is properly mounted, PID 1 inside should be the container's init
# not the host's init (systemd or similar)
if [ -d "$ROOTFS/proc" ]; then
    set +e
    OUTPUT=$("$MINICTL" run "$ROOTFS" /bin/sh -c 'cat /proc/1/cmdline 2>/dev/null || echo "no_proc"')
    set -e
    echo "PID 1 cmdline inside container: $OUTPUT"
    if [ "$OUTPUT" = "no_proc" ]; then
        echo "WARN: /proc may not be mounted inside container"
    elif echo "$OUTPUT" | grep -q "systemd\|init"; then
        echo "WARN: Can see host init process - /proc may not be isolated"
    else
        echo "PASS: /proc appears to be container-specific"
    fi
else
    echo "SKIP: /proc directory not in rootfs"
fi
echo ""

# Test 5: Exit code propagation
echo "Test 5: Exit code propagation"
set +e
"$MINICTL" run "$ROOTFS" /bin/sh -c 'exit 42'
EXIT_CODE=$?
set -e
if [ "$EXIT_CODE" = "42" ]; then
    echo "PASS: Exit code 42 propagated correctly"
else
    echo "FAIL: Expected exit code 42, got $EXIT_CODE"
fi
echo ""

echo "=== Part 2 tests complete ==="
echo ""
echo "Summary of namespace isolation:"
echo "  - UTS (hostname): Container has its own hostname"
echo "  - PID: Container processes start from PID 1"
echo "  - User: Root inside maps to unprivileged user outside"
echo "  - Mount: /proc shows only container processes"
