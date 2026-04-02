---
marp: true
theme: default
paginate: true
header: 'Advanced Operating Systems — Week 7'
footer: 'Network Data Plane and Observability'
---

# Week 7: Network Data Plane and Observability
## Sidecar Proxies and eBPF

> "The network is just another syscall layer — with its own latency surprises."

---

# Today's Agenda

**Seminar** (90 min):
- Container networking fundamentals
- Service mesh architecture and overhead
- iptables vs eBPF datapath
- Network observability with eBPF

**Lab** (90 min):
- Measure p99 latency with and without sidecar
- Use eBPF to attribute latency to components

---

# Why Network Latency Matters

In a microservices world:
- Single request → 10+ service hops
- Each hop: serialize → network → deserialize → process
- **Tail latency compounds exponentially**

If each hop has 1% chance of being slow:
- 1 hop: 1% slow requests
- 10 hops: ~10% slow requests

---

# The Hidden Cost of "Simple" HTTP

```
Time breakdown for a single HTTP request:

User space:          ████████░░░░  (40%)
  - App processing
  - Serialization

Kernel space:        ████░░░░░░░░  (20%)
  - Syscalls
  - TCP/IP stack
  - Interrupts

Network:             ████████░░░░  (40%)
  - Wire time
  - Other host processing
```

With a sidecar, user space grows significantly.

---

# Container Networking Path

```
┌─────────────────────────────────────────────────────┐
│                    Host Kernel                       │
│                                                     │
│  ┌─────────┐    ┌─────────┐    ┌──────────────┐   │
│  │   Pod   │    │  veth   │    │   bridge     │   │
│  │  eth0   │◄──►│  pair   │◄──►│   (cni0)     │   │
│  └─────────┘    └─────────┘    └──────────────┘   │
│                                       │            │
│                               ┌───────▼───────┐   │
│                               │   iptables    │   │
│                               │   /nftables   │   │
│                               └───────────────┘   │
└─────────────────────────────────────────────────────┘
```

Each hop adds latency and CPU cycles.

---

# What's a veth Pair?

**Virtual Ethernet**: Two virtual NICs connected as a pipe.

```
Container Namespace          Host Namespace
┌─────────────────┐         ┌─────────────────┐
│   eth0          │◄───────►│   veth1234      │
│   (container)   │  pipe   │   (host side)   │
└─────────────────┘         └─────────────────┘
```

- Packets sent on one end appear on the other
- Bridge connects multiple veth endpoints
- Cost: ~1-5μs per veth traversal

---

# iptables: The Traditional Way

iptables processes packets through chains:

```
PREROUTING → INPUT → (local process)
                     ↓
                  OUTPUT → POSTROUTING
```

**Service mesh adds rules:**
```bash
# Typical Istio rules per pod
iptables -t nat -A PREROUTING -p tcp -j REDIRECT --to-port 15001
iptables -t nat -A OUTPUT -p tcp -j REDIRECT --to-port 15001
```

Problem: O(n) rule traversal!

---

# iptables Performance Issue

```
Number of rules    Lookup time
─────────────────────────────────
10                 ~0.5μs
100                ~5μs
1000               ~50μs
10000              ~500μs
```

In a large cluster:
- 1000 services × 10 rules each = 10,000 rules
- Every packet pays the lookup cost!

---

# eBPF/XDP: The Modern Way

eBPF can hook at multiple points:

```
Packet arrives
     │
     ▼
┌────────────┐
│    XDP     │ ← Before driver, fastest
└────────────┘
     │
     ▼
┌────────────┐
│    TC      │ ← Traffic control layer
└────────────┘
     │
     ▼
┌────────────┐
│  Netfilter │ ← Traditional (iptables)
└────────────┘
     │
     ▼
   Socket
```

---

# eBPF Advantages

| Aspect | iptables | eBPF/XDP |
|--------|----------|----------|
| Lookup | O(n) linear | O(1) hash map |
| Context switch | Multiple | Fewer/none |
| Programmability | Limited | Full BPF program |
| Overhead | Higher | Lower |

Cilium claims 2-4x throughput improvement over iptables.

---

# Service Mesh Architecture

```
┌──────────────────────────────────────────────────┐
│                    Pod                            │
│                                                  │
│  ┌─────────────┐         ┌──────────────────┐   │
│  │    App      │         │     Sidecar      │   │
│  │  Container  │◄───────►│    (Envoy)       │   │
│  │  :8080      │ loopback│    :15001        │   │
│  └─────────────┘         └──────────────────┘   │
│                                  │               │
│                           iptables REDIRECT     │
│                                  │               │
│                           ┌──────▼──────┐       │
│                           │    eth0     │       │
│                           └─────────────┘       │
└──────────────────────────────────────────────────┘
```

---

# What the Sidecar Does

Envoy sidecar provides:
1. **Traffic interception** — All traffic goes through proxy
2. **TLS termination/origination** — mTLS between services
3. **Protocol parsing** — HTTP/2, gRPC, etc.
4. **Policy enforcement** — Rate limiting, circuit breakers
5. **Observability** — Metrics, traces, access logs

**Each feature adds CPU and latency.**

---

# Sidecar Overhead Breakdown

Typical latency additions:

| Component | Latency |
|-----------|---------|
| iptables redirect | 10-50μs |
| Connection to sidecar | 50-200μs |
| TLS handshake (first req) | 1-10ms |
| HTTP parsing | 10-100μs |
| Policy evaluation | 1-50μs |
| **Total per request** | **0.5-2ms** |

At p99, these can be **much higher**.

---

# When Does Overhead Matter?

**Low latency services:**
- p50 = 1ms baseline
- +1ms sidecar = 100% overhead!

**High latency services:**
- p50 = 100ms baseline
- +1ms sidecar = 1% overhead (acceptable)

**High throughput services:**
- Overhead accumulates
- CPU becomes the bottleneck

---

# Real World Numbers

From various benchmarks and studies:

| Setup | p50 | p99 |
|-------|-----|-----|
| Direct (no mesh) | 0.5ms | 2ms |
| Istio sidecar | 1.5ms | 8ms |
| Linkerd sidecar | 1.0ms | 5ms |
| Cilium (eBPF) | 0.7ms | 3ms |

Your mileage will vary based on:
- Request size
- Connection reuse
- TLS configuration
- Load level

---

# Measuring Network Latency with eBPF

We can trace at multiple levels:

1. **Syscall level** — time in send/recv
2. **TCP stack** — time in TCP processing
3. **Socket buffer** — time waiting in buffers
4. **Application** — time in user code

---

# bpftrace: Syscall Latency

```bash
sudo bpftrace -e '
tracepoint:syscalls:sys_enter_sendto { 
    @start[tid] = nsecs; 
}
tracepoint:syscalls:sys_exit_sendto /@start[tid]/ {
    @send_us = hist((nsecs - @start[tid]) / 1000);
    delete(@start[tid]);
}
'
```

Output:
```
@send_us:
[1]    |@@@@@@@@@@@@@@@@@@@@  |
[2, 4) |@@@@@@@@@@            |
[4, 8) |@@@                   |
[8, 16)|@                     |
```

---

# bpftrace: TCP Connection Time

```bash
sudo bpftrace -e '
kprobe:tcp_v4_connect { @start[tid] = nsecs; }
kretprobe:tcp_v4_connect /@start[tid]/ {
    @connect_us = hist((nsecs - @start[tid]) / 1000);
    delete(@start[tid]);
}
'
```

This measures time to establish TCP connection.

---

# bpftrace: Network Stack Time

```bash
sudo bpftrace -e '
tracepoint:net:netif_receive_skb { 
    @start[args->skbaddr] = nsecs; 
}
tracepoint:skb:consume_skb /@start[args->skbaddr]/ {
    @stack_us = hist((nsecs - @start[args->skbaddr]) / 1000);
    delete(@start[args->skbaddr]);
}
'
```

Time from packet received to socket delivery.

---

# Lab 6: What You'll Measure

**Setup 1: Direct (baseline)**
```
Client → Server (no proxy)
```

**Setup 2: With Sidecar**
```
Client → Sidecar → Server
```

**Questions to answer:**
1. What is the latency difference? (p50, p90, p99)
2. Where does the extra time go?
3. Is the overhead acceptable for your use case?

---

# Lab Methodology

1. **Deploy baseline service** — Simple HTTP server
2. **Load test** — Use `wrk` or `hey` for consistent load
3. **Capture latencies** — Record distribution
4. **Add sidecar** — Deploy Envoy in same pod
5. **Load test again** — Same load, same duration
6. **Compare** — Focus on p99 difference
7. **Attribute** — Use eBPF to find where time goes

---

# Load Testing Tools

**wrk** — High-performance HTTP benchmarking
```bash
wrk -t2 -c10 -d30s --latency http://server:8080/
```

**hey** — HTTP load generator with latency histogram
```bash
hey -n 10000 -c 50 http://server:8080/
```

**ab** — Apache benchmark (simple but effective)
```bash
ab -n 10000 -c 50 http://server:8080/
```

---

# Interpreting Results

Look for:
- **p50** — Typical experience
- **p90** — Most users
- **p99** — Worst 1% (often much worse!)
- **Max** — Outliers

Red flags:
- p99 >> 10× p50 — Something causes occasional delays
- High variance — Inconsistent behavior
- Throughput plateau — Resource bottleneck

---

# Common Bottlenecks

| Symptom | Likely Cause |
|---------|--------------|
| High CPU on sidecar | TLS, parsing overhead |
| p99 spikes | Connection setup, cold TLS |
| Throughput limited | Connection pool exhaustion |
| Memory growth | Connection state accumulation |

---

# When to Use Service Mesh

**Good fit:**
- Security-critical (need mTLS everywhere)
- Complex traffic management needs
- High observability requirements
- Latency not critical (>10ms baseline)

**Poor fit:**
- Ultra-low latency requirements
- Simple topology
- Resource-constrained environment
- Internal, trusted network

---

# Alternatives to Full Sidecar

1. **eBPF-based mesh** (Cilium Service Mesh)
   - Lower overhead
   - Less flexibility

2. **Proxyless gRPC** (Istio experimental)
   - xDS API directly to app
   - gRPC-only

3. **Ambient mesh** (Istio experimental)
   - Per-node proxy instead of per-pod
   - Reduced resource usage

---

# Connection to Course Themes

This week connects:
- **Week 4**: eBPF skills for observability
- **Week 5**: Network namespaces
- **Week 6**: Kubernetes networking
- **Week 9**: mTLS and security policies
- **Final project**: Network often dominates system behavior

---

# Live Demo: Tracing Network Path

Let's trace a request through the stack:

```bash
# Terminal 1: Start tracing
sudo bpftrace scripts/network_trace.bt

# Terminal 2: Send request
curl http://localhost:8080/

# Observe: Where did time go?
```

---

# Questions?

- Container networking path
- Sidecar overhead
- eBPF tracing

---

# Break (5 min)

Then we start Lab 6: p99 Latency Breakdown

---

# Lab 6 Overview

**Goal:** Quantify sidecar overhead and attribute it to specific components

**Steps:**
1. Deploy HTTP server (baseline)
2. Measure latency distribution
3. Add Envoy sidecar
4. Measure again
5. Use eBPF to attribute

**Deliverable:** Comparison table + analysis

---

# Thank You

**Week 7 deliverables:**
- Lab 6 report (before Week 8)
  - Latency comparison
  - Attribution analysis
  - Recommendations
- Seminar critique memo

Next week: Storage and Tail Latency!
