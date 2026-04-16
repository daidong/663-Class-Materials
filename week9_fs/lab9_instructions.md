# Lab 9: Anatomy of a File Write

> **Goal:** Trace the full path of a file write — from VFS through page cache to disk — using observability tools and controlled experiments.

## Overview

In this lab, you will:
1. Inspect a file's on-disk layout using `filefrag` and `debugfs`
2. Observe page cache and dirty page behavior in real time
3. Measure write latency with and without `fsync`
4. Create interference experiments and measure tail latency

**Estimated time:** 90 minutes

---

## Prerequisites

### Cache sudo credentials (recommended)

This lab uses `sudo` frequently. Run this once up front so later steps don’t keep prompting for a password:

```bash
sudo -v
```

### System Requirements

- Ubuntu VM (VirtualBox)
- ext4 filesystem (default on Ubuntu)
- Root access (`sudo`)

### VirtualBox Storage Note

VirtualBox uses a virtual disk (VDI/VMDK) that may cache writes at the host level. This can make `fsync` appear faster than it would on real hardware. Expected observations:

- Buffered vs fsync difference may be **10–100×** instead of 1000×
- This is still valid — the mechanism is the same, just the magnitude changes because the "disk" is actually host RAM/SSD
- If fsync latency is < 0.01 ms, check that your test file is NOT on `tmpfs`: `df -T /tmp`

For `filefrag`: the extent layout reflects the virtual disk's internal layout, which is still meaningful for understanding ext4 allocation — it just doesn't map to physical platters.

### Verify Your Environment

```bash
# Check filesystem type — must be ext4 for this lab
df -T /
# Expected: /dev/sdaX  ext4  ...

# Check tools are available
which filefrag   # Should exist on Ubuntu
iostat -V        # Install if missing: sudo apt install sysstat
strace --version # Should exist

# Check you can read /proc/meminfo
grep Dirty /proc/meminfo

# Check available disk space (need ~1 GB free)
df -h /tmp
```

### Directory Setup

```bash
mkdir -p ~/lab9/{data,results}
cd ~/lab9
```

---

## Part 1: File Layout on Disk (20 min)

In this part, you inspect how ext4 physically stores files. This connects the VFS/inode concepts from the lecture to real on-disk structures.

### 1.1 Create Test Files

```bash
cd ~/lab9/data

# A sequential file (should be contiguous)
dd if=/dev/zero of=sequential.dat bs=1M count=10

# A file written in small random chunks
python3 -c "
import os, random
fd = os.open('fragmented.dat', os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o644)
for _ in range(2560):
    os.lseek(fd, random.randint(0, 10*1024*1024 - 4096), os.SEEK_SET)
    os.write(fd, b'X' * 4096)
os.close(fd)
"
```

### 1.2 Inspect Physical Layout with `filefrag`

```bash
# Sequential file — expect few extents
filefrag -v sequential.dat

# Fragmented file — expect many extents
filefrag -v fragmented.dat
```

**Record in your report:**
- How many extents does each file have?
- What is the extent length for the sequential file?
- What does this tell you about ext4's allocation strategy?

### 1.3 Inspect Inode with `debugfs`

Find the device your filesystem is on:
```bash
df / | tail -1 | awk '{print $1}'
# e.g., /dev/sda1
```

<!-- ```bash
# Get inode number
ls -i ~/lab9/data/sequential.dat
# e.g., 131073

# Inspect inode (read-only, safe on mounted filesystem)
sudo debugfs -R "stat <INODE_NUMBER>" /dev/sdXN
```

Replace `<INODE_NUMBER>` and `/dev/sdXN` with your actual values. -->
**Quick one-liner** (does both lookups for you):
```bash
DEV=$(df / | tail -1 | awk '{print $1}')
INO=$(ls -i ~/lab9/data/sequential.dat | awk '{print $1}')
sudo debugfs -R "stat <${INO}>" ${DEV}
```

**Record in your report:**
- What fields does the inode contain?
- How large is the extent tree?
- What is the block count vs file size?

### 1.4 Hard Links and Inodes

```bash
# Create a hard link
ln ~/lab9/data/sequential.dat ~/lab9/data/seq_link.dat

# Check inode numbers — they should be the same
ls -i ~/lab9/data/sequential.dat ~/lab9/data/seq_link.dat

# Check link count
stat ~/lab9/data/sequential.dat | grep Links
```

**Question for report:** Why does the filename not appear in the inode?

---

## Part 2: Observing the Page Cache (20 min)

### 2.1 Watch Dirty Pages in Real Time

Open two terminals.

**Terminal 1 — Monitor dirty pages:**
```bash
watch -n 0.5 'grep -E "Dirty|Writeback|MemFree|Cached" /proc/meminfo'
```

**Terminal 2 — Generate writes:**
```bash
# This creates dirty pages without fsync
for i in $(seq 1 200); do
  dd if=/dev/zero of=/tmp/dirty_test bs=1M count=1 oflag=append conv=notrunc status=none
  sleep 0.05
done

# Watch Terminal 1: Dirty should climb, then fall as writeback kicks in
```

**Record in your report:**
- What was the peak `Dirty` value?
- How long before `Dirty` started dropping (writeback began)?
- What was the `Dirty` value after `dd` finished?

### 2.2 Force Writeback

```bash
# Trigger immediate writeback
sync

# Check dirty pages — should drop to near zero
grep Dirty /proc/meminfo
```

### 2.3 Drop Page Cache

```bash
# Record current memory usage (baseline)
free -h

# First sync to avoid data loss
sync

# Drop page cache (clean pages only)
sudo sysctl vm.drop_caches=3

# Check how much memory was freed (compare to baseline)
free -h
```

### 2.4 Demonstrate Cache Effect on Read

```bash
# Drop caches
sync && sudo sysctl vm.drop_caches=3

# First read (cold cache) — measure time
time cat ~/lab9/data/sequential.dat > /dev/null

# Second read (warm cache) — measure time
time cat ~/lab9/data/sequential.dat > /dev/null
```

**Record in your report:** What is the speedup factor from cold to warm cache?

---

## Part 3: Write Latency Measurement (30 min)

### 3.1 Build the Measurement Tool

Create `write_latency.c` in the folder `lab9`:

```c
/*
 * write_latency.c — Measure per-write latency with optional fsync
 *
 * Usage: ./write_latency [options] <output_file>
 *   -n <count>   Number of writes (default: 10000)
 *   -s <size>    Write size in bytes (default: 4096)
 *   -f           Enable fsync after each write
 *   -d           Enable fdatasync after each write
 *   -c <csv>     Write latencies to CSV file
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <getopt.h>

#define DEFAULT_COUNT 10000
#define DEFAULT_SIZE  4096

static long long ts_diff_ns(struct timespec *a, struct timespec *b) {
    return (b->tv_sec - a->tv_sec) * 1000000000LL + (b->tv_nsec - a->tv_nsec);
}

static int cmp_ll(const void *a, const void *b) {
    long long la = *(const long long *)a, lb = *(const long long *)b;
    return (la > lb) - (la < lb);
}

int main(int argc, char *argv[]) {
    int count = DEFAULT_COUNT, size = DEFAULT_SIZE;
    int use_fsync = 0, use_fdatasync = 0;
    const char *csv_file = NULL;
    int opt;

    while ((opt = getopt(argc, argv, "n:s:fdc:h")) != -1) {
        switch (opt) {
        case 'n': count = atoi(optarg); break;
        case 's': size = atoi(optarg); break;
        case 'f': use_fsync = 1; break;
        case 'd': use_fdatasync = 1; break;
        case 'c': csv_file = optarg; break;
        default:
            fprintf(stderr, "Usage: %s [-n count] [-s size] [-f] [-d] [-c csv] <file>\n", argv[0]);
            return 1;
        }
    }
    if (optind >= argc) { fprintf(stderr, "Error: output file required\n"); return 1; }

    const char *path = argv[optind];
    char *buf = malloc(size);
    memset(buf, 'A', size);

    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) { perror("open"); return 1; }

    long long *lat = malloc(count * sizeof(long long));
    struct timespec t0, t1;

    printf("Config: %d writes × %d bytes, fsync=%s fdatasync=%s\n",
           count, size, use_fsync ? "yes" : "no", use_fdatasync ? "yes" : "no");

    for (int i = 0; i < count; i++) {
        clock_gettime(CLOCK_MONOTONIC, &t0);
        if (write(fd, buf, size) != size) { perror("write"); break; }
        if (use_fsync) fsync(fd);
        else if (use_fdatasync) fdatasync(fd);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        lat[i] = ts_diff_ns(&t0, &t1);
    }
    close(fd);

    /* Statistics */
    long long sum = 0, mn = lat[0], mx = lat[0];
    for (int i = 0; i < count; i++) {
        sum += lat[i];
        if (lat[i] < mn) mn = lat[i];
        if (lat[i] > mx) mx = lat[i];
    }
    qsort(lat, count, sizeof(long long), cmp_ll);

    printf("\nResults (%d writes):\n", count);
    printf("  min:   %8lld ns  (%.3f ms)\n", mn, mn/1e6);
    printf("  p50:   %8lld ns  (%.3f ms)\n", lat[count/2], lat[count/2]/1e6);
    printf("  p90:   %8lld ns  (%.3f ms)\n", lat[(int)(count*0.9)], lat[(int)(count*0.9)]/1e6);
    printf("  p99:   %8lld ns  (%.3f ms)\n", lat[(int)(count*0.99)], lat[(int)(count*0.99)]/1e6);
    printf("  max:   %8lld ns  (%.3f ms)\n", mx, mx/1e6);
    printf("  avg:   %8.0f ns  (%.3f ms)\n", (double)sum/count, (double)sum/count/1e6);

    if (csv_file) {
        FILE *fp = fopen(csv_file, "w");
        fprintf(fp, "index,latency_ns\n");
        for (int i = 0; i < count; i++) fprintf(fp, "%d,%lld\n", i, lat[i]);
        fclose(fp);
        printf("CSV written to %s\n", csv_file);
    }

    unlink(path);  /* Deletes the test file — this is intentional for /tmp cleanup */
    free(lat); free(buf);
    return 0;
}
```

```bash
gcc -O2 -o write_latency write_latency.c
```

> **Note:** The program calls `unlink()` at the end to delete the test file (`/tmp/testfile`). This is intentional — the measurement data is captured in the CSV files under `results/`. If you want to inspect the test file itself, comment out the `unlink(path)` line and recompile.

### 3.2 Baseline: Buffered Writes (No fsync)

```bash
sync && sudo sysctl vm.drop_caches=3

./write_latency -n 10000 -c results/baseline_buffered.csv /tmp/testfile
```

### 3.3 Baseline: With fsync

```bash
sync && sudo sysctl vm.drop_caches=3

./write_latency -n 10000 -f -c results/baseline_fsync.csv /tmp/testfile
```

### 3.4 Baseline: With fdatasync

```bash
sync && sudo sysctl vm.drop_caches=3

./write_latency -n 10000 -d -c results/baseline_fdatasync.csv /tmp/testfile
```

### 3.5 Record Results

| Configuration | p50 | p90 | p99 | max |
|---------------|-----|-----|-----|-----|
| Buffered (no fsync) | | | | |
| fsync | | | | |
| fdatasync | | | | |

**Question for report:** Why is buffered write much faster than fsync? (On real hardware this gap is ~1000×; in VirtualBox it may be 10–100× due to host-level caching.) Trace the exact steps that differ in the kernel path.

---

## Part 4: Interference Experiments (20 min)

Pick **at least two** of the following interference experiments. Run each while measuring fsync latency in a separate terminal.

### 4A: Background Sequential I/O

```bash
# Terminal 1: Start background writer
dd if=/dev/zero of=/tmp/bg_write bs=1M count=500 conv=fdatasync &
BG_PID=$!

# Terminal 2: Measure while background I/O is running
./write_latency -n 10000 -f -c results/interf_bg_io.csv /tmp/testfile

# Terminal 1: Clean up
kill $BG_PID 2>/dev/null; rm -f /tmp/bg_write
```

### 4B: Memory Pressure (Shrink Page Cache)

```bash
# Terminal 1: Continuously pressure memory
python3 -c "
import time
blocks = []
try:
    while True:
        blocks.append(b'X' * (50 * 1024 * 1024))  # 50 MB chunks
        time.sleep(0.5)
except MemoryError:
    time.sleep(999)  # Hold the memory until killed
" &
MEM_PID=$!

# Terminal 2: Measure
./write_latency -n 5000 -f -c results/interf_memory.csv /tmp/testfile

# Terminal 1: Kill memory pressure
kill $MEM_PID 2>/dev/null
```

### 4C: Concurrent fsync from Other Processes

```bash
# Terminal 1: Run 4 concurrent fsync writers
PIDS=()
for i in 1 2 3 4; do
  bash -c "
    while true; do
      dd if=/dev/zero of=/tmp/concurrent_\$1 bs=4096 count=100 conv=fsync 2>/dev/null
    done
  " -- $i &
  PIDS+=($!)
done

# Terminal 2: Measure
./write_latency -n 10000 -f -c results/interf_concurrent.csv /tmp/testfile

# Terminal 1: Clean up (kills only our writers, not other dd processes)
for pid in "${PIDS[@]}"; do kill $pid 2>/dev/null; done
rm -f /tmp/concurrent_*
```

### 4D: Periodic Cache Drops (Simulating Cold-Cache Bursts)

```bash
# Terminal 1: Drop caches every 2 seconds
while true; do sync; sudo sysctl vm.drop_caches=3; sleep 2; done &
DROP_PID=$!

# Terminal 2: Measure
./write_latency -n 5000 -f -c results/interf_cold.csv /tmp/testfile

# Terminal 1: Stop
kill $DROP_PID 2>/dev/null
```

### Record Interference Results

| Experiment | p50 | p90 | p99 | max | vs Baseline p99 |
|------------|-----|-----|-----|-----|-----------------|
| Baseline (fsync) | | | | | 1× |
| Background I/O | | | | | ×? |
| Memory pressure | | | | | ×? |
| Concurrent fsync | | | | | ×? |
| Cold cache | | | | | ×? |

---

## Part 5: System-Level Observation (Optional, Bonus)

Use `iostat` or `strace` to correlate your latency measurements with system events.

### Option A: iostat

```bash
# Terminal 1: Watch disk stats every second
DEV=$(df / | tail -1 | awk '{print $1}')          # e.g., /dev/sda2
DISK=$(echo "${DEV}" | sed -E 's#[0-9]+$##')      # e.g., /dev/sda
iostat -x 1 -p "${DISK}"

# Terminal 2: Run an interference experiment and observe
# await and %util columns in Terminal 1
```

Record the `await` and `%util` values during your experiment.

### Option B: strace

```bash
# Trace fsync latency directly
strace -T -e fsync ./write_latency -n 100 -f /tmp/testfile 2>&1 | tail -20
```

The `-T` flag shows time spent in each syscall. Look for outlier fsync durations.

---

## Deliverables

Submit a `lab9/` directory containing:

```
lab9/
├── write_latency.c          # Your measurement program
├── results/
│   ├── baseline_buffered.csv
│   ├── baseline_fsync.csv
│   ├── baseline_fdatasync.csv
│   └── interf_*.csv          # At least 2 interference experiments
└── lab9_report.md            # Use the provided template
```

---

## Grading Rubric

| Criterion | Points |
|-----------|--------|
| Part 1: File layout inspection complete (filefrag + debugfs) | 15 |
| Part 2: Page cache observations recorded | 15 |
| Part 3: Baseline measurements (3 configurations) | 20 |
| Part 4: At least 2 interference experiments with data | 25 |
| Report: Analysis depth and mechanism explanation | 25 |
| **Total** | **100** |

---

## Common Issues

### `filefrag: command not found`

```bash
sudo apt install e2fsprogs
```

### `debugfs` shows nothing useful

Make sure you use the correct device (not a partition, not a loop device). Check with `df /`.

### Very low fsync latency (~0.01 ms)

Your filesystem may be on a RAM-backed device (e.g., tmpfs). Verify with `df -T /tmp` — use a real disk path.

### Results vary wildly between runs

Expected! Storage latency is inherently variable. Run each experiment at least twice and report the range.
