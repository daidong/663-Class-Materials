# Lab 8: Cluster Scheduling — Algorithms and Observation

> **Goal:** Implement two cluster scheduling algorithms (FIFO and Backfill), compare their behavior on the same workload, and observe the Kubernetes scheduler making real decisions.

This lab is the hands-on companion to `week8_real/week8_real_slides.md`.
It turns the Week 8 control-plane story into something students can measure:

* scheduling as a placement problem rather than a CPU-timeslice problem;

* `filter → score → bind` as an observable pipeline;

* fairness, packing, and fragmentation as competing goals.

## Week 7 → Week 8 bridge

In Week 7, you studied how Raft / etcd keep a **single durable control-plane history**.
In this lab, you study what Kubernetes does **after** that shared history exists:

* controllers create Pod objects;

* the scheduler chooses a node for an unscheduled Pod;

* kubelet turns the assignment into running containers;

* status and events flow back through the API server.

So this lab is not just about “one more scheduling algorithm.”
It is about how a distributed control plane turns **agreed state** into **placement action**.

## Overview

| Part                                      | Focus                                                                | Key Measurement                                           |
| ----------------------------------------- | -------------------------------------------------------------------- | --------------------------------------------------------- |
| **A: Scheduler simulation**               | Implement FIFO and Backfill in Python                                | Makespan, avg completion time, avg wait time, utilization |
| **B: Workload analysis**                  | Run both against a mixed workload trace                              | Compare metrics across algorithms                         |
| **C: K8s scheduler observation**          | Use kind to watch filter → score → bind in action                    | Scheduling decisions under resource contention            |
| **D: Conceptual bridge (in your report)** | Connect observations back to API server / etcd / control-plane state | Can you explain where the decision became durable?        |

## Prerequisites

```bash
# Python 3.8+
python3 --version

# For Part C: kind + kubectl + Docker
kind version
kubectl version --client
docker version
```

### Install kind and kubectl if missing (Ubuntu)

For this course, Part C should be run **inside your Ubuntu VM**, not on the macOS host. The host may be an Intel Mac or an Apple Silicon Mac, but the commands below should match the **Ubuntu guest** architecture reported by `uname -m`.

`kubectl` is convenient to install via the official Kubernetes apt repository. For `kind`, it is usually better to use the official release binary instead of `apt install`, because Ubuntu repositories may not provide the current version used by the lab.

```bash
# Install kubectl
sudo apt-get update
sudo apt-get install -y apt-transport-https ca-certificates curl gpg
sudo mkdir -p -m 755 /etc/apt/keyrings
curl -fsSL https://pkgs.k8s.io/core:/stable:/v1.30/deb/Release.key | \
  sudo gpg --dearmor -o /etc/apt/keyrings/kubernetes-apt-keyring.gpg
echo 'deb [signed-by=/etc/apt/keyrings/kubernetes-apt-keyring.gpg] https://pkgs.k8s.io/core:/stable:/v1.30/deb/ /' | \
  sudo tee /etc/apt/sources.list.d/kubernetes.list > /dev/null
sudo apt-get update
sudo apt-get install -y kubectl

# Install kind (choose the right binary for the Ubuntu VM architecture)
ARCH="$(uname -m)"
case "$ARCH" in
  x86_64|amd64) KIND_ARCH="amd64" ;;
  aarch64|arm64) KIND_ARCH="arm64" ;;
  *) echo "Unsupported architecture: $ARCH"; exit 1 ;;
esac
curl -Lo ./kind "https://kind.sigs.k8s.io/dl/v0.31.0/kind-linux-${KIND_ARCH}"
chmod +x ./kind
sudo mv ./kind /usr/local/bin/kind

# Verify
uname -m
kubectl version --client
kind version
docker version
```

If you are running the lab directly on macOS instead of inside Ubuntu, use the macOS installation method for your host platform (for example, Homebrew), but note that the teaching staff will primarily support the Ubuntu VM setup used in this course.

If Docker is missing or the daemon is not running, install Docker Engine for Ubuntu first and make sure `docker version` works before starting Part C.

### Environment note

This course normally runs on **Ubuntu inside VirtualBox**.
For Part C, you should run `kind`, `kubectl`, and Docker in the same Ubuntu environment so the local cluster, container runtime, and CLI tools all see the same host resources.

Because `kind` runs Kubernetes nodes as Docker containers, Part C assumes your VM has enough headroom to run a small multi-node cluster. A practical target is **4 vCPU and 6–8 GB RAM**. If `kind create cluster` fails because of nested-container or memory issues, keep Part A/B as required and treat Part C as best-effort until the environment is fixed.

***

## Part A: Implement two schedulers (40 min)

You are given a simulation framework (`scheduler_sim.py`) with the data structures and metrics code. Your job: implement two scheduling algorithms.

### A1: Create the simulation framework

Save the following as `scheduler_sim.py`:

```python
from __future__ import annotations

"""
Cluster scheduler simulation for Lab 8.
Students implement: schedule_fifo(), schedule_backfill()
"""

import heapq
from dataclasses import dataclass, field
from typing import Optional


@dataclass
class Job:
    """A job to be scheduled."""
    id: str
    user: str
    submit_time: int          # when the job was submitted (time unit)
    cpu: int                  # CPU cores needed
    mem: int                  # GB memory needed
    duration: int             # how long the job runs (time units)
    nodes_required: int       # number of machines needed (use 1 in this lab)
    # Filled in by the scheduler:
    start_time: Optional[int] = None
    end_time: Optional[int] = None


@dataclass
class Machine:
    """A machine in the cluster."""
    id: str
    total_cpu: int
    total_mem: int
    # Current allocations: list of (job_id, cpu, mem, end_time)
    allocations: list = field(default_factory=list)

    def available_cpu(self, at_time: int) -> int:
        used = sum(a[1] for a in self.allocations if a[3] > at_time)
        return self.total_cpu - used

    def available_mem(self, at_time: int) -> int:
        used = sum(a[2] for a in self.allocations if a[3] > at_time)
        return self.total_mem - used

    def can_fit(self, job: Job, at_time: int) -> bool:
        return (self.available_cpu(at_time) >= job.cpu and
                self.available_mem(at_time) >= job.mem)

    def allocate(self, job: Job, start_time: int):
        end_time = start_time + job.duration
        self.allocations.append((job.id, job.cpu, job.mem, end_time))
        return end_time

    def next_free_time(self, cpu_needed: int, mem_needed: int, after: int) -> int:
        """Find earliest time >= after when this machine can fit the request."""
        # Collect all end times as candidate times to check
        times = sorted(set([after] + [a[3] for a in self.allocations if a[3] > after]))
        for t in times:
            if self.available_cpu(t) >= cpu_needed and self.available_mem(t) >= mem_needed:
                return t
        # After all current jobs finish
        last_end = max((a[3] for a in self.allocations), default=after)
        return last_end


def create_cluster(num_machines: int = 4, cpu_per_machine: int = 8,
                   mem_per_machine: int = 32) -> list[Machine]:
    """Create a homogeneous cluster."""
    return [Machine(id=f"node-{i}", total_cpu=cpu_per_machine,
                    total_mem=mem_per_machine) for i in range(num_machines)]


# ──────────────────────────────────────────────────────────────
# WORKLOAD TRACE
# ──────────────────────────────────────────────────────────────

def mixed_workload() -> list[Job]:
    """
    A mixed workload with small interactive jobs and large batch jobs.
    Two users: alice (interactive-heavy) and bob (batch-heavy).
    All jobs use a single machine in this lab so you can focus on FIFO vs. Backfill.
    """
    jobs = [
        # Alice: many small, short interactive jobs
        Job("a1", "alice", submit_time=0,  cpu=1, mem=2,  duration=3,  nodes_required=1),
        Job("a2", "alice", submit_time=1,  cpu=1, mem=2,  duration=2,  nodes_required=1),
        Job("a3", "alice", submit_time=2,  cpu=2, mem=4,  duration=4,  nodes_required=1),
        Job("a4", "alice", submit_time=5,  cpu=1, mem=2,  duration=2,  nodes_required=1),
        Job("a5", "alice", submit_time=6,  cpu=1, mem=1,  duration=1,  nodes_required=1),
        Job("a6", "alice", submit_time=8,  cpu=2, mem=4,  duration=3,  nodes_required=1),
        Job("a7", "alice", submit_time=10, cpu=1, mem=2,  duration=2,  nodes_required=1),
        Job("a8", "alice", submit_time=12, cpu=1, mem=2,  duration=3,  nodes_required=1),

        # Bob: fewer large, long batch jobs
        Job("b1", "bob", submit_time=0,  cpu=4, mem=16, duration=10, nodes_required=1),
        Job("b2", "bob", submit_time=3,  cpu=6, mem=24, duration=8,  nodes_required=1),
        Job("b3", "bob", submit_time=7,  cpu=4, mem=16, duration=12, nodes_required=1),
        Job("b4", "bob", submit_time=15, cpu=2, mem=8,  duration=6,  nodes_required=1),

        # Charlie: medium jobs, arrives later
        Job("c1", "charlie", submit_time=4,  cpu=3, mem=8,  duration=5, nodes_required=1),
        Job("c2", "charlie", submit_time=9,  cpu=2, mem=4,  duration=4, nodes_required=1),
        Job("c3", "charlie", submit_time=11, cpu=4, mem=8,  duration=6, nodes_required=1),
    ]
    return sorted(jobs, key=lambda j: j.submit_time)


# ──────────────────────────────────────────────────────────────
# METRICS
# ──────────────────────────────────────────────────────────────

def compute_metrics(jobs: list[Job], machines: list[Machine]) -> dict:
    """Compute scheduling quality metrics."""
    scheduled = [j for j in jobs if j.start_time is not None]
    if not scheduled:
        return {"error": "no jobs scheduled"}

    makespan = max(j.end_time for j in scheduled)
    avg_completion = sum(j.end_time - j.submit_time for j in scheduled) / len(scheduled)
    avg_wait = sum(j.start_time - j.submit_time for j in scheduled) / len(scheduled)

    # Utilization: total CPU-time used / (total CPU capacity × makespan)
    total_cpu_time = sum(j.cpu * j.duration * j.nodes_required for j in scheduled)
    total_capacity = sum(m.total_cpu for m in machines) * makespan
    utilization = total_cpu_time / total_capacity if total_capacity > 0 else 0

    # Fairness: Jain's fairness index on per-user CPU-time
    user_cpu_time = {}
    for j in scheduled:
        user_cpu_time.setdefault(j.user, 0)
        user_cpu_time[j.user] += j.cpu * j.duration * j.nodes_required
    values = list(user_cpu_time.values())
    n = len(values)
    jain = (sum(values) ** 2) / (n * sum(v ** 2 for v in values)) if n > 0 else 1.0

    return {
        "makespan": makespan,
        "avg_completion_time": round(avg_completion, 2),
        "avg_wait_time": round(avg_wait, 2),
        "utilization": round(utilization, 4),
        "jain_fairness": round(jain, 4),
        "jobs_scheduled": len(scheduled),
        "jobs_total": len(jobs),
        "per_user_cpu_time": user_cpu_time,
    }


def print_schedule(jobs: list[Job], name: str):
    """Print a Gantt-chart-like view of the schedule."""
    scheduled = [j for j in jobs if j.start_time is not None]
    if not scheduled:
        print(f"\n=== {name}: no jobs scheduled ===")
        return

    makespan = max(j.end_time for j in scheduled)
    print(f"\n{'='*60}")
    print(f" {name}")
    print(f"{'='*60}")
    print(f" {'Job':<6} {'User':<10} {'Submit':>6} {'Start':>6} {'End':>6} {'Wait':>6}  Timeline")
    print(f" {'-'*6} {'-'*10} {'-'*6} {'-'*6} {'-'*6} {'-'*6}  {'-'*40}")

    for j in sorted(scheduled, key=lambda x: x.start_time):
        wait = j.start_time - j.submit_time
        bar = '.' * j.start_time + '#' * j.duration + '.' * (makespan - j.end_time)
        # Truncate bar if too long
        if len(bar) > 40:
            bar = bar[:37] + '...'
        print(f" {j.id:<6} {j.user:<10} {j.submit_time:>6} {j.start_time:>6} "
              f"{j.end_time:>6} {wait:>6}  |{bar}|")

    print()


def reset_jobs(jobs: list[Job]) -> list[Job]:
    """Reset scheduling state for reuse."""
    for j in jobs:
        j.start_time = None
        j.end_time = None
    return jobs


# ──────────────────────────────────────────────────────────────
# TODO: Implement the two scheduling algorithms below.
# ──────────────────────────────────────────────────────────────

def schedule_fifo(jobs: list[Job], machines: list[Machine]) -> list[Job]:
    """
    FIFO scheduler: schedule jobs in submission order.
    For each job, find the earliest time when a machine can fit it.

    In this lab, every job uses one machine (`nodes_required = 1`).

    Algorithm:
      1. Process jobs in submit_time order.
      2. For each job, find the earliest time >= submit_time when
         some machine can fit the job.
      3. Assign the job to that machine.
    """
    # TODO: Implement this
    # Hints:
    # - For each job, find the machine with earliest availability
    # - Use machine.next_free_time() and machine.can_fit()
    # - Set job.start_time and job.end_time
    # - Call machine.allocate(job, start_time)
    pass


def schedule_backfill(jobs: list[Job], machines: list[Machine]) -> list[Job]:
    """
    Backfill scheduler: FIFO with backfill optimization.

    The idea: process jobs in FIFO order. If the head-of-queue job can't run
    now, compute its expected start time. Then check if later jobs can
    run now AND finish before the head job's expected start time.

    Algorithm:
      1. Maintain a queue of unscheduled jobs (FIFO order).
      2. At each scheduling round (advance time as needed):
         a. Try to schedule the head-of-queue job.
         b. If it can't run, compute its reservation (earliest start time).
         c. Scan remaining jobs: if a job can run NOW and finishes before
            the reservation time, schedule it (backfill).
      3. Advance time to next event (job completion or new arrival).

    For simplicity, you can implement a simpler version:
      - Schedule in FIFO order.
      - After each FIFO pass, check if any later jobs can be started
        in gaps without delaying any earlier job's start.
    """
    # TODO: Implement this
    # Hints:
    # - First, run FIFO to establish a baseline schedule.
    # - Then, for each unscheduled-at-the-current-time job, check if it can
    #   "fit in the gaps" before the next large job starts.
    # - Key insight: a small job can backfill if it finishes before any
    #   reservation starts.
    pass


# ──────────────────────────────────────────────────────────────
# MAIN: run both and compare
# ──────────────────────────────────────────────────────────────

if __name__ == "__main__":
    results = {}

    for name, scheduler_fn in [
        ("FIFO", schedule_fifo),
        ("Backfill", schedule_backfill),
    ]:
        jobs = mixed_workload()
        machines = create_cluster(num_machines=4, cpu_per_machine=8, mem_per_machine=32)
        scheduler_fn(jobs, machines)
        print_schedule(jobs, name)
        metrics = compute_metrics(jobs, machines)
        results[name] = metrics
        print(f"  Metrics: {metrics}\n")

    # Summary comparison table
    print(f"\n{'='*62}")
    print(f" COMPARISON")
    print(f"{'='*62}")
    print(f" {'Metric':<25} {'FIFO':>12} {'Backfill':>12}")
    print(f" {'-'*25} {'-'*12} {'-'*12}")
    for metric in ["makespan", "avg_completion_time", "avg_wait_time", "utilization"]:
        vals = [str(results[s].get(metric, "N/A")) for s in ["FIFO", "Backfill"]]
        print(f" {metric:<25} {vals[0]:>12} {vals[1]:>12}")
```

### A2: Implement the two schedulers

**FIFO** (start here — simplest):

* Process jobs in `submit_time` order

* For each job, find the earliest time when one machine can fit it

* Allocate and record `start_time`, `end_time`

**Backfill**:

* Start with FIFO logic

* When the head-of-queue job can't run, compute when it will start (reservation)

* Check if later (smaller) jobs can run now and finish before the reservation

### A3: Test your implementation

```bash
python3 scheduler_sim.py
```

Expected output: two Gantt charts and a comparison table.

Because Backfill is intentionally simplified here, **your exact numbers may differ slightly** depending on implementation details. What matters is that the qualitative behavior is sensible.

**Verify your results make sense**:

* FIFO: Bob's large jobs should create some head-of-line blocking

* Backfill: some smaller jobs should start earlier, improving utilization or completion time

***

## Part B: Workload analysis (15 min)

### B1: Compare the two algorithms

Fill in this table from your simulation output:

| Metric                | FIFO   | Backfill |
| --------------------- | ------ | -------- |
| Makespan (total time) | <br /> | <br />   |
| Avg completion time   | <br /> | <br />   |
| Avg wait time         | <br /> | <br />   |
| CPU utilization       | <br /> | <br />   |

### B2: Analysis questions

Answer in your report:

1. **Backfill vs. FIFO**: by how much did backfill reduce the makespan? Which specific jobs were backfilled (ran earlier than they would under FIFO)?

2. **Waiting-time impact**: compare average wait time under FIFO and Backfill. Which users or jobs benefited most from backfilling?

3. **The trade-off**: did Backfill improve utilization, completion time, and makespan all at once in your implementation? If not, what trade-off did you observe?

4. **Large-job impact**: which of Bob's larger jobs created the most waiting for later jobs? Did Backfill reduce that head-of-line blocking in your run?

### B3: Design your own workload (bonus)

Create a workload trace where Backfill helps small jobs but does **not** improve overall makespan very much. Explain why.

***

## Part C: Observing the K8s scheduler (25 min)

### C1: Create a small kind cluster

Use a plain 1-control-plane + 2-worker cluster first. This is more robust than relying on kubelet patching, and it makes the lab easier to debug in a VM.

```yaml
# Save as kind-scheduling-lab.yaml
kind: Cluster
apiVersion: kind.x-k8s.io/v1alpha4
nodes:
- role: control-plane
- role: worker
- role: worker
```

```bash
kind create cluster --name sched-lab --config kind-scheduling-lab.yaml
kubectl cluster-info
kubectl get nodes -o wide

# Keep the lab isolated from other objects
kubectl create namespace lab8-sched
```

### C2: Observe scheduler events

Open a second terminal to watch scheduler events in real time:

```bash
kubectl get events -n lab8-sched --watch --field-selector reason=Scheduled
```

Optional: in a third terminal, keep a live Pod view open so you can correlate events with object state transitions:

```bash
kubectl get pods -n lab8-sched -o wide -w
```

What to pay attention to:

* the scheduler is not writing directly to a worker node; it is updating **cluster state** through the control plane;

* the event stream is one of the easiest ways to see the transition from “Pod exists” to “Pod is bound to a node”;

* in production, that decision becomes durable only because the API server / etcd path from Week 7 exists underneath it.

In the first terminal, create a simple Pod and watch the scheduler's decision:

```bash
kubectl run small-job -n lab8-sched --image=busybox --restart=Never \
  --overrides='{"spec":{"containers":[{"name":"small-job","image":"busybox","command":["sleep","300"],"resources":{"requests":{"cpu":"100m","memory":"64Mi"}}}]}}'

kubectl get pod -n lab8-sched small-job -o wide
```

### C3: Make scoring visible with a soft preference

In a tiny cluster, **filtering** is easy to observe, but **scoring** is often hidden because multiple nodes look equivalent. To make scoring more visible, add a soft node preference.

```bash
kubectl label node sched-lab-worker disk=ssd --overwrite
kubectl label node sched-lab-worker2 disk=hdd --overwrite
```

```bash
kubectl apply -n lab8-sched -f - <<'EOF'
apiVersion: v1
kind: Pod
metadata:
  name: prefer-ssd
spec:
  affinity:
    nodeAffinity:
      preferredDuringSchedulingIgnoredDuringExecution:
      - weight: 100
        preference:
          matchExpressions:
          - key: disk
            operator: In
            values: ["ssd"]
  containers:
  - name: bb
    image: busybox
    command: ["sleep", "300"]
    resources:
      requests:
        cpu: "100m"
        memory: "64Mi"
EOF

kubectl get pod -n lab8-sched prefer-ssd -o wide
```

If the preferred node is feasible, `prefer-ssd` should usually land on `sched-lab-worker`. This is not a hard requirement; it is a **soft scoring preference**.

### C4: Create resource contention and trigger Pending

```bash
# Starting point: fill the workers with medium Pods
for i in $(seq 1 6); do
  kubectl run filler-$i -n lab8-sched --image=busybox --restart=Never \
    --overrides="{\"spec\":{\"containers\":[{\"name\":\"filler-$i\",\"image\":\"busybox\",\"command\":[\"sleep\",\"600\"],\"resources\":{\"requests\":{\"cpu\":\"500m\",\"memory\":\"256Mi\"}}}]}}"
done

kubectl describe nodes | grep -A 6 "Allocated resources"
```

Now try to schedule a larger Pod:

```bash
kubectl run big-job -n lab8-sched --image=busybox --restart=Never \
  --overrides='{"spec":{"containers":[{"name":"big-job","image":"busybox","command":["sleep","300"],"resources":{"requests":{"cpu":"2","memory":"1Gi"}}}]}}'

kubectl get pod -n lab8-sched big-job
kubectl describe pod -n lab8-sched big-job | grep -A 10 Events
```

If `big-job` schedules immediately on your machine, your cluster still has enough free allocatable resources. In that case, **increase the pressure** by either:

* increasing the number of filler Pods (for example, `seq 1 8` or `seq 1 10`), or

* increasing each filler Pod request (for example, CPU `700m` and memory `512Mi`).

The goal is not to match one exact number. The goal is to create at least one real `Pending` Pod with a visible `FailedScheduling` reason.

### C5: Observe scheduling failure and recovery

```bash
kubectl get events -n lab8-sched --field-selector reason=FailedScheduling
kubectl describe pod -n lab8-sched big-job | grep -A 5 "Events"
```

The scheduler will report which **filter** rejected the Pod (for example, `Insufficient cpu` or `Insufficient memory`). This corresponds directly to the `NodeResourcesFit` filter from the lecture.

Important teaching note:

* failures are easiest to trigger at the **filter** stage, because Kubernetes surfaces them clearly;

* the `prefer-ssd` Pod above gives you one concrete **scoring** example;

* together, these two mini-experiments make Part C more complete: one shows soft preference, the other shows hard feasibility.

Now free resources and watch the pending Pod move forward:

```bash
kubectl delete pod -n lab8-sched filler-1 filler-2 filler-3
kubectl get pod -n lab8-sched big-job -w
```

**Record:**

* How long was `big-job` in Pending state?

* What filter rejected it initially?

* Which node was it scheduled on after resources were freed?

* Did `prefer-ssd` land on the preferred node? If so, what does that say about scoring vs. filtering?

### C6: Verbose scheduler logging (optional)

```bash
# Get recent kube-scheduler logs from the control plane
kubectl logs -n kube-system \
  $(kubectl get pods -n kube-system -l component=kube-scheduler \
    -o jsonpath='{.items[0].metadata.name}') | tail -30

# If that path is unavailable in your environment, try reading logs from the kind node container instead
docker exec sched-lab-control-plane sh -lc 'ls /var/log/containers/*scheduler* >/dev/null 2>&1 && tail -30 /var/log/containers/*scheduler*'
```

### Cleanup

```bash
kind delete cluster --name sched-lab
```

***

## What this lab is really teaching

* **Part A** gives a controllable model where policy differences are easy to see.

* **Part B** forces students to explain trade-offs rather than just report numbers.

* **Part C** shows that the real Kubernetes scheduler exposes some decisions clearly, but not all of them as neatly as a simulator.

* **Across all parts**, the deeper lesson is that scheduling decisions are not meaningful unless the control plane can make them **visible, durable, and recoverable**.

Keep reminding yourself: the point is not to memorize Kubernetes plugin names. The point is to build a usable mental model of cluster placement.

***

## Report requirements

### 1. Scheduler implementation (Part A)

* Include your completed `schedule_fifo()` and `schedule_backfill()` code

* Show the Gantt chart output for both

* Briefly describe your implementation approach for each (2-3 sentences each)

### 2. Workload analysis (Part B)

* Completed comparison table

* Answers to the 4 analysis questions

* (Bonus) Custom workload where Backfill helps some jobs but does not significantly improve makespan

### 3. K8s scheduler observation (Part C)

* Screenshot or output of scheduling events

* The `FailedScheduling` message for the resource-constrained Pod

* A short note on the `prefer-ssd` soft-preference experiment and where that Pod landed

* Description of what happened when you freed resources

* **Connect to the Week 8 lecture**: which filter stage rejected your Pod? In your soft-preference example, what kinds of scoring considerations seemed to matter?

### 4. Week 7 → Week 8 bridge (required short answer)

Write a short paragraph answering all of the following:

* At what point did the scheduler's decision become part of **shared cluster state**?

* Why is this not just a local choice made on one worker node?

* Which Week 7 mechanisms make it reasonable for controllers, kubelets, and users to trust that decision after a control-plane failure?

### 5. Reflection (answer at least one)

1. You've now seen scheduling at three levels: CPU (Week 4), cluster simulation (Part A), and real Kubernetes placement (Part C). What surprised you most about the differences?

2. If you were designing a scheduler for a cluster running **only AI training jobs** (long-running and GPU-hungry), would you start from FIFO or Backfill? What modifications would you make?

