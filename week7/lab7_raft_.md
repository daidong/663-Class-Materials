# Lab 7: Observing Raft Consensus in etcd

> **Goal:** Build a 3-node etcd cluster, map real observations to the Week 7 slides (`week7/week7_paxos.pdf`), and explain consensus behavior using **terms, elections, quorum intersection, log replication, commit, and recovery**.

# Learning objectives

By the end of this lab, you should be able to:

* Identify the current **leader**, **term**, and **Raft index** of an etcd cluster;

* explain why a leader crash causes temporary unavailability but not split-brain;

* relate write latency to the etcd write path: **leader append -> WAL fsync -> follower replication -> majority ack -> commit**;

* compare **linearizable** and **serializable** reads;

* explain why a **minority partition cannot make progress**;

* distinguish **benchmark-harness overhead** from the actual cost of etcd / Raft operations;

* observe how a lagging follower catches up after missing committed entries.

***

## Overview

| Part | Focus                            | Main concept from slides                | Required output               |
| ---- | -------------------------------- | --------------------------------------- | ----------------------------- |
| A    | Build cluster and inspect status | terms, leader, Raft index               | cluster status table          |
| B    | Kill leader and observe election | leader election, randomized timeout     | election timeline             |
| C    | Benchmark 1-node vs 3-node       | etcd write/read path, consistency modes | Phase 1 vs Phase 2 comparison |
| D    | Failure under load               | availability vs liveness                | failover disturbance evidence |
| E    | Network partition                | quorum intersection, no split-brain     | majority/minority behavior    |
| F    | Follower catch-up                | log replication, recovery, snapshots    | before/after index evidence   |

***

## Preflight

### Required tools

```bash
docker version
```

This lab uses the official etcd image and runs all `etcdctl` commands inside containers, so you do **not** need a host installation of `etcdctl`.

If you do not yet have a working Ubuntu VM, first complete the course VM setup in `week1/README_VirtualBox_Setup.md`.

### If Docker is not installed in your VirtualBox Ubuntu VM

For this class, the fastest setup is the Docker **convenience script** plus **rootless mode** inside the Ubuntu guest:

```bash
sudo apt-get update
sudo apt-get install -y curl
curl -fsSL https://get.docker.com | sudo sh
sudo apt-get install -y uidmap
dockerd-rootless-setuptool.sh install
export DOCKER_HOST=unix:///run/user/$(id -u)/docker.sock
systemctl --user start docker
docker version
docker run --rm hello-world
```

What each step is doing:

* `sudo apt-get install -y curl`: installs `curl` so you can download the installer script.

* `curl -fsSL https://get.docker.com | sudo sh`: downloads and runs Docker's convenience installer, which quickly installs the Docker engine and CLI packages in the Ubuntu VM.

* `sudo apt-get install -y uidmap`: installs `newuidmap` / `newgidmap`, which Docker rootless mode needs for user-namespace ID mapping.

* `dockerd-rootless-setuptool.sh install`: configures a **per-user** Docker daemon, so you can use Docker without talking to the root-owned system socket.

* `export DOCKER_HOST=unix:///run/user/$(id -u)/docker.sock`: tells the Docker CLI to talk to your **rootless** Docker socket.

* `systemctl --user start docker`: starts your user-level Docker service.

* `docker version` and `docker run --rm hello-world`: verify that the client can reach the daemon and launch containers successfully.

If `dockerd-rootless-setuptool.sh` is not found, install the extra package and retry:

```bash
sudo apt-get install -y docker-ce-rootless-extras
```

If you open a **new terminal**, re-run:

```bash
export DOCKER_HOST=unix:///run/user/$(id -u)/docker.sock
```

### Recommended environment

* Ubuntu 22.04 or 24.04 guest VM in VirtualBox

* at least 2 vCPUs and \~2 GB free RAM inside the VM (4 GB VM allocation recommended)

* enough free disk space to pull container images (\~5-10 GB is comfortable)

* VirtualBox NAT networking is sufficient for this lab; you do **not** need a special host-only adapter just to run the 3-node cluster

* stable laptop power mode (avoid aggressive sleep during timing experiments)

**VM note:** Docker inside an Ubuntu VM does **not** require nested virtualization for this lab. The containers share the Ubuntu guest kernel; as long as Docker works in the guest, the etcd containers can talk to each other over Docker's bridge network. Rootless Docker is also fine here because the lab only binds unprivileged ports (`2379`, `2381`, `2382`, `2390`).

### Cleanup from previous attempts

Run this before starting if you have tried the lab before:

```bash
docker stop etcd1 etcd2 etcd3 etcd-single 2>/dev/null || true
docker rm etcd1 etcd2 etcd3 etcd-single 2>/dev/null || true
docker network rm etcd-net 2>/dev/null || true
```

### Helper convention used in this lab

We will run `etcdctl` from inside a container and set API v3 explicitly.

The lab is pinned to **etcd v3.5.17** because the `v3.5` docs and release materials support the exact operations used here, including `endpoint health`, `endpoint status -w table`, and the v3 `put/get` workflow. The etcd release page lists `gcr.io/etcd-development/etcd` as the primary registry and `quay.io/coreos/etcd` as a secondary registry, so the following tag is valid for this lab:

```bash
export ETCD_IMAGE=quay.io/coreos/etcd:v3.5.17
# Fallback if Quay is slow or blocked in your region:
# export ETCD_IMAGE=gcr.io/etcd-development/etcd:v3.5.17
```

Before continuing, verify that the image can run the binaries we need:

```bash
docker pull ${ETCD_IMAGE}
docker run --rm ${ETCD_IMAGE} etcd --version
docker run --rm ${ETCD_IMAGE} etcdctl version
```

Expected result: both commands report version `3.5.17` (or a matching `etcdctl` 3.5.17 build), with no `command not found` error.

For Part C Phase 2, install the official `benchmark` tool on the **Ubuntu VM host**. The most reliable method for `v3.5.17` is to build it from the matching etcd source tree:

```bash
sudo apt update
sudo apt install -y git golang-go

go version
git --version

mkdir -p ~/src
cd ~/src
git clone --depth 1 --branch v3.5.17 https://github.com/etcd-io/etcd.git
cd etcd
go install -v ./tools/benchmark

export PATH=$PATH:$(go env GOPATH)/bin
benchmark --help | head -20
```

If you prefer not to modify `PATH`, use the full binary path instead:

```bash
~/go/bin/benchmark --help | head -20
```

Why use this workflow instead of `go install go.etcd.io/...@v3.5.17`? For etcd `v3.5.17`, the source-tree build is more reliable because the repository uses module `replace` directives that can make the direct module-install approach fail.

Also define these helpers in your **host shell** once before Part A / Part C:

```bash
etcdctl_in() {
  docker exec -e ETCDCTL_API=3 "$1" etcdctl "${@:2}"
}

# Container-reachable endpoints (used by etcdctl commands executed inside containers)
export CLUSTER_EP=http://etcd1:2379,http://etcd2:2379,http://etcd3:2379

# Host-reachable endpoints (used by the host-installed benchmark tool in Part C Phase 2)
export SINGLE_EP=http://127.0.0.1:2390
export HOST_CLUSTER_EP=http://127.0.0.1:2379,http://127.0.0.1:2381,http://127.0.0.1:2382
```

We use `etcdctl_in` because some etcd container images do **not** include `/bin/sh`, so commands of the form `docker exec <container> sh -lc '...'` can fail with `executable file not found in $PATH`. The helper calls `etcdctl` directly and avoids that problem.

***

## Part A. Build a 3-node etcd cluster and inspect Raft state

### A1. Create the Docker network

```bash
docker network create etcd-net
```

### A2. Launch three etcd nodes

```bash
# etcd1
docker run -d --name etcd1 --network etcd-net -p 2379:2379 \
  ${ETCD_IMAGE} etcd \
  --name node1 \
  --initial-advertise-peer-urls http://etcd1:2380 \
  --listen-peer-urls http://0.0.0.0:2380 \
  --advertise-client-urls http://etcd1:2379 \
  --listen-client-urls http://0.0.0.0:2379 \
  --initial-cluster node1=http://etcd1:2380,node2=http://etcd2:2380,node3=http://etcd3:2380 \
  --initial-cluster-state new \
  --initial-cluster-token lab7-cluster

# etcd2
docker run -d --name etcd2 --network etcd-net -p 2381:2379 \
  ${ETCD_IMAGE} etcd \
  --name node2 \
  --initial-advertise-peer-urls http://etcd2:2380 \
  --listen-peer-urls http://0.0.0.0:2380 \
  --advertise-client-urls http://etcd2:2379 \
  --listen-client-urls http://0.0.0.0:2379 \
  --initial-cluster node1=http://etcd1:2380,node2=http://etcd2:2380,node3=http://etcd3:2380 \
  --initial-cluster-state new \
  --initial-cluster-token lab7-cluster

# etcd3
docker run -d --name etcd3 --network etcd-net -p 2382:2379 \
  ${ETCD_IMAGE} etcd \
  --name node3 \
  --initial-advertise-peer-urls http://etcd3:2380 \
  --listen-peer-urls http://0.0.0.0:2380 \
  --advertise-client-urls http://etcd3:2379 \
  --listen-client-urls http://0.0.0.0:2379 \
  --initial-cluster node1=http://etcd1:2380,node2=http://etcd2:2380,node3=http://etcd3:2380 \
  --initial-cluster-state new \
  --initial-cluster-token lab7-cluster
```

**VirtualBox Ubuntu note:** these three `docker run` commands are expected to work inside a normal Ubuntu 22.04/24.04 guest VM. The etcd nodes communicate over Docker's internal bridge network (`etcd-net`) **inside the guest**, so VirtualBox NAT is fine for this lab. You do **not** need a separate VirtualBox host-only network just to form the cluster.

Before moving on, confirm that all three containers stayed up:

```bash
docker ps --format 'table {{.Names}}\t{{.Status}}\t{{.Ports}}'
```

If any container exits immediately, inspect logs before debugging Raft behavior:

```bash
docker logs etcd1 | tail -30
docker logs etcd2 | tail -30
docker logs etcd3 | tail -30
```

Also make sure ports `2379`, `2381`, and `2382` are free **inside the Ubuntu VM**. Because all `etcdctl` commands in this lab run from the guest/containers, you do not need extra VirtualBox port forwarding unless you want to access etcd from the host OS.

### A3. Verify health and inspect leader / term / index

```bash
etcdctl_in etcd1 --endpoints=${CLUSTER_EP} endpoint health
etcdctl_in etcd1 --endpoints=${CLUSTER_EP} endpoint status -w table
```

**Why this matters:** this is the concrete version of the slides on **terms**, **leader election**, and **log state**.

**Record:**

* which endpoint is leader;

* current term;

* Raft index on each node;

* whether all nodes agree on the current committed prefix.

### A4. Write data and confirm replication

```bash
etcdctl_in etcd1 put /lab7/msg "hello-raft"

etcdctl_in etcd1 get /lab7/msg
etcdctl_in etcd2 get /lab7/msg
etcdctl_in etcd3 get /lab7/msg

etcdctl_in etcd1 --endpoints=${CLUSTER_EP} endpoint status -w table
```

**Explain in your report:** why does a successful `put` imply the entry was committed by a **majority**, not merely received by one node?

### A5. Observe election-related logs

```bash
docker logs etcd1 2>&1 | grep -Ei 'raft|leader|elected|campaign|vote' | tail -30
```

Map at least two log lines to slide concepts:

* candidate starts an election;

* node grants a vote / becomes leader / starts serving.

***

## Part B. Kill the leader and observe re-election

### B1. Identify and stop the current leader

Use the Part A status table to identify the leader, then stop it.

```bash
docker stop etcdX
```

Replace `etcdX` with the actual leader.

### B2. Observe the surviving majority

```bash
etcdctl_in etcd2 --endpoints=http://etcd2:2379,http://etcd3:2379 endpoint status -w table

docker logs etcd2 2>&1 | grep -Ei 'raft|leader|elected|campaign|vote|term' | tail -40
```

If `etcd2` was the node you stopped, run the command from `etcd3` instead.

**Record:**

* old leader;

* new leader;

* old term vs new term;

* measured downtime from failure to first successful post-failure write.

### B3. Verify progress still happens with a majority

```bash
etcdctl_in etcd2 put /lab7/after_failover ok
etcdctl_in etcd3 get /lab7/after_failover
```

**Explain:** why can a 3-node cluster survive one crash, but not two simultaneous crashes?

### B4. Restart the failed node and inspect its role

```bash
docker start etcdX
sleep 5
etcdctl_in etcd2 --endpoints=${CLUSTER_EP} endpoint status -w table
```

**Record:** whether the restarted node returns as a **follower** and whether its index catches up.

***

## Part C. Benchmark consensus cost: quick benchmark first, then cleaner measurement

### C1. Launch a single-node control group

```bash
docker run -d --name etcd-single --network etcd-net -p 2390:2379 \
  ${ETCD_IMAGE} etcd \
  --name single \
  --advertise-client-urls http://etcd-single:2379 \
  --listen-client-urls http://0.0.0.0:2379
```

### C2. Phase 1 — Quick host-shell benchmark

Use the **same payload** to avoid measuring `/dev/urandom` and `base64` overhead.

```bash
PAYLOAD=$(printf 'x%.0s' $(seq 1 256))

START=$(date +%s%N)
for i in $(seq 1 300); do
  etcdctl_in etcd-single put /bench/single-$i "$PAYLOAD" >/dev/null
done
END=$(date +%s%N)
ELAPSED=$(( (END-START)/1000000 ))
echo "single total_ms=$ELAPSED avg_ms=$((ELAPSED/300)) ops_per_sec=$((300000/ELAPSED))"

START=$(date +%s%N)
for i in $(seq 1 300); do
  etcdctl_in etcd2 --endpoints=${CLUSTER_EP} put /bench/cluster-$i "$PAYLOAD" >/dev/null
done
END=$(date +%s%N)
ELAPSED=$(( (END-START)/1000000 ))
echo "cluster total_ms=$ELAPSED avg_ms=$((ELAPSED/300)) ops_per_sec=$((300000/ELAPSED))"
```

**Important:** `etcd2` does **not** need to be leader here, but it must be **alive**. Because the client uses `--endpoints=${CLUSTER_EP}`, it can still talk to the cluster through the endpoint list.

### C3. Phase 1 — Quick read comparison

```bash
START=$(date +%s%N)
for i in $(seq 1 300); do
  etcdctl_in etcd2 --endpoints=${CLUSTER_EP} get /lab7/msg >/dev/null
done
END=$(date +%s%N)
ELAPSED=$(( (END-START)/1000000 ))
echo "linearizable total_ms=$ELAPSED avg_ms=$((ELAPSED/300))"

START=$(date +%s%N)
for i in $(seq 1 300); do
  etcdctl_in etcd2 --endpoints=${CLUSTER_EP} get /lab7/msg --consistency=s >/dev/null
done
END=$(date +%s%N)
ELAPSED=$(( (END-START)/1000000 ))
echo "serializable total_ms=$ELAPSED avg_ms=$((ELAPSED/300))"
```

### C4. Analyze why Phase 1 may fail to show the expected gap

Before moving on, write down whether the results looked **less/more different than the slides suggested**?

In your report, explicitly discuss which of the following may dominate Phase 1:

* `docker exec` overhead on every operation;

* starting a fresh `etcdctl` process for every operation;

* client-side shell loop overhead;

* all three etcd nodes running on the **same VM / same virtual disk / same Docker host**;

* VirtualBox and rootless-Docker overhead masking sub-millisecond differences.

**Key idea:** P**benchmark harness** overhead v.s. actually measures etcd.

### C5. Phase 2 — Cleaner measurement with etcd's benchmark tool

The official etcd performance docs describe a dedicated `benchmark` CLI for measuring etcd with persistent connections. In this lab, run the `benchmark` binary from the **Ubuntu VM host**, not from `docker exec` loops. This avoids the **one** **`docker exec`** **per request** pattern used in Phase 1 while also making the endpoint names explicit.

First, confirm that the host-installed benchmark tool works:

```bash
benchmark --help | head -20
# or, without editing PATH:
~/go/bin/benchmark --help | head -20
```

For Phase 2, use the **host-reachable** endpoint variables from Preflight:

* `SINGLE_EP=http://127.0.0.1:2390`
* `HOST_CLUSTER_EP=http://127.0.0.1:2379,http://127.0.0.1:2381,http://127.0.0.1:2382`

Do **not** use container-name endpoints such as `http://etcd-single:2379` or `http://etcd2:2379` with the host-installed benchmark tool.

Also do **not** use `--target-leader` in this deployment. Here the etcd nodes advertise **container-internal** addresses such as `etcd-single:2379` or `etcd2:2379`. If `benchmark` runs on the host and you pass `--target-leader`, it may ask etcd for the leader address, receive a container-only hostname, and then fail to resolve it from the host. For this lab, simply give `benchmark` the host-reachable endpoints directly and let etcd forward writes to the leader as needed.

Then run the cleaner benchmarks below. Repeat **each command three times** and use the **median** throughput / latency numbers in your report.

#### C5.1 Single-node writes

```bash
benchmark --endpoints=${SINGLE_EP} --conns=1 --clients=1 \
  put --key-size=8 --sequential-keys --total=10000 --val-size=256
```

#### C5.2 3-node writes

```bash
benchmark --endpoints=${HOST_CLUSTER_EP} --conns=1 --clients=1 \
  put --key-size=8 --sequential-keys --total=10000 --val-size=256
```

#### C5.3 3-node linearizable reads

```bash
benchmark --endpoints=${HOST_CLUSTER_EP} --conns=1 --clients=1 \
  range /lab7/msg --consistency=l --total=10000
```

#### C5.4 3-node serializable reads

```bash
benchmark --endpoints=${HOST_CLUSTER_EP} --conns=1 --clients=1 \
  range /lab7/msg --consistency=s --total=10000
```

Record from the benchmark output:

* average latency;

* p50 / p95 / p99 latency;

* requests per second.

### C6. Explain why Phase 2 is methodologically better

Use full sentences to explain **why** the Phase 2 results should be more trustworthy than Phase 1 for performance comparison.

Your explanation should mention at least these ideas:

* fewer client-side process launches;

* persistent connections instead of one-shot CLI calls;

* larger sample size (`10000` instead of `300`);

* repeated trials with a median summary.

Then connect the improved measurements back to the slides:

* why are 3-node writes slower than single-node writes?

* why are serializable reads usually faster than linearizable reads?

* what guarantee are you giving up with `--consistency=s`?

***

## Part D. Kill the leader during sustained writes

### D1. Start a write loop

```bash
# Pick a container that will stay up as the client launcher.
# Example: if you plan to stop etcd2, use etcd1 or etcd3 here.
CLIENT_NODE=etcd1

i=0
while true; do
  i=$((i+1))
  START=$(date +%s%N)
  if etcdctl_in ${CLIENT_NODE} --endpoints=${CLUSTER_EP} put /load/key-$i value-$i >/dev/null 2>&1; then
    END=$(date +%s%N)
    echo "OK $i $(( (END-START)/1000000 ))ms"
  else
    END=$(date +%s%N)
    echo "ERR $i $(( (END-START)/1000000 ))ms"
  fi
  sleep 0.1
done | tee /tmp/etcd_load_test.log
```

Choose any **live** `CLIENT_NODE` that you will not kill, and keep `--endpoints=${CLUSTER_EP}` so the client can follow the cluster across failover.

### D2. In a second terminal, stop the current leader

First verify which node is currently leader, then stop that node:

```bash
etcdctl_in etcd1 --endpoints=${CLUSTER_EP} endpoint status -w table
```

Then stop the leader container:

```bash
docker stop etcdX
```

If you accidentally stop a follower instead of the leader, the write loop may show little or no visible disturbance.

### D3. Measure the availability gap

After the cluster recovers, stop the loop with `Ctrl+C` and inspect the log:

```bash
grep -c '^ERR' /tmp/etcd_load_test.log
grep -E '^(OK|ERR)' /tmp/etcd_load_test.log | head -60
```

You may observe either of these outcomes:

1. **explicit failures**: a short run of `ERR` lines during leader failover; or
2. **latency-only disturbance**: little or no `ERR`, but one or more `OK` lines with noticeably higher latency than the steady-state baseline.

Both outcomes are acceptable for this lab. In this setup, the client talks to `--endpoints=${CLUSTER_EP}` rather than a single fixed node, and `etcdctl` / gRPC may reconnect or retry while the new leader is being elected. As a result, some runs show visible `ERR` lines, while others show a temporary latency spike but still end in `OK`.

**Record:**

* number of failed writes (this may be `0`);

* if you saw `ERR`, estimate the duration from the last pre-failure `OK` to the first post-recovery `OK`;

* if you saw no `ERR`, estimate the disturbance window from the start of the latency spike to the return to near-baseline latency;

* whether the observed pause / spike is roughly consistent with election timeout + election + client reconnect / retry.

If your run shows no `ERR`, do **not** treat that as a failed experiment. Instead, explain that the failover was visible as increased latency rather than as application-visible write failures.

***

## Part E. Simulate a minority partition

This part targets the slide claim: **a minority partition cannot elect a leader and cannot commit writes**.

### E1. Make one follower the minority side

Pick a follower, for example `etcd3`, and disconnect it from the cluster network:

```bash
docker network disconnect etcd-net etcd3
```

### E2. Compare majority vs minority behavior

From the majority side:

```bash
etcdctl_in etcd1 --endpoints=${CLUSTER_EP} put /partition/test majority-ok
```

From the minority side, the local process may still run, but it should **not** be able to make the cluster commit new writes by itself.

Try to inspect logs:

```bash
docker logs etcd3 2>&1 | tail -40
```

**Record and explain:**

* majority side still makes progress;

* minority side cannot form a quorum;

* no second committed leader appears.

### E3. Reconnect the minority node

```bash
docker network connect etcd-net etcd3
sleep 5
```

Then re-run endpoint status and confirm recovery.

**Note:** depending on Docker/Desktop version, client behavior on the disconnected node may vary. The required conclusion is about **quorum and progress**, not UI polish.

***

## Part F. Observe follower lag and catch-up

### F1. Stop a follower

Choose a follower, not the leader.

```bash
docker stop etcd3
```

### F2. Commit writes while it is down

```bash
for i in $(seq 1 30); do
  etcdctl_in etcd1 --endpoints=http://etcd1:2379,http://etcd2:2379 put /catchup/key-$i value-$i >/dev/null
done

etcdctl_in etcd1 --endpoints=http://etcd1:2379,http://etcd2:2379 endpoint status -w table
```

### F3. Restart the lagging follower and observe recovery

```bash
docker start etcd3
sleep 5

etcdctl_in etcd1 --endpoints=${CLUSTER_EP} endpoint status -w table
etcdctl_in etcd3 get /catchup/key-30
```

**Explain:** did the follower catch up via ordinary log replication, or do you have evidence that a snapshot was needed? For a small gap like 30 keys, normal AppendEntries-style catch-up is the expected answer.

***

## Troubleshooting

* **`container name already in use`**: run the cleanup block and retry.

* **`network etcd-net already exists`**: remove it first or ignore if the cluster is fresh.

* **`Cannot connect to the Docker daemon at unix:///var/run/docker.sock`** after rootless setup: re-run `export DOCKER_HOST=unix:///run/user/$(id -u)/docker.sock`, then `systemctl --user start docker`.

* **`OCI runtime exec failed ... exec: "sh": executable file not found in $PATH`**: do **not** use `docker exec <container> sh -lc ...` with this image. Use the `etcdctl_in` helper from Preflight, which calls `etcdctl` directly.

* **one endpoint hangs after failure**: query only surviving endpoints for status.

* **writes fail after reconnect**: wait a few seconds and inspect `docker logs <node>`.

* **strange benchmark numbers**: close background workloads and rerun; this lab is about relative behavior, not microsecond precision.

***

## Report requirements

Your report must include the following sections.

### 1. Cluster state and election evidence

Provide a table with endpoint, leader/follower role, term, and Raft index before and after failover.

### 2. Consensus cost measurements

Provide **two** Part C summaries.

**Phase 1: quick benchmark table**

| Configuration            | Avg write latency | Throughput | Avg read latency |
| ------------------------ | ----------------- | ---------- | ---------------- |
| Single node              | <br />            | <br />     | <br />           |
| 3-node write path        | <br />            | <br />     | <br />           |
| 3-node linearizable read | N/A               | N/A        | <br />           |
| 3-node serializable read | N/A               | N/A        | <br />           |

**Phase 2: cleaner benchmark summary**

| Configuration            | Avg latency | p50    | p95    | p99    | Throughput |
| ------------------------ | ----------- | ------ | ------ | ------ | ---------- |
| Single node writes       | <br />      | <br /> | <br /> | <br /> | <br />     |
| 3-node writes            | <br />      | <br /> | <br /> | <br /> | <br />     |
| 3-node linearizable read | <br />      | <br /> | <br /> | <br /> | <br />     |
| 3-node serializable read | <br />      | <br /> | <br /> | <br /> | <br />     |

Also include a short paragraph on **measurement methodology** explaining why Phase 1 may fail to reveal the expected performance gap.

### 3. Mechanism explanations

Answer all of these:

1. Why does a committed etcd write require a **majority**?
2. Why does leader failure cause a pause but not split-brain?
3. Why are serializable reads faster than linearizable reads?
4. Why can a minority partition not make progress?
5. Why does a restarted follower rejoin as a follower instead of “continuing as leader”?
6. Why is Phase 2 a better performance experiment than Phase 1?

### 4. Advanced analysis

Answer at least **two**:

* Why are etcd clusters usually 3 or 5 nodes, not 7 or 9?

* The slides say etcd is often **fsync-bound**. Design an experiment to test that claim.

* If etcd is unavailable but worker nodes are still alive, what happens to already-running workloads? What stops working first?

* Compare Raft leader election with a single-node kernel panic recovery path. What is analogous, and what is fundamentally different?

***

## Cleanup

```bash
docker stop etcd1 etcd2 etcd3 etcd-single 2>/dev/null || true
docker rm etcd1 etcd2 etcd3 etcd-single 2>/dev/null || true
docker network rm etcd-net 2>/dev/null || true
```

