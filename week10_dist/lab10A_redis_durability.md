# Lab 10A: From fsync to Redis — Observing the Cost of Durability

---

## Prerequisite Lectures

This lab builds directly on **Week 10A, Parts 1–3.5**:

- **Parts 1–2**: The `write()` → page cache → `fsync` → media path; the cost of durability
- **Part 3**: Write-ahead logging (WAL) as the solution to crash safety
- **Part 3.5**: Redis as a concrete WAL case study — AOF, `appendfsync` modes, RDB snapshots

If you need to review Redis's persistence mechanisms, revisit the Part 3.5 slides before starting. Redis design philosophy and comparison with other systems (etcd, LSM engines) will be covered in Week 10B.

---

## Prerequisites

### System Requirements

* Ubuntu VM (VirtualBox)

* Docker installed (`sudo apt install docker.io` if needed)

* `gcc` for compiling C programs

* `strace` for syscall tracing

* Python 3 with `matplotlib` (`pip3 install matplotlib` if needed)

* At least 2 GB free RAM, 1 GB free disk

### VirtualBox Storage Note

VirtualBox uses a virtual disk that may cache writes at the host level. That means:

* `fsync` may appear much faster than on real hardware

* The gap between persistence modes may be compressed

* **This is itself an observation worth recording and explaining** — do not treat it as a broken experiment

### Install and Verify Tools

```bash
sudo apt install -y redis-tools gcc strace python3-matplotlib

redis-cli --version
strace --version
docker --version
python3 -c "import matplotlib; print(matplotlib.__version__)"
```

### Directory Setup

```bash
mkdir -p ~/lab10A/{results,scripts,figures,redisdata}
cd ~/lab10A
```

### Cleanup Helper

```bash
docker rm -f redis-test redis-crash redis-rdb 2>/dev/null || true
```

---

## Part 0: Observing the fsync Gap (20 min)

This part directly exercises the core claim from Week 10A:

> `write()` copies data to the page cache (kernel RAM). `fsync()` forces that data to stable storage. The latency difference between the two is the cost of durability.

### 0.1 The Basic Gap

Compile and run the `fsync_demo` program from the lecture, under `strace`:

```bash
cat > /tmp/fsync_demo.c << 'EOF'
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

int main() {
    int fd = open("/tmp/testfile", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    char buf[4096];
    memset(buf, 'A', 4096);
    write(fd, buf, 4096);
    fsync(fd);
    close(fd);
    return 0;
}
EOF

gcc -o /tmp/fsync_demo /tmp/fsync_demo.c
strace -T -e trace=write,fsync,close /tmp/fsync_demo 2>&1 | tee results/strace_basic.txt
```

**Record** the time shown for `write()` and `fsync()`. Run it 3 times; record all three.

### 0.2 How fsync Cost Scales with Dirty Data

The lecture claimed that fsync cost depends on how much dirty data is in the page cache. Test this:

```bash
cat > /tmp/fsync_scale.c << 'EOF'
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    int pages = argc > 1 ? atoi(argv[1]) : 1;  /* number of 4KB pages */
    char buf[4096];
    memset(buf, 'X', sizeof(buf));

    int fd = open("/tmp/testfile_scale", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    for (int i = 0; i < pages; i++)
        write(fd, buf, sizeof(buf));

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    fsync(fd);
    clock_gettime(CLOCK_MONOTONIC, &t1);

    double us = (t1.tv_sec - t0.tv_sec) * 1e6 + (t1.tv_nsec - t0.tv_nsec) / 1e3;
    printf("%d pages (%d KB): fsync = %.1f us\n", pages, pages * 4, us);
    close(fd);
    return 0;
}
EOF

gcc -O2 -o /tmp/fsync_scale /tmp/fsync_scale.c
```

Run with increasing data sizes and record the results:

```bash
for PAGES in 1 16 256 1024 6400 25600; do
    /tmp/fsync_scale $PAGES
done 2>&1 | tee results/fsync_scale.txt
```

This covers 4 KB → 100 MB. **Record all six measurements in a table** and note where the fsync cost becomes clearly visible even inside VirtualBox.

### 0.3 Repeated fsync: The Per-Operation Cost

Now measure the cost of calling `fsync` on every single write — this is what Redis `appendfsync always` does internally:

```bash
cat > /tmp/fsync_per_write.c << 'EOF'
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

int main() {
    char buf[256];
    memset(buf, 'Z', sizeof(buf));
    int fd = open("/tmp/testfile_per", O_WRONLY | O_CREAT | O_TRUNC, 0644);

    struct timespec t0, t1;
    int N = 5000;

    /* Batch: N writes, then one fsync */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int i = 0; i < N; i++)
        write(fd, buf, sizeof(buf));
    fsync(fd);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double batch_us = (t1.tv_sec - t0.tv_sec) * 1e6 + (t1.tv_nsec - t0.tv_nsec) / 1e3;

    close(fd);
    fd = open("/tmp/testfile_per2", O_WRONLY | O_CREAT | O_TRUNC, 0644);

    /* Per-write: fsync after every write */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int i = 0; i < N; i++) {
        write(fd, buf, sizeof(buf));
        fsync(fd);
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double per_us = (t1.tv_sec - t0.tv_sec) * 1e6 + (t1.tv_nsec - t0.tv_nsec) / 1e3;

    printf("Batch (%d writes + 1 fsync):     %.1f us total, %.1f us/write\n", N, batch_us, batch_us / N);
    printf("Per-write (%d x write+fsync):     %.1f us total, %.1f us/write\n", N, per_us, per_us / N);
    printf("Slowdown factor: %.1fx\n", per_us / batch_us);

    close(fd);
    return 0;
}
EOF

gcc -O2 -o /tmp/fsync_per_write /tmp/fsync_per_write.c
/tmp/fsync_per_write 2>&1 | tee results/fsync_per_write.txt
```

**Questions for report:**

1. What was the slowdown factor between batch and per-write fsync? Why?
2. Before you run the Redis benchmarks, **predict**: how much slower should `appendfsync always` be compared to `appendfsync no`? Write down your prediction and the reasoning now — you will compare it to the actual measurement later.

---

## Part 1: Redis AOF as an Applied WAL (20 min)

Now connect the raw `fsync` observations to a real system. This part has you **look inside** Redis's persistence mechanism rather than treating it as a black box.

### 1.1 Start Redis and Write a Few Keys

```bash
rm -rf redisdata/aof-inspect && mkdir -p redisdata/aof-inspect

docker run -d --name redis-test -p 6379:6379 \
  -v "$PWD/redisdata/aof-inspect:/data" \
  redis:7 redis-server --save "" --appendonly yes --appendfsync always

redis-cli SET user:1 alice
redis-cli SET user:2 bob
redis-cli LPUSH queue:jobs "job-A" "job-B"
redis-cli INCR counter
```

### 1.2 Inspect the AOF File

```bash
echo "=== AOF file size ==="
ls -la redisdata/aof-inspect/appendonlydir/

echo ""
echo "=== AOF content (first 80 lines) ==="
cat redisdata/aof-inspect/appendonlydir/*.aof | head -80
```

**Record in your report:**

* The exact content of the AOF file for the four commands above

* How many lines does each command produce in the AOF?

* Can you identify the RESP (Redis Serialization Protocol) structure? (`*N` = array of N elements, `$N` = next string is N bytes)

### 1.3 Observe fsync Behavior with strace

Attach `strace` to the running Redis process and watch what happens when you write.

> **Why `fdatasync`, not `fsync`?** Redis uses `fdatasync()` on Linux for AOF persistence. Since the AOF is append-only, Redis only needs to flush *data*, not metadata like `mtime` — exactly the `fsync` vs `fdatasync` distinction from the lecture slides. This is a real engineering decision you are observing, not a textbook simplification.

```bash
# Find the Redis server PID (docker inspect is more reliable than docker top)
REDIS_PID=$(docker inspect --format '{{.State.Pid}}' redis-test)

# Trace fsync/fdatasync calls (Redis uses fdatasync on Linux for AOF)
sudo strace -p $REDIS_PID -e trace=fsync,fdatasync -T 2>&1 | tee results/strace_redis_always.txt &
STRACE_PID=$!
sleep 1   # wait for strace to fully attach before sending writes

# In the same or another terminal, send 20 writes
for i in $(seq 1 20); do redis-cli SET "strace:$i" "$i" > /dev/null; done

sleep 1
kill $STRACE_PID 2>/dev/null
wait $STRACE_PID 2>/dev/null
```

**Count the number of** **`fdatasync`** **calls** in `results/strace_redis_always.txt`. You should see approximately one `fdatasync` per write command.

### 1.4 Compare: Switch to `everysec`

```bash
docker rm -f redis-test

rm -rf redisdata/aof-inspect && mkdir -p redisdata/aof-inspect

docker run -d --name redis-test -p 6379:6379 \
  -v "$PWD/redisdata/aof-inspect:/data" \
  redis:7 redis-server --save "" --appendonly yes --appendfsync everysec

REDIS_PID=$(docker inspect --format '{{.State.Pid}}' redis-test)

sudo strace -p $REDIS_PID -e trace=fsync,fdatasync -T 2>&1 | tee results/strace_redis_everysec.txt &
STRACE_PID=$!
sleep 1   # wait for strace to attach

for i in $(seq 1 100); do redis-cli SET "strace:$i" "$i" > /dev/null; done

sleep 3  # wait for at least one group fsync cycle
kill $STRACE_PID 2>/dev/null
wait $STRACE_PID 2>/dev/null

docker rm -f redis-test
```

**Compare the two strace logs:**

```bash
echo "always: $(grep -c 'fdatasync\|fsync' results/strace_redis_always.txt) fdatasync calls for 20 writes"
echo "everysec: $(grep -c 'fdatasync\|fsync' results/strace_redis_everysec.txt) fdatasync calls for 100 writes"
```

**Questions for report:**

1. How many `fdatasync` calls did `always` make for 20 writes? How many did `everysec` make for 100 writes?
2. Explain why `everysec` has far fewer `fdatasync` calls despite more writes.
3. Based on Part 0 and Part 1 observations, explain in your own words what `appendfsync` is really controlling — use the phrase "acknowledgement contract" in your answer.

---

## Part 2: Persistence Benchmark with Latency Analysis (25 min)

Now measure throughput and latency systematically. Unlike Part 1 (which was about *seeing the mechanism*), this part is about *measuring the cost*.

### 2.1 Benchmark Three Modes

Create a script that benchmarks all three modes and captures detailed output:

```bash
cat > scripts/bench_modes.sh << 'SCRIPT'
#!/bin/bash
set -e

run_bench() {
  local MODE=$1; shift
  docker rm -f redis-test 2>/dev/null || true
  docker run -d --name redis-test -p 6379:6379 redis:7 \
    redis-server "$@"
  sleep 2
  redis-cli ping > /dev/null

  echo "=== Benchmarking: $MODE ==="
  redis-benchmark -t set -n 100000 -P 1 --csv \
    2>&1 | tee results/bench_${MODE}.csv

  docker rm -f redis-test
}

run_bench none     --save "" --appendonly no
run_bench everysec --save "" --appendonly yes --appendfsync everysec
run_bench always   --save "" --appendonly yes --appendfsync always
SCRIPT
chmod +x scripts/bench_modes.sh
./scripts/bench_modes.sh
```

### 2.2 Extract and Compare Results

```bash
echo "Mode,Requests/sec" > results/bench_summary.csv
for MODE in none everysec always; do
  RPS=$(grep '"SET"' results/bench_${MODE}.csv | head -1 | cut -d',' -f2 | tr -d '"')
  echo "$MODE,$RPS" >> results/bench_summary.csv
done
cat results/bench_summary.csv | column -t -s','
```

### 2.3 Visualize Throughput Comparison

Create a bar chart of throughput across modes:

```bash
cat > scripts/plot_throughput.py << 'PY'
#!/usr/bin/env python3
import csv, sys
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

modes, rps = [], []
with open('results/bench_summary.csv') as f:
    reader = csv.DictReader(f)
    for row in reader:
        modes.append(row['Mode'])
        rps.append(float(row['Requests/sec']))

fig, ax = plt.subplots(figsize=(7, 4))
colors = ['#2563eb', '#16a34a', '#dc2626']
bars = ax.bar(modes, rps, color=colors, edgecolor='black', linewidth=0.5)
ax.set_ylabel('Requests/sec')
ax.set_title('Redis SET Throughput by Persistence Mode')
ax.set_ylim(0, max(rps) * 1.2)

for bar, v in zip(bars, rps):
    ax.text(bar.get_x() + bar.get_width()/2, bar.get_height() + max(rps)*0.02,
            f'{v:.0f}', ha='center', va='bottom', fontsize=10)

plt.tight_layout()
plt.savefig('figures/throughput_comparison.png', dpi=150)
print("Saved figures/throughput_comparison.png")
PY

python3 scripts/plot_throughput.py
```

### 2.4 Analyze

**Questions for report** (include the figure):

1. Compare your Part 0 prediction to the actual measurement. Were you close? What did you over- or under-estimate?
2. Calculate the **cost of durability**: what fraction of throughput do you lose going from `none` → `everysec` → `always`?
3. If you were running Redis as a session store for a web application (where losing a few seconds of sessions on crash is acceptable), which mode would you choose and why?

---

## Part 3: Crash-Loss Window Experiment (25 min)

Throughput tells you the cost. This part tells you the **risk**: how much data does each policy actually lose on crash?

### 3.1 Create the Sequential Writer

```bash
cat > scripts/write_seq.sh << 'EOF'
#!/bin/bash
OUT=${1:-results/last_sent.txt}
PREFIX=${2:-seq}
i=1
trap 'exit 0' INT TERM
: > "$OUT"
while true; do
  if redis-cli SET "${PREFIX}:${i}" "${i}" > /dev/null; then
    echo "$i" > "$OUT"
    i=$((i+1))
  else
    exit 0
  fi
done
EOF
chmod +x scripts/write_seq.sh
```

### 3.2 Crash Test: Run 3 Trials Per Policy

A single crash test is not reliable — results vary with timing. Run **3 trials each** for `everysec` and `always`:

```bash
cat > scripts/crash_test.sh << 'SCRIPT'
#!/bin/bash
set -e
POLICY=$1        # everysec or always
TRIAL=$2         # trial number
DATADIR="redisdata/${POLICY}_t${TRIAL}"
PREFIX="${POLICY:0:2}${TRIAL}"
RESULT="results/crash_${POLICY}_t${TRIAL}.txt"

rm -rf "$DATADIR" && mkdir -p "$DATADIR"

docker rm -f redis-crash 2>/dev/null || true
docker run -d --name redis-crash -p 6379:6379 \
  -v "$PWD/$DATADIR:/data" \
  redis:7 redis-server --save "" --appendonly yes --appendfsync "$POLICY"

# Wait until Redis is ready (not just container started)
for attempt in $(seq 1 10); do
  redis-cli ping > /dev/null 2>&1 && break
  sleep 1
done

./scripts/write_seq.sh "results/last_sent_tmp.txt" "$PREFIX" > /dev/null 2>&1 &
WRITER_PID=$!

sleep 3
LAST_SENT=$(cat results/last_sent_tmp.txt 2>/dev/null)
LAST_SENT=${LAST_SENT:-0}   # guard against empty file on slow VMs

if [ "$LAST_SENT" -eq 0 ]; then
  echo "WARNING: writer produced 0 keys in 3 seconds — VM may be too slow. Trying 5 more seconds..."
  sleep 5
  LAST_SENT=$(cat results/last_sent_tmp.txt 2>/dev/null)
  LAST_SENT=${LAST_SENT:-0}
fi

docker kill -s KILL redis-crash
docker rm redis-crash
wait $WRITER_PID 2>/dev/null || true

docker run -d --name redis-crash -p 6379:6379 \
  -v "$PWD/$DATADIR:/data" \
  redis:7 redis-server --save "" --appendonly yes --appendfsync "$POLICY"

# Wait until Redis finishes loading AOF before querying
for attempt in $(seq 1 15); do
  redis-cli ping > /dev/null 2>&1 && break
  sleep 1
done

SURVIVED=$(redis-cli --scan --pattern "${PREFIX}:*" | tr -d '\r' | sed "s/^${PREFIX}://" | sort -n | tail -1)
SURVIVED=${SURVIVED:-0}
LOSS=$((LAST_SENT - SURVIVED))

echo "policy=${POLICY} trial=${TRIAL} sent=${LAST_SENT} survived=${SURVIVED} lost=${LOSS}" | tee "$RESULT"

docker rm -f redis-crash
SCRIPT
chmod +x scripts/crash_test.sh
```

Run 3 trials for each policy:

```bash
for TRIAL in 1 2 3; do
  ./scripts/crash_test.sh everysec $TRIAL
done

for TRIAL in 1 2 3; do
  ./scripts/crash_test.sh always $TRIAL
done
```

### 3.3 Inspect the AOF After a Crash

Pick one `everysec` trial directory and examine the AOF file after the crash:

```bash
PICK=$(ls -d redisdata/everysec_t* | head -1)

echo "=== AOF file size after crash ==="
ls -la "$PICK"/appendonlydir/*.aof

echo ""
echo "=== Last 10 lines of the AOF ==="
cat "$PICK"/appendonlydir/*.aof | tail -10

echo ""
echo "=== Total command count in AOF ==="
grep -c '^\*' "$PICK"/appendonlydir/*.aof || echo "0"
```

### 3.4 Summarize Results

```bash
echo ""
echo "=== Crash Test Summary ==="
cat results/crash_*.txt | column -t
```

Fill this table in your report:

| Policy     | Trial | Keys sent | Keys survived | Keys lost |
| ---------- | ----- | --------- | ------------- | --------- |
| `everysec` | 1     | <br />    | <br />        | <br />    |
| `everysec` | 2     | <br />    | <br />        | <br />    |
| `everysec` | 3     | <br />    | <br />        | <br />    |
| `always`   | 1     | <br />    | <br />        | <br />    |
| `always`   | 2     | <br />    | <br />        | <br />    |
| `always`   | 3     | <br />    | <br />        | <br />    |

**Questions for report:**

1. What is the range (min–max) of lost writes across your 3 `everysec` trials? Why does it vary?
2. Did `always` lose zero writes in every trial? If not, what might explain the residual loss? (Hint: think about VirtualBox's storage behavior from Part 0.)
3. Examine the last few lines of the AOF file you inspected. Does it end cleanly, or is there a truncated entry? What does this tell you about when the crash interrupted the write path?

---

## Part 4: RDB Snapshot and COW Overhead (15 min)

This part observes the cost of **background persistence work** — the deferred cleanup from Week 10A.

### 4.1 Prepare: Load Data into Redis

> **VM memory note**: This part loads ~100 MB into Redis and then `fork()`s. Peak RSS during the snapshot can approach 2× (~200 MB). If your VM has only 1 GB free RAM, reduce `-n` to `200000` or `-d` to `100`. The COW effect will still be observable, just smaller.

```bash
docker run -d --name redis-rdb -p 6379:6379 redis:7 \
  redis-server --save "10 1" --appendonly no

redis-cli ping

# Pre-load ~100 MB of data
redis-benchmark -t set -n 500000 -d 200 -P 50 -q
```

### 4.2 Capture Baseline Memory

```bash
echo "=== Before snapshot ===" | tee results/cow_before.txt
redis-cli info memory | grep -E "used_memory_human|used_memory_rss_human" | tee -a results/cow_before.txt
```

### 4.3 Trigger Snapshot Under Write Load

In **Terminal 1**, watch memory continuously:

```bash
watch -n 1 'redis-cli info memory | grep -E "used_memory_human|used_memory_rss_human"; \
             redis-cli info persistence | grep -E "rdb_bgsave_in_progress|rdb_last_cow"'
```

In **Terminal 2**, generate writes and trigger a snapshot:

```bash
redis-benchmark -t set -n 200000 -d 200 -P 1 -q &
BM_PID=$!

redis-cli bgsave
wait $BM_PID
```

### 4.4 Capture Post-Snapshot Metrics

```bash
echo "=== After snapshot ===" | tee results/cow_after.txt
redis-cli info persistence | grep -E 'rdb_last_bgsave_time_sec|rdb_last_cow_size' | tee -a results/cow_after.txt
redis-cli info memory | grep -E "used_memory_human|used_memory_rss_human" | tee -a results/cow_after.txt
```

```bash
docker rm -f redis-rdb
```

**Questions for report:**

1. What was `rdb_last_cow_size`? Express it as a percentage of `used_memory`.
2. Explain why `fork()` for the snapshot is initially cheap (shared pages) but becomes expensive under write load (COW faults).
3. Connect this to Week 3: in what other systems have you seen `fork()` + COW? What is the common pattern?

---

## Part 5: Design Your Own Experiment (20 min)

This part has no step-by-step commands. You must **design, execute, and analyze** a short experiment that explores one aspect of Redis persistence behavior.

### Choose one question (or propose your own):

**Option A:** How does value size affect the throughput gap between `always` and `no`?

* Test with at least 4 different value sizes (e.g., 10 B, 100 B, 1 KB, 10 KB)

* Hypothesis: does the gap widen or narrow with larger values? Why?

**Option B:** Does pipelining (`-P` flag in redis-benchmark) reduce the relative cost of `appendfsync always`?

* Test with pipeline depths 1, 10, 50

* Hypothesis: can pipelining amortize the per-command fsync cost?

**Option C:** How does the AOF rewrite (`BGREWRITEAOF`) behave under load?

* Write 100K keys, then trigger `BGREWRITEAOF`

* Measure: AOF file size before and after, time to complete, memory during rewrite

* How does this connect to the fork/COW observation from Part 4?

### Requirements for your experiment

1. **State your hypothesis** before running the experiment
2. **Write a script** (`scripts/experiment.sh` or `.py`) that automates data collection
3. **Collect quantitative results** (at least one table)
4. **Produce at least one figure** (plot) using Python
5. **Interpret the results**: did they match your hypothesis? What mechanism explains what you observed?

---

## Deliverables

Submit a `lab10A/` directory containing:

```text
lab10A/
├── scripts/
│   ├── write_seq.sh
│   ├── bench_modes.sh
│   ├── crash_test.sh
│   ├── plot_throughput.py
│   └── experiment.{sh,py}       # Part 5
├── figures/
│   ├── throughput_comparison.png
│   └── experiment_*.png          # Part 5 figure(s)
├── results/
│   ├── strace_basic.txt
│   ├── fsync_scale.txt
│   ├── fsync_per_write.txt
│   ├── strace_redis_always.txt
│   ├── strace_redis_everysec.txt
│   ├── bench_*.csv
│   ├── bench_summary.csv
│   ├── crash_*.txt
│   ├── cow_before.txt
│   └── cow_after.txt
└── lab10A_report.md              # Use the provided template
```

---

## Grading Rubric

| Criterion                                                                   | Points  |
| --------------------------------------------------------------------------- | ------- |
| Part 0: fsync observation (scale + per-write measurements, prediction)      | 10      |
| Part 1: AOF inspection + strace comparison (always vs everysec)             | 15      |
| Part 2: Persistence benchmark + throughput figure + prediction comparison   | 15      |
| Part 3: Crash-loss with 3 trials per policy + AOF inspection                | 20      |
| Part 4: COW observation + fork/COW analysis                                 | 10      |
| Part 5: Independent experiment (hypothesis, script, data, figure, analysis) | 20      |
| Report quality: mechanism-based explanations, not just observations         | 10      |
| **Total**                                                                   | **100** |

---

## Common Issues

### `redis-benchmark` not found

```bash
sudo apt install redis-tools
```

### `strace` permission denied on Docker process

Use `sudo strace ...`. The lab uses `docker inspect` to find the PID:

```bash
REDIS_PID=$(docker inspect --format '{{.State.Pid}}' redis-test)
sudo strace -p $REDIS_PID -e trace=fsync,fdatasync -T
```

If `docker inspect` returns `0`, the container may not be running — check with `docker ps`.

### Port 6379 already in use

```bash
docker rm -f redis-test redis-crash redis-rdb 2>/dev/null || true
```

### `SURVIVED` is blank after crash restart

No matching keys were found. The script defaults to `0`. Possible causes: AOF was empty/corrupted, or Redis is still loading the AOF when you query. The script now waits for `redis-cli ping` to succeed before scanning, but if loading is very slow, increase the wait.

### `always` mode still loses 1–2 keys in VirtualBox

This is expected. VirtualBox may not honor the guest OS's `fsync` — the hypervisor intercepts the flush command and returns immediately without forcing data to host media. This means even `appendfsync always` cannot guarantee durability inside a VM. **Record this observation and explain the mechanism in your report.**

### Crash test writer produces 0 keys

The VM or Docker may be slow to start. The script will automatically wait 5 extra seconds. If it still shows 0, check that Redis is running (`redis-cli ping`) and that port 6379 is not in use by another container.

### `matplotlib` not available

```bash
pip3 install matplotlib
# Or: sudo apt install python3-matplotlib
```

### Docker permission denied

```bash
sudo usermod -aG docker $USER
# Then log out and back in, or use: sudo docker ...
```

