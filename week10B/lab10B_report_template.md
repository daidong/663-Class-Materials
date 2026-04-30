# Lab 10B Report: etcd as Durable Metadata Under Consensus

**Name:**  
**Date:**  

---

## 1. Environment

| Property | Value |
|----------|-------|
| OS / kernel (`uname -r`) | |
| Docker version | |
| `etcdctl` version | |
| Total RAM (`free -h`) | |
| Storage type (virtual / SSD / HDD) | |
| VM or bare metal | |

---

## 2. Part 1: Cluster Status

### 2.1 Initial Status

| Node | Leader? | Term | Revision | DB Size |
|------|---------|------|----------|---------|
| etcd1 | | | | |
| etcd2 | | | | |
| etcd3 | | | | |

### 2.2 Analysis

**Q1. Why is a 3-node cluster a natural minimum size for demonstrating quorum commit?**

<Your answer>

**Q2. What does the reported revision number represent at a high level?**

<Your answer>

---

## 3. Part 2: Revisions and Watch

### 3.1 Observations

- Watch events observed:
- Revisions increased across writes? 
- Final value of `/registry/leader-record/scheduler`:

### 3.2 Analysis

**Q1. Why is watch useful for a scheduler or controller loop?**

<Your answer>

**Q2. How is a watch different from repeatedly polling with `get`?**

<Your answer>

**Q3. Why does MVCC make watch and revision-based reasoning cleaner?**

<Your answer>

---

## 4. Part 3: Metadata Read vs Write Cost

### 4.1 Results

| Operation | Avg Latency (ms) | p50 (ms) | p99 (ms) |
|-----------|------------------|----------|----------|
| write | | | |
| read | | | |

### 4.2 Analysis

**Q1. Where does the gap between the `write` and `read` _client-call latencies_ come from?**

Separate (a) client-side overhead that both ops pay (binary startup, gRPC dial, TLS/handshake) from (b) server-side cost that diverges between ops.

<Your answer>

**Q2. Identify at least two _server-side_ architectural reasons why a write is more expensive than a read, even before any client-side overhead.**

<Your answer>

---

## 5. Part 4: Compaction and Defragmentation

### 5.1 Results

| Phase | Write avg (ms) | Write p50 | Write p99 | Read avg (ms) | Read p50 | Read p99 |
|------|----------------|-----------|-----------|---------------|----------|----------|
| Baseline | | | | | | |
| During compaction / defrag | | | | | | |

DB size before compaction:  
DB size after compaction / defrag:  

### 5.2 Analysis

**Q1. Why does etcd need compaction at all?**

<Your answer>

**Q2. Why might compaction or defragmentation increase latency temporarily?**

<Your answer>

**Q3. Why is that tradeoff still acceptable for etcd's intended workload?**

<Your answer>

---

## 6. Synthesis

### 6.1 What etcd Is Paying For

In one paragraph, explain what etcd is optimizing for and what it is willing to sacrifice.

<Your answer>

### 6.2 Connect Back to Week 10A and Week 10B

Use 1–2 paragraphs to answer:

- Where does truth live first in etcd?
- When is it honest for etcd to reply `OK`?
- What background work keeps etcd healthy over time?
- Why is etcd a good fit for cluster metadata but a poor fit for bulk application data?

<Your answer>

---

## 7. Reflection

- One result that surprised you:
- One reason Kubernetes benefits from etcd's design:
- One reason you would not use etcd as your main application database:

---

## 8. Files Included

| File | Description |
|------|-------------|
| `scripts/etcd_meta_bench.sh` | Benchmark script |
| `results/endpoint_status_initial.txt` | |
| `results/endpoint_status_after_watch.txt` | |
| `results/endpoint_status_before_compact.txt` | |
| `results/endpoint_status_after_compact.txt` | |
| `results/leader_record_get.json` | |
| `results/leader_record_final.txt` | |
| `results/etcd_meta_baseline.csv` | |
| `results/etcd_meta_during_compact.csv` | |
