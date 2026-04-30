# Lab 10B: etcd as Durable Metadata Under Consensus

> **Goal:** Observe how etcd stores small, correctness-critical metadata under quorum commit, and measure the costs of revisions, watch, compaction, and defragmentation.

## Overview

In this lab, you will:
1. Launch a 3-node etcd cluster in Docker
2. Observe revisions and watch behavior on metadata-like keys
3. Measure the cost difference between writes and reads for small values
4. Trigger compaction and defragmentation and observe their effects

**Estimated time:** 90 minutes

This lab is deliberately narrower than Week 7. Week 7 focused on consensus behavior such as election and failover. Week 10B focuses on etcd as a **storage design**: replicated history, MVCC revisions, watch-driven control loops, and background cleanup.

---

## Prerequisites

### System Requirements

- Ubuntu VM (VirtualBox)
- Docker installed
- At least 2 GB free RAM
- At least 1 GB free disk space

### Install and Verify Tools

Install `etcdctl` on the VM host:

```bash
ETCD_VER=v3.5.17
ARCH=$(dpkg --print-architecture)
curl -fsSL https://github.com/etcd-io/etcd/releases/download/${ETCD_VER}/etcd-${ETCD_VER}-linux-${ARCH}.tar.gz \
  | sudo tar xz -C /usr/local/bin --strip-components=1 \
    etcd-${ETCD_VER}-linux-${ARCH}/etcdctl

etcdctl version
docker --version
python3 --version
```

### Directory Setup

```bash
mkdir -p ~/lab10B/{results,scripts}
cd ~/lab10B
```

### Cleanup from Older Runs

```bash
docker rm -f etcd1 etcd2 etcd3 2>/dev/null || true
docker network rm etcd-net 2>/dev/null || true
```

---

## Part 1: Launch a 3-Node etcd Cluster (20 min)

Create an isolated Docker network:

```bash
docker network create etcd-net
```

Launch three nodes.

### 1.1 Start `etcd1`

```bash
docker run -d --name etcd1 --network etcd-net -p 2379:2379 \
  quay.io/coreos/etcd:v3.5.17 etcd \
  --name node1 \
  --data-dir /etcd-data \
  --initial-advertise-peer-urls http://etcd1:2380 \
  --listen-peer-urls http://0.0.0.0:2380 \
  --advertise-client-urls http://etcd1:2379 \
  --listen-client-urls http://0.0.0.0:2379 \
  --initial-cluster node1=http://etcd1:2380,node2=http://etcd2:2380,node3=http://etcd3:2380 \
  --initial-cluster-state new
```

### 1.2 Start `etcd2`

```bash
docker run -d --name etcd2 --network etcd-net -p 2381:2379 \
  quay.io/coreos/etcd:v3.5.17 etcd \
  --name node2 \
  --data-dir /etcd-data \
  --initial-advertise-peer-urls http://etcd2:2380 \
  --listen-peer-urls http://0.0.0.0:2380 \
  --advertise-client-urls http://etcd2:2379 \
  --listen-client-urls http://0.0.0.0:2379 \
  --initial-cluster node1=http://etcd1:2380,node2=http://etcd2:2380,node3=http://etcd3:2380 \
  --initial-cluster-state new
```

### 1.3 Start `etcd3`

```bash
docker run -d --name etcd3 --network etcd-net -p 2382:2379 \
  quay.io/coreos/etcd:v3.5.17 etcd \
  --name node3 \
  --data-dir /etcd-data \
  --initial-advertise-peer-urls http://etcd3:2380 \
  --listen-peer-urls http://0.0.0.0:2380 \
  --advertise-client-urls http://etcd3:2379 \
  --listen-client-urls http://0.0.0.0:2379 \
  --initial-cluster node1=http://etcd1:2380,node2=http://etcd2:2380,node3=http://etcd3:2380 \
  --initial-cluster-state new
```

Define a reusable endpoint variable:

```bash
EP=http://localhost:2379,http://localhost:2381,http://localhost:2382
```

Check cluster health and status:

```bash
etcdctl --endpoints=$EP endpoint health
etcdctl --endpoints=$EP endpoint status --write-out=table | tee results/endpoint_status_initial.txt
```

**Record in your report:**
- which node is leader
- current term
- current revision
- DB size reported for each node

---

## Part 2: Revisions and Watch on Metadata-Like Keys (20 min)

This part makes etcd look like Kubernetes metadata instead of a generic key-value toy.

### 2.1 Start a Watch

Open two terminals.

**Terminal 1 — watch a prefix:**

```bash
etcdctl --endpoints=$EP watch /registry/ --prefix
```

### 2.2 Write a Small Sequence of Metadata Updates

**Terminal 2:**

Re-define a reusable endpoint variable:
```bash
EP=http://localhost:2379,http://localhost:2381,http://localhost:2382
```

Then:
```bash
etcdctl --endpoints=$EP put /registry/pods/demo pending
etcdctl --endpoints=$EP put /registry/pods/demo running
etcdctl --endpoints=$EP put /registry/leader-record/scheduler holder=nodeA
etcdctl --endpoints=$EP del /registry/pods/demo
```

Check the current global revision:

```bash
etcdctl --endpoints=$EP endpoint status --write-out=table | tee results/endpoint_status_after_watch.txt
```

Inspect one key in JSON form to see revision metadata:

```bash
etcdctl --endpoints=$EP get /registry/leader-record/scheduler -w json | tee results/leader_record_get.json
```

> **Naming note:** the key path `/registry/leader-record/scheduler` mimics the *shape* of Kubernetes leader-election metadata. It is **not** an etcd `Lease` object. Real etcd leases are created with `etcdctl lease grant <ttl>` and have TTL-based expiry; what we use here is a plain key-value entry. We avoid the path `/registry/leases/...` to prevent confusion with that other concept.

Also capture the final value so you can quote it in the report after teardown:

```bash
etcdctl --endpoints=$EP get /registry/leader-record/scheduler | tee results/leader_record_final.txt
```

**Record in your report:**
- what events appeared in the watch output
- whether revisions increased across writes
- final value of `/registry/leader-record/scheduler`
- why a watch is useful for a control loop such as a scheduler or controller

---

## Part 3: Measure Metadata Read and Write Cost (25 min)

This part asks a simple question:

> Where does the gap between an etcd small **write** and an etcd small **read** come from?

> **Measurement caveat — read this before running the script.**
>
> The benchmark below times each `etcdctl put` / `etcdctl get` from bash. Each call **forks a fresh `etcdctl` Go binary**, sets up a gRPC connection, performs one RPC, and exits. On a typical VM that startup + connection step alone is **10–40 ms**, which is comparable to (or larger than) the actual server-side write/read cost.
>
> So what you are measuring here is **client-call latency**, defined as:
>
> ```
> client-call latency  =  client startup  +  connection setup  +  RPC + server work
> ```
>
> The *gap* between the `write` and `read` distributions still reflects the server-side difference (quorum commit + `fsync` for writes vs. ReadIndex-only for linearizable reads), because client startup is paid by both sides equally. But the **absolute numbers are not etcd's pure write/read latency** — do not quote them as such in the report. If you want pure server-side numbers, use etcd's built-in `benchmark` tool against a long-lived gRPC connection.

### 3.1 Create a Benchmark Script

Create `scripts/etcd_meta_bench.sh`:

```bash
cat > scripts/etcd_meta_bench.sh <<'EOF'
#!/bin/bash
EP=${EP:-http://localhost:2379,http://localhost:2381,http://localhost:2382}
N=${1:-200}
OUTPUT=${2:-results/etcd_meta_bench.csv}
VALUE=${3:-metadata-value}

ETCD="etcdctl --endpoints=${EP}"
echo "op,key,latency_ms" > "$OUTPUT"

for i in $(seq 1 $N); do
  START=$(date +%s%N)
  $ETCD put "/meta/k-$i" "$VALUE" > /dev/null 2>&1
  END=$(date +%s%N)
  LAT=$(awk "BEGIN {printf \"%.3f\", ($END-$START)/1000000}")
  echo "write,/meta/k-$i,$LAT" >> "$OUTPUT"
done

for i in $(seq 1 $N); do
  START=$(date +%s%N)
  $ETCD get "/meta/k-$i" > /dev/null 2>&1
  END=$(date +%s%N)
  LAT=$(awk "BEGIN {printf \"%.3f\", ($END-$START)/1000000}")
  echo "read,/meta/k-$i,$LAT" >> "$OUTPUT"
done
EOF

chmod +x scripts/etcd_meta_bench.sh
```

### 3.2 Run the Benchmark

```bash
EP=$EP ./scripts/etcd_meta_bench.sh 200 results/etcd_meta_baseline.csv metadata-value
```

### 3.3 Summarize the CSV

```bash
python3 - <<'PY'
import csv
from pathlib import Path

def pct(vals, p):
    vals = sorted(vals)
    idx = int((len(vals)-1) * p)
    return vals[idx]

rows = list(csv.DictReader(open('results/etcd_meta_baseline.csv')))
for op in ('write', 'read'):
    vals = [float(r['latency_ms']) for r in rows if r['op'] == op]
    print(op, 'count=', len(vals), 'avg=', round(sum(vals)/len(vals), 3), 'p50=', round(pct(vals, 0.50), 3), 'p99=', round(pct(vals, 0.99), 3))
PY
```

Copy those numbers into your report.

**Question for report:** Where does the gap between the `write` and `read` **client-call latencies** come from? Decompose your answer into:

1. **Client-side overhead** that both ops pay (binary startup, gRPC dial, TLS/handshake if any).
2. **Server-side cost** that diverges between ops (write: Raft proposal → majority `fsync` → bbolt apply; read: ReadIndex round-trip but no log append, no `fsync`).

Which component would you expect to dominate on a single-host VM cluster like this one? Which would dominate on a real 3-node cluster across a network?

---

## Part 4: Compaction and Defragmentation (25 min)

This part focuses on background maintenance.

### 4.1 Create Revision Pressure

Rewrite the same keys many times to create a deep revision history:

```bash
for round in $(seq 1 20); do
  for i in $(seq 1 200); do
    etcdctl --endpoints=$EP put "/meta/k-$i" "round-$round" > /dev/null 2>&1
  done
  echo "round $round done"
done

etcdctl --endpoints=$EP endpoint status --write-out=table | tee results/endpoint_status_before_compact.txt
```

### 4.2 Benchmark During Cleanup

Start a benchmark in the background:

```bash
EP=$EP ./scripts/etcd_meta_bench.sh 200 results/etcd_meta_during_compact.csv compact-phase &
BENCH_PID=$!
```

Capture the latest revision and compact to it:

```bash
LATEST_REV=$(etcdctl --endpoints=$EP endpoint status --write-out=json | python3 -c "import sys,json; data=json.load(sys.stdin); print(data[0]['Status']['header']['revision'])")
echo "Compacting to revision ${LATEST_REV}"

etcdctl --endpoints=$EP compact ${LATEST_REV}

# Defrag each member one at a time. With a multi-endpoint --endpoints flag,
# etcdctl defrags each endpoint sequentially; doing it explicitly per node
# makes the per-node stop-the-world clearer in the latency trace.
for ep in http://localhost:2379 http://localhost:2381 http://localhost:2382; do
  echo "Defragmenting $ep ..."
  etcdctl --endpoints=$ep defrag
done

wait $BENCH_PID
```

Check status again:

```bash
etcdctl --endpoints=$EP endpoint status --write-out=table | tee results/endpoint_status_after_compact.txt
```

**Summarize the second CSV using the same Python snippet from Part 3.3, changing the filename to `results/etcd_meta_during_compact.csv`.**

### 4.3 Record and Interpret

Fill this table in your report:

| Phase | Write avg (ms) | Write p50 | Write p99 | Read avg (ms) | Read p50 | Read p99 |
|------|----------------|-----------|-----------|---------------|----------|----------|
| Baseline | | | | | | |
| During compaction / defrag | | | | | | |

Also record:
- DB size before compaction
- DB size after compaction / defrag

**Questions for report:**
1. Why does etcd need compaction at all?
2. Why might compaction or defragmentation increase latency temporarily?
3. Why is this still acceptable for etcd's intended role?

---

## Deliverables

Submit a `lab10B/` directory containing:

```text
lab10B/
├── scripts/
│   └── etcd_meta_bench.sh
├── results/
│   ├── endpoint_status_initial.txt
│   ├── endpoint_status_after_watch.txt
│   ├── endpoint_status_before_compact.txt
│   ├── endpoint_status_after_compact.txt
│   ├── leader_record_get.json
│   ├── leader_record_final.txt
│   ├── etcd_meta_baseline.csv
│   └── etcd_meta_during_compact.csv
└── lab10B_report.md
```

---

## Grading Rubric

| Criterion | Points |
|-----------|--------|
| Part 1: Correct cluster launch and status capture | 20 |
| Part 2: Revisions and watch observations | 25 |
| Part 3: Metadata read/write measurement | 25 |
| Part 4: Compaction / defrag experiment and analysis | 20 |
| Report quality and mechanism-based explanations | 10 |
| **Total** | **100** |

---

## Common Issues

### `etcdctl: command not found`

Re-run the tool installation step and verify `/usr/local/bin` is in your `PATH`.

### A container exits immediately

Check logs:

```bash
docker logs etcd1 | tail -20
docker logs etcd2 | tail -20
docker logs etcd3 | tail -20
```

A common cause is forgetting to recreate `etcd-net` after cleanup.

### `connection refused`

Wait a few seconds after launching the nodes, then retry:

```bash
etcdctl --endpoints=$EP endpoint health
```

### Docker permission denied

```bash
sudo usermod -aG docker $USER
# Then log out and back in, or use: sudo docker ...
```

### `defrag` seems slow

That is normal. `defrag` is real maintenance work, not a metadata-only operation. Because we defrag each of the three members in turn, the latency spike in the during-compaction CSV is the *sum* of three stop-the-world windows, not one.

### Do not `docker rm` an etcd container mid-lab

The containers are launched without a host volume mount, so the data dir lives inside the container filesystem. `docker stop` / `docker start` are safe (state survives), but `docker rm` (or `docker rm -f`) wipes the data dir. If you remove one node, the cluster will keep running on the surviving two (still has quorum), but you cannot simply re-`docker run` it back with `--initial-cluster-state new` — that flag is only valid for the *initial* bring-up of all three. If you need to recover, the simplest path is to tear down all three and restart from Part 1.

---

## Before You Submit

The report template (`lab10B_report_template.md`) contains additional reflection and synthesis questions beyond what each Part explicitly asks. Open the template **before you tear down the cluster** — in particular:

- **§2.2** connects this lab back to the Raft quorum reasoning from Week 7.
- **§3.2** asks you to compare watch with polling and to discuss MVCC.
- **§4.2** asks for *two* architectural reasons for the read/write gap, not just one.
- **§6 (Synthesis)** and **§7 (Reflection)** tie Week 10A and Week 10B together — what etcd is paying for, where truth lives first, when `OK` is honest.

Skim those sections before running `Cleanup`, in case you want to re-run a quick experiment for evidence (e.g. one extra `etcdctl get` to nail down a number).

When you are ready to submit, **rename `lab10B_report_template.md` → `lab10B_report.md`** and place it at the top of the `lab10B/` directory shown under *Deliverables*.

---

## Cleanup

```bash
docker rm -f etcd1 etcd2 etcd3 2>/dev/null || true
docker network rm etcd-net 2>/dev/null || true
```
