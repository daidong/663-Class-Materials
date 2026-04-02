# Week 6 Reading Guide: Kubernetes as a Distributed OS

## Required Reading (Before Seminar)

### 1) Kubernetes Resource Model (Official Docs)

**Managing Resources for Containers**
- URL: https://kubernetes.io/docs/concepts/configuration/manage-resources-containers/
- Focus on:
  - Difference between requests and limits
  - How CPU and memory are specified (millicores, bytes)
  - What happens when limits are exceeded

**Configure Quality of Service for Pods**
- URL: https://kubernetes.io/docs/tasks/configure-pod-container/quality-service-pod/
- Focus on:
  - How QoS class is computed (not set!)
  - Implications for eviction priority

Time: ~40 minutes total

### 2) Kubernetes Scheduler Overview

**Kubernetes Scheduler**
- URL: https://kubernetes.io/docs/concepts/scheduling-eviction/kube-scheduler/
- Focus on:
  - Scheduling pipeline (filtering → scoring → binding)
  - What predicates/priorities mean

**Scheduler Configuration**
- URL: https://kubernetes.io/docs/reference/scheduling/config/
- Skim: Just understand that scheduler plugins exist and can be configured

Time: ~25 minutes

### 3) Eviction and Resource Pressure

**Node-pressure Eviction**
- URL: https://kubernetes.io/docs/concepts/scheduling-eviction/node-pressure-eviction/
- Focus on:
  - Eviction signals (memory.available, nodefs.available, etc.)
  - Eviction order (BestEffort → Burstable → Guaranteed)

Time: ~20 minutes

---

## Recommended Reading (Deepen Understanding)

### CPU Throttling Deep Dive

Choose ONE of these blog posts:

**Option A: "CPU limits and aggressive throttling in Kubernetes"**
- Various engineering blogs discuss how CFS bandwidth control causes latency spikes
- Search for recent posts (2022+) on this topic

**Option B: Academic/Technical Paper**
- "Reconciling High Server Utilization and Sub-millisecond Quality-of-Service" (EuroSys '14)
- Or similar papers on CPU scheduling and tail latency

### Kubernetes Internals

**"Kubernetes: Up and Running" by Kelsey Hightower et al.**
- Chapter on Resource Management
- Good for practical examples

---

## Guiding Questions (Answer Before Class)

### Scheduler and Placement

1. **Requests vs Limits**: If I set `cpu: 500m` for requests and `cpu: 1000m` for limits, what does each value control?

2. **Node Selection**: Describe the two main phases of scheduling. What happens in each?

3. **Scheduling Failure**: If no node has enough resources for my pod, what happens? Where would you look to debug?

### Resource Enforcement

4. **CPU Throttling**: Explain what happens (at the cgroup level) when a container tries to use more CPU than its limit.

5. **Memory OOM**: Explain the sequence of events when a container tries to allocate memory beyond its limit.

6. **QoS Classes**: How would you configure a pod to be "Guaranteed"? What does that buy you?

### Eviction

7. **Node Pressure**: What is the default threshold for memory.available that triggers eviction? Why does this exist?

8. **Eviction Order**: If a node is under memory pressure, in what order are pods evicted? What determines the order within a QoS class?

---

## Practical Preparation

### Install kind (Before Lab)

```bash
# Install kind
curl -Lo ./kind https://kind.sigs.k8s.io/dl/v0.20.0/kind-linux-amd64
chmod +x ./kind
sudo mv ./kind /usr/local/bin/kind

# Verify
kind version
```

### Install kubectl (If Not Present)

```bash
curl -LO "https://dl.k8s.io/release/$(curl -L -s https://dl.k8s.io/release/stable.txt)/bin/linux/amd64/kubectl"
chmod +x kubectl
sudo mv kubectl /usr/local/bin/kubectl

# Verify
kubectl version --client
```

### Test kind Cluster Creation

```bash
# Create a test cluster
kind create cluster --name test-cluster

# Verify
kubectl cluster-info
kubectl get nodes

# Clean up
kind delete cluster --name test-cluster
```

---

## Key Vocabulary

| Term | Definition |
|------|------------|
| **Request** | Amount of resource reserved for scheduling; scheduler uses this to find fitting nodes |
| **Limit** | Maximum amount of resource enforced via cgroup; exceeding causes throttling (CPU) or OOM (memory) |
| **QoS Class** | Priority tier for eviction: Guaranteed, Burstable, BestEffort |
| **Throttling** | CPU bandwidth control pausing container execution when quota exhausted |
| **OOM Kill** | Kernel terminating a process when cgroup memory limit exceeded |
| **Eviction** | Kubelet removing pods when node is under resource pressure |
| **kind** | "Kubernetes IN Docker" — tool for running local K8s clusters |
| **Kubelet** | Node agent that manages containers and enforces resource limits |

---

## How This Connects to Previous Weeks

| Week | Topic | Connection |
|------|-------|------------|
| Week 4 | eBPF | You can use eBPF to trace scheduler decisions and cgroup events |
| Week 5 | Containers | K8s uses the same namespaces and cgroups you implemented |
| Week 6 | K8s | **This week** — distributed control plane for containers |
| Week 7 | Networking | K8s networking adds another layer to diagnose |

---

## What to Pay Attention to in Seminar

- When someone says "set limits to avoid problems," ask: **what problem specifically?** Throttling can be a problem too.
- When someone says "QoS class," ask: **computed or configured?** (It's computed!)
- When someone says "pod was evicted," ask: **by cgroup OOM or kubelet eviction?** Different mechanisms!
