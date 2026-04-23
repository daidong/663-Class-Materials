# Lab 10 Report: KV Store Under Pressure

**Name:**  
**Date:**  

---

## 1. Environment

| Property | Value |
|----------|-------|
| OS / kernel (`uname -r`) | |
| Docker version | |
| Total RAM (`free -h`) | |
| Storage type (SSD / HDD / Virtual) | |
| VM or bare metal | |

---

## 2. Part 1: Redis AOF Persistence

### 2.1 Results

| AOF Policy | Requests/sec | Avg Latency (ms) | p99 Latency (ms) |
|------------|-------------|-------------------|-------------------|
| none | | | |
| everysec | | | |
| always | | | |

### 2.2 Analysis

**Q: Map each AOF policy to the write path. Why does `always` have the lowest throughput?**

<Your answer — reference fsync behavior from Week 9>

**Q: If you were building a session store that can tolerate losing 1 second of data on crash, which policy would you choose and why?**

<Your answer>

---

## 3. Part 2: Redis Fork Snapshot (RDB)

### 3.1 Observations

| Metric | Value |
|--------|-------|
| `rdb_last_bgsave_time_sec` | |
| `rdb_last_cow_size` (bytes) | |
| `used_memory_rss` before snapshot | |
| `used_memory_rss` during snapshot | |

### 3.2 Analysis

**Q: Why does fork + COW cause memory spikes? Connect to Week 2/9 concepts.**

<Your answer>

**Q: When would RDB snapshots be preferable to AOF? When would they not?**

<Your answer>

---

## 4. Part 3: etcd Under Write Pressure

### 4.1 Baseline Results

| Operation | Count | Total Time (s) | Per-op Avg (ms) |
|-----------|-------|-----------------|-----------------|
| Writes | 500 | | |
| Reads | 500 | | |

### 4.2 Compaction Impact

| Phase | Write p50 (ms) | Write p99 (ms) | Read p50 (ms) | Read p99 (ms) |
|-------|----------------|----------------|----------------|----------------|
| Baseline | | | | |
| During compaction | | | | |

DB size before compaction:  
DB size after compaction:  

### 4.3 Analysis

**Q: Why might compaction affect read latency?**

<Your answer>

**Q: etcd write latency is much higher than Redis. Why? Identify at least two architectural reasons.**

<Your answer — consider Raft consensus and fdatasync>

---

## 5. Part 4: MinIO Object Store (Optional)

### 5.1 Results

| Operation | Local FS | MinIO | Ratio |
|-----------|----------|-------|-------|
| 10 MB write | | | |
| 10 MB read | | | |
| 100 × 4 KB writes | | | |

### 5.2 Analysis

**Q: Why is per-object overhead much higher for MinIO?**

<Your answer>

---

## 6. Cross-System Comparison

### 6.1 The Tradeoff Map

Fill in based on your measurements:

| System | Latency Range | Best For | Worst For |
|--------|--------------|----------|-----------|
| Redis (no persist) | | | |
| Redis (AOF always) | | | |
| etcd | | | |
| MinIO (optional) | | | |
| Local FS + fsync (Week 9) | | | |

### 6.2 Connecting to Week 9

<In 1–2 paragraphs: How did Week 9's fsync lesson show up in this week's experiments? Which system was most affected by fsync cost?>

---

## 7. Reflection

- One thing that surprised you:
- If you were designing a new distributed application, how would you decide which storage tier to use for each type of data?
- One question you still have:

---

## 8. Files Included

| File | Description |
|------|-------------|
| `scripts/etcd_bench.sh` | etcd benchmark script |
| `results/redis_none.csv` | |
| `results/redis_everysec.csv` | |
| `results/redis_always.csv` | |
| `results/etcd_baseline.csv` | |
| `results/etcd_during_compact.csv` | |
