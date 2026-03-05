# SchedLab: eBPF-Based Scheduler Observation

This directory contains starter code for Lab 3: observing Linux scheduler behavior using eBPF.

## Overview

**Note (modern kernels):** Linux kernel ≥ 6.6 uses **EEVDF** inside the CFS scheduling class for normal tasks. This lab still works unchanged because it relies on stable scheduler **tracepoints** (`sched_wakeup`, `sched_switch`), not internal function names.

SchedLab attaches to kernel scheduler tracepoints to capture:
- **Scheduling latency**: Time from `sched_wakeup` to `sched_switch`
- **Run time**: Time a task spends actually running
- **Wait time**: Time a task spends waiting in the run queue
- **Context switches**: How often the scheduler switches between tasks

## Directory Structure

```
schedlab/
├── README.md           # This file
├── Makefile            # Build instructions
├── schedlab.bpf.c      # BPF program (runs in kernel)
├── schedlab.h          # Shared definitions
├── schedlab_user.c     # User-space program (reads events)
├── vmlinux.h           # Kernel type definitions (generated)
└── scripts/
    ├── gen_vmlinux.sh  # Generate vmlinux.h
    └── analyze.py      # Post-process CSV data
```

## Prerequisites

**Kernel Requirements:**
- Linux kernel 5.15+ with BTF enabled
- Ubuntu 22.04 recommended

**Packages Required:**
```bash
sudo apt update
sudo apt install -y clang llvm libbpf-dev bpftool linux-headers-$(uname -r)
sudo apt install -y stress-ng  # For workload generation
```

**Verify Environment:**
```bash
# Check BTF is available
ls /sys/kernel/btf/vmlinux
# Should exist and not be empty

# Check bpftool works
sudo bpftool version
```

## Quick Start

```bash
# 1. Generate vmlinux.h (one-time, or after kernel update)
./scripts/gen_vmlinux.sh

# 2. Build
make clean && make

# 3. Run basic test
sudo ./schedlab --help
sudo ./schedlab --mode stream --duration 5

# 4. Run latency measurement
sudo ./schedlab --mode latency --duration 10 --output latency.csv
```

## Usage

```
Usage: schedlab [OPTIONS]

Modes:
  --mode stream     Print all scheduler events (debugging)
  --mode latency    Measure scheduling latency distribution
  --mode fairness   Measure per-task run/wait time

Options:
  --duration N      Run for N seconds (default: 10)
  --output FILE     Write results to CSV file
  --pid PID         Filter events for specific PID
  --verbose         Show detailed output
```

## Tasks for Lab 3

### Task 1: Understanding the Event Model

Read the source code and answer:
1. What tracepoints does SchedLab attach to?
2. What data is captured at each tracepoint?
3. How does data flow from kernel to user space?

### Task 2: Scheduling Latency Distribution

Run latency measurement under idle and loaded conditions:

```bash
# Idle system
sudo ./schedlab --mode latency --duration 20 --output latency_idle.csv

# Loaded system (in another terminal, run stress)
stress-ng --cpu 4 --timeout 30s &
sudo ./schedlab --mode latency --duration 20 --output latency_loaded.csv
```

Compute p50, p90, p99 percentiles. Why does p99 differ under load?

### Task 3: Fairness Study

Measure per-task run time and wait time:

```bash
# Run with multiple processes
stress-ng --cpu 2 --timeout 30s &
sudo ./schedlab --mode fairness --duration 20 --output fairness.csv
```

Analyze: Are CPU-bound tasks getting equal time? What about I/O-bound tasks?

## Troubleshooting

### "Operation not permitted"
```bash
# eBPF requires root
sudo ./schedlab ...
```

### "BTF not found"
Your kernel may not have BTF enabled. Check:
```bash
cat /boot/config-$(uname -r) | grep BTF
# Should show CONFIG_DEBUG_INFO_BTF=y
```

### Build errors
```bash
# Make sure vmlinux.h exists
ls vmlinux.h
# If not, generate it:
./scripts/gen_vmlinux.sh
```

### Events not showing
```bash
# Check if tracepoints exist
sudo bpftrace -l 'tracepoint:sched:*'
# Should list sched_switch, sched_wakeup, etc.
```

## Understanding the Code

### schedlab.bpf.c

Key sections:

```c
// Tracepoint for when a task becomes runnable
SEC("tp_btf/sched_wakeup")
int handle_sched_wakeup(u64 *ctx) {
    // Record wakeup timestamp in a map
    // Key = PID, Value = timestamp
}

// Tracepoint for context switch
SEC("tp_btf/sched_switch")
int handle_sched_switch(u64 *ctx) {
    // prev = task being switched out
    // next = task being switched in
    
    // If next was previously woken up, compute latency
    // latency = now - wakeup_time
}
```

### Data Flow

```
1. Kernel event (e.g., task wakes up)
        │
        ▼
2. BPF program executes (handle_sched_wakeup)
        │
        ▼
3. Data stored in BPF map or ring buffer
        │
        ▼
4. User-space program reads via bpf() syscall
        │
        ▼
5. Output to CSV or screen
```

## References

- [BPF and XDP Reference Guide](https://docs.cilium.io/en/stable/bpf/)
- [libbpf Documentation](https://libbpf.readthedocs.io/)
- [bpftrace Reference](https://github.com/iovisor/bpftrace/blob/master/docs/reference_guide.md)
- [Brendan Gregg's eBPF Page](https://www.brendangregg.com/ebpf.html)
