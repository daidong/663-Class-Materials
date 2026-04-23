# Lab 10: KV Store Under Pressure

> **Goal:** Measure how persistence and replication choices affect latency and throughput in Redis and etcd, connecting distributed storage tradeoffs to the local fsync fundamentals from Week 9.

## Overview

In this lab, you will:

1. Benchmark Redis with different AOF fsync policies and observe tail latency
2. Observe Redis fork-snapshot (RDB) overhead using COW page metrics
3. Stress-test etcd writes and observe compaction impact on read latency
4. (Optional) Deploy MinIO and compare object store vs local file I/O

**Estimated time:** 90 minutes

---

## Prerequisites

### System Requirements

- Ubuntu VM (VirtualBox)
- Docker installed (`sudo apt install docker.io` if not already)
- At least 2 GB free RAM
- At least 2 GB free disk space

### Install Tools

```bash
# Redis CLI (for benchmarking — we run Redis server via Docker)
sudo apt update
sudo apt install -y redis-tools

# etcd client
ETCD_VER=v3.5.17
ARCH=$(dpkg --print-architecture)   # amd64 or arm64
curl -fsSL https://github.com/etcd-io/etcd/releases/download/${ETCD_VER}/etcd-${ETCD_VER}-linux-${ARCH}.tar.gz \
  | sudo tar xz -C /usr/local/bin --strip-components=1 \
    etcd-${ETCD_VER}-linux-${ARCH}/etcdctl

# Verify
redis-cli --version
etcdctl version
docker --version
```

### Directory Setup

```bash
mkdir -p ~/lab10/{results,scripts}
cd ~/lab10
```

---

## Part 1: Redis AOF Persistence Benchmark (30 min)

In this part, you measure how Redis fsync policy directly affects latency — the same fsync tradeoff from Week 9, now in a real application.

### 1.1 Launch Redis with AOF Disabled (Baseline)

```bash
# Start Redis with no persistence (pure in-memory)
docker run -d --name redis-none -p 6379:6379 redis:7 \
  redis-server --save "" --appendonly no

# Verify it's running
redis-cli ping
# Should return: PONG
```

### 1.2 Benchmark Baseline

```bash
# Run redis-benchmark: 100k SET operations, pipeline 1 (one at a time)
redis-benchmark -t set -n 100000 -P 1 -q --csv > results/redis_none.csv

# Also capture the latency distribution
redis-benchmark -t set -n 100000 -P 1 --csv 2>&1 | head -5
```

Record the reported requests/second and latency percentiles.

### 1.3 Redis with AOF `everysec`

```bash
# Stop previous container
docker stop redis-none && docker rm redis-none

# Start Redis with AOF, fsync every second
docker run -d --name redis-everysec -p 6379:6379 redis:7 \
  redis-server --save "" --appendonly yes --appendfsync everysec

# Benchmark
redis-benchmark -t set -n 100000 -P 1 -q --csv > results/redis_everysec.csv
```

### 1.4 Redis with AOF `always`

```bash
docker stop redis-everysec && docker rm redis-everysec

# Start Redis with AOF, fsync every write
docker run -d --name redis-always -p 6379:6379 redis:7 \
  redis-server --save "" --appendonly yes --appendfsync always

# Benchmark
redis-benchmark -t set -n 100000 -P 1 -q --csv > results/redis_always.csv
```

### 1.5 Compare Results

```bash
# Clean up
docker stop redis-always && docker rm redis-always
```


| AOF Policy            | Requests/sec | Avg Latency (ms) | p99 Latency (ms) |
| --------------------- | ------------ | ---------------- | ---------------- |
| none (no persistence) |              |                  |                  |
| everysec              |              |                  |                  |
| always                |              |                  |                  |


**Question for report:** Map each AOF policy to what happens in the write path from Week 9. Why does `always` have the lowest throughput?

---

## Part 2: Redis Fork Snapshot (RDB) Overhead (20 min)

### 2.1 Launch Redis with RDB Persistence

```bash
# Start Redis with RDB snapshots enabled (save every 10 sec if ≥1 key changed)
docker run -d --name redis-rdb -p 6379:6379 redis:7 \
  redis-server --save "10 1" --appendonly no

# Pre-load data (fill ~100 MB of keys)
redis-benchmark -t set -n 500000 -d 200 -P 50 -q
```

### 2.2 Trigger Snapshot and Observe

Open two terminals.

**Terminal 1 — Watch Redis info during snapshot:**

```bash
# Watch memory usage and fork stats
watch -n 0.2 'redis-cli info memory | grep -E "used_memory_human|used_memory_rss_human"; \
             redis-cli info persistence | grep -E "rdb_last|rdb_bgsave"'
```

**Terminal 2 — Trigger snapshot while benchmarking:**

```bash
# Start a heavier write load during BGSAVE (more COW pressure)
redis-benchmark -t set -n 2000000 -d 512 -P 10 -q &
BM_PID=$!

# Trigger BGSAVE
redis-cli bgsave

# Wait for benchmark to finish
wait $BM_PID
```

### 2.3 Check Fork Impact

```bash
# Check fork duration
redis-cli info persistence | grep -E "rdb_last_bgsave|rdb_last_cow"
```

**Record in your report:**

- `rdb_last_bgsave_time_sec`: How long the snapshot took
- `rdb_last_cow_size`: How many bytes were COW'd (copy-on-write)
- Did you notice any latency spike during the snapshot?

```bash
# Clean up
docker stop redis-rdb && docker rm redis-rdb
```

**Question for report:** Why does fork + COW cause memory spikes? Connect this to page cache and virtual memory concepts from Week 2/9.

---

## Part 3: etcd Under Write Pressure (30 min)

### 3.1 Launch a Single-Node etcd

```bash
# Run etcd in Docker
docker run -d --name etcd-lab -p 2379:2379 \
  quay.io/coreos/etcd:v3.5.17 \
  etcd --advertise-client-urls http://0.0.0.0:2379 \
       --listen-client-urls http://0.0.0.0:2379

# Verify
etcdctl --endpoints=http://localhost:2379 endpoint health
```

### 3.2 Baseline: Read and Write Latency

```bash
ETCD="etcdctl --endpoints=http://localhost:2379"

# Write 1000 keys, measure time
time for i in $(seq 1 1000); do
  $ETCD put "/bench/key-$i" "value-$i" > /dev/null
done

# Read 1000 keys, measure time
time for i in $(seq 1 1000); do
  $ETCD get "/bench/key-$i" > /dev/null
done
```

Record the total time for 1000 writes and 1000 reads.

### 3.3 Write a Benchmark Script

Create `scripts/etcd_bench.sh`:

```bash
#!/bin/bash
# Benchmark etcd write and read latency
# Usage: ./etcd_bench.sh <num_keys> <value_size_bytes> <output_csv>

ETCD="etcdctl --endpoints=http://localhost:2379"
N=${1:-1000}
VSIZE=${2:-256}
OUTPUT=${3:-results/etcd_bench.csv}

# Generate a value of specified size
VALUE=$(head -c $VSIZE /dev/urandom | base64 | head -c $VSIZE)

echo "op,key,latency_ms" > "$OUTPUT"

echo "Writing $N keys (value size: $VSIZE bytes)..."
for i in $(seq 1 $N); do
  START=$(date +%s%N)
  $ETCD put "/bench/k-$i" "$VALUE" > /dev/null 2>&1
  END=$(date +%s%N)
  LAT=$(echo "scale=3; ($END - $START) / 1000000" | bc)
  echo "write,/bench/k-$i,$LAT" >> "$OUTPUT"
done

echo "Reading $N keys..."
for i in $(seq 1 $N); do
  START=$(date +%s%N)
  $ETCD get "/bench/k-$i" > /dev/null 2>&1
  END=$(date +%s%N)
  LAT=$(echo "scale=3; ($END - $START) / 1000000" | bc)
  echo "read,/bench/k-$i,$LAT" >> "$OUTPUT"
done

echo "Results written to $OUTPUT"
```

```bash
chmod +x scripts/etcd_bench.sh
```

### 3.4 Measure Baseline

```bash
./scripts/etcd_bench.sh 500 256 results/etcd_baseline.csv
```

### 3.5 Observe Compaction Impact

```bash
# Write many revisions of the same keys (fills up revision history)
echo "Filling revision history..."
for round in $(seq 1 20); do
  for i in $(seq 1 500); do
    $ETCD put "/bench/k-$i" "round-$round-value" > /dev/null 2>&1
  done
  echo "  Round $round done"
done

# Check DB size
$ETCD endpoint status --write-out=table

# Now trigger compaction
LATEST_REV=$($ETCD endpoint status --write-out=json | python3 -c "import sys,json; print(json.load(sys.stdin)[0]['Status']['header']['revision'])")
echo "Compacting to revision $LATEST_REV..."

# Benchmark reads DURING compaction
./scripts/etcd_bench.sh 500 256 results/etcd_during_compact.csv &
BENCH_PID=$!

$ETCD compact $LATEST_REV
$ETCD defrag

wait $BENCH_PID

# Check DB size after compaction
$ETCD endpoint status --write-out=table
```

### 3.6 Compare Results


| Phase             | Write p50 (ms) | Write p99 (ms) | Read p50 (ms) | Read p99 (ms) |
| ----------------- | -------------- | -------------- | ------------- | ------------- |
| Baseline (fresh)  |                |                |               |               |
| During compaction |                |                |               |               |


**Question for report:** Why might compaction affect read latency? (Hint: think about what's happening on disk — B+ tree reorganization, fsync.)

```bash
# Clean up
docker stop etcd-lab && docker rm etcd-lab
```

---

## Part 4: MinIO Object Store (Optional — 15 min)

### 4.1 Launch MinIO

```bash
docker run -d --name minio-lab -p 9000:9000 -p 9001:9001 \
  -e MINIO_ROOT_USER=admin \
  -e MINIO_ROOT_PASSWORD=password123 \
  minio/minio server /data --console-address ":9001"
```

### 4.2 Install MinIO Client

```bash
ARCH=$(dpkg --print-architecture)
curl -fsSL "https://dl.min.io/client/mc/release/linux-${ARCH}/mc" -o /tmp/mc
chmod +x /tmp/mc
sudo install /tmp/mc /usr/local/bin/mc
mc --version

# Configure
mc alias set local http://localhost:9000 admin password123
mc mb local/test-bucket
```

### 4.3 Benchmark: Object Store vs Local FS

```bash
# Create a 10 MB test file
dd if=/dev/urandom of=/tmp/testobj.dat bs=1M count=10

# Time local file copy
time cp /tmp/testobj.dat /tmp/testobj_copy.dat

# Time MinIO upload
time mc cp /tmp/testobj.dat local/test-bucket/testobj.dat

# Time MinIO download
time mc cp local/test-bucket/testobj.dat /tmp/testobj_minio.dat

# Small object test (4 KB)
dd if=/dev/urandom of=/tmp/smallobj.dat bs=4096 count=1
time for i in $(seq 1 100); do mc cp /tmp/smallobj.dat local/test-bucket/small-$i.dat; done
time for i in $(seq 1 100); do cp /tmp/smallobj.dat /tmp/small-$i.dat; done
```


| Operation         | Local FS | MinIO | Ratio |
| ----------------- | -------- | ----- | ----- |
| 10 MB write       |          |       |       |
| 10 MB read        |          |       |       |
| 100 × 4 KB writes |          |       |       |


**Question for report:** Why is the per-object overhead much higher for MinIO? What layers does an HTTP PUT go through that a local `write()` doesn't?

```bash
# Clean up
docker stop minio-lab && docker rm minio-lab
rm -f /tmp/testobj* /tmp/smallobj* /tmp/small-*.dat
```

---

## Deliverables

Submit a `lab10/` directory containing:

```
lab10/
├── scripts/
│   └── etcd_bench.sh
├── results/
│   ├── redis_none.csv
│   ├── redis_everysec.csv
│   ├── redis_always.csv
│   ├── etcd_baseline.csv
│   └── etcd_during_compact.csv
└── lab10_report.md           # Use the provided template
```

---

## Grading Rubric


| Criterion                                              | Points    |
| ------------------------------------------------------ | --------- |
| Part 1: Redis AOF benchmark (3 policies compared)      | 25        |
| Part 2: Redis fork/COW observation                     | 20        |
| Part 3: etcd write + compaction experiment             | 30        |
| Report: Analysis connecting results to Week 9 concepts | 25        |
| **Total**                                              | **100**   |
| Part 4 (Optional): MinIO comparison                    | +10 bonus |


---

## Common Issues

### `redis-benchmark` not found

```bash
sudo apt install redis-tools
```

### Docker permission denied

```bash
sudo usermod -aG docker $USER
# Then log out and back in, or use: sudo docker ...
```

### etcd container exits immediately

Check logs: `docker logs etcd-lab`. Common cause: port 2379 already in use.

### etcdctl connection refused

Make sure the container is running and you're using the correct endpoint:

```bash
etcdctl --endpoints=http://localhost:2379 endpoint health
```

### MinIO `mc` command conflicts with Midnight Commander

If `mc` is aliased to Midnight Commander:

```bash
# Use full path
/usr/local/bin/mc alias set local ...
```