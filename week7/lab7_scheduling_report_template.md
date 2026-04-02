# Lab 7 Report: Cluster Scheduling — Algorithms and Observation

**Name:**  
**Student ID:**  
**Date:**  

---

## 1. Environment

| Property | Value |
|----------|-------|
| Host OS | |
| Python version | |
| kind version | |
| kubectl version | |
| Docker version | |
| CPU / RAM available | |
| Any environment limitation | |

### Setup notes

Briefly describe any setup issues and how you resolved them (e.g., Docker installation, inotify limits, cluster creation errors).

---

## 2. Part A — Scheduler implementation

### 2.1 FIFO implementation

Paste your completed `schedule_fifo()` code:

```python
# paste here
```

Describe your implementation approach (2–3 sentences):

<your answer>

### 2.2 Backfill implementation

Paste your completed `schedule_backfill()` code:

```python
# paste here
```

Describe your implementation approach (2–3 sentences):

<your answer>

### 2.3 DRF implementation

Paste your completed `schedule_drf()` code:

```python
# paste here
```

Describe your implementation approach (2–3 sentences):

<your answer>

### 2.4 Simulation output

Paste the Gantt chart output for all three schedulers (`python3 scheduler_sim.py`):

```text
# paste here
```

---

## 3. Part B — Workload analysis

### 3.1 Comparison table

Fill in from your simulation output:

| Metric | FIFO | Backfill | DRF |
|--------|------|----------|-----|
| Makespan (total time) | | | |
| Avg completion time | | | |
| Avg wait time | | | |
| CPU utilization | | | |
| Jain's fairness index | | | |

### 3.2 Analysis questions

**Q1. Backfill vs. FIFO:** By how much did backfill reduce the makespan? Which specific jobs were backfilled (ran earlier than they would under FIFO)?

<your answer>

**Q2. DRF's fairness impact:** Compare the per-user CPU-time distribution. Under FIFO, does Bob (large jobs) get more total CPU-time than Alice? How does DRF change this?

<your answer>

**Q3. The trade-off:** Which algorithm has the best makespan? The best fairness? Can you find an algorithm that wins on both? Why or why not?

<your answer>

**Q4. Gang scheduling impact:** Identify Bob's gang-scheduled jobs (b1: 2 nodes, b3: 3 nodes). How does each algorithm handle them? Does gang scheduling create more waiting for other users?

<your answer>

### 3.3 Bonus: custom workload where DRF loses on makespan

Paste your custom workload and explain why DRF produces a worse makespan than FIFO on it:

```python
# paste workload here
```

<your explanation>

---

## 4. Part C — K8s scheduler observation

### 4.1 Cluster setup

| Property | Value |
|----------|-------|
| kind cluster name | `sched-lab` |
| Node image | `kindest/node:v1.29.4` |
| Number of nodes | 3 (1 control-plane + 2 workers) |

Paste the output of `kubectl get nodes`:

```text
# paste here
```

### 4.2 Scheduling events (C2)

Paste the output from `kubectl get events --watch --field-selector reason=Scheduled` showing `small-job` being scheduled:

```text
# paste here
```

Which node was `small-job` scheduled on, and why not the control-plane?

<your answer>

### 4.3 Resource contention (C3)

Paste the output of `kubectl describe nodes | grep -A 5 "Allocated resources"` after creating the filler pods:

```text
# paste here
```

How many filler pods were successfully scheduled, and how many remained Pending? Why?

<your answer>

### 4.4 Scheduling failure (C4)

Paste the `FailedScheduling` event for `big-job`:

```text
# paste here
```

Which **filter plugin** rejected the Pod, and what exactly did it check?

<your answer>

Did the scheduler attempt preemption? What was the result?

<your answer>

### 4.5 Scheduling after freeing resources (C5)

| Item | Value |
|------|-------|
| Time `big-job` spent in Pending | |
| Filter that rejected it | |
| Node it was eventually scheduled on | |

Paste the `-w` watch output showing the status transition:

```text
# paste here
```

If multiple nodes were available, which **scoring plugin(s)** would the scheduler have used to pick between them?

<your answer>

---

## 5. Reflection

Answer **at least one** of the following:

**Option 1.** You've now seen scheduling at three levels: CPU scheduling (Week 4), cluster simulation (Part A), and real Kubernetes (Part C). What surprised you most about the differences?

<your answer>

**Option 2.** If you were designing a scheduler for a cluster running **only AI training jobs** (long-running, gang-scheduled, GPU-hungry), which of the three approaches (FIFO / Backfill / DRF) would you start from? What modifications would you make?

<your answer>

---

## 6. Attached evidence checklist

- [ ] `schedule_fifo()` code
- [ ] `schedule_backfill()` code
- [ ] `schedule_drf()` code
- [ ] Simulation Gantt chart output (all three schedulers)
- [ ] `kubectl get nodes` output (3 nodes Ready)
- [ ] `small-job` scheduling event
- [ ] `big-job` FailedScheduling event
- [ ] `big-job` Pending → Running watch output
