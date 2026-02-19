# Lab 2: Tail Latency Debugging Under Memory Pressure

**Theme:** Average latency hides risk. Tail latency (p99) is where outages live.

## Learning goals

By completing this lab, you will be able to:

1. Measure tail latency correctly (p50/p90/p99) from raw samples.
2. Create a controlled memory pressure scenario using **cgroup v2**.
3. Build an evidence chain from symptom to mechanism using at least two independent signals.
4. Verify a mitigation by showing tail latency returns to baseline.

## Workload

You will use a provided CLI workload, `latcli`, that prints one line per iteration:

```
iter=123 latency_us=847
```

This makes the lab fully local and reproducible.

## Prerequisites

Inside your Ubuntu VM:

```bash
sudo apt update
sudo apt install -y build-essential python3 linux-tools-common linux-tools-$(uname -r)
```

You also need permission to use `perf`.

## Setup

Build the workload:

```bash
make -C week2B/lab2_latcli
```

## Part 0: Baseline measurement (no memory pressure)

Run a baseline workload and save raw output:

```bash
./week2B/lab2_latcli/latcli --iters 20000 --workset-mb 256 --touch-per-iter 1024 > baseline.log
```

Convert to CSV and compute percentiles:

```bash
python3 week2B/lab2_latcli/scripts/latency_to_csv.py baseline.log baseline.csv
python3 week2B/lab2_latcli/scripts/percentiles.py baseline.csv
```

Record p50/p90/p99 in your report.

## Part 1: Apply memory pressure with a cgroup limit

### Create a cgroup (v2)

```bash
sudo mkdir -p /sys/fs/cgroup/lab2
printf "%d" $((512*1024*1024)) | sudo tee /sys/fs/cgroup/lab2/memory.max
```

### Run inside the cgroup

```bash
sudo bash week2B/lab2_latcli/scripts/run_in_cgroup.sh \
  /sys/fs/cgroup/lab2 \
  ./week2B/lab2_latcli/latcli --iters 20000 --workset-mb 256 --touch-per-iter 1024 > pressured.log
```

Convert and compute percentiles:

```bash
python3 week2B/lab2_latcli/scripts/latency_to_csv.py pressured.log pressured.csv
python3 week2B/lab2_latcli/scripts/percentiles.py pressured.csv
```

### If you do not see tail inflation

Increase pressure gradually:

- Reduce the cgroup limit: 768 MiB → 512 MiB → 384 MiB
- Increase `--workset-mb` step-by-step (256 → 320 → 384)
- Increase `--touch-per-iter` (touch more pages per iteration)

If the program gets OOM-killed, you pushed too far. Back off and try a slightly larger limit.

## Part 2: Collect paging and fault evidence

You must show at least **two independent signals**.

### Signal A (required): tail latency percentiles

Report p50/p90/p99 for baseline vs pressured.

### Signal B: perf fault counters

Run `perf stat` and include the relevant counters:

```bash
sudo perf stat -e page-faults,major-faults \
  ./week2B/lab2_latcli/latcli --iters 20000 --workset-mb 256 > /dev/null
```

To measure under memory pressure, combine with the cgroup runner:

```bash
sudo bash week2B/lab2_latcli/scripts/run_in_cgroup.sh \
  /sys/fs/cgroup/lab2 \
  perf stat -e page-faults,major-faults \
  ./week2B/lab2_latcli/latcli --iters 20000 --workset-mb 256 > /dev/null
```

### Signal C: /proc/vmstat

Capture a snapshot:

```bash
grep -E 'pgfault|pgmajfault' /proc/vmstat
```

## Part 3: Fix and verify

Pick one mitigation:

- Increase `memory.max` (more headroom), or
- Reduce `--workset-mb` (smaller working set)

Then rerun and show p99 returns toward baseline.

## Submission

Submit three files:

1. `report.md` (use the provided template)
2. `results.csv` (one row per iteration, or two CSVs)
3. `use_checklist.md` (filled)
