# Lab 6: Cluster Scheduling — Algorithms and Observation

> **Goal:** Implement three cluster scheduling algorithms (FIFO, Backfill, DRF), compare their behavior on the same workload, and observe the Kubernetes scheduler making real decisions.

## Overview

| Part | Focus | Key Measurement |
|------|-------|-----------------|
| **A: Scheduler simulation** | Implement FIFO, Backfill, and DRF in Python | Makespan, avg completion time, utilization, fairness |
| **B: Workload analysis** | Run all three against a mixed workload trace | Compare metrics across algorithms |
| **C: K8s scheduler observation** | Use kind to watch filter → score → bind in action | Scheduling decisions under resource contention |

## Prerequisites

```bash
# Python 3.8+
python3 --version

# For Part C: kind + kubectl + Docker
kind version
kubectl version --client
docker version
```

### ⚠️ Check your Docker installation (Ubuntu users)

On Ubuntu, Docker can be installed in two ways — only the **apt (official) version** works correctly with kind. Before starting Part C, verify which one you have:

```bash
# If "docker" appears here with publisher "canonical", you have the snap version
snap list 2>/dev/null | grep docker

# The correct apt version won't appear in snap list; confirm it with:
docker info --format '{{.ServerVersion}}'
```

**If you have the snap version of Docker**, kind will fail to create the cluster with an error like `Chain 'DOCKER-ISOLATION-STAGE-2' does not exist`. This happens because the snap package bundles its own `iptables-nft` binary in an isolated environment, completely ignoring the system's `update-alternatives` settings. The fix is to remove the snap version and install the official apt package:

```bash
# 1. Remove snap Docker (note: existing images and containers will be deleted)
sudo snap remove --purge docker

# 2. Install the official Docker CE via apt
curl -fsSL https://get.docker.com | sudo sh

# 3. Add your user to the docker group (avoids needing sudo every time)
sudo usermod -aG docker $USER
newgrp docker

# 4. Confirm the iptables backend is "legacy" (required by Docker)
iptables --version   # should show (legacy); if it shows (nf_tables), run the two lines below
sudo update-alternatives --set iptables  /usr/sbin/iptables-legacy
sudo update-alternatives --set ip6tables /usr/sbin/ip6tables-legacy

# 5. Restart Docker
sudo systemctl restart docker
```

Run `docker version` to confirm everything is working, then continue with Part C.

### ⚠️ Increase inotify limits before creating a multi-node cluster

kind runs each Kubernetes node as a Docker container. Every container's kubelet uses **inotify** (a Linux kernel mechanism) to watch cgroup directories for resource monitoring. When you run 3+ nodes on the same host, they share the host's inotify quota — the default limits are too low and will cause one or more worker nodes to fail to start with:

```
inotify_init: too many open files
Failed to start cAdvisor
kubelet.service: Main process exited, status=1/FAILURE
```

Run these **before** `kind create cluster`:

```bash
sudo sysctl fs.inotify.max_user_watches=524288
sudo sysctl fs.inotify.max_user_instances=512
```

To make the change survive reboots:

```bash
echo "fs.inotify.max_user_watches=524288" | sudo tee -a /etc/sysctl.conf
echo "fs.inotify.max_user_instances=512"  | sudo tee -a /etc/sysctl.conf
```

---

## Part A: Implement three schedulers (40 min)

You are given a simulation framework (`scheduler_sim.py`) with the data structures and metrics code. Your job: implement three scheduling algorithms.

### A1: Create the simulation framework

Save the following as `scheduler_sim.py`:

```python
"""
Cluster scheduler simulation for Lab 6.
Students implement: schedule_fifo(), schedule_backfill(), schedule_drf()
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
    nodes_required: int       # number of machines needed (gang scheduling)
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

        # Bob: few large, long batch jobs (some gang-scheduled)
        Job("b1", "bob", submit_time=0,  cpu=4, mem=16, duration=10, nodes_required=2),
        Job("b2", "bob", submit_time=3,  cpu=6, mem=24, duration=8,  nodes_required=1),
        Job("b3", "bob", submit_time=7,  cpu=4, mem=16, duration=12, nodes_required=3),
        Job("b4", "bob", submit_time=15, cpu=2, mem=8,  duration=6,  nodes_required=1),

        # Charlie: medium jobs, arrives later
        Job("c1", "charlie", submit_time=4,  cpu=3, mem=8,  duration=5, nodes_required=1),
        Job("c2", "charlie", submit_time=9,  cpu=2, mem=4,  duration=4, nodes_required=1),
        Job("c3", "charlie", submit_time=11, cpu=4, mem=8,  duration=6, nodes_required=2),
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
# TODO: Implement the three scheduling algorithms below.
# ──────────────────────────────────────────────────────────────

def schedule_fifo(jobs: list[Job], machines: list[Machine]) -> list[Job]:
    """
    FIFO scheduler: schedule jobs in submission order.
    For each job, find the earliest time when enough machines can fit it.

    For gang-scheduled jobs (nodes_required > 1):
      - Find nodes_required machines that can ALL fit the job at the same time.
      - All machines must start the job at the same time.

    Algorithm:
      1. Process jobs in submit_time order.
      2. For each job, find the earliest time >= submit_time when
         nodes_required machines can simultaneously fit the job.
      3. Assign the job to those machines.
    """
    # TODO: Implement this
    # Hints:
    # - For single-node jobs: find the machine with earliest availability
    # - For gang jobs: find earliest time T where >= nodes_required machines
    #   can fit the job at time T (use machine.next_free_time() and machine.can_fit())
    # - Set job.start_time and job.end_time
    # - Call machine.allocate(job, start_time) for each assigned machine
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


def schedule_drf(jobs: list[Job], machines: list[Machine]) -> list[Job]:
    """
    Simplified DRF (Dominant Resource Fairness) scheduler.

    Instead of FIFO, prioritize the user whose dominant resource share is
    smallest. This prevents one user from monopolizing the cluster.

    Dominant resource share for user u at time t:
      dominant_share(u) = max(total_cpu_allocated(u) / cluster_total_cpu,
                              total_mem_allocated(u) / cluster_total_mem)

    Algorithm:
      1. At each scheduling point, compute dominant_share for each user
         (based on currently RUNNING jobs).
      2. Among users with pending jobs, pick the user with the smallest
         dominant_share.
      3. Schedule that user's next job (earliest submitted, unscheduled).
      4. If it can't fit now, skip and try the next user.
      5. Advance time when no job can be scheduled.

    For gang-scheduled jobs, same rules as FIFO: need all machines at once.
    """
    # TODO: Implement this
    # Hints:
    # - Track running jobs to compute current resource usage per user.
    # - Use a time-stepping simulation: at each time step, check which
    #   jobs have completed (free resources), then try to schedule.
    # - The key difference from FIFO: job selection is by DRF priority,
    #   not by submission order.
    # - Total cluster CPU = sum(m.total_cpu for m in machines)
    # - Total cluster mem = sum(m.total_mem for m in machines)
    pass


# ──────────────────────────────────────────────────────────────
# MAIN: run all three and compare
# ──────────────────────────────────────────────────────────────

if __name__ == "__main__":
    results = {}

    for name, scheduler_fn in [
        ("FIFO", schedule_fifo),
        ("Backfill", schedule_backfill),
        ("DRF", schedule_drf),
    ]:
        jobs = mixed_workload()
        machines = create_cluster(num_machines=4, cpu_per_machine=8, mem_per_machine=32)
        scheduler_fn(jobs, machines)
        print_schedule(jobs, name)
        metrics = compute_metrics(jobs, machines)
        results[name] = metrics
        print(f"  Metrics: {metrics}\n")

    # Summary comparison table
    print(f"\n{'='*70}")
    print(f" COMPARISON")
    print(f"{'='*70}")
    print(f" {'Metric':<25} {'FIFO':>12} {'Backfill':>12} {'DRF':>12}")
    print(f" {'-'*25} {'-'*12} {'-'*12} {'-'*12}")
    for metric in ["makespan", "avg_completion_time", "avg_wait_time",
                    "utilization", "jain_fairness"]:
        vals = [str(results[s].get(metric, "N/A")) for s in ["FIFO", "Backfill", "DRF"]]
        print(f" {metric:<25} {vals[0]:>12} {vals[1]:>12} {vals[2]:>12}")
```

### A2: Implement the three schedulers

**FIFO** (start here — simplest):
- Process jobs in `submit_time` order
- For each job, find the earliest time when `nodes_required` machines can all fit it
- Allocate and record `start_time`, `end_time`

**Backfill**:
- Start with FIFO logic
- When the head-of-queue job can't run, compute when it will start (reservation)
- Check if later (smaller) jobs can run now and finish before the reservation

**DRF**:
- At each scheduling point, compute each user's dominant resource share
- Schedule the job from the user with the smallest share
- This requires time-stepping: advance time, free completed jobs, re-evaluate

### A3: Test your implementation

```bash
python3 scheduler_sim.py
```

Expected output: three Gantt charts and a comparison table.

**Verify your results make sense**:
- FIFO: Bob's large jobs should cause head-of-line blocking
- Backfill: Alice's small jobs should fill gaps, improving utilization
- DRF: CPU-time should be more evenly distributed across users

---

## Part B: Workload analysis (15 min)

### B1: Compare the three algorithms

Fill in this table from your simulation output:

| Metric | FIFO | Backfill | DRF |
|--------|------|----------|-----|
| Makespan (total time) | | | |
| Avg completion time | | | |
| Avg wait time | | | |
| CPU utilization | | | |
| Jain's fairness index | | | |

### B2: Analysis questions

Answer in your report:

1. **Backfill vs. FIFO**: by how much did backfill reduce the makespan? Which specific jobs were backfilled (ran earlier than they would under FIFO)?

2. **DRF's fairness impact**: compare the per-user CPU-time distribution. Under FIFO, does Bob (large jobs) get more total CPU-time than Alice? How does DRF change this?

3. **The trade-off**: which algorithm has the best makespan? The best fairness? Can you find an algorithm that wins on both? Why or why not?

4. **Gang scheduling impact**: identify Bob's gang-scheduled jobs (b1: 2 nodes, b3: 3 nodes). How does each algorithm handle them? Does gang scheduling create more waiting for other users?

### B3: Design your own workload (bonus)

Create a workload trace where DRF produces a **worse** makespan than FIFO. Explain why. (Hint: think about a scenario where fairness forces suboptimal packing.)

---

## Part C: Observing the K8s scheduler (25 min)

### C1: Create a resource-constrained kind cluster

```yaml
# Save as kind-scheduling-lab.yaml
kind: Cluster
apiVersion: kind.x-k8s.io/v1alpha4
nodes:
- role: control-plane
  image: kindest/node:v1.29.4
- role: worker
  image: kindest/node:v1.29.4
  # Simulate a small node
  kubeadmConfigPatches:
  - |
    kind: KubeletConfiguration
    systemReserved:
      cpu: "500m"
      memory: "500Mi"
- role: worker
  image: kindest/node:v1.29.4
  kubeadmConfigPatches:
  - |
    kind: KubeletConfiguration
    systemReserved:
      cpu: "500m"
      memory: "500Mi"
```

> **Why pin the image?** Without an explicit `image:` tag, kind picks the default node image bundled with your kind binary, which may be an older version with poor compatibility on ARM64 / cgroupv2 systems (Ubuntu 22.04+, kernel 6.x). Pinning to `v1.29.4` avoids several kubelet startup issues on these platforms.

```bash
kind create cluster --name sched-lab --config kind-scheduling-lab.yaml
```

> **If you see `Chain 'DOCKER-ISOLATION-STAGE-2' does not exist`**: go back to the "Check your Docker installation" section above and switch to the apt version of Docker.

> **If one worker node stays `NotReady`**: this is almost always the inotify limit issue. Run the two `sysctl` commands from the Prerequisites section, then restart the failed node's kubelet without recreating the cluster:
> ```bash
> # Check which node is NotReady
> kubectl get nodes
>
> # Restart its kubelet (replace "sched-lab-worker2" if the name differs)
> docker exec sched-lab-worker2 systemctl restart kubelet
>
> # Wait ~15 seconds, then confirm all nodes are Ready
> sleep 15 && kubectl get nodes
> ```

Once the cluster is up, **verify that kubectl is pointed at the right cluster**. If you have done previous labs, you may have multiple kind clusters locally, and kubectl might still be targeting an old one:

```bash
# List all contexts — confirm the * is on kind-sched-lab
kubectl config get-contexts

# Switch if needed
kubectl config use-context kind-sched-lab

# Confirm you see 3 nodes (1 control-plane + 2 workers), all Ready
kubectl get nodes
```

Expected output:

```
NAME                        STATUS   ROLES           AGE
sched-lab-control-plane     Ready    control-plane   ...
sched-lab-worker            Ready    <none>          ...
sched-lab-worker2           Ready    <none>          ...
```

If you only see 1 node (e.g. `lab6-control-plane`), you are connected to an old cluster. Switch the context and check again.

### C2: Observe scheduler events

Open a second terminal to watch scheduler events in real-time:

```bash
# Watch scheduling events
kubectl get events --watch --field-selector reason=Scheduled
```

In the first terminal, create Pods and watch the scheduler's decisions:

```bash
# Small Pod — should schedule easily
kubectl run small-job --image=busybox --restart=Never \
  --overrides='{"spec":{"containers":[{"name":"small-job","image":"busybox","command":["sleep","300"],"resources":{"requests":{"cpu":"100m","memory":"64Mi"}}}]}}'

# Check where it was scheduled
kubectl get pod small-job -o wide
```

### C3: Create resource contention

```bash
# Fill up the cluster with medium Pods
for i in $(seq 1 6); do
  kubectl run filler-$i --image=busybox --restart=Never \
    --overrides="{\"spec\":{\"containers\":[{\"name\":\"filler-$i\",\"image\":\"busybox\",\"command\":[\"sleep\",\"600\"],\"resources\":{\"requests\":{\"cpu\":\"500m\",\"memory\":\"256Mi\"}}}]}}"
done

# Check resource usage per node
kubectl describe nodes | grep -A 5 "Allocated resources"
```

Now try to schedule a large Pod:

```bash
# This Pod requests more resources — it may be Pending
kubectl run big-job --image=busybox --restart=Never \
  --overrides='{"spec":{"containers":[{"name":"big-job","image":"busybox","command":["sleep","300"],"resources":{"requests":{"cpu":"600m","memory":"512Mi"}}}]}}'

# Check its status
kubectl get pod big-job
kubectl describe pod big-job | grep -A 10 Events
```


### C4: Observe scheduling failure

```bash
# Look for FailedScheduling events
kubectl get events --field-selector reason=FailedScheduling

# Detailed scheduler message
kubectl describe pod big-job | grep -A 5 "Events"
```

The scheduler will report which **filter** rejected the Pod (e.g., "Insufficient cpu" or "Insufficient memory"). This corresponds directly to the `NodeResourcesFit` filter from the lecture.

### C5: Observe scheduling after freeing resources

```bash
# Delete all filler Pods to free up enough CPU
kubectl delete pod filler-1 filler-2 filler-3 filler-4 filler-5 filler-6

# Watch the big-job Pod — it should get scheduled now
kubectl get pod big-job -w
```

You should see the status transition:

```
NAME      READY   STATUS              RESTARTS   AGE
big-job   0/1     Pending             0          ...
big-job   0/1     ContainerCreating   0          ...
big-job   1/1     Running             0          ...
```

Once you see `Running`, press **Ctrl-C** to stop the watch. You may see the same status line printed multiple times — this is normal; Kubernetes periodically resyncs object state and emits duplicate watch events. **Use the first occurrence of each status for your timing records.**

After `big-job` finishes its `sleep 300`, it will transition to `Completed` on its own.

**Record:**
- How long was `big-job` in Pending state? (time from creation to first `ContainerCreating` line)
- What filter rejected it initially?
- Which node was it scheduled on after resources freed up? (`kubectl get pod big-job -o wide`)

### C6: Verbose scheduler logging (optional)

```bash
# Get detailed scheduler logs from the kind control plane
kubectl logs -n kube-system $(kubectl get pods -n kube-system -l component=kube-scheduler -o name) | tail -30
```

### Cleanup

```bash
kind delete cluster --name sched-lab
```

---

## Report requirements

### 1. Scheduler implementation (Part A)

- Include your completed `schedule_fifo()`, `schedule_backfill()`, and `schedule_drf()` code
- Show the Gantt chart output for all three
- Briefly describe your implementation approach for each (2-3 sentences each)

### 2. Workload analysis (Part B)

- Completed comparison table
- Answers to the 4 analysis questions
- (Bonus) Custom workload where DRF loses on makespan

### 3. K8s scheduler observation (Part C)

- Screenshot or output of scheduling events
- The `FailedScheduling` message for the resource-constrained Pod
- Description of what happened when you freed resources
- **Connect to the lecture**: which filter plugin rejected your Pod? Which scoring plugin would have been used if multiple nodes were feasible?

### 4. Reflection (answer at least one)

1. You've now seen scheduling at three levels: CPU (Week 4), cluster simulation (Part A), and real K8s (Part C). What surprised you most about the differences?

2. If you were designing a scheduler for a cluster running **only AI training jobs** (long-running, gang-scheduled, GPU-hungry), which of the three approaches (FIFO/Backfill/DRF) would you start from? What modifications would you make?
