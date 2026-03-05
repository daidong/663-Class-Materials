# Lab 3: SchedLab — eBPF Scheduler Observation

> **Goal:** Use eBPF to observe Linux scheduler behavior in real-time, measure scheduling latency, and analyze fairness.

## Overview

This lab uses SchedLab, an eBPF-based tool that attaches to scheduler tracepoints to capture:
- Wake-to-switch latency (scheduling latency)
- Per-task run time and wait time
- Context switch patterns

You'll complete Tasks 1-3 from the full SchedLab assignment.

## Prerequisites

- Ubuntu VM with BTF-enabled kernel (5.15+ recommended)
- eBPF tools installed (clang, libbpf-dev, bpftool)
- `stress-ng` for workload generation

### Verify Your Environment

```bash
# Grant sudo privileges to the entire experiment
sudo -v

# Check BTF is available
ls /sys/kernel/btf/vmlinux
# Should exist

# Check bpftool
sudo bpftool version
# Should show version info

# Check stress-ng
stress-ng --version
# If not installed: sudo apt install stress-ng

# Check the libraries installation in Python
pip3 install pandas numpy matplotlib
```

## Getting Started

### Step 1: Access SchedLab Code

The SchedLab code should be in `schedlab/` or `assignment_2/` directory.

```bash
cd schedlab
ls -la
# Should see: schedlab.bpf.c, schedlab_user.c, Makefile, etc.
```

### Step 2: Build

```bash
make clean
make
```

If build fails, check:
- clang/llvm installed: `clang --version`
- libbpf-dev installed: `dpkg -l | grep libbpf`
- bpftool can generate skeleton: `sudo bpftool btf dump file /sys/kernel/btf/vmlinux format c > vmlinux.h`

### Step 3: Test Basic Functionality

```bash
# Run in stream mode (shows all events, limited to 3 seconds)
sudo timeout 3s ./schedlab --mode stream
```

You should see event output. If you get permission errors, make sure you're using `sudo`.

---

## Task 1: Understanding the Event Model (10 min)

### Objective

Read the code and understand how events flow from kernel to user space.

### Files to Examine

1. **`schedlab.bpf.c`** — The kernel-side BPF program
   - Look for `SEC("tp_btf/...")` macros — these attach to tracepoints
   - Look for `bpf_ringbuf_submit()` — this sends events to user space

2. **`schedlab_user.c`** — The user-side program
   - Look for `ring_buffer__new()` — creates ring buffer consumer
   - Look for event handling callbacks

### Questions to Answer

1. What tracepoints does SchedLab attach to?
2. What data is captured at each tracepoint?
3. How does data flow from tracepoint to user space?
4. What BPF maps are used and what do they store?

### Deliverable

Write a short summary (1/2 page) explaining:
- The tracepoints used and what they capture
- How events flow from kernel to user space
- What data is stored in BPF maps

---

## Task 2: Scheduling Latency Distribution (35 min)

### Goal

Measure the distribution of **wake→switch latency** (how long a task waits after becoming runnable).

### Step 1: Collect Baseline (Idle System)

We use a **probe process** — a lightweight Python loop that sleeps and wakes 1000 times/sec — and track only its scheduling latency with `--pid`. This gives us a consistent measurement target across both conditions.

```bash
# Start probe process (sleeps 1ms in a loop)
python3 -c "
import time, os
print(f'Probe PID: {os.getpid()}')
while True: time.sleep(0.001)
" &
PROBE_PID=$!

# Measure only the probe's scheduling latency on an idle system
sudo ./schedlab --mode latency --duration 15 --pid $PROBE_PID --output latency_idle.csv
kill $PROBE_PID
```

### Step 2: Collect Under Load

```bash
# Start the same probe
python3 -c "
import time, os
print(f'Probe PID: {os.getpid()}')
while True: time.sleep(0.001)
" &
PROBE_PID=$!

# Start CPU load in background
stress-ng --cpu 8 --timeout 20s &

# Measure the probe's latency under load
sudo ./schedlab --mode latency --duration 15 --pid $PROBE_PID --output latency_loaded.csv
kill $PROBE_PID
```

### Step 3: Analyze the Data

Use Python, R, or any tool to compute:

```python
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt

# Load histogram data (columns: bucket_us, count)
df = pd.read_csv('latency_idle.csv')

# Compute percentiles from histogram
total = df['count'].sum()
df['cumulative'] = df['count'].cumsum()

p50_row = df[df['cumulative'] >= total * 0.50].iloc[0]
p90_row = df[df['cumulative'] >= total * 0.90].iloc[0]
p99_row = df[df['cumulative'] >= total * 0.99].iloc[0]

p50 = p50_row['bucket_us'] / 1000  # us to ms
p90 = p90_row['bucket_us'] / 1000
p99 = p99_row['bucket_us'] / 1000

print(f"Total samples: {total}")
print(f"p50: {p50:.3f} ms")
print(f"p90: {p90:.3f} ms")
print(f"p99: {p99:.3f} ms")

# Plot histogram
plt.figure(figsize=(10, 6))
plt.bar(df['bucket_us'] / 1000, df['count'], width=0.001, edgecolor='black')
plt.xlabel('Scheduling Latency (ms)')
plt.ylabel('Count')
plt.title('Scheduling Latency Distribution')
plt.axvline(p50, color='green', linestyle='--', label=f'p50: {p50:.3f}ms')
plt.axvline(p99, color='red', linestyle='--', label=f'p99: {p99:.3f}ms')
plt.legend()
plt.savefig('latency_histogram.png')
plt.show()
```

### Step 4: Compare Idle vs. Loaded

| Metric | Idle | Loaded | Ratio |
|--------|------|--------|-------|
| p50 | ? ms | ? ms | ? |
| p90 | ? ms | ? ms | ? |
| p99 | ? ms | ? ms | ? |

### Deliverable

- `latency_idle.csv` and `latency_loaded.csv`
- p50, p90, p99 values for both
- **TWO** Histogram plots
- 1-2 paragraphs: What affects tail latency? Why does p99 differ from p50?

---

## Task 3: Fairness Study (35 min)

### Goal

Compare **per-task run time** vs **wait time** to assess fairness.

### Step 1: Collect Fairness Data

```bash
stress-ng --cpu 1 --fork 5 --timeout 25s &
sudo ./schedlab --mode fairness --duration 20 --output fairness.csv
```

### Step 2: Understand the Data

CSV columns:
- `pid`: Process ID
- `run_time_ms`: Total run time in milliseconds
- `wait_time_ms`: Total wait time in milliseconds
- `switches`: Number of context switches
- `wakeups`: Number of wakeups

### Step 3: Analyze Fairness

```python
import pandas as pd
import matplotlib.pyplot as plt

# Load data (columns: pid, run_time_ms, wait_time_ms, switches, wakeups)
df = pd.read_csv('fairness.csv')

# Filter to significant PIDs (ran at least some time)
active = df[df['run_time_ms'] > 10].nlargest(10, 'run_time_ms')

# Calculate CPU share
active['cpu_share'] = active['run_time_ms'] / (active['run_time_ms'] + active['wait_time_ms'])

# Plot bar chart
fig, axes = plt.subplots(1, 3, figsize=(15, 5))

axes[0].bar(active['pid'].astype(str), active['run_time_ms'])
axes[0].set_xlabel('PID')
axes[0].set_ylabel('Run Time (ms)')
axes[0].set_title('Run Time per PID')
axes[0].tick_params(axis='x', rotation=45)

axes[1].bar(active['pid'].astype(str), active['wait_time_ms'])
axes[1].set_xlabel('PID')
axes[1].set_ylabel('Wait Time (ms)')
axes[1].set_title('Wait Time per PID')
axes[1].tick_params(axis='x', rotation=45)

axes[2].bar(active['pid'].astype(str), active['cpu_share'])
axes[2].set_xlabel('PID')
axes[2].set_ylabel('CPU Share')
axes[2].set_title('CPU Share per PID')
axes[2].tick_params(axis='x', rotation=45)
axes[2].axhline(0.5, color='red', linestyle='--', alpha=0.5)

plt.tight_layout()
plt.savefig('fairness_analysis.png')
plt.show()

# Print summary
print("\nTop 10 PIDs by run time:")
print(active[['pid', 'run_time_ms', 'wait_time_ms', 'cpu_share', 'switches']])
```

### Step 4: Compare Different Workloads

Run with more CPU workers:

```bash
stress-ng --cpu 4 --timeout 25s &
sudo ./schedlab --mode fairness --duration 20 --output fairness_heavy.csv
```

Compare:
- Does fairness change with more contention?
- Are any tasks "left behind"?

### Deliverable

- `fairness.csv` (and optionally `fairness_heavy.csv`)
- Bar chart of top 10 PIDs showing run_ms, wait_ms, and CPU share
- 1-2 paragraphs: Is CFS achieving fairness? Any tasks starved?

---

## Lab Report Structure

Submit `lab3_report.md` (or PDF) with:

### 1. Environment
- Ubuntu version, kernel version
- Number of CPUs
- Any setup issues encountered

### 2. Task 1: Event Model Summary (1/2 page)
- Tracepoints used
- Data flow explanation
- BPF maps description

### 3. Task 2: Latency Analysis
- CSVs (attached or linked)
- Percentile values table
- Histogram plot
- Comparison: idle vs. loaded
- Explanation of tail latency causes

### 4. Task 3: Fairness Analysis
- CSV (attached or linked)
- Bar chart
- CPU share calculations
- Explanation of fairness observations

### 5. Reflection (1/2 page)
- What surprised you?
- How would you use this in your final project?
- What would you measure next?

---

## Grading Rubric

| Criterion | Points |
|-----------|--------|
| Task 1: Event model explained correctly | 15 |
| Task 2: Data collected and percentiles computed | 20 |
| Task 2: Histogram and comparison | 15 |
| Task 2: Explanation of tail latency | 15 |
| Task 3: Fairness data and visualization | 15 |
| Task 3: Fairness analysis | 10 |
| Report clarity and completeness | 10 |

---

## Common Issues

### "Permission denied"
```bash
sudo ./schedlab ...
# Always use sudo for BPF operations
```

### "BTF not found"
Your kernel may not have BTF. Check:
```bash
ls /sys/kernel/btf/vmlinux
# If not present, you may need a different kernel or to enable BTF
```

### "libbpf error: loading program"
- Check kernel version (5.15+ recommended)
- Check BPF verifier log for hints
- Try simpler mode first: `--mode stream`

### Large CSV files
- Limit recording time (20-30 seconds max)
- Filter by PID if needed: `--pid <PID>`

---

## Timeline

- **Lab session**: Complete setup, run data collection, start analysis
- **After class**: Complete analysis, write report
- **Due**: Before Week 5 class
