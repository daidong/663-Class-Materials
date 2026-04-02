# Lab 7 Report: Observing Raft Consensus in etcd

**Name:**  
**Student ID:**  
**Date:**  

---

## 1. Environment

| Property | Value |
|----------|-------|
| Host OS | |
| Docker version | |
| etcd image tag | `quay.io/coreos/etcd:v3.5.17` or equivalent |
| CPU / RAM available | |
| Any environment limitation | |

### Setup notes

Briefly describe any setup issues and how you resolved them.

---

## 2. Part A — Initial cluster state

### 2.1 Endpoint status before failures

| Endpoint | Role (leader/follower) | Term | Raft index | Notes |
|----------|-------------------------|------|------------|-------|
| etcd1 | | | | |
| etcd2 | | | | |
| etcd3 | | | | |

### 2.2 Replication check

Record the result of writing `/lab7/msg` and reading it back from all three nodes.

```text
Paste relevant command output here
```

### 2.3 Short explanation

Why does a successful `put` imply the entry was committed by a **majority**, not just accepted by one node?

---

## 3. Part B — Leader failure and re-election

### 3.1 Failover summary

| Item | Value |
|------|-------|
| Old leader | |
| New leader | |
| Old term | |
| New term | |
| Approximate election / recovery time | |

### 3.2 Evidence from logs

Paste 2-4 log lines that show election-related behavior.

```text
Paste log lines here
```

### 3.3 Mechanism explanation

Explain all of the following in full sentences:

1. Why does leader failure cause temporary unavailability?
2. Why does it **not** create split-brain?
3. Why does the restarted node return as a **follower**?

---

## 4. Part C — Consensus cost and measurement methodology

### 4.1 Phase 1: quick benchmark table

| Configuration | Total time | Avg write latency | Throughput | Avg read latency |
|---------------|------------|-------------------|------------|------------------|
| Single-node writes | | | | N/A |
| 3-node writes | | | | N/A |
| 3-node linearizable reads | N/A | N/A | N/A | |
| 3-node serializable reads | N/A | N/A | N/A | |

### 4.2 Phase 1: why the gap may look smaller than expected

Did Phase 1 clearly show the expected performance gap from the slides?

<your answer>

Discuss which overheads may dominate this quick benchmark:

- `docker exec` on every operation
- launching a fresh `etcdctl` process every time
- host-shell loop overhead
- same-VM / same-disk / same-Docker-host placement
- VirtualBox / rootless-Docker overhead

### 4.3 Phase 2: cleaner benchmark summary

Run Phase 2 with the **host-installed** `benchmark` tool from the Ubuntu VM host. Repeat each benchmark three times and summarize the **median** result.

Use the **host-reachable** endpoints from the lab instructions:

- `SINGLE_EP=http://127.0.0.1:2390`
- `HOST_CLUSTER_EP=http://127.0.0.1:2379,http://127.0.0.1:2381,http://127.0.0.1:2382`

Do **not** use container-name endpoints such as `etcd-single:2379` or `etcd2:2379` in Phase 2, because the host-installed `benchmark` tool cannot resolve Docker-internal container DNS names.

Do **not** use `--target-leader` in this deployment. In our lab setup, etcd may advertise container-internal leader addresses, which can make a host-side benchmark fail name resolution. Instead, point `benchmark` directly at the host-reachable endpoints and let etcd forward writes internally when needed.

| Configuration | Avg latency | p50 | p95 | p99 | Throughput |
|---------------|-------------|-----|-----|-----|------------|
| Single-node writes | | | | | |
| 3-node writes | | | | | |
| 3-node linearizable reads | | | | | |
| 3-node serializable reads | | | | | |

### 4.4 Interpretation

Answer all of the following in full sentences:

1. Did Phase 2 make the 1-node vs 3-node write difference easier to see? Why?
2. Did Phase 2 make the linearizable vs serializable read difference easier to see? Why?
3. Why is Phase 2 methodologically better than Phase 1?
4. Why are 3-node writes slower than single-node writes?
5. Where does the extra latency come from in the etcd write path?
6. Why are serializable reads faster?
7. What guarantee are you giving up with `--consistency=s`?

---

## 5. Part D — Leader failure under sustained load

### 5.1 Load experiment summary

| Metric | Value |
|--------|-------|
| Number of failed writes | |
| Last successful write before failure / before spike | |
| First successful write after recovery / after spike | |
| Estimated unavailability gap or disturbance window | |

If your run showed **explicit `ERR` lines**, summarize the gap from the last pre-failure `OK` to the first post-recovery `OK`.

If your run showed **no `ERR` lines**, summarize the temporary **latency spike** instead: identify the baseline latency range, the elevated-latency window around failover, and when latency returned near baseline. A result with `0` failed writes is still valid if you can show a failover-related disturbance.

### 5.2 Output evidence

Paste a short excerpt of the `OK` / `ERR` sequence. If there were no `ERR` lines, paste a short excerpt that shows the latency spike instead.

```text
Paste log excerpt here
```

### 5.3 Explanation

Why is the observed gap roughly equal to:

`election timeout + election duration + client retry / reconnection effects`?

---

## 6. Part E — Minority partition

### 6.1 Partition setup

Which node was isolated, and was it a leader or follower at the time?

### 6.2 Observed behavior

| Side | Could it still make progress? | Evidence |
|------|-------------------------------|----------|
| Majority side | | |
| Minority side | | |

### 6.3 Explanation

Explain why a **minority partition cannot elect a leader or commit new writes**.

Then explain why this property prevents split-brain.

---

## 7. Part F — Follower lag and catch-up

### 7.1 Before / after evidence

| Moment | etcd1 index | etcd2 index | etcd3 index | Notes |
|--------|-------------|-------------|-------------|-------|
| Before follower stop | | | | |
| After writes while follower is down | | | | |
| After follower restarts | | | | |

### 7.2 Key-value verification

Record whether the restarted follower can read a key written during its downtime.

```text
Paste relevant output here
```

### 7.3 Explanation

Did the follower appear to catch up via ordinary log replication, or do you have evidence that a snapshot was needed?

For this lab's scale, what behavior did you expect and why?

---

## 8. Synthesis: map observations to concepts

Match your experimental observations to the following concepts from the slides.

| Slide concept | Your evidence | Your explanation |
|---------------|---------------|------------------|
| Term | | |
| Leader election | | |
| Quorum intersection | | |
| Commit rule | | |
| Linearizable read | | |
| Serializable read | | |
| Follower recovery | | |

---

## 9. Advanced analysis

Answer at least **two** of the following.

### 9.1 Why are etcd clusters commonly 3 or 5 nodes, rather than 7 or 9?

<your answer>

### 9.2 The lecture says etcd is often `fsync`-bound. Design an experiment to test that claim.

<your answer>

### 9.3 If etcd is unavailable but worker nodes are still alive, what continues to run, and what stops working first in Kubernetes?

<your answer>

### 9.4 Compare Raft leader failover with recovery after a single-node kernel panic.

<your answer>

---

## 10. Reflection

### 10.1 What was the most surprising observation?

<your answer>

### 10.2 What part of Raft became clearer after running the lab?

<your answer>

### 10.3 What remains confusing or worth exploring further?

<your answer>

---

## 11. Attached evidence checklist

- [ ] endpoint status output before failure
- [ ] endpoint status output after failover
- [ ] election-related logs
- [ ] benchmark outputs
- [ ] load-test excerpt (`OK` / `ERR`)
- [ ] minority partition evidence
- [ ] follower catch-up evidence
- [ ] any extra screenshots or notes
