# Week 7 Facilitator Notes (Instructor Only)

> Week 7 theme: Network data plane observability — understanding where latency lives.
>
> Primary risk: Network setups can be fragile. Have fallbacks ready.

## Pre-Class Checklist (1 day before)

### Environment Verification
- [ ] Verify kind cluster still works from Week 6
- [ ] Test Envoy sidecar deployment
- [ ] Verify wrk/hey load testing tools
- [ ] Test bpftrace network tracing scripts
- [ ] Prepare fallback: local Docker setup without Kubernetes

### Student Readiness
- [ ] Confirm Week 6 deliverables received
- [ ] Identify any students with broken environments
- [ ] Review assigned presenter's slides

### Demo Preparation
- [ ] Prepare live demo script
- [ ] Have backup recordings/screenshots
- [ ] Test screen sharing for remote students

## Session Structure (90 + 90)

### Seminar (90 min)

**1) Student Presentation** (25 min)

Topic: Service mesh overhead and observability

Must include:
- Latency numbers from at least one benchmark study
- Comparison of different approaches (iptables vs eBPF)
- Trade-off discussion

Watch for:
- Vague claims like "it adds some latency" — push for numbers
- Confusion between L4 and L7 proxying
- Missing discussion of connection pooling/reuse

**2) Cross-Examination** (20 min)

Suggested questions:
- "If Envoy adds 1ms p50 latency, what does that translate to for p99?"
- "How many iptables rules does Istio create per service?"
- "What happens to latency under connection exhaustion?"

Push for concrete mechanisms:
- "Which kernel function is responsible for iptables lookup?"
- "What makes XDP faster than the regular network path?"

**3) Instructor Synthesis** (20 min)

Key points to ensure students understand:

1. **Network latency is often software-bound**
   - Hardware is fast (us scale)
   - Software adds ms (TCP stack, proxies, TLS)

2. **The container network path is complex**
   - veth → bridge → iptables → NIC
   - Each hop adds CPU cycles

3. **Service mesh is a trade-off**
   - Security and observability vs latency
   - Decision depends on baseline latency

Board diagram to draw:
```
Pod A → veth → bridge → iptables → veth → Pod B (same node)
Pod A → veth → bridge → NIC → wire → NIC → bridge → veth → Pod B (diff node)
```

**4) Live Demo** (15 min)

Run one of these:

Option A: bpftrace syscall tracing
```bash
# Terminal 1: Start trace
sudo bpftrace -e '
tracepoint:syscalls:sys_enter_sendto { @start[tid] = nsecs; }
tracepoint:syscalls:sys_exit_sendto /@start[tid]/ {
    @send_us = hist((nsecs - @start[tid]) / 1000);
    delete(@start[tid]);
}'

# Terminal 2: Generate traffic
curl http://localhost:8080/
```

Option B: Show Envoy stats
```bash
# Get Envoy admin stats
curl localhost:15000/stats | grep -E '(latency|rq_total|cx_active)'
```

**5) Quick Write** (10 min)

Prompt: "Your service's p99 latency just increased by 5ms after enabling Istio. List three specific things you would check, in order."

### Lab (90 min)

**1) Warmup** (10 min)
- Quick review: Everyone has kind cluster?
- Fallback plan: Use local Docker for anyone with cluster issues

**2) Baseline Deployment** (20 min)
- Walk through manifest together
- Ensure everyone can port-forward and test
- Run first load test together

**3) Sidecar Deployment** (25 min)
- Deploy sidecar version
- Common issues:
  - Envoy config errors → provide working config
  - Port-forward conflicts → use different ports
  - Probe failures → check health endpoints

**4) eBPF Attribution** (25 min)
- This is the critical learning moment
- Walk through first trace together
- Let students run independently after

**5) Wrap-up** (10 min)
- Quick poll: What was your p99 increase?
- Discussion: Was it expected?
- Preview homework requirements

## Common Issues and Fixes

### Envoy Not Starting

Symptoms:
- CrashLoopBackOff
- Probe failures

Fixes:
- Check Envoy config syntax
- Ensure admin port (15000) is not conflicting
- Check resource limits

```bash
kubectl logs -n lab6 <pod> -c envoy
kubectl describe pod -n lab6 <pod>
```

### wrk Low Throughput

Symptoms:
- Much lower RPS than expected
- "Connection refused" errors

Fixes:
- Check server is ready: `kubectl get pods -n lab6`
- Check port-forward: `netstat -tlnp | grep 8080`
- Try local Docker setup instead

### bpftrace Permission Errors

Symptoms:
- "Permission denied"
- "Failed to attach probe"

Fixes:
- Run with sudo
- Check kernel version (need 5.x)
- Some tracepoints may not exist on older kernels

### Network Namespace Confusion

Students often confuse:
- Host network namespace
- Pod network namespace
- Container network namespace

Draw a clear diagram showing which namespace each component runs in.

## What Counts as a Good Week 7 Result

Students should produce:

1. **Clear comparison table**
   - p50, p90, p99, max for both baseline and sidecar
   - RPS and CPU usage

2. **Quantified overhead**
   - "Sidecar adds 0.8ms p50, 3.2ms p99"
   - Not just "sidecar adds latency"

3. **Attribution evidence**
   - eBPF traces showing where time goes
   - Or at minimum, reasoned hypothesis based on architecture

4. **Actionable recommendations**
   - When to use/not use sidecar
   - Based on their measurements, not generic advice

## Alternative Lab Setup (Fallback)

If Kubernetes setup fails for many students:

### Local Docker Only

```bash
# Backend
docker run -d --name backend -p 8080:80 nginx:alpine

# Test baseline
wrk -t2 -c10 -d30s http://localhost:8080/

# Add proxy
docker run -d --name proxy -p 8081:80 \
  -v $(pwd)/nginx-proxy.conf:/etc/nginx/conf.d/default.conf:ro \
  --link backend nginx:alpine

# Test with proxy
wrk -t2 -c10 -d30s http://localhost:8081/
```

This gives a simpler setup that still demonstrates the core concept.

## After-Class Actions

1. **Collect submissions:**
   - Lab 6 report + results files
   - Week 7 memo

2. **Review for common misunderstandings:**
   - Confusion about what adds latency
   - Incorrect attribution

3. **Prepare Week 8:**
   - Storage and tail latency
   - fsync experiments need specific setup

## Grading Notes

### Lab Report Rubric (100 points)

| Criterion | Points | Notes |
|-----------|--------|-------|
| Baseline measurement | 15 | Must have p50, p90, p99 |
| Sidecar measurement | 20 | Same metrics, comparable conditions |
| eBPF attribution | 25 | Attempted tracing, some evidence |
| Comparison table | 15 | Complete, correct calculation |
| Analysis quality | 15 | Explains why, not just what |
| Recommendations | 10 | Actionable, based on evidence |

### Common Deductions

- -10: Missing p99 (only has p50)
- -10: No eBPF tracing attempted
- -5: Generic recommendations not based on their data
- -5: No load sensitivity analysis

### Extra Credit Opportunities

- +5: Tested multiple load levels
- +5: Generated flamegraph
- +5: Compared different proxy configurations
