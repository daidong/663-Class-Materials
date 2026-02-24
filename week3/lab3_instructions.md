# Lab 3: Scheduling Latency Under CPU Contention

**Theme:** Tail latency is often scheduling latency.

## Learning goals

By completing this lab, you will be able to:

1. Measure a latency distribution from raw samples (p50/p90/p99).
2. Explain scheduling latency as runnable-queue delay.
3. Create controlled CPU contention and observe tail inflation.
4. Apply a mitigation (nice / CPU affinity / optional cgroup) and verify improvement.

## Prerequisites

Ubuntu VM (or Linux host):

```bash
sudo apt update
sudo apt install -y build-essential python3
# Optional (if permitted in your environment)
sudo apt install -y linux-tools-common linux-tools-$(uname -r)
```

## Build

```bash
make -C week3/lab3_sched_latency
```

## Part 0: Baseline

Run the wakeup-latency probe (pin it to one CPU for reproducibility):

```bash
./week3/lab3_sched_latency/wakeup_lat --iters 20000 --period-us 1000 --cpu 0 > baseline.log
python3 week3/lab3_sched_latency/scripts/latency_to_csv.py baseline.log baseline.csv
python3 week3/lab3_sched_latency/scripts/percentiles.py baseline.csv
```

Record p50/p90/p99 in your report.

## Part 1: Add CPU contention

In another terminal, start CPU hogs **on the same CPU**:

```bash
./week3/lab3_sched_latency/cpu_hog --threads 4 --cpu 0
```

Rerun the probe:

```bash
./week3/lab3_sched_latency/wakeup_lat --iters 20000 --period-us 1000 --cpu 0 > contended.log
python3 week3/lab3_sched_latency/scripts/latency_to_csv.py contended.log contended.csv
python3 week3/lab3_sched_latency/scripts/percentiles.py contended.csv
```

Expected: p99 should increase (often dramatically).

## Part 2: Collect at least one supporting OS signal

Choose at least one.

### Option A (preferred): perf stat

```bash
perf stat -e context-switches,cpu-migrations \
  ./week3/lab3_sched_latency/wakeup_lat --iters 20000 --period-us 1000 --cpu 0 > /dev/null
```

If `perf` requires sudo in your VM, use `sudo perf ...`.

### Option B: /proc scheduling stats

Capture a snapshot while the probe runs:

```bash
PID=$(pgrep -n wakeup_lat)
cat /proc/$PID/sched | sed -n '1,80p'
```

(You can also inspect `/proc/schedstat`.)

## Part 3: Apply one mitigation and verify

Pick **one** mitigation.

### Mitigation 1: nice the background hogs

Stop hogs, restart with low priority:

```bash
nice -n 19 ./week3/lab3_sched_latency/cpu_hog --threads 4 --cpu 0
```

Rerun the probe; compare p99.

### Mitigation 2: isolate with CPU affinity

Run the probe on CPU 1 while hogs stay on CPU 0:

```bash
./week3/lab3_sched_latency/wakeup_lat --iters 20000 --period-us 1000 --cpu 1 > isolated.log
```

Compare p99 vs contended.

### Optional advanced: cgroup CPU control

If your environment supports cgroup v2, put hogs in a limited cgroup and show the probe tail improves.

## Submission

Submit:

1. `report.md` (use template)
2. `baseline.csv` and `contended.csv` (+ optional `mitigated.csv`)
3. Command lines used (copy/paste)

## Grading rubric (suggested)

- Correct baseline + contended percentiles (30%)
- Supporting OS signal + interpretation (25%)
- One mitigation + before/after evidence (25%)
- Mechanism explanation quality (20%)
