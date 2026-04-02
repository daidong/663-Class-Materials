# Lab 6 Report: p99 Latency Breakdown

**Name:**  
**Date:**  

---

## 1. Environment

| Property | Value |
|----------|-------|
| OS / Distro | |
| Kernel version | |
| Container runtime | Docker / containerd |
| Kubernetes | kind / minikube / N/A |
| Load testing tool | wrk / hey / ab |
| eBPF tool | bpftrace / bcc |

### Setup Notes
<Any issues encountered during setup and how you resolved them>

---

## 2. Methodology

### 2.1 Test Configuration

| Parameter | Value |
|-----------|-------|
| Number of threads | |
| Number of connections | |
| Test duration (seconds) | |
| Request type | GET / POST |
| Request size | |

### 2.2 Baseline Setup

<Describe your baseline server configuration>
- What server (nginx, custom, etc.)?
- Running in Kubernetes or Docker?
- Any specific configuration?

### 2.3 Sidecar Setup

<Describe your sidecar configuration>
- What proxy (Envoy, nginx, etc.)?
- How was traffic intercepted (iptables, manual)?
- Any specific configuration (TLS, HTTP/2)?

---

## 3. Results

### 3.1 Latency Comparison

| Metric | Baseline | With Sidecar | Absolute Δ | % Increase |
|--------|----------|--------------|------------|------------|
| p50 latency (ms) | | | | |
| p75 latency (ms) | | | | |
| p90 latency (ms) | | | | |
| p99 latency (ms) | | | | |
| Max latency (ms) | | | | |
| Requests/sec | | | | |
| Transfer/sec (MB) | | | | |

### 3.2 Latency Distribution

<Include histogram or CDF if available>

```
Baseline latency distribution:
[paste wrk/hey output or description]

Sidecar latency distribution:
[paste wrk/hey output or description]
```

### 3.3 Resource Usage Comparison


Get resource usage while the load test is running:
```bash
# Kind does not include metrics-server; use this instead:
kubectl exec -n lab6 <pod-name> -- top -bn1 | head -5
```

| Resource | Baseline | With Sidecar |
|----------|----------|--------------|
| CPU usage (%) | | |
| Memory usage (MB) | | |
| Network I/O | | |

---

## 4. eBPF Attribution

### 4.1 Syscall Latency

<Paste relevant portions of syscall trace>

```
Observations:
- 
- 
```

### 4.2 TCP Processing Time

<Paste relevant portions of TCP trace>

```
Observations:
- 
- 
```

### 4.3 Network Stack Time

<Paste relevant portions of network stack trace>

```
Observations:
- 
- 
```

### 4.4 Attribution Summary

| Component | Estimated Overhead | Evidence |
|-----------|-------------------|----------|
| Extra network hop (app→envoy→app) | | |
| Connection to sidecar | | |
| Proxy processing | | |
| TLS (if enabled) | | |
| Other | | |
| **Total** | | |

---

## 5. Analysis

### 5.1 Primary Overhead Source

<What is the main source of added latency? Provide evidence.>

### 5.2 p99 vs p50 Analysis

<Why is the p99 overhead often larger than p50 overhead?>

### 5.3 Load Sensitivity

<Did you test at different load levels? How did overhead change?>

| Connections | Baseline p99 | Sidecar p99 | Overhead |
|-------------|--------------|-------------|----------|
| 5 | | | |
| 10 | | | |
| 20 | | | |
| 50 | | | |

---

## 6. Recommendations

### 6.1 When to Use Sidecar Proxy

<Based on your measurements, when is the overhead acceptable?>

- Acceptable when:
  - 
  - 

- Not acceptable when:
  - 
  - 

### 6.2 Potential Optimizations

<How could the overhead be reduced?>

1. 
2. 
3. 

---

## 7. Reflection

### 7.1 What Surprised You?

<One thing that was unexpected>

### 7.2 Connection to Course Themes

<How does this connect to what you learned about containers, cgroups, or eBPF?>

### 7.3 Application to Final Project

<If applicable, how might this affect your system case?>

---

## 8. Raw Data

### 8.1 Baseline Test Output

```
<Paste complete wrk/hey output>
```

### 8.2 Sidecar Test Output

```
<Paste complete wrk/hey output>
```

### 8.3 eBPF Trace Samples

<Paste relevant trace excerpts>

