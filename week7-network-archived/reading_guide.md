# Week 7 Reading Guide: Network Data Plane and Observability

## Required Reading (Before Seminar)

### 1. Container Networking Fundamentals (~30 min)

**"Life of a Packet in Kubernetes"** — Multiple articles cover this topic

Key concepts to understand:
- Pod network interface (eth0 inside container)
- veth pairs connecting container to host
- Bridge network (cni0 or similar)
- iptables/nftables rules for routing
- kube-proxy modes (iptables vs IPVS)

**Questions to answer:**
1. How does a packet get from Pod A to Pod B on the same node?
2. How does it differ when pods are on different nodes?
3. Where does NAT happen?

### 2. Service Mesh Architecture (~30 min)

**Envoy Proxy Architecture Overview**
- https://www.envoyproxy.io/docs/envoy/latest/intro/arch_overview/

Focus on:
- Listeners and filter chains
- Connection pooling
- HTTP/2 handling
- xDS API (configuration mechanism)

**Questions to answer:**
1. What happens when a request arrives at Envoy?
2. How does Envoy know where to route a request?
3. What is the difference between L4 and L7 proxying?

### 3. eBPF for Networking (~20 min)

**Cilium eBPF Datapath**
- https://docs.cilium.io/en/stable/concepts/ebpf/

Key concepts:
- XDP (eXpress Data Path) — hooks before network stack
- TC (Traffic Control) — hooks at qdisc layer
- Socket operations — per-socket policies

**Questions to answer:**
1. Why is eBPF faster than iptables for some operations?
2. What can XDP do that iptables cannot?
3. What are the limitations of eBPF networking?

---

## Guiding Questions (Prepare Answers Before Class)

### Conceptual Questions

1. **Latency breakdown**: For a single HTTP request from client to server (both in containers), list every component that adds latency. Estimate the contribution of each.

2. **iptables scaling**: Kubernetes uses iptables for service routing. If you have 1000 services with 10 pods each, approximately how many iptables rules exist? What is the lookup cost?

3. **Sidecar pattern**: Why does the sidecar proxy pattern exist? What would be the alternative, and what tradeoffs does each approach have?

4. **Observer effect**: If you add a sidecar for observability (metrics, traces), you've also added latency. How would you measure the observability overhead itself?

### Practical Questions

5. **Tracing strategy**: You notice p99 latency increased from 5ms to 15ms after enabling Istio. What tools would you use to find the cause? What specific metrics would you look at?

6. **eBPF hooks**: You want to measure time spent in the kernel network stack. Which eBPF tracepoints or kprobes would you use? Write the probe names.

7. **Connection reuse**: HTTP/1.1 keep-alive and HTTP/2 multiplexing both aim to reduce connection overhead. How do they differ, and which helps more with latency?

---

## Recommended Reading (Optional)

### Deep Dives

- **Brendan Gregg: Linux Network Performance**
  - https://www.brendangregg.com/linuxperf.html (network section)
  - Covers tools and methodology for network analysis

- **"Understanding Modern Linux Networking"**
  - Covers NAPI, GRO/GSO, and other kernel optimizations

- **LWN.net: "A brief introduction to XDP"**
  - Technical introduction to XDP programming

### Service Mesh Performance Studies

- **"Performance Evaluation of Service Mesh Frameworks"** (IEEE)
  - Academic comparison of Istio, Linkerd, and Consul Connect

- **Istio Performance FAQ**
  - https://istio.io/latest/docs/ops/deployment/performance-and-scalability/

- **Linkerd Benchmarks**
  - https://linkerd.io/2021/05/27/linkerd-vs-istio-benchmarks/

---

## Pre-Lab Environment Preparation

### Required Tools

```bash
# Load testing tools
sudo apt install -y wrk apache2-utils

# Or use hey (Go-based)
go install github.com/rakyll/hey@latest

# Network debugging
sudo apt install -y tcpdump tshark netcat-openbsd

# eBPF tools (should have from Week 4)
sudo apt install -y bpftrace bpfcc-tools
```

### Verify Environment

```bash
# kind cluster running
kubectl get nodes

# Can deploy pods
kubectl run test --image=nginx --rm -it --restart=Never -- /bin/sh

# bpftrace works
sudo bpftrace -e 'BEGIN { printf("OK\n"); exit(); }'

# wrk works
wrk --version
```

### Prepare Envoy Configuration (Optional, done in lab)

If you want to get ahead, review the Envoy configuration in `manifests/envoy-sidecar.yaml`.

---

## What to Focus On During Seminar

When the presenter discusses service mesh overhead:

1. **Ask for numbers**: "What's the typical p99 overhead?" Request specific measurements.

2. **Ask about methodology**: "How was that measured? What was the baseline?"

3. **Ask about conditions**: "Does that hold under high load? With TLS? With HTTP/2?"

4. **Connect to mechanisms**: "Which kernel path is hot when proxy CPU is high?"

When discussing eBPF vs iptables:

1. **Ask about tradeoffs**: "What can iptables do that eBPF cannot?"

2. **Ask about complexity**: "How hard is it to debug eBPF programs?"

3. **Ask about maturity**: "Which is better tested for production use?"

---

## Key Terms to Know

| Term | Definition |
|------|------------|
| **veth pair** | Virtual ethernet pair connecting namespaces |
| **CNI** | Container Network Interface, plugins that set up pod networking |
| **kube-proxy** | Component that implements service routing |
| **IPVS** | IP Virtual Server, alternative to iptables for routing |
| **XDP** | eXpress Data Path, eBPF hook before network stack |
| **TC** | Traffic Control, qdisc layer for traffic shaping |
| **mTLS** | Mutual TLS, both sides authenticate |
| **xDS** | Envoy's discovery service API family |
| **Sidecar** | Container that runs alongside app container |
| **L4 vs L7** | Transport layer (TCP) vs Application layer (HTTP) proxying |

---

## How This Connects to Your Final Project

If your system case involves:

- **Network-intensive services** → Measure baseline network latency
- **Microservices** → Consider service mesh overhead in your analysis
- **High throughput** → Network often becomes the bottleneck
- **Latency-sensitive** → Every millisecond matters; trace the path

The skills from this week directly apply to:
- Identifying network bottlenecks
- Measuring overhead of infrastructure components
- Making informed decisions about proxy/mesh usage
