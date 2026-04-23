# Lab 10A Report: From fsync to Redis — Observing the Cost of Durability

**Name:**
**Date:**

---

## 0. Environment

| Property | Value |
|----------|-------|
| OS / kernel (`uname -r`) | |
| Docker version | |
| Redis version | |
| Total RAM (`free -h`) | |
| Storage type (virtual / SSD / HDD) | |
| VM or bare metal | |

Note any environment detail that may affect results (especially VirtualBox storage caching).

---

## 1. Part 0: Observing the fsync Gap

### 1.1 Basic Gap (strace)

| Run | `write()` time | `fsync()` time | Ratio (fsync/write) |
|-----|---------------|----------------|---------------------|
| 1 | | | |
| 2 | | | |
| 3 | | | |

### 1.2 fsync vs. Data Size

| Pages | Data size | fsync time (µs) |
|-------|-----------|-----------------|
| 1 | 4 KB | |
| 16 | 64 KB | |
| 256 | 1 MB | |
| 1024 | 4 MB | |
| 6400 | 25 MB | |
| 25600 | 100 MB | |

At what data size did fsync cost become clearly visible in your VM?

<Your answer>

### 1.3 Per-Write fsync Cost

| Mode | Total time | Per-write cost | Slowdown factor |
|------|-----------|----------------|-----------------|
| Batch (N writes + 1 fsync) | | | |
| Per-write (N × write+fsync) | | | — |

**Q1. What was the slowdown factor? Why?**

<Your answer>

**Q2. Prediction: how much slower should `appendfsync always` be compared to `no` in Redis?**

<Your prediction and reasoning — write this BEFORE running Part 2>

---

## 2. Part 1: Redis AOF as Applied WAL

### 2.1 AOF File Content

Paste the first ~30 lines of the AOF file content here. Annotate the RESP structure for at least one command.

```
<paste AOF content>
```

### 2.2 strace fsync Comparison

| Mode | Number of writes sent | Number of fsync calls observed |
|------|----------------------|-------------------------------|
| `always` | 20 | |
| `everysec` | 100 | |

**Q1. Explain why `everysec` has far fewer fsync calls despite more writes.**

<Your answer>

**Q2. What is `appendfsync` really controlling? Use the phrase "acknowledgement contract."**

<Your answer>

---

## 3. Part 2: Persistence Benchmark

### 3.1 Throughput Results

| Mode | Requests/sec |
|------|--------------|
| none | |
| AOF `everysec` | |
| AOF `always` | |

Include `figures/throughput_comparison.png` in your submission.

### 3.2 Analysis

**Q1. Compare your Part 0 prediction to the actual measurement. Were you close?**

<Your answer>

**Q2. Calculate the cost of durability: what fraction of throughput is lost at each step?**

- none → everysec: ___% reduction
- everysec → always: ___% reduction
- none → always: ___% reduction

**Q3. If you were running Redis as a session store (losing a few seconds of sessions on crash is acceptable), which mode would you choose and why?**

<Your answer>

---

## 4. Part 3: Crash-Loss Window

### 4.1 Results (3 trials per policy)

| Policy | Trial | Keys sent | Keys survived | Keys lost |
|--------|-------|-----------|---------------|-----------|
| `everysec` | 1 | | | |
| `everysec` | 2 | | | |
| `everysec` | 3 | | | |
| `always` | 1 | | | |
| `always` | 2 | | | |
| `always` | 3 | | | |

### 4.2 AOF File Inspection

Paste the last ~10 lines of the AOF file from one `everysec` trial after the crash:

```
<paste AOF tail>
```

Does it end cleanly or is there a truncated entry?

### 4.3 Analysis

**Q1. What is the range (min–max) of lost writes across your 3 `everysec` trials? Why does it vary?**

<Your answer>

**Q2. Did `always` lose zero writes in every trial? If not, what explains the residual loss?**

<Your answer>

**Q3. What does the AOF tail tell you about when the crash interrupted the write path?**

<Your answer>

---

## 5. Part 4: RDB Snapshot and COW Overhead

### 5.1 Observations

| Metric | Value |
|--------|-------|
| `used_memory_human` (before snapshot) | |
| `used_memory_rss_human` (before snapshot) | |
| `rdb_last_bgsave_time_sec` | |
| `rdb_last_cow_size` | |
| COW as % of used_memory | |
| Visible latency spike during benchmark? | |

### 5.2 Analysis

**Q1. What was `rdb_last_cow_size` as a percentage of `used_memory`? Is this expected?**

<Your answer>

**Q2. Explain why `fork()` is initially cheap but becomes expensive under write load.**

<Your answer>

**Q3. Connect to Week 3: where else have you seen fork() + COW? What is the common pattern?**

<Your answer>

---

## 6. Part 5: Independent Investigation

### 6.1 Question and Hypothesis

**Question chosen** (A / B / C / custom):

**Hypothesis** (state BEFORE running the experiment):

<Your hypothesis and reasoning>

### 6.2 Experiment Design

Briefly describe:
- What variable you are changing
- What you are measuring
- How many data points / trials

### 6.3 Results

<Include your data table>

<Include your figure (reference the filename in figures/)>

### 6.4 Analysis

**Did the results match your hypothesis? Why or why not?**

<Your answer>

**What mechanism explains the results?**

<Your answer>

---

## 7. Synthesis

### 7.1 The Cost of Durability

In one paragraph, summarize what you learned about the relationship between durability and performance. Reference specific numbers from your experiments.

<Your answer>

### 7.2 Connecting to Week 10A

Answer using specific observations from this lab:
- Where does truth live first in Redis? (At each `appendfsync` level)
- When is it honest for Redis to reply `OK`?
- What background work keeps Redis operational over time?

<Your answer>

---

## 8. Reflection

- One result that surprised you:
- One thing you would measure differently if you ran this lab again:
- One question you still have:

---

## 9. Files Included

| File | Description |
|------|-------------|
| `scripts/write_seq.sh` | Sequential writer for crash experiment |
| `scripts/bench_modes.sh` | Automated benchmark for 3 modes |
| `scripts/crash_test.sh` | Parameterized crash test (policy + trial) |
| `scripts/plot_throughput.py` | Throughput bar chart |
| `scripts/experiment.*` | Part 5 experiment script |
| `figures/throughput_comparison.png` | Part 2 figure |
| `figures/experiment_*.png` | Part 5 figure(s) |
| `results/strace_basic.txt` | Part 0 strace output |
| `results/fsync_scale.txt` | Part 0 scale measurements |
| `results/fsync_per_write.txt` | Part 0 batch vs per-write |
| `results/strace_redis_*.txt` | Part 1 Redis strace outputs |
| `results/bench_*.csv` | Part 2 benchmark data |
| `results/crash_*.txt` | Part 3 crash test results |
| `results/cow_*.txt` | Part 4 COW observations |
