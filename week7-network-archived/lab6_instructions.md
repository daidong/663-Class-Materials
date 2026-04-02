# Lab 6: p99 Latency Breakdown — Sidecar Proxy Overhead

> **Goal:** Measure the latency impact of adding a sidecar proxy, and use eBPF to attribute the overhead to specific components.

## Overview

In this lab, you will:
1. Deploy a simple HTTP service (baseline)
2. Measure latency distribution with load testing
3. Add an Envoy sidecar proxy
4. Measure latency again and compare
5. Use eBPF to identify where the extra latency comes from

## Prerequisites

### System Requirements
- Linux system with kernel 5.10+ (for eBPF)
- Working `kind` cluster (from Week 6) OR local Docker
- `wrk` or `hey` for load testing
- `bpftrace` for eBPF tracing

### Create Kind Cluster

If you don't have a running Kind cluster, create one first:

```bash
# Check if a cluster already exists
kind get clusters

# If no clusters listed, create one:
kind create cluster --name lab6
# This takes ~1-2 minutes. kubeconfig is configured automatically.

# Verify the cluster is up
kubectl get nodes
# Should show a node in "Ready" state
```

> **Note:** If you get `The connection to the server localhost:8080 was refused`, it means no Kind cluster is running. Run `kind create cluster --name lab6` to create one before proceeding.

### Verify Environment

```bash
# Check kind cluster (if using)
kubectl get nodes

# Check wrk is installed
wrk --version
# If not: sudo apt install wrk

# Or use hey
hey --version
# If not: go install github.com/rakyll/hey@latest

# Check bpftrace
sudo bpftrace --version
```

---

## Part 1: Baseline Deployment (20 min)

### Option A: Using Kind (Kubernetes)

#### 1.1 Deploy Simple HTTP Server

```bash
# Create namespace and results directory
kubectl create namespace lab6
mkdir -p results

# Deploy nginx as baseline
kubectl apply -f manifests/baseline-server.yaml -n lab6

# Wait for pod to be ready
kubectl wait --for=condition=ready pod -l app=baseline-server -n lab6 --timeout=60s

# Port-forward for testing
kubectl port-forward -n lab6 svc/baseline-server 8080:80 &
```

#### 1.2 Verify Server is Working

```bash
# Quick test
curl http://localhost:8080/

# Should return nginx default page
```

### Option B: Using Local Docker

```bash
# Run nginx directly
docker run -d --name baseline-server -p 8080:80 nginx:alpine

# Verify
curl http://localhost:8080/
```

### 1.3 Baseline Load Test

Run a load test to establish baseline latency:

```bash
# Using wrk (recommended)
wrk -t2 -c10 -d30s --latency http://localhost:8080/

# Or using hey
hey -n 10000 -c 10 http://localhost:8080/
```

**Record these baseline numbers:**

| Metric | Value |
|--------|-------|
| p50 latency | |
| p90 latency | |
| p99 latency | |
| Max latency | |
| Requests/sec | |
| Transfer/sec | |

### 1.4 Save Baseline Results

```bash
# Save wrk output
wrk -t2 -c10 -d30s --latency http://localhost:8080/ > results/baseline_wrk.txt

# Or capture hey output with histogram
hey -n 10000 -c 10 http://localhost:8080/ > results/baseline_hey.txt
```

---

## Part 2: Add Sidecar Proxy (25 min)

### Option A: Using Kind (Envoy Sidecar)

#### 2.1 Deploy Server with Envoy Sidecar

```bash
# Deploy server with Envoy sidecar
kubectl apply -f manifests/sidecar-server.yaml -n lab6

# Wait for pod to be ready
kubectl wait --for=condition=ready pod -l app=sidecar-server -n lab6 --timeout=120s

# Port-forward to Envoy's port
kubectl port-forward -n lab6 svc/sidecar-server 8081:80 &
```

#### 2.2 Verify Sidecar is Working

```bash
# Test through sidecar
curl http://localhost:8081/

# Should return same response, but through Envoy
```

### Option B: Using Local Docker (nginx as proxy)

For simplicity, we'll use nginx as a reverse proxy to simulate sidecar behavior:

```bash
# Create proxy config
cat > /tmp/proxy.conf << 'EOF'
server {
    listen 80;
    location / {
        proxy_pass http://backend:80;
        proxy_http_version 1.1;
        proxy_set_header Connection "";
    }
}
EOF

# Create network
docker network create lab6-net

# Rename baseline to backend
docker stop baseline-server
docker run -d --name backend --network lab6-net nginx:alpine

# Run proxy
docker run -d --name proxy \
    --network lab6-net \
    -p 8081:80 \
    -v /tmp/proxy.conf:/etc/nginx/conf.d/default.conf:ro \
    nginx:alpine
```

### 2.3 Sidecar Load Test

Run the same load test against the sidecar endpoint:

```bash
# Using wrk
wrk -t2 -c10 -d30s --latency http://localhost:8081/

# Or using hey
hey -n 10000 -c 10 http://localhost:8081/
```

**Record sidecar numbers:**

| Metric | Baseline | With Sidecar | Difference |
|--------|----------|--------------|------------|
| p50 latency | | | |
| p90 latency | | | |
| p99 latency | | | |
| Max latency | | | |
| Requests/sec | | | |

### 2.4 Save Sidecar Results

```bash
wrk -t2 -c10 -d30s --latency http://localhost:8081/ > results/sidecar_wrk.txt
```

---

## Part 3: eBPF Attribution (25 min)

Now we'll use eBPF to understand WHERE the latency is added.

### 3.1 Trace Syscall Latency

This measures time spent in send/recv syscalls:

```bash
# Run this in one terminal while load test runs in another
sudo bpftrace scripts/syscall_latency.bt > results/syscall_trace.txt &

# Run load test
wrk -t2 -c10 -d10s http://localhost:8081/

# Stop tracing
sudo pkill bpftrace
```

### 3.2 Trace TCP Processing Time

```bash
# Trace TCP connection setup time
sudo bpftrace scripts/tcp_latency.bt > results/tcp_trace.txt &

# Run load test
wrk -t2 -c10 -d10s http://localhost:8081/

# Stop tracing
sudo pkill bpftrace
```

### 3.3 Trace Network Stack Time

```bash
# Trace time in kernel network stack
sudo bpftrace scripts/network_stack.bt > results/network_trace.txt &

# Run load test
wrk -t2 -c10 -d10s http://localhost:8081/

# Stop tracing
sudo pkill bpftrace
```

### 3.4 Analyze Trace Results

Look at the histogram outputs:

```bash
# Compare syscall times
cat results/syscall_trace.txt

# Look for patterns:
# - Bimodal distribution? (fast and slow paths)
# - Long tail? (occasional delays)
# - Different between baseline and sidecar?
```

---

## Part 4: Advanced Attribution (Optional)

### 4.1 Flamegraph Generation

Generate a CPU flamegraph during the load test:

```bash
# Get FlameGraph tools (one-time setup)
git clone --depth=1 https://github.com/brendangregg/FlameGraph.git

# Record CPU profile
sudo perf record -F 99 -g -p $(pgrep envoy) -- sleep 30 &

# Run load test
wrk -t2 -c10 -d30s http://localhost:8081/

# Generate flamegraph
sudo perf script > out.perf
./FlameGraph/stackcollapse-perf.pl out.perf > out.folded
./FlameGraph/flamegraph.pl out.folded > results/sidecar_flamegraph.svg
```

### 4.2 Envoy Statistics

If using Envoy, check its internal metrics:

```bash
# Get Envoy stats
kubectl exec -n lab6 $(kubectl get pod -n lab6 -l app=sidecar-server -o jsonpath='{.items[0].metadata.name}') -c envoy -- curl -s localhost:15000/stats | grep -E '(latency|connection|request)'
```

### 4.3 Compare CPU Usage

```bash
# During baseline test
kubectl top pod -n lab6 -l app=baseline-server

# During sidecar test
kubectl top pod -n lab6 -l app=sidecar-server
```

---

## Part 5: Analysis Questions

Answer these questions in your report:

### Latency Analysis

1. **What is the p99 latency overhead of the sidecar?**
   - Absolute increase (ms)
   - Percentage increase

2. **How does the overhead vary with load?**
   - Test with -c5, -c20, -c50 connections
   - Does overhead increase with load?

3. **Where does the extra latency come from?**
   - User space (proxy processing)
   - Kernel (iptables, TCP stack)
   - Connection setup

### Attribution Analysis

4. **What do the eBPF traces show?**
   - Which histogram shows the biggest increase?
   - Is there a bimodal distribution?

5. **What kernel functions are hot?**
   - From flamegraph or bpftrace
   - TCP functions? iptables? Scheduler?

### Recommendations

6. **When is this overhead acceptable?**
   - What baseline latency makes 1ms overhead tolerable?
   - What features justify the overhead?

7. **How could the overhead be reduced?**
   - Connection pooling?
   - eBPF-based mesh?
   - Different proxy configuration?

---

## Deliverables

### 1. Results Directory

```
results/
├── baseline_wrk.txt      # Baseline load test output
├── sidecar_wrk.txt       # Sidecar load test output
├── syscall_trace.txt     # Syscall latency trace
├── tcp_trace.txt         # TCP latency trace
├── network_trace.txt     # Network stack trace
└── comparison_table.csv  # Summary comparison
```

### 2. Report: `lab6_report.md`

Use the template provided. Must include:

1. **Methodology** — How you ran the tests
2. **Comparison Table** — Baseline vs sidecar numbers
3. **Graphs** — Latency histograms (if possible)
4. **Attribution** — Where the overhead comes from
5. **Analysis** — Interpretation and recommendations

### 3. Comparison Table (CSV)

```csv
metric,baseline,sidecar,difference,pct_increase
p50_ms,0.5,1.2,0.7,140
p90_ms,1.0,2.5,1.5,150
p99_ms,2.0,8.0,6.0,300
max_ms,5.0,25.0,20.0,400
rps,5000,3500,-1500,-30
```

---

## Cleanup

```bash
# Kind cleanup
kubectl delete namespace lab6
kubectl delete -f manifests/

# Docker cleanup
docker stop baseline-server proxy backend 2>/dev/null
docker rm baseline-server proxy backend 2>/dev/null
docker network rm lab6-net 2>/dev/null
```

---

## Grading Rubric

| Criterion | Points |
|-----------|--------|
| Baseline measurement complete | 15 |
| Sidecar measurement complete | 20 |
| eBPF attribution attempted | 25 |
| Comparison table with all metrics | 15 |
| Analysis explains overhead sources | 15 |
| Recommendations are actionable | 10 |

---

## Troubleshooting

### kubectl "connection to localhost:8080 was refused"
- No Kind cluster is running. Check with: `kind get clusters`
- If empty, create one: `kind create cluster --name lab6`
- If cluster exists but kubectl can't connect: `kind export kubeconfig --name lab6`

### wrk "Connection refused"
- Check if service is running: `curl http://localhost:8080/`
- Check port-forward is active: `ps aux | grep port-forward`

### eBPF "Permission denied"
- Run with sudo: `sudo bpftrace ...`
- Check kernel version: `uname -r` (need 5.x+)

### Envoy not starting
- Check logs: `kubectl logs -n lab6 <pod> -c envoy`
- Check config: Envoy is sensitive to configuration errors

### Low RPS numbers
- Increase wrk threads: `-t4` instead of `-t2`
- Check CPU usage during test
- Ensure network is not the bottleneck

---

## Resources

- [wrk documentation](https://github.com/wg/wrk)
- [hey documentation](https://github.com/rakyll/hey)
- [Envoy documentation](https://www.envoyproxy.io/docs/envoy/latest/)
- [bpftrace reference](https://github.com/iovisor/bpftrace/blob/master/docs/reference_guide.md)
