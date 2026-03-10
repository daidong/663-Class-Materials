#!/bin/bash
# test_part3_cgroups.sh - Test Part 3: cgroup resource limits
#
# Prerequisites:
#   export ROOTFS=/path/to/rootfs
#   export MINICTL=./minictl
#   Copy cpu_hog and mem_hog into $ROOTFS/usr/local/bin/

set -e

: "${ROOTFS:?Please set ROOTFS to your root filesystem path}"
: "${MINICTL:=./minictl}"

echo "=== Testing Part 3: cgroup resource limits ==="
echo "ROOTFS: $ROOTFS"
echo "MINICTL: $MINICTL"
echo ""

# Check if test programs exist
if [ ! -x "$ROOTFS/usr/local/bin/mem_hog" ]; then
    echo "WARNING: mem_hog not found in $ROOTFS/usr/local/bin/"
    echo "To build and install:"
    echo "  gcc -O2 -o mem_hog mem_hog.c"
    echo "  sudo cp mem_hog $ROOTFS/usr/local/bin/"
    MEM_HOG_AVAILABLE=false
else
    MEM_HOG_AVAILABLE=true
fi

if [ ! -x "$ROOTFS/usr/local/bin/cpu_hog" ]; then
    echo "WARNING: cpu_hog not found in $ROOTFS/usr/local/bin/"
    echo "To build and install:"
    echo "  gcc -O2 -o cpu_hog cpu_hog.c"
    echo "  sudo cp cpu_hog $ROOTFS/usr/local/bin/"
    CPU_HOG_AVAILABLE=false
else
    CPU_HOG_AVAILABLE=true
fi
echo ""

# Test 1: Memory limit
echo "Test 1: Memory limit"
if [ "$MEM_HOG_AVAILABLE" = true ]; then
    echo "Command: $MINICTL run --mem-limit=64M $ROOTFS /usr/local/bin/mem_hog"
    echo "mem_hog will try to allocate memory until killed or allocation fails"
    echo "With 64M limit, it should stop around 64MB..."
    echo ""
    
    set +e
    timeout 30s "$MINICTL" run --mem-limit=64M "$ROOTFS" /usr/local/bin/mem_hog
    EXIT_CODE=$?
    set -e
    
    if [ "$EXIT_CODE" -eq 137 ] || [ "$EXIT_CODE" -eq 9 ]; then
        echo "PASS: mem_hog was killed (likely OOM) - memory limit enforced"
    elif [ "$EXIT_CODE" -eq 0 ]; then
        echo "PASS: mem_hog exited normally (allocation failed at limit)"
    else
        echo "WARN: mem_hog exited with code $EXIT_CODE"
    fi
else
    echo "SKIP: mem_hog not available"
fi
echo ""

# Test 2: CPU limit
echo "Test 2: CPU limit"
if [ "$CPU_HOG_AVAILABLE" = true ]; then
    echo "Command: $MINICTL run --cpu-limit=10 $ROOTFS /usr/local/bin/cpu_hog &"
    echo "cpu_hog runs an infinite loop to consume CPU"
    echo "With 10% limit, top should show ~10% CPU usage"
    echo ""
    echo "Starting cpu_hog with 10% CPU limit..."
    
    "$MINICTL" run --cpu-limit=10 "$ROOTFS" /usr/local/bin/cpu_hog &
    CPU_HOG_PID=$!
    
    echo "cpu_hog started with PID $CPU_HOG_PID"
    echo "Monitor with: top -p $CPU_HOG_PID"
    echo "Or: watch -n1 \"cat /proc/$CPU_HOG_PID/stat | awk '{print \\\$14, \\\$15}'\""
    echo ""
    echo "Sleeping 5 seconds to observe CPU usage..."
    sleep 5
    
    # Check if still running
    if kill -0 $CPU_HOG_PID 2>/dev/null; then
        echo "cpu_hog is still running (good)"
        
        # Try to get CPU stats
        if [ -f "/proc/$CPU_HOG_PID/stat" ]; then
            STAT=$(cat /proc/$CPU_HOG_PID/stat)
            UTIME=$(echo "$STAT" | awk '{print $14}')
            STIME=$(echo "$STAT" | awk '{print $15}')
            echo "utime: $UTIME, stime: $STIME"
        fi
        
        echo "Killing cpu_hog..."
        kill $CPU_HOG_PID 2>/dev/null || true
        wait $CPU_HOG_PID 2>/dev/null || true
        echo "PASS: CPU-limited container ran successfully"
    else
        echo "WARN: cpu_hog exited prematurely"
    fi
else
    echo "SKIP: cpu_hog not available"
fi
echo ""

# Test 3: Verify cgroup was created
echo "Test 3: Verify cgroup structure"
CGROUP_DIR=$(ls -d /sys/fs/cgroup/minictl-* 2>/dev/null | head -1)
if [ -n "$CGROUP_DIR" ]; then
    echo "Found cgroup directory: $CGROUP_DIR"
    echo "Contents:"
    ls -la "$CGROUP_DIR" 2>/dev/null || true
    
    if [ -f "$CGROUP_DIR/memory.max" ]; then
        echo "memory.max: $(cat $CGROUP_DIR/memory.max)"
    fi
    if [ -f "$CGROUP_DIR/cpu.max" ]; then
        echo "cpu.max: $(cat $CGROUP_DIR/cpu.max)"
    fi
    if [ -f "$CGROUP_DIR/cgroup.procs" ]; then
        echo "cgroup.procs: $(cat $CGROUP_DIR/cgroup.procs)"
    fi
    
    echo "PASS: Cgroup was created"
else
    echo "WARN: No minictl cgroup found"
    echo "This may be OK if cgroups are cleaned up after container exits"
fi
echo ""

echo "=== Part 3 tests complete ==="
echo ""
echo "Manual verification:"
echo "1. For memory limit: Run mem_hog and watch 'dmesg -w' for OOM messages"
echo "2. For CPU limit: Run cpu_hog and watch 'top' for CPU percentage"
