# Lab 9 Report: Anatomy of a File Write

**Name:**  
**Date:**  

---

## 1. Environment

| Property | Value |
|----------|-------|
| OS / kernel (`uname -r`) | |
| Filesystem type (`df -T /`) | |
| Storage device type (SSD / HDD / Virtual) | |
| Total RAM (`free -h`) | |
| VM or bare metal | |

```bash
# Output of: mount | grep " / "

# Output of: cat /proc/sys/vm/dirty_ratio

# Output of: cat /proc/sys/vm/dirty_background_ratio
```

---

## 2. Part 1: File Layout on Disk

### 2.1 filefrag Results

| File | Size | Number of Extents | Largest Extent |
|------|------|-------------------|----------------|
| sequential.dat | 10 MB | | |
| fragmented.dat | ~10 MB | | |

### 2.2 debugfs Inode Inspection

```
# Paste debugfs output for sequential.dat here
```

### 2.3 Hard Link Observation

- Inode number of `sequential.dat`:
- Inode number of `seq_link.dat`:
- Are they the same?
- Link count after creating hard link:

**Q: Why is the filename not stored in the inode?**

<Your answer>

---

## 3. Part 2: Page Cache Observations

### 3.1 Dirty Page Monitoring

| Event | Dirty (kB) | Writeback (kB) | Timestamp |
|-------|------------|----------------|-----------|
| Before dd | | | |
| Peak during dd | | | |
| After dd (before sync) | | | |
| After sync | | | |

### 3.2 Cache Effect on Read

| Read | Time | Cache Status |
|------|------|-------------|
| First (cold) | | dropped |
| Second (warm) | | cached |
| **Speedup** | **×** | |

---

## 4. Part 3: Baseline Latency Results

### 4.1 Summary Table

| Configuration | p50 (ms) | p90 (ms) | p99 (ms) | max (ms) | avg (ms) |
|---------------|----------|----------|----------|----------|----------|
| Buffered (no fsync) | | | | | |
| fsync | | | | | |
| fdatasync | | | | | |

### 4.2 Analysis

**Q: Why is buffered write ~1000× faster than fsync? Trace the exact steps that differ.**

<Your answer — reference the write path from the lecture>

**Q: Is fdatasync meaningfully faster than fsync in your measurements? Why or why not?**

<Your answer>

---

## 5. Part 4: Interference Experiments

### 5.1 Experiment A: [Name]

**Setup:**
```bash
# Commands used to create interference
```

**Results:**

| Metric | Baseline (fsync) | With Interference | Ratio |
|--------|------------------|-------------------|-------|
| p50 | | | |
| p90 | | | |
| p99 | | | |
| max | | | |

**Why did this interference affect latency?** (Explain the mechanism)

<Your answer>

### 5.2 Experiment B: [Name]

**Setup:**
```bash
# Commands used
```

**Results:**

| Metric | Baseline (fsync) | With Interference | Ratio |
|--------|------------------|-------------------|-------|
| p50 | | | |
| p90 | | | |
| p99 | | | |
| max | | | |

**Why did this interference affect latency?**

<Your answer>

---

## 6. Part 5: System Observation (Optional / Bonus)

### Tool Used

<iostat / strace / other>

### Key Observations

```
# Paste relevant output here
```

**Could you correlate system-level events with latency spikes?**

<Your answer>

---

## 7. Summary

### 7.1 Which interference caused the highest tail latency?

| Experiment | p99 / Baseline p99 Ratio |
|------------|--------------------------|
| | |
| | |

### 7.2 Root Cause

<In 1–2 paragraphs, explain the dominant mechanism behind the tail latency you observed. Reference specific OS components: page cache, journal, block layer, etc.>

### 7.3 Mitigation

<If you were building a database on this system, what would you do to reduce tail latency? List 2–3 concrete strategies with tradeoffs.>

---

## 8. Reflection

- One thing that surprised you:
- How this connects to earlier weeks (scheduling, containers, etc.):
- One question you still have:

---

## 9. Files Included

| File | Description |
|------|-------------|
| `write_latency.c` | Measurement program |
| `results/baseline_buffered.csv` | |
| `results/baseline_fsync.csv` | |
| `results/baseline_fdatasync.csv` | |
| `results/interf_*.csv` | |
